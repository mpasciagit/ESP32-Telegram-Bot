#include "app_telegram.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "esp_log.h"
#include "esp_http_client.h"
#include "cJSON.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_timer.h"
#include "esp_random.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "wifi_manager.h"
#include "system_state.h"
#include "storage_nvs.h" 
#include "wifi_scanner.h"

static const char *TAG = "TELEGRAM_BOT";

// CONFIGURATION
#define BOT_TOKEN "YOUR_BOT_TOKEN_HERE"
#define MY_CHAT_ID "YOUR_CHAT_ID_HERE"
#define TELEGRAM_BASE_URL "https://api.telegram.org/bot"

static int last_update_id = 0;

// ACCESS MANAGEMENT (RAM)
static char session_key[8];
#define MAX_GUESTS 3
static char guests_id[MAX_GUESTS][32];
static int guests_count = 0;

// SCAN CACHE
#define SCAN_CACHE_SIZE 10
static char scan_cache[SCAN_CACHE_SIZE][33]; 
static int cached_networks_count = 0;

/**
 * @brief Genera una clave hexadecimal única de 6 caracteres por cada booteo
 */
void generate_session_key() {
    const char *charset = "0123456789ABCDEF";
    for (int i = 0; i < 6; i++) {
        session_key[i] = charset[esp_random() % 16];
    }
    session_key[6] = '\0';
    ESP_LOGI(TAG, "🔑 Session Key generated: %s", session_key);
}

/**
 * @brief RBAC (Role Based Access Control) check
 */
bool is_authorized(const char *id) {
    if (strcmp(id, MY_CHAT_ID) == 0) return true;
    for (int i = 0; i < guests_count; i++) {
        if (strcmp(guests_id[i], id) == 0) return true;
    }
    return false;
}

esp_err_t telegram_send_message(const char *chat_id, const char *text) {
    char post_data[1536]; // Buffer aumentado para mensajes largos de ayuda/scan
    snprintf(post_data, sizeof(post_data), "{\"chat_id\":\"%s\",\"text\":\"%s\"}", chat_id, text);

    char url[256];
    snprintf(url, sizeof(url), "%s%s/sendMessage?parse_mode=Markdown", TELEGRAM_BASE_URL, BOT_TOKEN);

    esp_http_client_config_t config = {
        .url = url,
        .method = HTTP_METHOD_POST,
        .timeout_ms = 8000,
        .skip_cert_common_name_check = true,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, post_data, strlen(post_data));

    esp_err_t err = esp_http_client_perform(client);
    esp_http_client_cleanup(client);
    return err;
}

