idf_component_register(
    SRCS
        "main.c"
        "wifi_manager.c"
        "led_status.c"
        "storage_nvs.c"
        "wifi_scanner.c"
        "wifi_provisioning.c"
        "http_server.c"
        "system_state.c"
        "dns_server.c"
        "app_telegram.c"  
    INCLUDE_DIRS "."
    REQUIRES
        esp_wifi
        esp_netif
        esp_event
        esp_http_server
        nvs_flash
        lwip 
        driver
        esp_http_client json
        esp_timer
)
