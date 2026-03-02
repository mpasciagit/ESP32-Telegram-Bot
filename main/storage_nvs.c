#include "storage_nvs.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "cJSON.h"
#include <string.h>

static const char *TAG = "storage_nvs";
static const char *NVS_NAMESPACE = "wifi_storage";
static const char *KEY_KEYCHAIN = "keychain";

#define MAX_NETWORKS 10  // El límite que acordamos

// --- FUNCIONES INTERNAS ---

static bool save_keychain_to_nvs(cJSON *root) {
    char *json_str = cJSON_PrintUnformatted(root);
    if (!json_str) return false;

    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        free(json_str);
        return false;
    }

    err = nvs_set_str(handle, KEY_KEYCHAIN, json_str);
    if (err == ESP_OK) nvs_commit(handle);
    
    nvs_close(handle);
    free(json_str);
    return (err == ESP_OK);
}

static cJSON* load_keychain_from_nvs() {
    nvs_handle_t handle;
    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle) != ESP_OK) {
        return cJSON_CreateArray();
    }

    size_t required_size;
    if (nvs_get_str(handle, KEY_KEYCHAIN, NULL, &required_size) != ESP_OK) {
        nvs_close(handle);
        return cJSON_CreateArray();
    }

    char *json_str = malloc(required_size);
    nvs_get_str(handle, KEY_KEYCHAIN, json_str, &required_size);
    nvs_close(handle);

    cJSON *root = cJSON_Parse(json_str);
    free(json_str);

    if (!root) return cJSON_CreateArray();
    return root;
}

// --- API PÚBLICA ---

void storage_nvs_init(void) {
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    } else {
        ESP_ERROR_CHECK(err);
    }
    ESP_LOGI(TAG, "NVS Multi-Red inicializada");
}

/**
 * @brief Guarda o actualiza, respetando el límite de MAX_NETWORKS
 */
bool storage_save_wifi_credentials(const char *ssid, const char *password) {
    if (!ssid || !password) return false;

    cJSON *keychain = load_keychain_from_nvs();
    bool found = false;

    cJSON *item = NULL;
    cJSON_ArrayForEach(item, keychain) {
        cJSON *s = cJSON_GetObjectItem(item, "s");
        if (s && strcmp(s->valuestring, ssid) == 0) {
            cJSON_ReplaceItemInObject(item, "p", cJSON_CreateString(password));
            found = true;
            break;
        }
    }

    if (!found) {
        if (cJSON_GetArraySize(keychain) >= MAX_NETWORKS) {
            ESP_LOGW(TAG, "Límite de redes alcanzado (%d). No se puede añadir.", MAX_NETWORKS);
            cJSON_Delete(keychain);
            return false; // El Bot informará que está lleno
        }
        cJSON *new_net = cJSON_CreateObject();
        cJSON_AddStringToObject(new_net, "s", ssid);
        cJSON_AddStringToObject(new_net, "p", password);
        cJSON_AddItemToArray(keychain, new_net);
    }

    bool ok = save_keychain_to_nvs(keychain);
    cJSON_Delete(keychain);
    return ok;
}

/**
 * @brief NUEVA: Borra una red específica del JSON por su índice (1..N)
 */
bool storage_delete_network_by_index(int index) {
    cJSON *keychain = load_keychain_from_nvs();
    int size = cJSON_GetArraySize(keychain);
    
    // El índice que viene de Telegram es 1-based, cJSON es 0-based
    if (index < 1 || index > size) {
        cJSON_Delete(keychain);
        return false;
    }

    cJSON_DeleteItemFromArray(keychain, index - 1);
    bool ok = save_keychain_to_nvs(keychain);
    cJSON_Delete(keychain);
    
    if (ok) ESP_LOGI(TAG, "Red en índice %d eliminada", index);
    return ok;
}

// ... Las demás funciones (load, get_json, clear, exist) se mantienen igual ...
char* storage_get_networks_json() {
    cJSON *keychain = load_keychain_from_nvs();
    char *out = cJSON_Print(keychain);
    cJSON_Delete(keychain);
    return out; 
}

bool storage_wifi_credentials_exist(void) {
    cJSON *keychain = load_keychain_from_nvs();
    int count = cJSON_GetArraySize(keychain);
    cJSON_Delete(keychain);
    return (count > 0);
}