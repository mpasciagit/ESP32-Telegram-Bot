#include <string.h>
#include <sys/socket.h>
#include <netdb.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "dns_server.h"
#include <lwip/sockets.h> // Importante para close() y sockets

static const char *TAG = "dns_server";
static int socket_fd = -1;
static TaskHandle_t dns_task_handle = NULL;

#define DNS_PORT 53

void dns_server_task(void *pvParameters) {
    uint8_t rx_buffer[512];
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_addr_len = sizeof(client_addr);

    socket_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (socket_fd < 0) {
        ESP_LOGE(TAG, "No se pudo crear el socket DNS");
        vTaskDelete(NULL);
        return;
    }
    
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(DNS_PORT);

    if (bind(socket_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        ESP_LOGE(TAG, "Error en bind puerto 53");
        close(socket_fd);
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI(TAG, "DNS Server de Producción iniciado...");

    while (1) {
        int len = recvfrom(socket_fd, rx_buffer, sizeof(rx_buffer), 0, (struct sockaddr *)&client_addr, &client_addr_len);
        if (len > 12) {
            // HEADER DNS
            rx_buffer[2] |= 0x80; // QR = 1 (Respuesta)
            rx_buffer[2] |= 0x04; // AA = 1 (Autoritativo)
            
            // Importante: Copiar el bit de recursión deseada (RD) del cliente al bit de recursión disponible (RA)
            if (rx_buffer[2] & 0x01) rx_buffer[3] |= 0x80; 
            
            rx_buffer[3] &= 0xF0; // RCODE = 0 (No error)
            rx_buffer[6] = 0; rx_buffer[7] = 1; // 1 Answer

            // ANSWER SECTION
            uint8_t *answer = &rx_buffer[len];
            *answer++ = 0xc0; *answer++ = 0x0c; // Pointer a la pregunta
            *answer++ = 0x00; *answer++ = 0x01; // Type A
            *answer++ = 0x00; *answer++ = 0x01; // Class IN
            *answer++ = 0x00; *answer++ = 0x00; *answer++ = 0x00; *answer++ = 0x3c; // TTL 60s
            *answer++ = 0x00; *answer++ = 0x04; // Len 4
            *answer++ = 192;  *answer++ = 168;  *answer++ = 4;    *answer++ = 1;

            sendto(socket_fd, rx_buffer, len + 16, 0, (struct sockaddr *)&client_addr, client_addr_len);
        }
        // ELIMINADO: vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void dns_server_start(void) {
    if (dns_task_handle == NULL) {
        xTaskCreate(dns_server_task, "dns_task", 4096, NULL, 5, &dns_task_handle);
    }
}

void dns_server_stop(void) {
    if (dns_task_handle) {
        vTaskDelete(dns_task_handle);
        dns_task_handle = NULL;
    }
    if (socket_fd != -1) {
        close(socket_fd);
        socket_fd = -1;
    }
}