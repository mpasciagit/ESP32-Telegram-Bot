#include "wifi_manager.h"
#include "storage_nvs.h"
#include "led_status.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include <string.h>

#define WIFI_CONNECTED_BIT BIT0

static const char *TAG = "wifi_manager";
static EventGroupHandle_t wifi_event_group;
static bool wifi_connected = false;
static char ip_str[16] = {0};
static uint8_t last_disconnect_reason = 0;

static char saved_ssid[WIFI_SSID_MAX_LEN] = {0};
static char saved_pass[WIFI_PASS_MAX_LEN] = {0};

/* Declaración interna del handler */
static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data);

/* --- FUNCIONES DE CONSULTA --- */

uint8_t wifi_manager_get_last_disconnect_reason(void) {
    return last_disconnect_reason;
}

bool wifi_manager_is_connected(void) {
    return wifi_connected;
}

const char* wifi_manager_get_ip(void) {
    return wifi_connected ? ip_str : "0.0.0.0";
}

// Esta es la función que system_state.c usará para grabar en NVS
void wifi_manager_get_credentials(char *ssid_out, char *pass_out) {
    if (ssid_out) strcpy(ssid_out, saved_ssid);
    if (pass_out) strcpy(pass_out, saved_pass);
}

/* --- LOGICA DE CONTROL --- */

void wifi_manager_init(void) {
    if (wifi_event_group == NULL) wifi_event_group = xEventGroupCreate();
    
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, NULL));

    esp_netif_create_default_wifi_sta();
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));

    wifi_config_t sta_config = { .sta = { .scan_method = WIFI_FAST_SCAN, .failure_retry_cnt = 0 } };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &sta_config));
    esp_wifi_set_ps(WIFI_PS_NONE);

    ESP_LOGI(TAG, "Driver inicializado.");
}

void wifi_manager_set_credentials(const char* ssid, const char* password) {
    if (ssid) strncpy(saved_ssid, ssid, sizeof(saved_ssid) - 1);
    if (password) strncpy(saved_pass, password, sizeof(saved_pass) - 1);
}

void wifi_manager_reconnect(void) {
    wifi_config_t wifi_config = {0};
    strncpy((char *)wifi_config.sta.ssid, saved_ssid, 32);
    strncpy((char *)wifi_config.sta.password, saved_pass, 64);
    esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    esp_wifi_disconnect();
    esp_wifi_connect();
}

static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } 
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        wifi_event_sta_disconnected_t* event = (wifi_event_sta_disconnected_t*) event_data;
        last_disconnect_reason = event->reason;
        wifi_connected = false;
        if (wifi_event_group) xEventGroupClearBits(wifi_event_group, WIFI_CONNECTED_BIT);
        ESP_LOGW(TAG, "Desconectado. Razón: %d", last_disconnect_reason);
    } 
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        snprintf(ip_str, sizeof(ip_str), IPSTR, IP2STR(&event->ip_info.ip));
        wifi_connected = true;
        last_disconnect_reason = 0;
        if (wifi_event_group) xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
        ESP_LOGI(TAG, "IP: %s", ip_str);
    }
}

void wifi_manager_reset_last_disconnect_reason(void) {
    last_disconnect_reason = 0;
    ESP_LOGI("WIFI_MGR", "Razón de desconexión reseteada.");
}

EventGroupHandle_t wifi_manager_get_event_group(void) { return wifi_event_group; }