#ifndef APP_TELEGRAM_H
#define APP_TELEGRAM_H

#include "esp_err.h"

/**
 * @brief Inicializa y lanza la tarea del Bot de Telegram.
 * Crea la tarea con el stack necesario para HTTPS.
 */
void app_telegram_start(void);

/**
 * @brief Envía un mensaje de texto al Chat ID configurado.
 * @param chat_id El ID del destinatario (normalmente MY_CHAT_ID).
 * @param text El cuerpo del mensaje.
 * @return ESP_OK si se envió correctamente.
 */
esp_err_t telegram_send_message(const char *chat_id, const char *text);

#endif // APP_TELEGRAM_H