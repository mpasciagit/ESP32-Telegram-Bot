#include "http_server.h"
#include "wifi_scanner.h"
#include "wifi_manager.h"
#include "wifi_provisioning.h"
#include "system_state.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

static const char *TAG = "http_server";
static httpd_handle_t server = NULL;

#define WIFI_SCAN_MAX 15

/* =========================
   HTML Captive Portal Page (Template)
   ========================= */
static const char *portal_html_top = 
"<!DOCTYPE html><html><head><meta charset='utf-8' name='viewport' content='width=device-width, initial-scale=1'>"
"<title>Configuración de Red</title>"
"<style>"
"body{font-family:sans-serif;padding:20px;background:#f4f4f9;}"
"h2{color:#333;} .net-item{padding:15px;background:#fff;margin-bottom:8px;border-radius:5px;box-shadow:0 2px 4px rgba(0,0,0,0.1);cursor:pointer;}"
"input{display:block;margin:15px 0;padding:12px;width:100%;border:1px solid #ccc;border-radius:4px;box-sizing:border-box;}"
".btn{background:#007bff;color:white;border:none;padding:15px;width:100%;border-radius:4px;font-size:16px;font-weight:bold;}"
".err{color:white;background:#d9534f;padding:10px;border-radius:4px;margin-bottom:20px;text-align:center;}"
"</style></head>"
"<body onload='getNetworks()'>";

static const char *portal_html_bottom = 
"<h2>Seleccione su red</h2>"
"<div id='list'>Cargando redes disponibles...</div>"
"<form onsubmit='connect(event)'>"
"SSID:<input id='ssid' readonly placeholder='Toque una red de la lista'>"
"Password:<input id='pass' type='password' placeholder='Ingrese contraseña'>"
"<button type='submit' class='btn'>Conectar Equipo</button></form>"
"<script>"
"async function getNetworks(){"
"  let list=document.getElementById('list');"
"  try {"
"    let r=await fetch('/scan');let j=await r.json();"
"    list.innerHTML=''; if(j.length==0) list.innerHTML='No se encontraron redes.'; "
"    j.forEach(n=>{"
"      let d=document.createElement('div');d.className='net-item';"
"      let rssiIcon = n.rssi > -60 ? '📶' : '⚠️';"
"      d.innerHTML='<strong>'+rssiIcon+' '+n.ssid+'</strong> <small>'+n.rssi+' dBm</small>';"
"      d.onclick=()=>{document.getElementById('ssid').value=n.ssid; document.getElementById('pass').focus();};"
"      list.appendChild(d);"
"    });"
"  } catch(e){ list.innerHTML='Error al cargar redes.'; }"
"}"
"async function connect(e){"
"  e.preventDefault();let ssid=document.getElementById('ssid').value;let pass=document.getElementById('pass').value;"
"  if(!ssid){alert('Por favor, seleccione una red');return;}"
"  await fetch('/connect',{method:'POST',body:JSON.stringify({ssid,pass})});"
"  document.body.innerHTML='<h2 style=\"text-align:center\">Conectando...</h2><p style=\"text-align:center\">Intentando unir a la red. Si tiene éxito, este AP se cerrará.</p>';"
"}"
"</script></body></html>";

/* =========================
   Funciones Auxiliares
   ========================= */

// Función centralizada para enviar el HTML con o sin mensaje de error
static esp_err_t send_portal_html(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache, no-store, must-revalidate");

    // Enviamos la parte superior (Header, Styles, etc.)
    httpd_resp_sendstr_chunk(req, portal_html_top);

    // CAPTURA DINÁMICA DE ERRORES:
    uint8_t reason = wifi_manager_get_last_disconnect_reason();

    // Capturamos 203 (AUTH_FAIL) y 15 (HANDSHAKE_TIMEOUT)
    if (reason == WIFI_REASON_AUTH_FAIL || reason == 15) {
        ESP_LOGW("HTTP_SERVER", "Inyectando alerta de error en el portal (Razón: %d)", reason);
        
        // Inyectamos el cartel con un estilo rojo llamativo
        const char* alert = 
            "<div style='background:#fee2e2; border:1px solid #ef4444; color:#991b1b; "
            "padding:12px; border-radius:8px; margin:15px; font-family:sans-serif; text-align:center;'>"
            "<strong>⚠️ Error de Conexión</strong><br>"
            "La contraseña es incorrecta o el router rechazó la conexión."
            "</div>";
        
        httpd_resp_sendstr_chunk(req, alert);
    }

    // Enviamos el resto del cuerpo (Lista de redes y formulario)
    httpd_resp_sendstr_chunk(req, portal_html_bottom);
    
    // Finalizamos el envío
    httpd_resp_sendstr_chunk(req, NULL);
    return ESP_OK;
}

