#ifndef WIFI_PROVISIONING_H
#define WIFI_PROVISIONING_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    char ssid[32];
    char password[64];
} wifi_credentials_t;

void wifi_provisioning_init(void);
void wifi_provisioning_start(void);
void wifi_provisioning_stop(void);
bool wifi_provisioning_is_active(void);

/**
 * @brief Setea las credenciales recibidas desde el servidor HTTP
 */
void wifi_provisioning_set_credentials(const char* ssid, const char* password);

/**
 * @brief Indica si hay credenciales nuevas listas para ser procesadas
 */
bool wifi_provisioning_has_new_credentials(void);

/**
 * @brief Obtiene las credenciales para su persistencia en NVS
 */
void wifi_provisioning_get_credentials(wifi_credentials_t *creds);

#ifdef __cplusplus
}
#endif

#endif // WIFI_PROVISIONING_H