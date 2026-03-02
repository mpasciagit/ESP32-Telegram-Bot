#include "led_status.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define LED_GPIO 2 

static volatile led_status_t current_status = LED_STATUS_BOOTING;

void led_status_init(void) {
    gpio_reset_pin(LED_GPIO);
    gpio_set_direction(LED_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_level(LED_GPIO, 0);
}

void led_status_set(led_status_t status) {
    current_status = status;
}

void led_status_task(void *pvParameters) {
    while (1) {
        switch (current_status) {
            case LED_STATUS_BOOTING:      // Parpadeo lento y constante
                gpio_set_level(LED_GPIO, 1); vTaskDelay(pdMS_TO_TICKS(500));
                gpio_set_level(LED_GPIO, 0); vTaskDelay(pdMS_TO_TICKS(500));
                break;

            case LED_STATUS_SCANNING:     // Dos destellos rápidos y pausa
                for(int i=0; i<2; i++){
                    gpio_set_level(LED_GPIO, 1); vTaskDelay(pdMS_TO_TICKS(100));
                    gpio_set_level(LED_GPIO, 0); vTaskDelay(pdMS_TO_TICKS(100));
                }
                vTaskDelay(pdMS_TO_TICKS(600));
                break;

            case LED_STATUS_WIFI_CONNECTING: // Parpadeo rítmico medio
                gpio_set_level(LED_GPIO, 1); vTaskDelay(pdMS_TO_TICKS(250));
                gpio_set_level(LED_GPIO, 0); vTaskDelay(pdMS_TO_TICKS(250));
                break;

            case LED_STATUS_WIFI_CONNECTED:  // Encendido fijo
                gpio_set_level(LED_GPIO, 1); vTaskDelay(pdMS_TO_TICKS(1000));
                break;

            case LED_STATUS_PROVISIONING:    // Parpadeo tipo "frenético" (muy rápido)
                gpio_set_level(LED_GPIO, 1); vTaskDelay(pdMS_TO_TICKS(80));
                gpio_set_level(LED_GPIO, 0); vTaskDelay(pdMS_TO_TICKS(80));
                break;

            case LED_STATUS_ERROR:           // Tres pulsos cortos y pausa larga
                for(int i=0; i<3; i++){
                    gpio_set_level(LED_GPIO, 1); vTaskDelay(pdMS_TO_TICKS(100));
                    gpio_set_level(LED_GPIO, 0); vTaskDelay(pdMS_TO_TICKS(100));
                }
                vTaskDelay(pdMS_TO_TICKS(1000));
                break;

            default:
                gpio_set_level(LED_GPIO, 0); vTaskDelay(pdMS_TO_TICKS(500));
                break;
        }
    }
}