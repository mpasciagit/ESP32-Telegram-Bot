#ifndef SYSTEM_STATE_H
#define SYSTEM_STATE_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    SYSTEM_STATE_BOOT = 0,
    SYSTEM_STATE_SCANNING,
    SYSTEM_STATE_TRY_STA,
    SYSTEM_STATE_CONNECTED,
    SYSTEM_STATE_DISCONNECTED,
    SYSTEM_STATE_PROVISIONING,
    SYSTEM_STATE_ERROR
} system_state_t;

void system_state_init(void);
void system_state_set(system_state_t state);
system_state_t system_state_get(void);
void system_state_task(void *pvParameters);
void system_state_request_connection(const char* ssid, const char* pass);

bool system_state_is_connected(void);
bool system_state_is_provisioning(void);

#ifdef __cplusplus
}
#endif

#endif