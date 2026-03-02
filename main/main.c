#include "esp_log.h"
#include "esp_system.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// Componentes mínimos para el arranque
#include "storage_nvs.h"
#include "led_status.h"
#include "system_state.h"
#include "wifi_manager.h"
#include "wifi_provisioning.h"
#include "app_telegram.h"

// Necesario para la línea de interfaz que vamos a agregar
#include "esp_netif.h"

static const char *TAG = "MAIN_APP";

void app_main(void)
{
    ESP_LOGI(TAG, "== ARRANCANDO ESP32 PROVISIONING SYSTEM ==");

    // 1. Capa de Datos: Disco duro (NVS) y Periféricos (LED)
    storage_nvs_init();
    led_status_init();

    // 2. Capa de Red: Inicializa Pilas, Eventos, Interfaces (AP/STA) y Driver en modo RAM.
    // Aquí es donde se silencia el auto-conectado.
    wifi_manager_init();

    // 3. Capa de Aplicación: Prepara buffers de credenciales y Scanner.
    wifi_provisioning_init();
    // wifi_scanner_init(); // Asegúrate de llamar al init del scanner si lo tienes separado

    // 4. Feedback Visual: Lanzamos el LED antes del flujo lógico.
    xTaskCreate(led_status_task, "led_task", 2048, NULL, 5, NULL);
    
    // 5. El Cerebro: Inicia el Flowchart (BOOT -> SCANNING -> ...)
    // Importante: No llamar antes de wifi_manager_init.
    system_state_init();

    // 6. Lanzamnos Telegram Bot
    app_telegram_start();

    /* BUCLE DE SUPERVISIÓN */
    while (1) {
        if (system_state_get() == SYSTEM_STATE_ERROR) {
            ESP_LOGE(TAG, "Estado de ERROR crítico. Reiniciando en 5s...");
            vTaskDelay(pdMS_TO_TICKS(5000));
            esp_restart();
        }
        vTaskDelay(pdMS_TO_TICKS(10000)); 
    }
}