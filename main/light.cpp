// EH3D-NODE-BUNGARD-AUX - main/light.cpp: the ring of LEDs around the camera. The rules (levels, ceiling, sectors, colours, cues, the timer) are in core/light.hpp and are tested on a
// computer; this file only sends the bytes to the LEDs with the RMT peripheral (the WS2812 wants a pulse of 0.35 us for a 0 and of 0.9 us for a 1) and keeps the time. GPL-3.0-or-later.
// Written against ESP-IDF 5.5 and built with it; it has not been run on a board.
#include "light.hpp"

#include <cstring>
#include <vector>

#include "driver/rmt_encoder.h"
#include "driver/rmt_tx.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "sdkconfig.h"

#include "../core/light.hpp"

namespace {

const char* TAG = "aux-light";
constexpr std::uint32_t kResolutionHz = 10000000;  // 10 MHz: a tick is 0.1 us

bungard_aux::Settings settings;
bungard_aux::LightSettings cfg;
bungard_aux::LightState state;
bungard_aux::CueRun cue_run;
bungard_aux::AutoOff auto_off;
SemaphoreHandle_t lock = nullptr;
bool ready = false;

rmt_channel_handle_t channel = nullptr;
rmt_encoder_handle_t encoder = nullptr;

// ------------------------------------------------------------------------------------------------------------------------------ the encoder of the WS2812
// The bytes of the frame go through the bytes encoder (a bit is one symbol), and when they are all out a long low level latches the LEDs.

struct StripEncoder {
    rmt_encoder_t base;
    rmt_encoder_t* bytes;
    rmt_encoder_t* copy;
    int state;
    rmt_symbol_word_t reset;
};

size_t IRAM_ATTR strip_encode(rmt_encoder_t* e, rmt_channel_handle_t ch, const void* data, size_t size, rmt_encode_state_t* out) {
    auto* s = __containerof(e, StripEncoder, base);
    rmt_encode_state_t session = RMT_ENCODING_RESET, status = RMT_ENCODING_RESET;
    size_t encoded = 0;
    switch (s->state) {
        case 0:
            encoded += s->bytes->encode(s->bytes, ch, data, size, &session);
            if (session & RMT_ENCODING_COMPLETE) s->state = 1;
            if (session & RMT_ENCODING_MEM_FULL) {
                status = static_cast<rmt_encode_state_t>(status | RMT_ENCODING_MEM_FULL);
                break;
            }
            [[fallthrough]];
        case 1:
            encoded += s->copy->encode(s->copy, ch, &s->reset, sizeof(s->reset), &session);
            if (session & RMT_ENCODING_COMPLETE) {
                s->state = 0;
                status = static_cast<rmt_encode_state_t>(status | RMT_ENCODING_COMPLETE);
            }
            if (session & RMT_ENCODING_MEM_FULL) status = static_cast<rmt_encode_state_t>(status | RMT_ENCODING_MEM_FULL);
            break;
    }
    *out = status;
    return encoded;
}

esp_err_t strip_reset(rmt_encoder_t* e) {
    auto* s = __containerof(e, StripEncoder, base);
    rmt_encoder_reset(s->bytes);
    rmt_encoder_reset(s->copy);
    s->state = 0;
    return ESP_OK;
}

esp_err_t strip_del(rmt_encoder_t* e) {
    auto* s = __containerof(e, StripEncoder, base);
    rmt_del_encoder(s->bytes);
    rmt_del_encoder(s->copy);
    delete s;
    return ESP_OK;
}

bool make_encoder() {
    auto* s = new StripEncoder();
    s->base.encode = strip_encode;
    s->base.reset = strip_reset;
    s->base.del = strip_del;
    rmt_bytes_encoder_config_t bc = {};
    bc.bit0 = {.duration0 = 3, .level0 = 1, .duration1 = 9, .level1 = 0};   // 0.3 us high, 0.9 us low
    bc.bit1 = {.duration0 = 9, .level0 = 1, .duration1 = 3, .level1 = 0};   // 0.9 us high, 0.3 us low
    bc.flags.msb_first = 1;
    if (rmt_new_bytes_encoder(&bc, &s->bytes) != ESP_OK) { delete s; return false; }
    rmt_copy_encoder_config_t cc = {};
    if (rmt_new_copy_encoder(&cc, &s->copy) != ESP_OK) { rmt_del_encoder(s->bytes); delete s; return false; }
    const std::uint32_t ticks = kResolutionHz / 1000000 * 50 / 2;  // 50 us of low level latch the frame
    s->reset = {.duration0 = static_cast<std::uint16_t>(ticks), .level0 = 0, .duration1 = static_cast<std::uint16_t>(ticks), .level1 = 0};
    encoder = &s->base;
    return true;
}

// ------------------------------------------------------------------------------------------------------------------------------ showing

std::vector<std::uint8_t> shown;  // what the LEDs hold now

void push(const std::vector<std::uint8_t>& frame) {
    if (frame == shown) return;
    rmt_transmit_config_t tx = {};
    tx.loop_count = 0;
    if (rmt_transmit(channel, encoder, frame.data(), frame.size(), &tx) == ESP_OK) {
        rmt_tx_wait_all_done(channel, 100);
        shown = frame;
    }
}

std::uint64_t now_ms() { return static_cast<std::uint64_t>(esp_timer_get_time() / 1000); }

void show_task(void*) {
    while (true) {
        xSemaphoreTake(lock, portMAX_DELAY);
        const std::uint64_t t = now_ms();
        if (state.on && auto_off.expired(t)) {
            state.on = false;
            auto_off.disarm();
            ESP_LOGI(TAG, "the light turned itself off");
        }
        std::vector<std::uint8_t> frame;
        if (cue_run.active(t)) frame = bungard_aux::render_cue(cfg, state, cue_run.cue, t - cue_run.started_ms);
        else {
            cue_run.cue = bungard_aux::Cue::None;
            frame = bungard_aux::render_frame(cfg, state);
        }
        push(frame);
        xSemaphoreGive(lock);
        vTaskDelay(pdMS_TO_TICKS(40));
    }
}

std::string json_locked() {
    const std::uint64_t t = now_ms();
    return bungard_aux::json_light(cfg, state, state.on ? auto_off.remaining_s(t) : -1, cue_run.active(t) ? cue_run.cue : bungard_aux::Cue::None);
}

}  // namespace

