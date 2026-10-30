// EH3D-NODE-BUNGARD-AUX - main/sensors.cpp: the pins of the head. GPL-3.0-or-later.
#include "sensors.hpp"

#include "driver/gpio.h"
#include "esp_attr.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"
#include "sdkconfig.h"

#include "../core/spindle.hpp"

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
}  // namespace

void init(const bungard_aux::Settings& s) {
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
    ESP_LOGI(TAG, "sensors ready (laser and temperature drivers are not written yet)");
}

bool laser_read(double&, bool&) { return false; }

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

bool temperature_read(double&) { return false; }

void protect_set(bool open) {
#if CONFIG_AUX_PROTECT_GPIO >= 0
    gpio_set_level(static_cast<gpio_num_t>(CONFIG_AUX_PROTECT_GPIO), open ? 0 : 1);
#else
    (void)open;
#endif
}

}  // namespace sensors
