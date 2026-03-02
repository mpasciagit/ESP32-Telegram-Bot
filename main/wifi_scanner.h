#ifndef WIFI_SCANNER_H
#define WIFI_SCANNER_H

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief Estructura para almacenar los resultados del escaneo de redes.
 */
typedef struct {
    char ssid[33];         /**< Nombre de la red (SSID) */
    int8_t rssi;           /**< Fuerza de la señal en dBm */
    uint8_t authmode;      /**< Modo de cifrado */
    uint8_t channel;       /**< Canal de radio */
    bool hidden;           /**< Si la red es oculta */
} wifi_scan_result_t;

/**
 * @brief Inicializa el scanner y limpia el álbum en RAM.
 */
void wifi_scanner_init(void);

/**
 * @brief Realiza el escaneo físico y guarda los resultados en RAM.
 * @return Cantidad de redes encontradas, 0 si ninguna, -1 si hay error de hardware.
 */
int wifi_scanner_execute_actual_scan(void);

/**
 * @brief Obtiene los resultados DESDE LA RAM (sin tocar el hardware).
 */
int wifi_scanner_get_results(wifi_scan_result_t *results, int max_results);

/**
 * @brief Verifica si un SSID específico está presente en el "Álbum" de RAM.
 */
bool wifi_scanner_is_network_available(const char *target_ssid);

// --- LA NUEVA JOYA DE LA CORONA ---

/**
 * @brief Cruza el escaneo actual con el llavero NVS y devuelve la mejor red.
 * @param ssid_out Buffer para copiar el SSID encontrado.
 * @param pass_out Buffer para copiar el Password encontrado.
 * @return true si encontró una red conocida en el aire, false si no conoce ninguna.
 */
bool wifi_scanner_get_best_known_network(char *ssid_out, char *pass_out);

#endif // WIFI_SCANNER_H