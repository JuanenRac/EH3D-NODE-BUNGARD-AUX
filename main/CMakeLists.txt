idf_component_register(
    SRCS "app_main.cpp" "sensors.cpp"
    INCLUDE_DIRS "." ".."
    PRIV_REQUIRES driver esp_timer esp_eth esp_netif esp_event esp_http_server nvs_flash spiffs
)
