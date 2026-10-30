// EH3D-NODE-BUNGARD-AUX - main/app_main.cpp: the firmware of the node of the drill head (ESP32-S3 with a wired network port). GPL-3.0-or-later.
// Written against ESP-IDF 5.5 and NOT built or run on a board yet: the rules (settings, the measure of the laser, the speed and the temperature, the words of the
// network) are in core/ and are tested on a computer. The drivers of the laser and of the temperature sensors are not written (see sensors.hpp).
#include <cstdio>
#include <string>

#include "driver/spi_master.h"
#include "esp_camera.h"
#include "esp_eth.h"
#include "esp_event.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_spiffs.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "sdkconfig.h"

#include "../core/aux_config.hpp"
#include "../core/camera.hpp"
#include "../core/laser.hpp"
#include "../core/protocol.hpp"
#include "../core/spindle.hpp"
#include "sensors.hpp"
#include "stream.hpp"

namespace {

const char* TAG = "aux";
const char* VERSION = "0.0.1";

bungard_aux::Settings settings;
bungard_aux::Thermal thermal(55.0, 70.0, 3.0);
volatile bool protect_is_open = true;
volatile double temp_now_c = 0.0;
volatile bool temp_now_valid = false;

std::string read_file(const char* path) {
    std::string out;
    if (FILE* f = std::fopen(path, "rb")) {
        char buf[256];
        std::size_t n;
        while ((n = std::fread(buf, 1, sizeof buf, f)) > 0) out.append(buf, n);
        std::fclose(f);
    }
    return out;
}

void load_settings() {
    esp_vfs_spiffs_conf_t conf = {};
    conf.base_path = "/config";
    conf.partition_label = "config";
    conf.max_files = 4;
    conf.format_if_mount_failed = true;
    if (esp_vfs_spiffs_register(&conf) != ESP_OK) ESP_LOGW(TAG, "config partition not mounted: the defaults are used");
    std::vector<bungard_aux::Problem> problems;
    settings = bungard_aux::parse_settings(read_file(CONFIG_AUX_CONFIG_PATH), problems);
    for (const auto& p : problems) ESP_LOGW(TAG, "settings line %d (%s): %s", p.line, p.key.c_str(), p.what.c_str());
    thermal = bungard_aux::Thermal(settings.temp_warn_c, settings.temp_critical_c, settings.temp_hysteresis_c);
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
    httpd_resp_set_status(req, "401 Unauthorized");
    httpd_resp_send(req, "{\"error\":\"unauthorized\"}", HTTPD_RESP_USE_STRLEN);
    return false;
}

esp_err_t status_handler(httpd_req_t* req) {
    bungard_aux::Request r;
    if (!allowed(req, bungard_aux::Route::Status, r)) return ESP_OK;
    bungard_aux::Status s;
    s.name = settings.name;
    s.version = VERSION;
    s.camera = camera_ok && settings.camera_enabled;
    if (s.camera) s.camera_model = camera_model_seen;
    s.laser = settings.laser_enabled;
    s.rpm_on = settings.rpm_enabled;
    s.rpm = settings.rpm_enabled ? sensors::rpm_now() : 0.0;
    s.heat = thermal.state();
    s.has_temp = temp_now_valid;
    s.temp_c = temp_now_c;
    s.protect_open = protect_is_open;
    s.uptime_s = static_cast<std::uint32_t>(esp_timer_get_time() / 1000000);
    httpd_resp_set_type(req, "application/json");
    std::string body = bungard_aux::json_status(s);
    httpd_resp_send(req, body.c_str(), body.size());
    return ESP_OK;
}

esp_err_t measure_handler(httpd_req_t* req) {
    bungard_aux::Request r;
    if (!allowed(req, bungard_aux::Route::Measure, r)) return ESP_OK;
    int asked = bungard_aux::samples_of(settings, r);
    std::vector<bungard_aux::LaserReading> got;
    std::int64_t deadline = esp_timer_get_time() + static_cast<std::int64_t>(settings.laser_timeout_ms) * 1000;
    while (static_cast<int>(got.size()) < asked && esp_timer_get_time() < deadline && settings.laser_enabled) {
        double mm = 0.0;
        bool valid = false;
        if (sensors::laser_read(mm, valid)) got.push_back({mm, valid});
        else vTaskDelay(pdMS_TO_TICKS(5));
    }
    bungard_aux::LaserLimits lim;
    lim.range_min_mm = settings.laser_range_min_mm;
    lim.range_max_mm = settings.laser_range_max_mm;
    lim.sigma_max_mm = settings.laser_sigma_max_mm;
    lim.offset_mm = settings.laser_offset_mm;
    bungard_aux::Measure m = bungard_aux::measure_distance(got, asked, lim);
    httpd_resp_set_type(req, "application/json");
    std::string body = bungard_aux::json_measure(m);
    httpd_resp_send(req, body.c_str(), body.size());
    return ESP_OK;
}

esp_err_t stream_handler(httpd_req_t* req) {
    bungard_aux::Request r;
    if (!allowed(req, bungard_aux::Route::Stream, r)) return ESP_OK;
    if (!camera_ok || !settings.camera_enabled) {
        httpd_resp_set_status(req, "503 Service Unavailable");
        httpd_resp_send(req, "{\"error\":\"no camera\"}", HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
    }
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
    return ESP_OK;
}

void http_start() {
    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    cfg.server_port = settings.port;
    cfg.stack_size = 8192;
    httpd_handle_t h = nullptr;
    if (httpd_start(&h, &cfg) != ESP_OK) {
        ESP_LOGE(TAG, "the web server did not start");
        return;
    }
    httpd_uri_t status = {"/api/status", HTTP_GET, status_handler, nullptr};
    httpd_uri_t measure = {"/api/measure", HTTP_POST, measure_handler, nullptr};
    httpd_uri_t stream = {"/stream", HTTP_GET, stream_handler, nullptr};
    httpd_register_uri_handler(h, &status);
    httpd_register_uri_handler(h, &measure);
    httpd_register_uri_handler(h, &stream);
    ::stream::register_http(h);
}

// ------------------------------------------------------------------------------------------------------------------------------ the network

void eth_start() {
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_config_t ncfg = ESP_NETIF_DEFAULT_ETH();
    esp_netif_t* netif = esp_netif_new(&ncfg);

    spi_bus_config_t bus = {};
    bus.miso_io_num = CONFIG_AUX_ETH_MISO;
    bus.mosi_io_num = CONFIG_AUX_ETH_MOSI;
    bus.sclk_io_num = CONFIG_AUX_ETH_SCLK;
    bus.quadwp_io_num = -1;
    bus.quadhd_io_num = -1;
    ESP_ERROR_CHECK(spi_bus_initialize(static_cast<spi_host_device_t>(CONFIG_AUX_ETH_SPI_HOST), &bus, SPI_DMA_CH_AUTO));
    spi_device_interface_config_t dev = {};
    dev.mode = 0;
    dev.clock_speed_hz = 20 * 1000 * 1000;
    dev.spics_io_num = CONFIG_AUX_ETH_CS;
    dev.queue_size = 20;
    eth_w5500_config_t w = ETH_W5500_DEFAULT_CONFIG(static_cast<spi_host_device_t>(CONFIG_AUX_ETH_SPI_HOST), &dev);
    w.int_gpio_num = CONFIG_AUX_ETH_INT;
    eth_mac_config_t mac_cfg = ETH_MAC_DEFAULT_CONFIG();
    eth_phy_config_t phy_cfg = ETH_PHY_DEFAULT_CONFIG();
    phy_cfg.reset_gpio_num = CONFIG_AUX_ETH_RST;
    esp_eth_mac_t* mac = esp_eth_mac_new_w5500(&w, &mac_cfg);
    esp_eth_phy_t* phy = esp_eth_phy_new_w5500(&phy_cfg);
    esp_eth_config_t ecfg = ETH_DEFAULT_CONFIG(mac, phy);
    esp_eth_handle_t eth = nullptr;
    ESP_ERROR_CHECK(esp_eth_driver_install(&ecfg, &eth));
    ESP_ERROR_CHECK(esp_netif_attach(netif, esp_eth_new_netif_glue(eth)));
    if (!settings.address.empty()) {
        esp_netif_dhcpc_stop(netif);
        esp_netif_ip_info_t ip = {};
        ip.ip.addr = esp_ip4addr_aton(settings.address.c_str());
        // the gateway and the mask of a workshop network: the node sits in the same subnet as the machine's other nodes
        ip.netmask.addr = esp_ip4addr_aton("255.255.255.0");
        ip.gw.addr = (ip.ip.addr & ip.netmask.addr) | (1u << 24);
        esp_netif_set_ip_info(netif, &ip);
    }
    ESP_ERROR_CHECK(esp_eth_start(eth));
}

// ------------------------------------------------------------------------------------------------------------------------------ the watch of the spindle

void watch_task(void*) {
    while (true) {
        double rpm = settings.rpm_enabled ? sensors::rpm_now() : 0.0;
        double c = 0.0;
        bool got = settings.temp_sensor != bungard_aux::TempSensor::None && sensors::temperature_read(c);
        if (settings.temp_sensor != bungard_aux::TempSensor::None) {
            thermal.update(c, got);
            temp_now_valid = got;
            if (got) temp_now_c = c;
        }
        bool open = bungard_aux::protect_open(settings, rpm, thermal.state());
        protect_is_open = open;
        sensors::protect_set(open);
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

}  // namespace

extern "C" void app_main(void) {
    esp_err_t e = nvs_flash_init();
    if (e == ESP_ERR_NVS_NO_FREE_PAGES || e == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }
    load_settings();
    sensors::init(settings);
    if (settings.camera_enabled) camera_ok = camera_start();
    eth_start();
    ::stream::start(settings);
    http_start();
    xTaskCreate(watch_task, "watch", 4096, nullptr, 5, nullptr);
    ESP_LOGI(TAG, "EH3D-NODE-BUNGARD-AUX %s: %s on port %u", VERSION, settings.name.c_str(), static_cast<unsigned>(settings.port));
}
