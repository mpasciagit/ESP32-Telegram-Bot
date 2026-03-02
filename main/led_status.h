#ifndef LED_STATUS_H
#define LED_STATUS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    LED_STATUS_OFF = 0,
    LED_STATUS_BOOTING,
    LED_STATUS_SCANNING,          // Agregado para sincronizar con system_state
    LED_STATUS_WIFI_CONNECTING,
    LED_STATUS_WIFI_CONNECTED,
    LED_STATUS_WIFI_DISCONNECTED,
    LED_STATUS_PROVISIONING,
    LED_STATUS_ERROR
} led_status_t;

void led_status_init(void);
void led_status_task(void *pvParameters);
void led_status_set(led_status_t status);

#ifdef __cplusplus
}
#endif

#endif // LED_STATUS_H