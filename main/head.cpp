// EH3D-NODE-BUNGARD-AUX - main/head.cpp: what is only the head's: the camera, the laser, the speed and the temperature of the spindle, the protect output and the routes that
// programs use (the status, the measure, the camera as MJPEG, with the access token of the head or the login of the panel). GPL-3.0-or-later.
// Written against ESP-IDF 5.5 and built with it; it has not been flashed or run on a board. The rules are in core/ and are tested on a computer.
#include <atomic>
#include <cstdio>
#include <memory>
#include <mutex>
#include <new>
#include <string>

#include "esp_app_desc.h"
#include "esp_camera.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_task_wdt.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"

#include "../core/aux_config.hpp"
#include "../core/camera.hpp"
#include "../core/laser.hpp"
#include "../core/laser_hw.hpp"
#include "../core/protocol.hpp"
#include "../core/self_check.hpp"
#include "../core/spindle.hpp"
#include "head.hpp"
#include "light.hpp"
#include "node_guard.hpp"
#include "node_store.hpp"
#include "sensors.hpp"
#include "stream.hpp"
#include "web_server.hpp"

namespace {

const char* TAG = "aux-head";

bungard_aux::Settings settings;   // the `head` section of the settings of the node, as it was when the node started
bungard_aux::Thermal thermal(55.0, 70.0, 3.0);
volatile bool protect_is_open = true;
// the temperature is read by a task of its own (a DS18B20 takes up to 750 ms to convert); this lock guards what it leaves for the others
std::mutex thermal_lock;
double temp_now_c = 0.0;
bool temp_now_valid = false;
std::string node_name_text;  // the name of the node, for /api/status
std::atomic<int> streams_open{0};
constexpr int kMaxStreams = 2;   // each one is a task with a buffer of a picture; more viewers than this are told to wait

bungard_aux::Heat heat_now() {
    std::lock_guard<std::mutex> guard(thermal_lock);
    return thermal.state();
}

// ------------------------------------------------------------------------------------------------------------------------------ the camera

/// The driver's frame size for each row of `bungard_aux::frame_sizes()`, in the same order.
const framesize_t FRAME_OF[] = {FRAMESIZE_QQVGA, FRAMESIZE_QVGA, FRAMESIZE_CIF, FRAMESIZE_VGA, FRAMESIZE_SVGA, FRAMESIZE_XGA, FRAMESIZE_HD,
                                FRAMESIZE_SXGA,  FRAMESIZE_UXGA, FRAMESIZE_FHD, FRAMESIZE_QXGA, FRAMESIZE_QHD, FRAMESIZE_QSXGA, FRAMESIZE_5MP};
static_assert(sizeof(FRAME_OF) / sizeof(FRAME_OF[0]) == 14, "one driver size for each row of the table of sizes");

std::string camera_model_seen;  // what the sensor said it is, for /api/status

#if CONFIG_AUX_CAM_XCLK >= 0
esp_err_t camera_init(framesize_t size) {
    camera_config_t c = {};
    c.pin_pwdn = CONFIG_AUX_CAM_PWDN;
    c.pin_reset = CONFIG_AUX_CAM_RESET;
    c.pin_xclk = CONFIG_AUX_CAM_XCLK;
    c.pin_sccb_sda = CONFIG_AUX_CAM_SIOD;
    c.pin_sccb_scl = CONFIG_AUX_CAM_SIOC;
    c.pin_d0 = CONFIG_AUX_CAM_D0;
    c.pin_d1 = CONFIG_AUX_CAM_D1;
    c.pin_d2 = CONFIG_AUX_CAM_D2;
    c.pin_d3 = CONFIG_AUX_CAM_D3;
    c.pin_d4 = CONFIG_AUX_CAM_D4;
    c.pin_d5 = CONFIG_AUX_CAM_D5;
    c.pin_d6 = CONFIG_AUX_CAM_D6;
    c.pin_d7 = CONFIG_AUX_CAM_D7;
    c.pin_vsync = CONFIG_AUX_CAM_VSYNC;
    c.pin_href = CONFIG_AUX_CAM_HREF;
    c.pin_pclk = CONFIG_AUX_CAM_PCLK;
    c.xclk_freq_hz = 20000000;
    c.ledc_timer = LEDC_TIMER_0;
    c.ledc_channel = LEDC_CHANNEL_0;
    c.pixel_format = PIXFORMAT_JPEG;
    c.frame_size = size;
    c.jpeg_quality = settings.camera_quality;
    c.fb_count = 2;
    c.fb_location = CAMERA_FB_IN_PSRAM;
    c.grab_mode = CAMERA_GRAB_LATEST;
    return esp_camera_init(&c);
}
#endif

/// Starts the camera. The sensor is asked which one it is (OV2640 or OV5640) with a small picture first; then the camera is started again at the size of the settings
/// that this sensor can give, and the settings keep the real size and model so the status and ONVIF say the truth.
bool camera_start() {
#if CONFIG_AUX_CAM_XCLK >= 0
    using namespace bungard_aux;
    if (camera_init(FRAMESIZE_QVGA) != ESP_OK) return false;
    sensor_t* sensor = esp_camera_sensor_get();
    CameraModel seen = sensor ? model_from_pid(sensor->id.PID) : CameraModel::Auto;
    if (seen == CameraModel::Auto) ESP_LOGW(TAG, "the camera is not an OV2640 or an OV5640 (product id 0x%x)", sensor ? sensor->id.PID : 0);
    if (settings.camera_model != CameraModel::Auto && seen != CameraModel::Auto && seen != settings.camera_model) {
        ESP_LOGW(TAG, "head.cfg says %s but the sensor is %s: the sensor wins", model_text(settings.camera_model), model_text(seen));
    }
    const CameraModel model = seen != CameraModel::Auto ? seen : settings.camera_model;
    const std::size_t index = frame_index_for(settings.camera_width, settings.camera_height, model);
    settings.camera_model = model;
    settings.camera_width = static_cast<std::uint16_t>(frame_sizes()[index].width);
    settings.camera_height = static_cast<std::uint16_t>(frame_sizes()[index].height);
    camera_model_seen = model_text(model);
    esp_camera_deinit();
    if (camera_init(FRAME_OF[index]) != ESP_OK) return false;
    // the picture as head.cfg wants it (a setting that this sensor does not have answers -1 and is left alone)
    if (sensor_t* sn = esp_camera_sensor_get()) {
        if (sn->set_hmirror) sn->set_hmirror(sn, settings.camera_hmirror ? 1 : 0);
        if (sn->set_vflip) sn->set_vflip(sn, settings.camera_vflip ? 1 : 0);
        if (sn->set_brightness) sn->set_brightness(sn, settings.camera_brightness);
        if (sn->set_contrast) sn->set_contrast(sn, settings.camera_contrast);
        if (sn->set_saturation) sn->set_saturation(sn, settings.camera_saturation);
        if (sn->set_exposure_ctrl) sn->set_exposure_ctrl(sn, settings.camera_auto_exposure ? 1 : 0);
        if (!settings.camera_auto_exposure && sn->set_aec_value) sn->set_aec_value(sn, static_cast<int>(settings.camera_exposure));
        if (sn->set_whitebal) sn->set_whitebal(sn, settings.camera_auto_whitebalance ? 1 : 0);
    }
    ESP_LOGI(TAG, "camera %s at %ux%u", camera_model_seen.c_str(), static_cast<unsigned>(settings.camera_width), static_cast<unsigned>(settings.camera_height));
    return true;
#else
    ESP_LOGW(TAG, "camera pins are not set in menuconfig: no video");
    return false;
#endif
}

bool camera_ok = false;

// ------------------------------------------------------------------------------------------------------------------------------ the HTTP server

std::string bearer_of(httpd_req_t* req) {
    char value[128];
    if (httpd_req_get_hdr_value_str(req, "Authorization", value, sizeof value) != ESP_OK) return "";
    std::string v = value;
    const std::string prefix = "Bearer ";
    return v.compare(0, prefix.size(), prefix) == 0 ? v.substr(prefix.size()) : "";
}

bool allowed(httpd_req_t* req, bungard_aux::Route route, bungard_aux::Request& r) {
    r.method = req->method == HTTP_POST ? "POST" : "GET";
    std::string uri = req->uri;
    std::size_t q = uri.find('?');
    r.path = q == std::string::npos ? uri : uri.substr(0, q);
    r.query = q == std::string::npos ? "" : uri.substr(q + 1);
    if (bungard_aux::authorized(settings, route, bearer_of(req), r)) return true;
    // a person signed in to the panel sees the camera and measures without the access token of the programs (the picture of the panel cannot carry a header)
    // (calibrating changes what every reading means: it takes the token of the head, or the administrator's own route of the panel)
    if (route != bungard_aux::Route::Status && route != bungard_aux::Route::Calibrate && eh3d::web::signed_in(req)) return true;
    httpd_resp_set_status(req, "401 Unauthorized");
    httpd_resp_send(req, "{\"error\":\"unauthorized\"}", HTTPD_RESP_USE_STRLEN);
    return false;
}

std::string live_json_now() {
    bungard_aux::Status s;
    s.name = node_name_text;
    s.version = esp_app_get_description()->version;
    s.camera = camera_ok && settings.camera_enabled;
    if (s.camera) s.camera_model = camera_model_seen;
    s.laser = settings.laser_enabled;
    s.rpm_on = settings.rpm_enabled;
    s.rpm = settings.rpm_enabled ? sensors::rpm_now() : 0.0;
    s.heat = heat_now();
    s.rpm_input = bungard_aux::rpm_input_text(settings.rpm_input);
    sensors::rpm_diagnostics(s.rpm_quality, s.rpm_rejected, s.rpm_level_mv, s.rpm_th_hi_mv, s.rpm_th_lo_mv);
    {
        std::lock_guard<std::mutex> guard(thermal_lock);
        s.has_temp = temp_now_valid;
        s.temp_c = temp_now_c;
    }
    if (settings.temp_sensor == bungard_aux::TempSensor::Ds18b20) {
        for (const std::string& rom : sensors::temp_sensors_found()) s.temp_roms += (s.temp_roms.empty() ? "" : ",") + rom;
        s.temp_rom_used = sensors::temp_sensor_used();
        s.temp_guess = sensors::temp_sensor_ambiguous();
        s.temp_resolution = settings.temp_resolution;
    }
    s.protect_open = protect_is_open;
    s.uptime_s = static_cast<std::uint32_t>(esp_timer_get_time() / 1000000);
    s.light = light::present();
    s.light_on = light::is_on();
    s.light_level = light::level();
    s.laser_mode = bungard_aux::distance_mode_text(settings.laser_distance_mode);
    s.laser_budget_ms = static_cast<int>(settings.laser_timing_budget_ms);
    s.laser_roi = bungard_aux::roi_text(settings.laser_roi_w, settings.laser_roi_h);
    s.laser_cal_offset_mm = settings.laser_cal_offset_mm;
    s.laser_cal_xtalk_cps = settings.laser_cal_xtalk_cps;
    s.laser_cal_allowed = settings.laser_calibration_enabled;
    bungard_aux::LaserReading last;
    long age_ms = 0;
    if (settings.laser_enabled && sensors::laser_last(last, age_ms)) {
        s.laser_last_mm = last.distance_mm + settings.laser_offset_mm + settings.laser_cal_offset_mm;
        s.laser_last_status = bungard_aux::range_why_text(bungard_aux::range_why(last.status));
        s.laser_age_ms = age_ms;
    }
    bungard_aux::apply_self_check(s, settings);
    return bungard_aux::json_status(s);
}

bungard_aux::LaserLimits laser_limits_now() {
    bungard_aux::LaserLimits lim;
    lim.range_min_mm = settings.laser_range_min_mm;
    lim.range_max_mm = settings.laser_range_max_mm;
    lim.sigma_max_mm = settings.laser_sigma_max_mm;
    lim.offset_mm = settings.laser_offset_mm;
    lim.cal_offset_mm = settings.laser_cal_offset_mm;
    lim.outlier_k = settings.laser_outlier_k;
    lim.outlier_floor_mm = settings.laser_outlier_floor_mm;
    return lim;
}

std::string measure_json_now(int asked) {
    std::vector<bungard_aux::LaserReading> got;
    std::int64_t deadline = esp_timer_get_time() + static_cast<std::int64_t>(settings.laser_timeout_ms) * 1000;
    while (static_cast<int>(got.size()) < asked && esp_timer_get_time() < deadline && settings.laser_enabled) {
        bungard_aux::LaserReading r;
        if (sensors::laser_read(r)) got.push_back(r);
        else vTaskDelay(pdMS_TO_TICKS(5));
    }
    return bungard_aux::json_measure(bungard_aux::measure_distance(got, asked, laser_limits_now()));
}

/// The calibration of the laser at a known distance (kind offset or xtalk) or the clearing of both. Returns the JSON answer; `found` is true and `offset_mm`/`xtalk_cps` hold the new
/// values to store when it worked. Nothing is stored here: the caller saves them in the settings of the node.
std::string calibrate_now(const bungard_aux::CalibrateRequest& req, bool& found, double& offset_mm, double& xtalk_cps) {
    found = false;
    offset_mm = settings.laser_cal_offset_mm;
    xtalk_cps = settings.laser_cal_xtalk_cps;
    auto answer = [&](bool ok, const std::string& why, int used, double sigma) {
        return bungard_aux::json_calibration(req.kind, ok, why, offset_mm, xtalk_cps, used, sigma);
    };
    if (!settings.laser_calibration_enabled) return answer(false, "disabled", 0, 0.0);
    if (!settings.laser_enabled) return answer(false, "no_readings", 0, 0.0);
    if (req.kind == "reset") {
        if (!sensors::laser_set_xtalk(0.0)) return answer(false, "no_readings", 0, 0.0);
        settings.laser_cal_offset_mm = offset_mm = 0.0;
        settings.laser_cal_xtalk_cps = xtalk_cps = 0.0;
        found = true;
        return answer(true, "ok", 0, 0.0);
    }
    constexpr int kReadings = 50;
    const std::uint32_t timeout = static_cast<std::uint32_t>(kReadings) * (static_cast<std::uint32_t>(bungard_aux::inter_measurement_ms(static_cast<int>(settings.laser_timing_budget_ms))) + 60);
    std::vector<bungard_aux::LaserReading> got;
    if (req.kind == "xtalk") {
        // like the ST driver: the crosstalk is cleared in the sensor while the target is measured, then the one found is written
        if (!sensors::laser_set_xtalk(0.0)) return answer(false, "no_readings", 0, 0.0);
        sensors::laser_collect(kReadings, timeout, got);
        const bungard_aux::Calibration c = bungard_aux::calibrate_xtalk(got, req.target_mm);
        if (!c.ok) {
            sensors::laser_set_xtalk(settings.laser_cal_xtalk_cps);   // back to what it was
            return answer(false, c.why, c.used, c.sigma_mm);
        }
        if (!sensors::laser_set_xtalk(c.value)) return answer(false, "no_readings", c.used, c.sigma_mm);
        settings.laser_cal_xtalk_cps = xtalk_cps = c.value;
        found = true;
        return answer(true, "ok", c.used, c.sigma_mm);
    }
    sensors::laser_collect(kReadings, timeout, got);
    const bungard_aux::Calibration c = bungard_aux::calibrate_offset(got, kReadings, req.target_mm, laser_limits_now());
    if (!c.ok) return answer(false, c.why, c.used, c.sigma_mm);
    settings.laser_cal_offset_mm = offset_mm = c.value;
    found = true;
    return answer(true, "ok", c.used, c.sigma_mm);
}

esp_err_t status_handler(httpd_req_t* req) {
    bungard_aux::Request r;
    if (!allowed(req, bungard_aux::Route::Status, r)) return ESP_OK;
    httpd_resp_set_type(req, "application/json");
    const std::string body = live_json_now();
    httpd_resp_send(req, body.c_str(), body.size());
    return ESP_OK;
}

esp_err_t measure_handler(httpd_req_t* req) {
    bungard_aux::Request r;
    if (!allowed(req, bungard_aux::Route::Measure, r)) return ESP_OK;
    httpd_resp_set_type(req, "application/json");
    const std::string body = measure_json_now(bungard_aux::samples_of(settings, r));
    httpd_resp_send(req, body.c_str(), body.size());
    return ESP_OK;
}

esp_err_t calibrate_handler(httpd_req_t* req) {
    bungard_aux::Request r;
    if (!allowed(req, bungard_aux::Route::Calibrate, r)) return ESP_OK;
    httpd_resp_set_type(req, "application/json");
    bungard_aux::CalibrateRequest cr;
    if (!bungard_aux::parse_calibrate(r, cr)) {
        httpd_resp_set_status(req, "400 Bad Request");
        const std::string bad = bungard_aux::json_calibration("", false, "bad_target", settings.laser_cal_offset_mm, settings.laser_cal_xtalk_cps, 0, 0.0);
        httpd_resp_send(req, bad.c_str(), bad.size());
        return ESP_OK;
    }
    bool found = false;
    double offset = 0.0, xtalk = 0.0;
    const std::string body = calibrate_now(cr, found, offset, xtalk);
    if (found) head::store_calibration(offset, xtalk);
    httpd_resp_send(req, body.c_str(), body.size());
    return ESP_OK;
}

esp_err_t light_get_handler(httpd_req_t* req) {
    bungard_aux::Request r;
    if (!allowed(req, bungard_aux::Route::LightGet, r)) return ESP_OK;
    httpd_resp_set_type(req, "application/json");
    const std::string body = light::json();
    httpd_resp_send(req, body.c_str(), body.size());
    return ESP_OK;
}

esp_err_t light_set_handler(httpd_req_t* req) {
    bungard_aux::Request r;
    if (!allowed(req, bungard_aux::Route::LightSet, r)) return ESP_OK;
    httpd_resp_set_type(req, "application/json");
    std::string body;
    const int status = light::apply(r.query, body);
    if (status == 400) httpd_resp_set_status(req, "400 Bad Request");
    else if (status == 503) httpd_resp_set_status(req, "503 Service Unavailable");
    httpd_resp_send(req, body.c_str(), body.size());
    return ESP_OK;
}

/// One picture of the camera as JPEG, for the programs that look at the work (the checks of the holes and of the milling).
esp_err_t snapshot_handler(httpd_req_t* req) {
    bungard_aux::Request r;
    if (!allowed(req, bungard_aux::Route::Snapshot, r)) return ESP_OK;
    if (!camera_ok || !settings.camera_enabled) {
        httpd_resp_set_status(req, "503 Service Unavailable");
        httpd_resp_send(req, "{\"error\":\"no camera\"}", HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
    }
    // the sensor settles its exposure on the frames after a change of light: the first one is thrown away
    camera_fb_t* fb = esp_camera_fb_get();
    if (fb) esp_camera_fb_return(fb);
    fb = esp_camera_fb_get();
    if (!fb) {
        httpd_resp_set_status(req, "503 Service Unavailable");
        httpd_resp_send(req, "{\"error\":\"no picture\"}", HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
    }
    httpd_resp_set_type(req, "image/jpeg");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    const esp_err_t e = httpd_resp_send(req, reinterpret_cast<const char*>(fb->buf), fb->len);
    esp_camera_fb_return(fb);
    return e;
}

// The task that sends the picture of one viewer as MJPEG, until the viewer closes.
void stream_task(void* arg) {
    httpd_req_t* req = static_cast<httpd_req_t*>(arg);
    httpd_resp_set_type(req, "multipart/x-mixed-replace;boundary=frame");
    const int period_ms = 1000 / (settings.camera_fps_max ? settings.camera_fps_max : 1);
    while (true) {
        camera_fb_t* fb = esp_camera_fb_get();
        if (!fb) break;
        char head[96];
        int n = std::snprintf(head, sizeof head, "--frame\r\nContent-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n", static_cast<unsigned>(fb->len));
        esp_err_t e = httpd_resp_send_chunk(req, head, n);
        if (e == ESP_OK) e = httpd_resp_send_chunk(req, reinterpret_cast<const char*>(fb->buf), fb->len);
        if (e == ESP_OK) e = httpd_resp_send_chunk(req, "\r\n", 2);
        esp_camera_fb_return(fb);
        if (e != ESP_OK) break;  // the viewer closed
        vTaskDelay(pdMS_TO_TICKS(period_ms));
    }
    httpd_resp_send_chunk(req, nullptr, 0);
    httpd_req_async_handler_complete(req);
    streams_open.fetch_sub(1);
    vTaskDelete(nullptr);
}

esp_err_t stream_handler(httpd_req_t* req) {
    bungard_aux::Request r;
    if (!allowed(req, bungard_aux::Route::Stream, r)) return ESP_OK;
    if (!camera_ok || !settings.camera_enabled) {
        httpd_resp_set_status(req, "503 Service Unavailable");
        httpd_resp_send(req, "{\"error\":\"no camera\"}", HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
    }
    // The picture goes on for as long as somebody looks at it, so it must not hold the one task of the web server: the request is handed to a task of its own and the
    // server goes on answering the status, the measures and the panel. At most `kMaxStreams` at a time.
    if (streams_open.load() >= kMaxStreams) {
        httpd_resp_set_status(req, "503 Service Unavailable");
        httpd_resp_send(req, "{\"error\":\"too many viewers\"}", HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
    }
    httpd_req_t* async = nullptr;
    if (httpd_req_async_handler_begin(req, &async) != ESP_OK) {
        httpd_resp_set_status(req, "503 Service Unavailable");
        httpd_resp_send(req, "{\"error\":\"busy\"}", HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
    }
    streams_open.fetch_add(1);
    if (xTaskCreate(stream_task, "mjpeg", 6144, async, 4, nullptr) != pdPASS) {
        streams_open.fetch_sub(1);
        httpd_resp_set_status(async, "503 Service Unavailable");
        httpd_resp_send(async, "{\"error\":\"busy\"}", HTTPD_RESP_USE_STRLEN);
        httpd_req_async_handler_complete(async);
    }
    return ESP_OK;
}

// ------------------------------------------------------------------------------------------------------------------------------ the watch of the spindle

// The temperature has a task of its own: a DS18B20 conversion blocks for up to 750 ms (and the search for a lost sensor longer), and the speed must be judged every 100 ms.
void temperature_task(void*) {
    while (true) {
        double c = 0.0;
        const bool got = sensors::temperature_read(c);
        {
            std::lock_guard<std::mutex> guard(thermal_lock);
            thermal.update(c, got);
            temp_now_valid = got;
            if (got) temp_now_c = c;
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void watch_task(void*) {
    // The only task that writes the protect output: if it hangs (a bus that does not answer, a lock that is never freed) the output would stay open for good, so the task
    // watchdog watches it. With the watchdog set to panic, a hang restarts the chip and the pin goes back to input, which opens the stop line.
    esp_task_wdt_add(nullptr);
    while (true) {
        esp_task_wdt_reset();
        double rpm = settings.rpm_enabled ? sensors::rpm_now() : 0.0;
        bool open = bungard_aux::protect_open(settings, rpm, heat_now());
        protect_is_open = open;
        sensors::protect_set(open);
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

}  // namespace

namespace head {

void start(const bungard_aux::Settings& s) {
    settings = s;
    thermal = bungard_aux::Thermal(settings.temp_warn_c, settings.temp_critical_c, settings.temp_hysteresis_c);
    {
        // the name of the node, for the status (the settings are a big structure: on the heap)
        const std::unique_ptr<eh3d::config::Settings> node(new (std::nothrow) eh3d::config::Settings(eh3d::store::settings()));
        if (node) node_name_text = node->node_name;
    }
    sensors::init(settings);
    light::start(settings);
    if (settings.camera_enabled) camera_ok = camera_start();
    ::stream::start(settings);
    eh3d::nodeguard::expect_task("watch");
    xTaskCreate(watch_task, "watch", 4096, nullptr, 5, nullptr);
    if (settings.temp_sensor != bungard_aux::TempSensor::None) xTaskCreate(temperature_task, "temp", 4096, nullptr, 4, nullptr);
}

void register_routes(httpd_handle_t server) {
    // before the wildcard handlers of the panel: the first one that matches answers
    httpd_uri_t status = {"/api/status", HTTP_GET, status_handler, nullptr};
    httpd_uri_t measure = {"/api/measure", HTTP_POST, measure_handler, nullptr};
    httpd_uri_t stream = {"/stream", HTTP_GET, stream_handler, nullptr};
    httpd_register_uri_handler(server, &status);
    httpd_register_uri_handler(server, &measure);
    httpd_register_uri_handler(server, &stream);
    httpd_uri_t light_get = {"/api/light", HTTP_GET, light_get_handler, nullptr};
    httpd_uri_t light_set = {"/api/light", HTTP_POST, light_set_handler, nullptr};
    httpd_uri_t snapshot = {"/api/snapshot", HTTP_GET, snapshot_handler, nullptr};
    httpd_uri_t calibrate = {"/api/laser/calibrate", HTTP_POST, calibrate_handler, nullptr};
    httpd_register_uri_handler(server, &calibrate);
    httpd_register_uri_handler(server, &light_get);
    httpd_register_uri_handler(server, &light_set);
    httpd_register_uri_handler(server, &snapshot);
    ::stream::register_http(server);
}

std::string live_json() { return live_json_now(); }

void store_calibration(double offset_mm, double xtalk_cps) {
    eh3d::config::Settings next = eh3d::store::settings();
    next.head.laser_cal_offset_mm = offset_mm;
    next.head.laser_cal_xtalk_cps = xtalk_cps;
    if (!eh3d::store::save_settings(next)) ESP_LOGE(TAG, "the calibration of the laser could not be stored");
}

std::string calibrate_json(const std::string& kind, double target_mm, bool& found, double& offset_mm, double& xtalk_cps) {
    bungard_aux::CalibrateRequest cr;
    cr.kind = kind;
    cr.target_mm = target_mm;
    return calibrate_now(cr, found, offset_mm, xtalk_cps);
}
std::string measure_json(int samples) { return measure_json_now(samples < 1 ? 1 : samples > 31 ? 31 : samples); }

}  // namespace head