/* =========================
   HTTP Handlers
   ========================= */

static esp_err_t portal_handler(httpd_req_t *req) {
    return send_portal_html(req);
}

static esp_err_t captive_handler(httpd_req_t *req) {
    return send_portal_html(req);
}

static esp_err_t http_404_error_handler(httpd_req_t *req, httpd_err_code_t err) {
    return send_portal_html(req);
}

static esp_err_t scan_handler(httpd_req_t *req) {
    wifi_scan_result_t results[WIFI_SCAN_MAX];
    int count = wifi_scanner_get_results(results, WIFI_SCAN_MAX);
    
    char *json = malloc(2500); 
    if (!json) return ESP_FAIL;

    int offset = sprintf(json, "[");
    for (int i = 0; i < count; i++) {
        offset += snprintf(json + offset, 2500 - offset, 
            "{\"ssid\":\"%s\",\"rssi\":%d,\"auth\":%d}%s",
            results[i].ssid, results[i].rssi, results[i].authmode,
            (i < count - 1) ? "," : "");
    }
    sprintf(json + offset, "]");

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json, HTTPD_RESP_USE_STRLEN);
    
    free(json);
    return ESP_OK;
}

static esp_err_t connect_handler(httpd_req_t *req) {
    char buf[256];
    int len = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (len <= 0) return ESP_FAIL;
    buf[len] = '\0';

    char ssid[64] = {0}, pass[64] = {0};
    char *s = strstr(buf, "\"ssid\":\"");
    if (s) { s += 8; sscanf(s, "%63[^\"]", ssid); }

    char *p = strstr(buf, "\"pass\":\"");
    if (p) { p += 8; sscanf(p, "%63[^\"]", pass); }

    if (strlen(ssid) > 0) {
        ESP_LOGI(TAG, "Web: Recibido SSID: %s. Saltando a TRY_STA...", ssid);
        
        // Pasamos credenciales a memoria temporal
        wifi_provisioning_set_credentials(ssid, pass); 
        
        httpd_resp_send(req, "OK", HTTPD_RESP_USE_STRLEN);

        vTaskDelay(pdMS_TO_TICKS(500)); 
        system_state_set(SYSTEM_STATE_TRY_STA); 
    } else {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Datos inválidos");
    }
    return ESP_OK;
}

static esp_err_t status_handler(httpd_req_t *req) {
    char resp[32];
    snprintf(resp, sizeof(resp), "{\"state\":%d}", system_state_get());
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

/* =========================
   Server Control
   ========================= */

void http_server_start(void) {
    if (server) return;
    ESP_LOGI(TAG, "Iniciando servidor HTTP...");
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.stack_size = 10240;
    config.lru_purge_enable = true;

    if (httpd_start(&server, &config) == ESP_OK) {
        httpd_uri_t uri_root = { .uri = "/", .method = HTTP_GET, .handler = portal_handler };
        httpd_uri_t uri_scan = { .uri = "/scan", .method = HTTP_GET, .handler = scan_handler };
        httpd_uri_t uri_conn = { .uri = "/connect", .method = HTTP_POST, .handler = connect_handler };
        httpd_uri_t uri_stat = { .uri = "/status", .method = HTTP_GET, .handler = status_handler };
        httpd_uri_t uri_captive = { .uri = "*", .method = HTTP_GET, .handler = captive_handler };

        httpd_register_uri_handler(server, &uri_root);
        httpd_register_uri_handler(server, &uri_scan);
        httpd_register_uri_handler(server, &uri_conn);
        httpd_register_uri_handler(server, &uri_stat);
        httpd_register_uri_handler(server, &uri_captive);
        
        httpd_register_err_handler(server, HTTPD_404_NOT_FOUND, http_404_error_handler);
    }
}

void http_server_stop(void) {
    if (server) {
        httpd_stop(server);
        server = NULL;
    }
}