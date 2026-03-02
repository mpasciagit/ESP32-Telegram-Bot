#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <stdbool.h>
#include <stdint.h>
#include "esp_wifi_types.h" // Necesario para wifi_mode_t
#include "esp_wifi.h" // Asegura que reconozca los tipos de WiFi
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

#ifdef __cplusplus
extern "C" {
#endif

/* --- Configuración de Límites --- */
#define WIFI_SSID_MAX_LEN 32
#define WIFI_PASS_MAX_LEN 64

/* --- Funciones de Gestión de Ciclo de Vida --- */

/**
 * @brief Inicializa el stack de red y Wi-Fi en modo STA.
 */
void wifi_manager_init(void);

/**
 * @brief Configura y arranca el Wi-Fi.
 */
void wifi_manager_start(void);

/**
 * @brief Detiene el Wi-Fi por completo.
 */
void wifi_manager_stop(void);

/**
 * @brief Fuerza una reconexión (desconecta y vuelve a conectar).
 */
void wifi_manager_reconnect(void);
EventGroupHandle_t wifi_manager_get_event_group(void);

/* --- Funciones de Configuración (Nuevas) --- */

/**
 * @brief Actualiza las credenciales en memoria para la siguiente conexión.
 * Esta función es llamada por el sistema de estados cuando recibe datos del portal.
 */
void wifi_manager_set_credentials(const char* ssid, const char* password);

/**
 * @brief Cambia el modo de operación del driver WiFi (STA, AP, APSTA).
 */
void wifi_manager_set_mode(wifi_mode_t mode);

/* --- Funciones de Estado --- */

/**
 * @brief Indica si el Wi-Fi está actualmente conectado y con IP.
 */
bool wifi_manager_is_connected(void);

/**
 * @brief Bloquea la tarea hasta que se conecte o pase el tiempo de timeout.
 */
bool wifi_manager_wait_connected(uint32_t timeout_ms);

/**
 * @brief Retorna la dirección IP actual en formato string.
 */
const char* wifi_manager_get_ip(void);

/**
 * @brief Obtiene el código de razón de la última desconexión.
 * @return Código de razón (ej. 203 para error de contraseña).
 */
uint8_t wifi_manager_get_last_disconnect_reason(void);

/**
 * @brief Obtiene las credenciales actuales almacenadas en la RAM del manager.
 * @param ssid_out Puntero donde se copiará el SSID (mínimo 32 bytes).
 * @param pass_out Puntero donde se copiará el Password (mínimo 64 bytes).
 */
void wifi_manager_get_credentials(char *ssid_out, char *pass_out);

void wifi_manager_reset_last_disconnect_reason(void);

#ifdef __cplusplus
}
#endif

#endif // WIFI_MANAGER_H