void handle_telegram_command(const char *from_id, const char *text) {
    
    // GUEST LOG-IN (OTP Logic)
    if (!is_authorized(from_id)) {
        char key_cmd[32];
        snprintf(key_cmd, sizeof(key_cmd), "/key %s", session_key);
        
        if (strcmp(text, key_cmd) == 0) {
            if (guests_count < MAX_GUESTS) {
                strncpy(guests_id[guests_count++], from_id, 31);
                telegram_send_message(from_id, "✅ *Access granted.* Welcome to ESP32 Control.");
            } else {
                telegram_send_message(from_id, "⚠️ Guest list full. Contact Admin.");
            }
        } else if (strcmp(text, "/start") == 0 || text[0] == '/') {
            telegram_send_message(from_id, "🚫 *Access Denied.*\nPlease enter session key: `/key CODE`.");
        }
        return; 
    }

    // --- AUTHORIZED COMMANDS ---

    // 1. STATUS
    if (strcmp(text, "/status") == 0) {
        esp_netif_ip_info_t ip_info;
        esp_netif_t* netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
        esp_netif_get_ip_info(netif, &ip_info);
        char ip_str[16];
        esp_ip4addr_ntoa(&ip_info.ip, ip_str, sizeof(ip_str));

        wifi_ap_record_t ap_info;
        char ssid_actual[33] = "Disconnected";
        int rssi = 0;
        if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
            strncpy(ssid_actual, (char*)ap_info.ssid, 32);
            rssi = ap_info.rssi;
        }

        int uptime_min = (int)(esp_timer_get_time() / 1000000) / 60;
        char response[400];
        snprintf(response, sizeof(response), 
                 "🤖 *ESP32 STATUS*\n━━━━━━━━━━━━\n"
                 "📶 *SSID:* %s\n"
                 "📡 *Signal:* %d dBm\n"
                 "🌐 *IP:* %s\n"
                 "⏳ *Uptime:* %d min\n"
                 "👤 *Role:* %s\n"
                 "⚖️ *Priority:* Best Signal (RSSI)", 
                 ssid_actual, rssi, ip_str, uptime_min, 
                 (strcmp(from_id, MY_CHAT_ID) == 0 ? "Admin" : "Guest"));
        telegram_send_message(from_id, response);
    }

    // 2. LIST (Stored in NVS)
    else if (strcmp(text, "/list") == 0) {
        char *json_str = storage_get_networks_json();
        if (!json_str) {
            telegram_send_message(from_id, "⚠️ NVS Keychain is empty.");
            return;
        }
        cJSON *root = cJSON_Parse(json_str);
        if (cJSON_IsArray(root)) {
            char list_msg[512] = "🗂️ *NVS Keychain:*\n━━━━━━━━━━━━\n";
            int count = 0;
            cJSON *item;
            cJSON_ArrayForEach(item, root) {
                cJSON *s = cJSON_GetObjectItem(item, "s");
                if (s) {
                    char line[64];
                    snprintf(line, sizeof(line), "%d. 🔑 %s\n", ++count, s->valuestring);
                    strcat(list_msg, line);
                }
            }
            strcat(list_msg, "\nUse `/connect N` or `/delete N`.");
            telegram_send_message(from_id, list_msg);
        }
        cJSON_Delete(root);
        free(json_str);
    }

    // 3. SCAN (Real-time air)
    else if (strcmp(text, "/scan") == 0) {
        telegram_send_message(from_id, "🔎 Scanning environment...");
        wifi_scanner_execute_actual_scan(); 
        wifi_scan_result_t resultados[SCAN_CACHE_SIZE];
        int ap_count = wifi_scanner_get_results(resultados, SCAN_CACHE_SIZE);
        
        char resp[1024] = "📡 *Available Networks:*\n━━━━━━━━━━━━\n";
        cached_networks_count = 0;
        for (int i = 0; i < ap_count; i++) {
            strncpy(scan_cache[i], resultados[i].ssid, 32);
            scan_cache[i][32] = '\0';
            cached_networks_count++;
            char line[64];
            snprintf(line, sizeof(line), "%d. `%s` (%d dBm)\n", i+1, scan_cache[i], resultados[i].rssi);
            strcat(resp, line);
        }
        strcat(resp, "\nUse: `/test N,PASS` for new networks.");
        telegram_send_message(from_id, resp);
    }

    // 4. TEST (New network)
    else if (strncmp(text, "/test ", 6) == 0) {
        char *input = (char*)text + 6;
        char *comma = strchr(input, ',');
        if (comma) {
            *comma = '\0';
            int idx = atoi(input) - 1;
            char *pass = comma + 1;
            while (*pass == ' ') pass++; 
            if (idx >= 0 && idx < cached_networks_count) {
                telegram_send_message(from_id, "🧪 Validating connection...");
                system_state_request_connection(scan_cache[idx], pass);
            }
        }
    }

    // 5. CONNECT (Salto directo por índice)
    else if (strncmp(text, "/connect ", 9) == 0) {
        int idx_to_conn = atoi(text + 9);
        char *json_str = storage_get_networks_json();
        bool found = false;

        if (json_str) {
            cJSON *root = cJSON_Parse(json_str);
            if (cJSON_IsArray(root)) {
                cJSON *item = cJSON_GetArrayItem(root, idx_to_conn - 1);
                if (item) {
                    const char *ssid = cJSON_GetObjectItem(item, "s")->valuestring;
                    const char *pass = cJSON_GetObjectItem(item, "p")->valuestring;
                    
                    char msg[128];
                    snprintf(msg, sizeof(msg), "🔄 Switching to: %s...", ssid);
                    telegram_send_message(from_id, msg);
                    
                    system_state_request_connection(ssid, pass);
                    found = true;
                }
            }
            cJSON_Delete(root);
            free(json_str);
        }
        if (!found) telegram_send_message(from_id, "❌ Index not found.");
    }

    // 6. DELETE
    else if (strncmp(text, "/delete ", 8) == 0) {
        int idx = atoi(text + 8); 
        if (storage_delete_network_by_index(idx)) {
            char msg[64];
            snprintf(msg, sizeof(msg), "✅ Network #%d deleted.", idx);
            telegram_send_message(from_id, msg);
        }
    }

    // 7. REBOOT (Hardware command)
    else if (strcmp(text, "/reboot") == 0) {
        if (strcmp(from_id, MY_CHAT_ID) == 0) {
            ESP_LOGW(TAG, "Confirmando Reboot y limpiando cola...");
            
            // 1. Enviamos el mensaje de despedida
            telegram_send_message(from_id, "♻️ *Rebooting...* Bye!");

            // 2. IMPORTANTE: Hacemos una petición dummy para confirmar el mensaje actual
            // Esto le dice a Telegram: "Ya leí hasta el last_update_id"
            char confirm_url[256];
            snprintf(confirm_url, sizeof(confirm_url), 
                     "%s%s/getUpdates?offset=%d&limit=1", 
                     TELEGRAM_BASE_URL, BOT_TOKEN, last_update_id + 1);
            
            esp_http_client_config_t conf_cfg = { .url = confirm_url, .method = HTTP_METHOD_GET, .timeout_ms = 5000, .skip_cert_common_name_check = true };
            esp_http_client_handle_t conf_cl = esp_http_client_init(&conf_cfg);
            esp_http_client_perform(conf_cl); // No nos importa la respuesta, solo que el servidor reciba el offset
            esp_http_client_cleanup(conf_cl);

            // 3. Esperamos un segundo extra para que los buffers de red se vacíen
            vTaskDelay(pdMS_TO_TICKS(1500));
            
            esp_restart();
        } else {
            telegram_send_message(from_id, "🚫 Admin only.");
        }
    }

    // 8. HELP / START
    else if (strcmp(text, "/help") == 0 || strcmp(text, "/start") == 0) {
        char help_msg[700];
        snprintf(help_msg, sizeof(help_msg), 
                 "📖 *Command Guide*\n━━━━━━━━━━━━\n"
                 "🔍 *Discovery*\n"
                 "/status - System info & RSSI priority\n"
                 "/list - Stored networks (NVS)\n"
                 "/scan - Scan nearby WiFi (Air)\n\n"
                 "⚙️ *Management*\n"
                 "`/connect N` - Switch to stored net\n"
                 "`/test N,PASS` - Connect to new net\n"
                 "`/delete N` - Remove from NVS\n"
                 "/reboot - Restart device (Admin only)\n\n"
                 "🔑 *Session Key:* `%s` ", session_key);
        telegram_send_message(from_id, help_msg);
    }
}

