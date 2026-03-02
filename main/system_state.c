#include "system_state.h"
#include "wifi_manager.h"
#include "wifi_provisioning.h"
#include "wifi_scanner.h" 
#include "storage_nvs.h"
#include "led_status.h"
#include "dns_server.h"
#include "http_server.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include <string.h>

static const char *TAG = "system_state";
static system_state_t current_state = SYSTEM_STATE_BOOT;
static bool force_provisioning = false;
static bool es_nueva_config = false; 
static bool modo_espera_scan = false;
static int contador_espera = 0;

#define STA_CONNECT_TIMEOUT_MS 15000
#define RETRY_DELAY_MS         5000 
#define MAX_STA_RETRIES        3
#define WIFI_CONNECTED_BIT BIT0 

void system_state_set(system_state_t state) {
    current_state = state;
    ESP_LOGI(TAG, "Cambiando estado del sistema -> %d", state);
}

system_state_t system_state_get(void) {
    return current_state;
}

// --- FUNCIÓN PARA TELEGRAM (UBICACIÓN CORRECTA) ---
void system_state_request_connection(const char* ssid, const char* pass) {
    ESP_LOGI(TAG, "Telegram solicita probar red: %s", ssid);
    wifi_manager_set_credentials(ssid, pass);
    es_nueva_config = true;  // Sello de calidad: guardar si conecta
    system_state_set(SYSTEM_STATE_SCANNING); // Pasamos por SCANNING para validar presencia
}

void system_state_init(void) {
    ESP_LOGI(TAG, "Inicializando gestor de estados...");
    system_state_set(SYSTEM_STATE_BOOT);
    xTaskCreate(system_state_task, "system_state_task", 4096, NULL, 5, NULL);
}

