#ifndef STORAGE_NVS_H
#define STORAGE_NVS_H

#include <stdbool.h>

#define WIFI_SSID_MAX_LEN 32
#define WIFI_PASS_MAX_LEN 64

/**
 * @brief Inicializa el almacenamiento NVS y maneja errores de partición.
 */
void storage_nvs_init(void);

/**
 * @brief Guarda una red en el llavero JSON o actualiza su password si ya existe.
 * @note Tiene un límite interno de 10 redes.
 * @return true si se guardó con éxito, false si el llavero está lleno.
 */
bool storage_save_wifi_credentials(const char *ssid, const char *password);

/**
 * @brief Carga la PRIMERA red del llavero.
 * Útil para procesos de reconexión rápida tras el arranque.
 */
bool storage_load_wifi_credentials(char *ssid_out, char *password_out);

/**
 * @brief Borra una red específica del llavero basándose en su índice.
 * @param index Índice de la red (1..N) tal como se muestra en Telegram.
 * @return true si el borrado fue exitoso.
 */
bool storage_delete_network_by_index(int index);

/**
 * @brief Borra el llavero completo (formatea el almacenamiento de redes).
 */
bool storage_clear_wifi_credentials(void);

/**
 * @brief Verifica si el llavero contiene al menos una red.
 */
bool storage_wifi_credentials_exist(void);

/**
 * @brief Obtiene el llavero completo en formato JSON String.
 * @return Puntero a string (Heap). IMPORTANTE: El llamador DEBE ejecutar free().
 */
char* storage_get_networks_json(void);

#endif // STORAGE_NVS_H