void telegram_get_updates() {
    char url[256];
    snprintf(url, sizeof(url), "%s%s/getUpdates?offset=%d&timeout=10", TELEGRAM_BASE_URL, BOT_TOKEN, last_update_id + 1);

    char *response_buffer = malloc(2048);
    if (!response_buffer) return;

    esp_http_client_config_t config = { .url = url, .method = HTTP_METHOD_GET, .timeout_ms = 15000, .skip_cert_common_name_check = true };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    
    if (esp_http_client_open(client, 0) == ESP_OK) {
        esp_http_client_fetch_headers(client);
        int read_len = esp_http_client_read(client, response_buffer, 2048);
        if (read_len > 0) {
            response_buffer[read_len] = '\0';
            cJSON *root = cJSON_Parse(response_buffer);
            cJSON *result = cJSON_GetObjectItem(root, "result");
            cJSON *update;
            cJSON_ArrayForEach(update, result) {
                last_update_id = cJSON_GetObjectItem(update, "update_id")->valueint;
                cJSON *message = cJSON_GetObjectItem(update, "message");
                if (message) {
                    cJSON *from = cJSON_GetObjectItem(message, "from");
                    cJSON *id_obj = cJSON_GetObjectItem(from, "id");
                    cJSON *text = cJSON_GetObjectItem(message, "text");
                    if (text && id_obj) {
                        char sender_id[32];
                        snprintf(sender_id, sizeof(sender_id), "%.0f", id_obj->valuedouble);
                        handle_telegram_command(sender_id, text->valuestring);
                    }
                }
            }
            cJSON_Delete(root);
        }
    }
    esp_http_client_cleanup(client);
    free(response_buffer);
}

static void telegram_bot_task(void *pvParameters) {
    generate_session_key();
    bool welcome_sent = false;
    while (1) {
        if (wifi_manager_is_connected()) {
            if (!welcome_sent) {
                char msg[128];
                snprintf(msg, sizeof(msg), "🚀 *System Online*\nSession Key: `%s` ", session_key);
                telegram_send_message(MY_CHAT_ID, msg);
                welcome_sent = true;
            }
            telegram_get_updates();
        } else {
            welcome_sent = false; 
            vTaskDelay(pdMS_TO_TICKS(5000));
        }
    }
}

void app_telegram_start(void) {
    xTaskCreate(telegram_bot_task, "telegram_bot_task", 10240, NULL, 5, NULL);
}