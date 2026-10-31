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

#include <mutex>
#include <string>
#include <vector>

#include "../core/laser_hw.hpp"
#include "../core/onewire.hpp"
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

#if CONFIG_AUX_RPM_GPIO >= 0
void IRAM_ATTR on_pulse(void*) {
    ring[ring_head % kRing] = static_cast<std::uint64_t>(esp_timer_get_time());
    ring_head = ring_head + 1;
}
#endif

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
std::mutex laser_lock;  // one thing at a time talks to the sensor: a measure, a calibration or the status

constexpr std::uint16_t kLaserDev = 0x52;  // the 8-bit address the ST driver uses for 0x29

// the last reading the sensor gave, for the status of the node
bungard_aux::LaserReading last_reading;
std::int64_t last_reading_us = -1;

/// Writes the registers of a list (a word goes high byte first, as the platform layer of the library does).
bool laser_write(const std::vector<bungard_aux::RegWrite>& list) {
    for (const bungard_aux::RegWrite& w : list) {
        const std::uint8_t st = w.bytes == 1 ? VL53L1X_ULP_WrByte(kLaserDev, w.reg, static_cast<std::uint8_t>(w.value)) : VL53L1X_ULP_WrWord(kLaserDev, w.reg, w.value);
        if (st != 0) return false;
    }
    return true;
}

bungard_aux::LaserHw laser_hw_of(const bungard_aux::Settings& c, bool write_xtalk) {
    bungard_aux::LaserHw hw;
    hw.mode = c.laser_distance_mode;
    hw.budget_ms = static_cast<int>(c.laser_timing_budget_ms);
    hw.roi_w = c.laser_roi_w;
    hw.roi_h = c.laser_roi_h;
    hw.roi_center = static_cast<int>(c.laser_roi_center);
    std::uint8_t optical = 0;
    if (VL53L1X_ULP_RdByte(kLaserDev, 0x013E, &optical) == 0 && optical > 0) hw.optical_center = optical;  // the centre the sensor itself reports
    hw.write_xtalk = write_xtalk;
    hw.xtalk_cps = c.laser_cal_xtalk_cps;
    return hw;
}

void laser_start() {
    if (!cfg.laser_enabled || !bus_up()) return;
    std::lock_guard<std::mutex> guard(laser_lock);
    uint16_t id = 0;
    if (vl53l1x_init(&laser, bus, 0x29) != ESP_OK || vl53l1x_get_sensor_id(&laser, &id) != ESP_OK || id != 0xEACC) {
        ESP_LOGE(TAG, "no VL53L1X answers on the I2C bus (id 0x%04x)", static_cast<unsigned>(id));
        return;
    }
    // the distance mode, the timing budget, the area and the crosstalk are written after the library's own set-up, from the settings
    const bungard_aux::LaserHw hw = laser_hw_of(cfg, cfg.laser_cal_xtalk_cps > 0.0);
    if (vl53l1x_sensor_init(&laser) != ESP_OK || !laser_write(bungard_aux::registers_for(hw)) ||
        vl53l1x_set_intermeasurement_ms(&laser, static_cast<uint32_t>(bungard_aux::inter_measurement_ms(hw.budget_ms))) != ESP_OK || vl53l1x_start(&laser) != ESP_OK) {
        ESP_LOGE(TAG, "the VL53L1X did not start");
        return;
    }
    laser_up = true;
    ESP_LOGI(TAG, "laser ready (%s mode, %d ms, area %dx%d)", bungard_aux::distance_mode_text(hw.mode), hw.budget_ms, hw.roi_w, hw.roi_h);
}