void system_state_task(void *pvParameters) {
    TickType_t connect_start_time = 0;
    int retry_count = 0;
    char ssid[32], pass[64];

    // 1. Configuración del GPIO 0 (Vigilante)
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << GPIO_NUM_0),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
    };
    gpio_config(&io_conf);

    while (1) {
        // --- VIGILANTE DE BOTÓN ---
        if (gpio_get_level(GPIO_NUM_0) == 0) {
            vTaskDelay(pdMS_TO_TICKS(50)); 
            if (gpio_get_level(GPIO_NUM_0) == 0) {
                ESP_LOGW(TAG, "¡Botón detectado! Forzando Modo Configuración.");
                force_provisioning = true;
                contador_espera = 30; 
                system_state_set(SYSTEM_STATE_SCANNING);
            }
        }

        system_state_t state = system_state_get();
        switch (state) {
            case SYSTEM_STATE_BOOT:
                led_status_set(LED_STATUS_BOOTING);
                esp_err_t ret = esp_wifi_start();
                if (ret != ESP_OK) ESP_LOGE(TAG, "Fallo crítico al iniciar radio");
                vTaskDelay(pdMS_TO_TICKS(500));
                system_state_set(SYSTEM_STATE_SCANNING);
                break;

            case SYSTEM_STATE_SCANNING:
                if (modo_espera_scan) {
                    if (contador_espera < 30) {
                        contador_espera++;
                        if (contador_espera % 5 == 0) ESP_LOGI(TAG, "Esperando... (%d/30s)", contador_espera);
                        vTaskDelay(pdMS_TO_TICKS(500));
                        break; 
                    } else {
                        modo_espera_scan = false;
                        contador_espera = 0;
                    }
                }
    
                ESP_LOGI(TAG, "Estado: SCANNING (Armando álbum)");
                int redes_encontradas = wifi_scanner_execute_actual_scan();

                if (redes_encontradas <= 0) {
                    ESP_LOGW(TAG, "No hay redes. Iniciando cuenta de 30s...");
                    modo_espera_scan = true;
                    contador_espera = 0;
                } 
                else {
                    // --- NODO DE DECISIÓN PRIORIZADO ---
                    if (force_provisioning) {
                        ESP_LOGW(TAG, "Banderín activo. Saltando a Provisión.");
                        force_provisioning = false; 
                        system_state_set(SYSTEM_STATE_PROVISIONING);
                    }
                    // PRIORIDAD 1: Si hay una nueva config manual (Telegram/Portal), probarla primero
                    else if (es_nueva_config) {
                        ESP_LOGI(TAG, "Prioridad: Probando nueva configuración recibida.");
                        wifi_manager_reconnect(); 
                        system_state_set(SYSTEM_STATE_TRY_STA);
                        connect_start_time = xTaskGetTickCount();
                    }
                    // PRIORIDAD 2: Resiliencia (Mejor red del llavero)
                    else if (wifi_scanner_get_best_known_network(ssid, pass)) {
                        ESP_LOGI(TAG, "Match: La mejor red conocida es '%s'.", ssid);
                        wifi_manager_set_credentials(ssid, pass);
                        wifi_manager_reconnect(); 
                        system_state_set(SYSTEM_STATE_TRY_STA);
                        connect_start_time = xTaskGetTickCount();
                    } 
                    else {
                        ESP_LOGW(TAG, "Redes visibles pero ninguna conocida.");
                        system_state_set(SYSTEM_STATE_PROVISIONING);
                    }
                }
                break;

            case SYSTEM_STATE_PROVISIONING:
                led_status_set(LED_STATUS_PROVISIONING);
                if (!wifi_provisioning_is_active()) {
                    wifi_provisioning_start(); 
                    dns_server_start();
                    http_server_start();
                }
                
                if (wifi_provisioning_has_new_credentials()) {
                    wifi_credentials_t creds;
                    wifi_provisioning_get_credentials(&creds);
                    wifi_manager_set_credentials(creds.ssid, creds.password);
                    es_nueva_config = true; 

                    http_server_stop();
                    dns_server_stop();
                    wifi_provisioning_stop();
                    
                    // Al salir del portal, re-escaneamos para validar la red elegida
                    system_state_set(SYSTEM_STATE_SCANNING);
                }
                break;

            case SYSTEM_STATE_TRY_STA:
                led_status_set(LED_STATUS_WIFI_CONNECTING);
                EventGroupHandle_t ev_grp = wifi_manager_get_event_group();

                if (ev_grp != NULL) {
                    EventBits_t bits = xEventGroupWaitBits(ev_grp, WIFI_CONNECTED_BIT, pdFALSE, pdTRUE, pdMS_TO_TICKS(100));

                    if (bits & WIFI_CONNECTED_BIT) { 
                        if (es_nueva_config) {
                            wifi_manager_get_credentials(ssid, pass); 
                            storage_save_wifi_credentials(ssid, pass); 
                            es_nueva_config = false; 
                            ESP_LOGI(TAG, "Red validada y guardada en el JSON NVS.");
                        } else {
                            ESP_LOGI(TAG, "Conexión exitosa con red conocida.");
                        }

                        system_state_set(SYSTEM_STATE_CONNECTED);
                        retry_count = 0;
                        connect_start_time = 0;
                    } else {
                        uint8_t reason = wifi_manager_get_last_disconnect_reason();
                        // Error crítico de credenciales
                        if (reason == WIFI_REASON_AUTH_FAIL || reason == 15) { 
                            ESP_LOGE(TAG, "Fallo de credenciales (Bot/Portal).");
                            es_nueva_config = false; // Limpiamos para no reintentar algo mal escrito
                            system_state_set(SYSTEM_STATE_PROVISIONING);
                        } else {
                            if (connect_start_time == 0) connect_start_time = xTaskGetTickCount();
                            if ((xTaskGetTickCount() - connect_start_time) > pdMS_TO_TICKS(STA_CONNECT_TIMEOUT_MS)) {
                                ESP_LOGW(TAG, "Timeout alcanzado");
                                connect_start_time = 0;
                                system_state_set(SYSTEM_STATE_DISCONNECTED);
                            }
                        }
                    }
                }
                break;

            case SYSTEM_STATE_CONNECTED:
                led_status_set(LED_STATUS_WIFI_CONNECTED);
                if (!wifi_manager_is_connected()) {
                    ESP_LOGW(TAG, "Conexión perdida.");
                    system_state_set(SYSTEM_STATE_DISCONNECTED);
                }
                break;

            case SYSTEM_STATE_DISCONNECTED:
                if (retry_count >= MAX_STA_RETRIES) {
                    ESP_LOGW(TAG, "Reintentos agotados. Volviendo a escanear.");
                    retry_count = 0; 
                    es_nueva_config = false; // Si era una prueba, se descarta
                    system_state_set(SYSTEM_STATE_SCANNING); 
                } else {
                    retry_count++;
                    ESP_LOGI(TAG, "Reintento %d de %d...", retry_count, MAX_STA_RETRIES);
                    vTaskDelay(pdMS_TO_TICKS(RETRY_DELAY_MS));
                    wifi_manager_reconnect();
                    system_state_set(SYSTEM_STATE_TRY_STA);
                }
                break;

            case SYSTEM_STATE_ERROR:
                led_status_set(LED_STATUS_ERROR);
                vTaskDelay(pdMS_TO_TICKS(5000));
                system_state_set(SYSTEM_STATE_BOOT);
                break;

            default:
                system_state_set(SYSTEM_STATE_ERROR);
                break;
        }
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}