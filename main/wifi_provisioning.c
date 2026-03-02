#include "wifi_provisioning.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "lwip/ip_addr.h"
#include <string.h>

#include "http_server.h"
#include "dns_server.h" 

#define PROVISIONING_AP_SSID  "ESP32_Setup"
#define PROVISIONING_AP_PASS  "12345678"
#define PROVISIONING_CHANNEL  6
#define MAX_STA_CONN          4

static const char *TAG = "wifi_provisioning";

static bool provisioning_active = false;
static bool has_creds = false;
static wifi_credentials_t captured_creds;

void wifi_provisioning_init(void)
{
    ESP_LOGI(TAG, "Modulo de provision inicializado");
    memset(&captured_creds, 0, sizeof(wifi_credentials_t));
}

void wifi_provisioning_start(void)
{
    if (provisioning_active) return;
    ESP_LOGI(TAG, "Iniciando SoftAP de configuracion...");

    // 1. NO HACEMOS STOP. El radio ya está encendido desde el BOOT.
    // Solo cambiamos el modo para no perder la capacidad de cliente (STA)
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA)); 

    // 2. Configuración del SSID (Abierto)
    wifi_config_t ap_config = {
        .ap = {
            .ssid = "ESP32_OPEN_TEST",
            .ssid_len = 0,
            .channel = 1,
            .max_connection = 4,
            .authmode = WIFI_AUTH_OPEN 
        },
    };
    // Aplicamos configuración solo a la interfaz AP
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_config));

    // 3. CONFIGURAR LA RED (IP Y DHCP)
    // Usamos el handle estándar para asegurar que el DHCP asigne 192.168.4.1
    esp_netif_t *netif_ap = esp_netif_get_handle_from_ifkey("WIFI_AP_DEF");
    if (netif_ap) {
        esp_netif_dhcps_stop(netif_ap);
        esp_netif_ip_info_t ip_info;
        IP4_ADDR(&ip_info.ip, 192, 168, 4, 1);
        IP4_ADDR(&ip_info.gw, 192, 168, 4, 1);
        IP4_ADDR(&ip_info.netmask, 255, 255, 255, 0);
        esp_netif_set_ip_info(netif_ap, &ip_info);
        esp_netif_dhcps_start(netif_ap);
        ESP_LOGI(TAG, "Red configurada en 192.168.4.1");
    }

    // 4. Lanzar servicios (HTTP y DNS Captive Portal)
    http_server_start();
    dns_server_start();

    provisioning_active = true;
}

void wifi_provisioning_stop(void)
{
    if (!provisioning_active) return;
    ESP_LOGI(TAG, "Deteniendo modo provision");

    http_server_stop();
    dns_server_stop();
    
    // IMPORTANTE: No apagamos el radio (stop), solo limpiamos la config del AP
    // para que el radio quede libre para la conexión STA.
    wifi_config_t empty_ap_config = {0};
    esp_wifi_set_config(WIFI_IF_AP, &empty_ap_config);

    provisioning_active = false;
}

bool wifi_provisioning_is_active(void)
{
    return provisioning_active;
}

void wifi_provisioning_set_credentials(const char* ssid, const char* pass)
{
    memset(&captured_creds, 0, sizeof(wifi_credentials_t));
    strncpy(captured_creds.ssid, ssid, sizeof(captured_creds.ssid) - 1);
    strncpy(captured_creds.password, pass, sizeof(captured_creds.password) - 1);
    has_creds = true;
    ESP_LOGI(TAG, "Credenciales capturadas: %s", captured_creds.ssid);
}

bool wifi_provisioning_has_new_credentials(void)
{
    return has_creds;
}

void wifi_provisioning_get_credentials(wifi_credentials_t *creds)
{
    if (creds) {
        memcpy(creds, &captured_creds, sizeof(wifi_credentials_t));
        has_creds = false; // IMPORTANTE: Resetear para que el cerebro no re-procese
    }
}