/// One reading, with the lock held by the caller. `spads` also asks the sensor how many detectors were looking (for the crosstalk calibration).
bool laser_read_locked(bungard_aux::LaserReading& out, bool spads, uint32_t timeout_ms) {
    if (!laser_up) return false;
    vl53l1x_result_t r = {};
    if (vl53l1x_read(&laser, &r, timeout_ms) != ESP_OK) return false;
    out = {};
    out.distance_mm = r.distance_mm;
    out.status = r.status;
    out.valid = r.status == 0;
    out.sigma_mm = r.sigma_mm;
    out.signal_kcps = r.signal_kcps;
    if (spads) {
        std::uint16_t word = 0;
        std::uint16_t corrected = 0;
        if (VL53L1X_ULP_RdWord(kLaserDev, 0x008C, &word) == 0) out.spads = static_cast<double>(word >> 8);
        if (VL53L1X_ULP_RdWord(kLaserDev, 0x0098, &corrected) == 0) out.signal_kcps = static_cast<double>(corrected) * 8.0;  // the signal as the crosstalk calibration needs it
    }
    last_reading = out;
    last_reading_us = esp_timer_get_time();
    return true;
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

/// The wire as the search of core/onewire.hpp wants it.
struct Wire {
    bool reset() { return wire_reset(); }
    void write_byte(std::uint8_t b) { wire_write(b); }
    bool read_bit() { return wire_read_bit(); }
    void write_bit(bool b) { wire_write_bit(b); }
};

std::vector<bungard_aux::Rom> wire_roms;
bool wire_chosen = false;
bool wire_guess = false;
bungard_aux::Rom wire_rom{};

/// Addresses the sensor of the spindle (MATCH ROM) or, when it is the only one on the wire, all of them (SKIP ROM).
bool wire_select() {
    if (!wire_reset()) return false;
    if (wire_roms.size() <= 1 && !wire_chosen) return false;
    if (wire_roms.size() == 1) {
        wire_write(0xCC);
    } else {
        wire_write(0x55);
        for (std::uint8_t b : wire_rom) wire_write(b);
    }
    return true;
}

/// Looks for the sensors, takes the spindle's and writes the resolution of the settings into it. Called once, when the wire starts and again when no sensor answers.
void wire_discover() {
    Wire bus;
    wire_roms.clear();
    wire_chosen = false;
    bungard_aux::search_roms(bus, wire_roms);
    wire_chosen = bungard_aux::choose_sensor(wire_roms, cfg.temp_rom, wire_rom, wire_guess);
    ESP_LOGI(TAG, "one-wire: %u DS18B20 found, %s", static_cast<unsigned>(wire_roms.size()), wire_chosen ? bungard_aux::rom_text(wire_rom).c_str() : "none chosen");
    if (!wire_chosen || !wire_select()) return;
    wire_write(0x4E);  // WRITE SCRATCHPAD: the alarm bytes and the configuration (the resolution)
    wire_write(0x7F);
    wire_write(0x80);
    wire_write(bungard_aux::resolution_config(cfg.temp_resolution));
}

/// One conversion of the DS18B20 of the spindle: 94 ms at 9 bits, 750 ms at 12.
bool ds18b20_read(double& celsius) {
    if (wire_roms.empty() || !wire_chosen) wire_discover();
    if (!wire_chosen) return false;
    if (!wire_select()) {
        wire_roms.clear();  // nobody answers: it is looked for again at the next reading
        wire_chosen = false;
        return false;
    }
    wire_write(0x44);
    vTaskDelay(pdMS_TO_TICKS(bungard_aux::conversion_ms(cfg.temp_resolution) + 20));
    if (!wire_select()) return false;
    wire_write(0xBE);
    std::uint8_t pad[9];
    for (auto& b : pad) b = wire_read();
    return bungard_aux::thermometer_celsius(wire_rom, pad, celsius);
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

bool laser_read(bungard_aux::LaserReading& out) {
    std::lock_guard<std::mutex> guard(laser_lock);
    return laser_read_locked(out, false, 300);
}

bool laser_last(bungard_aux::LaserReading& out, long& age_ms) {
    std::lock_guard<std::mutex> guard(laser_lock);
    if (!laser_up || last_reading_us < 0) return false;
    out = last_reading;
    age_ms = static_cast<long>((esp_timer_get_time() - last_reading_us) / 1000);
    return true;
}

bool laser_collect(int count, uint32_t timeout_ms, std::vector<bungard_aux::LaserReading>& out) {
    std::lock_guard<std::mutex> guard(laser_lock);
    if (!laser_up) return false;
    const std::int64_t deadline = esp_timer_get_time() + static_cast<std::int64_t>(timeout_ms) * 1000;
    while (static_cast<int>(out.size()) < count && esp_timer_get_time() < deadline) {
        bungard_aux::LaserReading r;
        if (laser_read_locked(r, true, 300)) out.push_back(r);
        else vTaskDelay(pdMS_TO_TICKS(5));
    }
    return !out.empty();
}

bool laser_set_xtalk(double cps) {
    std::lock_guard<std::mutex> guard(laser_lock);
    if (!laser_up) return false;
    bungard_aux::LaserHw hw = laser_hw_of(cfg, true);
    hw.xtalk_cps = cps;
    std::vector<bungard_aux::RegWrite> list;
    for (const bungard_aux::RegWrite& w : bungard_aux::registers_for(hw)) {
        if (w.reg == 0x0016 || w.reg == 0x0018 || w.reg == 0x001A) list.push_back(w);   // only the crosstalk registers
    }
    // like the ST driver, the crosstalk is changed while the sensor is not ranging
    const bool ok = vl53l1x_stop(&laser) == ESP_OK && laser_write(list);
    vl53l1x_start(&laser);
    return ok;
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

std::vector<std::string> temp_sensors_found() {
    std::vector<std::string> out;
#if CONFIG_AUX_ONEWIRE_GPIO >= 0
    for (const auto& r : wire_roms) out.push_back(bungard_aux::rom_text(r));
#endif
    return out;
}

std::string temp_sensor_used() {
#if CONFIG_AUX_ONEWIRE_GPIO >= 0
    return wire_chosen ? bungard_aux::rom_text(wire_rom) : std::string();
#else
    return std::string();
#endif
}

bool temp_sensor_ambiguous() {
#if CONFIG_AUX_ONEWIRE_GPIO >= 0
    return wire_chosen && wire_guess;
#else
    return false;
#endif
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