namespace light {

void start(const bungard_aux::Settings& s) {
    settings = s;
    cfg = bungard_aux::light_of(s);
    state = bungard_aux::initial_state(cfg);
    auto_off = bungard_aux::AutoOff(cfg.auto_off_s);
    lock = xSemaphoreCreateMutex();
#if CONFIG_AUX_LIGHT_GPIO >= 0
    if (!cfg.enabled) return;
    rmt_tx_channel_config_t c = {};
    c.gpio_num = static_cast<gpio_num_t>(CONFIG_AUX_LIGHT_GPIO);
    c.clk_src = RMT_CLK_SRC_DEFAULT;
    c.resolution_hz = kResolutionHz;
    c.mem_block_symbols = 64;
    c.trans_queue_depth = 2;
    if (rmt_new_tx_channel(&c, &channel) != ESP_OK || !make_encoder() || rmt_enable(channel) != ESP_OK) {
        ESP_LOGW(TAG, "the ring on GPIO %d could not be started", CONFIG_AUX_LIGHT_GPIO);
        return;
    }
    ready = true;
    if (state.on) auto_off.arm(now_ms());
    xTaskCreate(show_task, "light", 4096, nullptr, 4, nullptr);
    ESP_LOGI(TAG, "ring of %u LEDs on GPIO %d", static_cast<unsigned>(cfg.leds), CONFIG_AUX_LIGHT_GPIO);
#else
    ESP_LOGW(TAG, "no GPIO for the ring in menuconfig: no light");
#endif
}

bool present() { return ready; }

bool is_on() {
    if (!ready) return false;
    xSemaphoreTake(lock, portMAX_DELAY);
    const bool on = state.on;
    xSemaphoreGive(lock);
    return on;
}

unsigned level() {
    if (!ready) return 0;
    xSemaphoreTake(lock, portMAX_DELAY);
    const unsigned l = state.on ? state.level : 0;
    xSemaphoreGive(lock);
    return l;
}

int apply(const std::string& query, std::string& body) {
    if (!ready) {
        body = "{\"error\":\"no_light\"}";
        return 503;
    }
    const bungard_aux::CueRequest cue = bungard_aux::parse_cue(query);
    if (!cue.ok) {
        body = "{\"error\":\"bad_request\",\"key\":\"" + cue.why + "\"}";
        return 400;
    }
    xSemaphoreTake(lock, portMAX_DELAY);
    const bungard_aux::LightChange change = bungard_aux::apply_request(cfg, state, query);
    int status = 200;
    if (!change.ok) {
        body = "{\"error\":\"bad_request\",\"key\":\"" + change.why + "\"}";
        status = 400;
    } else {
        const std::uint64_t t = now_ms();
        state = change.state;
        if (state.on) auto_off.arm(t); else auto_off.disarm();
        if (cue.present) {
            cue_run.cue = cue.cue;
            cue_run.started_ms = t;
            cue_run.duration_ms = cue.duration_ms;
        }
        body = json_locked();
    }
    xSemaphoreGive(lock);
    return status;
}

std::string json() {
    if (!ready) return bungard_aux::json_light(bungard_aux::LightSettings{false, 8, 0, 0, bungard_aux::Tone::Neutral, 0, false, bungard_aux::ColourOrder::Grb}, bungard_aux::LightState{}, -1);
    xSemaphoreTake(lock, portMAX_DELAY);
    std::string j = json_locked();
    xSemaphoreGive(lock);
    return j;
}

}  // namespace light
