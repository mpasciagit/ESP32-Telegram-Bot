#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Inicia el servidor HTTP para el portal cautivo.
 * Registra los manejadores para /, /scan, /connect y /status.
 */
void http_server_start(void);

/**
 * @brief Detiene el servidor HTTP y libera sus recursos.
 */
void http_server_stop(void);

/**
 * @brief Verifica si el servidor HTTP está actualmente en ejecución.
 * @return true si el servidor está activo, false de lo contrario.
 */
bool http_server_is_running(void);

#ifdef __cplusplus
}
#endif

#endif // HTTP_SERVER_H