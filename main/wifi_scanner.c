#include "wifi_scanner.h"
#include "storage_nvs.h" 
#include "cJSON.h"
#include <string.h>
#include "esp_wifi.h"
#include "esp_log.h"

static const char *TAG = "wifi_scanner";

static wifi_scan_result_t g_scan_album[20];
static int g_networks_found = 0;

void wifi_scanner_init(void) {
    memset(g_scan_album, 0, sizeof(g_scan_album));
    g_networks_found = 0;
    ESP_LOGI(TAG, "WiFi scanner inicializado");
}

int wifi_scanner_execute_actual_scan(void) {
    wifi_ap_record_t ap_info[20];
    uint16_t ap_count = 0;
    uint16_t max_number = 20;

    wifi_scan_config_t scan_config = {
        .show_hidden = true,
        .scan_type = WIFI_SCAN_TYPE_ACTIVE,
        .scan_time.active.min = 100,
        .scan_time.active.max = 300
    };

    ESP_LOGI(TAG, "Hardware: Iniciando escaneo real...");
    esp_err_t ret = esp_wifi_scan_start(&scan_config, true);
    
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error hardware radio: %s", esp_err_to_name(ret));
        return -1; 
    }

    esp_wifi_scan_get_ap_num(&ap_count);
    if (ap_count > max_number) ap_count = max_number;

    if (ap_count > 0) {
        ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&ap_count, ap_info));
        g_networks_found = ap_count;
        memset(g_scan_album, 0, sizeof(g_scan_album));

        for (int i = 0; i < ap_count; i++) {
            strncpy(g_scan_album[i].ssid, (char *)ap_info[i].ssid, sizeof(g_scan_album[i].ssid) - 1);
            g_scan_album[i].rssi = ap_info[i].rssi;
            g_scan_album[i].authmode = ap_info[i].authmode;
            g_scan_album[i].channel = ap_info[i].primary;
            g_scan_album[i].hidden = (strlen((char *)ap_info[i].ssid) == 0);
        }
    } else {
        g_networks_found = 0;
        memset(g_scan_album, 0, sizeof(g_scan_album));
    }

    esp_wifi_scan_stop(); 
    ESP_LOGI(TAG, "Escaneo finalizado. %d redes en RAM.", g_networks_found);
    return (int)g_networks_found;
}

bool wifi_scanner_get_best_known_network(char *ssid_out, char *pass_out) {
    // 1. Obtenemos el llavero completo desde NVS
    char *json_keychain = storage_get_networks_json();
    if (!json_keychain) return false;

    cJSON *keychain = cJSON_Parse(json_keychain);
    if (!keychain) {
        free(json_keychain);
        return false;
    }

    bool match_found = false;

    // 2. Recorremos el álbum (lo que hay en el aire)
    // El driver de ESP32 suele devolverlos ordenados por RSSI (potencia)
    for (int i = 0; i < g_networks_found; i++) {
        cJSON *net = NULL;
        cJSON_ArrayForEach(net, keychain) {
            cJSON *s = cJSON_GetObjectItem(net, "s");
            cJSON *p = cJSON_GetObjectItem(net, "p");

            if (s && p && strcmp(g_scan_album[i].ssid, s->valuestring) == 0) {
                // ¡MATCH! Encontramos una red conocida
                strncpy(ssid_out, s->valuestring, 31);
                strncpy(pass_out, p->valuestring, 63);
                ESP_LOGI(TAG, "Match: '%s' hallada con señal %d dBm", ssid_out, g_scan_album[i].rssi);
                match_found = true;
                break;
            }
        }
        if (match_found) break; // Ya tenemos la mejor, salimos del bucle
    }

    cJSON_Delete(keychain);
    free(json_keychain); 
    return match_found;
}

int wifi_scanner_get_results(wifi_scan_result_t *results, int max_results) {
    if (!results || max_results <= 0) return 0;
    int count_to_copy = (g_networks_found < max_results) ? g_networks_found : max_results;
    memcpy(results, g_scan_album, sizeof(wifi_scan_result_t) * count_to_copy);
    return count_to_copy;
}

bool wifi_scanner_is_network_available(const char *target_ssid) {
    if (!target_ssid) return false;
    for (int i = 0; i < g_networks_found; i++) {
        if (strcmp(g_scan_album[i].ssid, target_ssid) == 0) return true;
    }
    return false;
}