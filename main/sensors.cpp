// EH3D-NODE-BUNGARD-AUX - main/sensors.cpp: the pins and the sensors of the head. GPL-3.0-or-later.
// Written against ESP-IDF 5.5 and NOT built or run on a board: what the readings mean (the checksums, the scratchpad of the DS18B20, the divider of the NTC, the word of the
// MLX90614, the median of the laser) is in core/ and is tested on a computer; this file only moves bytes and voltages.
#include "sensors.hpp"

#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_attr.h"
#include "esp_log.h"
#include "esp_rom_sys.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"
#include "freertos/task.h"
#include "sdkconfig.h"
#include "vl53l1x.h"

#include "../core/spindle.hpp"
#include "../core/thermo.hpp"

namespace sensors {

namespace {
const char* TAG = "aux-sensors";

// the interrupt only stores the time of the pulse; the meter is fed from the task that asks for the speed
constexpr int kRing = 32;
volatile std::uint64_t ring[kRing];
volatile int ring_head = 0;
portMUX_TYPE ring_lock = portMUX_INITIALIZER_UNLOCKED;
bungard_aux::RpmMeter* meter = nullptr;
int tail = 0;

void IRAM_ATTR on_pulse(void*) {
    ring[ring_head % kRing] = static_cast<std::uint64_t>(esp_timer_get_time());
    ring_head = ring_head + 1;
}

bungard_aux::Settings cfg;

// ------------------------------------------------------------------------------------------------------------------------------ the shared I2C bus (laser and infrared thermometer)

i2c_master_bus_handle_t bus = nullptr;

bool bus_up() {
#if CONFIG_AUX_I2C_SDA >= 0 && CONFIG_AUX_I2C_SCL >= 0
    if (bus) return true;
    i2c_master_bus_config_t b = {};
    b.i2c_port = I2C_NUM_0;
    b.sda_io_num = static_cast<gpio_num_t>(CONFIG_AUX_I2C_SDA);
    b.scl_io_num = static_cast<gpio_num_t>(CONFIG_AUX_I2C_SCL);
    b.clk_source = I2C_CLK_SRC_DEFAULT;
    b.glitch_ignore_cnt = 7;
    b.flags.enable_internal_pullup = true;
    if (i2c_new_master_bus(&b, &bus) != ESP_OK) {
        bus = nullptr;
        ESP_LOGE(TAG, "the I2C bus did not start");
        return false;
    }
    return true;
#else
    return false;
#endif
}

// ------------------------------------------------------------------------------------------------------------------------------ the laser (VL53L1X)

vl53l1x_t laser = {};
bool laser_up = false;

void laser_start() {
    if (!cfg.laser_enabled || !bus_up()) return;
    uint16_t id = 0;
    if (vl53l1x_init(&laser, bus, 0x29) != ESP_OK || vl53l1x_get_sensor_id(&laser, &id) != ESP_OK || id != 0xEACC) {
        ESP_LOGE(TAG, "no VL53L1X answers on the I2C bus (id 0x%04x)", static_cast<unsigned>(id));
        return;
    }
    if (vl53l1x_sensor_init(&laser) != ESP_OK || vl53l1x_config_long_100ms(&laser) != ESP_OK || vl53l1x_set_intermeasurement_ms(&laser, 100) != ESP_OK ||
        vl53l1x_start(&laser) != ESP_OK) {
        ESP_LOGE(TAG, "the VL53L1X did not start");
        return;
    }
    laser_up = true;
    ESP_LOGI(TAG, "laser ready");
}

// ------------------------------------------------------------------------------------------------------------------------------ one-wire (DS18B20)

#if CONFIG_AUX_ONEWIRE_GPIO >= 0
portMUX_TYPE wire_lock = portMUX_INITIALIZER_UNLOCKED;
constexpr gpio_num_t kWire = static_cast<gpio_num_t>(CONFIG_AUX_ONEWIRE_GPIO);

void wire_init() {
    gpio_config_t io = {};
    io.pin_bit_mask = 1ULL << CONFIG_AUX_ONEWIRE_GPIO;
    io.mode = GPIO_MODE_INPUT_OUTPUT_OD;
    io.pull_up_en = GPIO_PULLUP_ENABLE;
    gpio_config(&io);
    gpio_set_level(kWire, 1);
}

bool wire_reset() {
    portENTER_CRITICAL(&wire_lock);
    gpio_set_level(kWire, 0);
    esp_rom_delay_us(480);
    gpio_set_level(kWire, 1);
    esp_rom_delay_us(70);
    const bool present = gpio_get_level(kWire) == 0;
    portEXIT_CRITICAL(&wire_lock);
    esp_rom_delay_us(410);
    return present;
}

void wire_write_bit(bool one) {
    portENTER_CRITICAL(&wire_lock);
    gpio_set_level(kWire, 0);
    esp_rom_delay_us(one ? 6 : 60);
    gpio_set_level(kWire, 1);
    esp_rom_delay_us(one ? 64 : 10);
    portEXIT_CRITICAL(&wire_lock);
}

bool wire_read_bit() {
    portENTER_CRITICAL(&wire_lock);
    gpio_set_level(kWire, 0);
    esp_rom_delay_us(6);
    gpio_set_level(kWire, 1);
    esp_rom_delay_us(9);
    const bool one = gpio_get_level(kWire) != 0;
    esp_rom_delay_us(55);
    portEXIT_CRITICAL(&wire_lock);
    return one;
}

void wire_write(std::uint8_t b) {
    for (int i = 0; i < 8; ++i) wire_write_bit((b >> i) & 1);
}

std::uint8_t wire_read() {
    std::uint8_t b = 0;
    for (int i = 0; i < 8; ++i) b |= static_cast<std::uint8_t>(wire_read_bit() ? 1 << i : 0);
    return b;
}

/// One conversion of the only DS18B20 on the wire (the ROM is skipped): it takes 750 ms at 12 bits.
bool ds18b20_read(double& celsius) {
    if (!wire_reset()) return false;
    wire_write(0xCC);
    wire_write(0x44);
    vTaskDelay(pdMS_TO_TICKS(800));
    if (!wire_reset()) return false;
    wire_write(0xCC);
    wire_write(0xBE);
    std::uint8_t pad[9];
    for (auto& b : pad) b = wire_read();
    return bungard_aux::ds18b20_celsius(pad, celsius);
}
#endif

// ------------------------------------------------------------------------------------------------------------------------------ the NTC on an analogue input

#if CONFIG_AUX_NTC_GPIO >= 0
adc_oneshot_unit_handle_t adc = nullptr;
adc_cali_handle_t adc_cali = nullptr;
adc_channel_t adc_channel = ADC_CHANNEL_0;

void adc_init() {
    adc_unit_t unit;
    if (adc_oneshot_io_to_channel(CONFIG_AUX_NTC_GPIO, &unit, &adc_channel) != ESP_OK) {
        ESP_LOGE(TAG, "GPIO %d has no analogue input", CONFIG_AUX_NTC_GPIO);
        return;
    }
    adc_oneshot_unit_init_cfg_t u = {};
    u.unit_id = unit;
    if (adc_oneshot_new_unit(&u, &adc) != ESP_OK) return;
    adc_oneshot_chan_cfg_t c = {};
    c.atten = ADC_ATTEN_DB_12;
    c.bitwidth = ADC_BITWIDTH_DEFAULT;
    adc_oneshot_config_channel(adc, adc_channel, &c);
    adc_cali_curve_fitting_config_t k = {};
    k.unit_id = unit;
    k.chan = adc_channel;
    k.atten = ADC_ATTEN_DB_12;
    k.bitwidth = ADC_BITWIDTH_DEFAULT;
    if (adc_cali_create_scheme_curve_fitting(&k, &adc_cali) != ESP_OK) adc_cali = nullptr;
}

bool ntc_read(double& celsius) {
    if (!adc) return false;
    long sum = 0;
    const int n = 16;
    for (int i = 0; i < n; ++i) {
        int raw = 0;
        if (adc_oneshot_read(adc, adc_channel, &raw) != ESP_OK) return false;
        sum += raw;
    }
    int mv = 0;
    if (!adc_cali || adc_cali_raw_to_voltage(adc_cali, static_cast<int>(sum / n), &mv) != ESP_OK) return false;
    return bungard_aux::ntc_celsius(mv / 1000.0, 3.3, cfg.ntc_series_ohm, cfg.ntc_r25_ohm, cfg.ntc_beta, celsius);
}
#endif

// ------------------------------------------------------------------------------------------------------------------------------ the infrared thermometer (MLX90614)

i2c_master_dev_handle_t mlx = nullptr;

void mlx_start() {
    if (!bus_up()) return;
    i2c_device_config_t d = {};
    d.dev_addr_length = I2C_ADDR_BIT_LEN_7;
    d.device_address = 0x5A;
    d.scl_speed_hz = 100000;  // the MLX90614 talks SMBus: 100 kHz at most
    if (i2c_master_bus_add_device(bus, &d, &mlx) != ESP_OK) mlx = nullptr;
}

bool mlx_read(double& celsius) {
    if (!mlx) return false;
    const std::uint8_t reg = 0x07;  // the temperature of what the sensor looks at
    std::uint8_t in[3] = {};
    if (i2c_master_transmit_receive(mlx, &reg, 1, in, 3, 100) != ESP_OK) return false;
    const std::uint8_t check[5] = {0xB4, reg, 0xB5, in[0], in[1]};
    if (bungard_aux::smbus_pec(check, 5) != in[2]) return false;
    return bungard_aux::mlx90614_celsius(static_cast<std::uint16_t>(in[0] | (in[1] << 8)), celsius);
}
}  // namespace

void init(const bungard_aux::Settings& s) {
    cfg = s;
    static bungard_aux::RpmMeter m(s.rpm_pulses_per_rev, s.rpm_stale_ms);
    meter = &m;
#if CONFIG_AUX_RPM_GPIO >= 0
    if (s.rpm_enabled) {
        gpio_config_t io = {};
        io.pin_bit_mask = 1ULL << CONFIG_AUX_RPM_GPIO;
        io.mode = GPIO_MODE_INPUT;
        io.pull_up_en = GPIO_PULLUP_ENABLE;
        io.intr_type = GPIO_INTR_POSEDGE;
        gpio_config(&io);
        gpio_install_isr_service(0);
        gpio_isr_handler_add(static_cast<gpio_num_t>(CONFIG_AUX_RPM_GPIO), on_pulse, nullptr);
    }
#endif
#if CONFIG_AUX_PROTECT_GPIO >= 0
    gpio_config_t out = {};
    out.pin_bit_mask = 1ULL << CONFIG_AUX_PROTECT_GPIO;
    out.mode = GPIO_MODE_OUTPUT;
    gpio_config(&out);
    gpio_set_level(static_cast<gpio_num_t>(CONFIG_AUX_PROTECT_GPIO), 0);  // low until the node has checked the spindle: the spindle cannot run before
#endif
    laser_start();
#if CONFIG_AUX_ONEWIRE_GPIO >= 0
    if (s.temp_sensor == bungard_aux::TempSensor::Ds18b20) wire_init();
#endif
#if CONFIG_AUX_NTC_GPIO >= 0
    if (s.temp_sensor == bungard_aux::TempSensor::Ntc) adc_init();
#endif
    if (s.temp_sensor == bungard_aux::TempSensor::Mlx90614) mlx_start();
    ESP_LOGI(TAG, "sensors ready (laser %s)", laser_up ? "up" : "not found");
}

bool laser_read(double& distance_mm, bool& valid) {
    if (!laser_up) return false;
    vl53l1x_result_t r = {};
    if (vl53l1x_read(&laser, &r, 300) != ESP_OK) return false;
    distance_mm = r.distance_mm;
    valid = r.status == 0;
    return true;
}

double rpm_now() {
    if (!meter) return 0.0;
    int head;
    portENTER_CRITICAL(&ring_lock);
    head = ring_head;
    portEXIT_CRITICAL(&ring_lock);
    while (tail < head) {
        meter->pulse(ring[tail % kRing]);
        ++tail;
    }
    return meter->rpm(static_cast<std::uint64_t>(esp_timer_get_time()));
}

bool temperature_read(double& celsius) {
    switch (cfg.temp_sensor) {
#if CONFIG_AUX_ONEWIRE_GPIO >= 0
        case bungard_aux::TempSensor::Ds18b20: return ds18b20_read(celsius);
#endif
#if CONFIG_AUX_NTC_GPIO >= 0
        case bungard_aux::TempSensor::Ntc: return ntc_read(celsius);
#endif
        case bungard_aux::TempSensor::Mlx90614: return mlx_read(celsius);
        default: return false;
    }
}

void protect_set(bool open) {
#if CONFIG_AUX_PROTECT_GPIO >= 0
    gpio_set_level(static_cast<gpio_num_t>(CONFIG_AUX_PROTECT_GPIO), open ? 0 : 1);
#else
    (void)open;
#endif
}

}  // namespace sensors
