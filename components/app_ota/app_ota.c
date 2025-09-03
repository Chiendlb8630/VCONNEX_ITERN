
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_http_client.h"
#include "esp_https_ota.h"
#include "protocol_examples_common.h"
#include "string.h"
#include <sys/socket.h>
#include "esp_wifi.h"
#include "app_ota.h"
#include "blufi_app.h "
#include "cJSON.h"
#include "mqtt_app.h"

static const char *TAG = "OTA_APP";
char* OTA_URL = NULL;
bool ota_IsRunning = false;

char *build_ota_res(int error_code);
void handle_ota_data(cJSON *value);
<<<<<<< HEAD
void handle_ota_data_mesh(const char *mesh_url);
=======
void handle_ota_data_mesh(char *mesh_url);
>>>>>>> b672db7 (first commit)

esp_err_t _http_event_handler(esp_http_client_event_t *evt)
{
    switch(evt->event_id) {
        case HTTP_EVENT_ERROR:
            ESP_LOGI(TAG, "HTTP_EVENT_ERROR");
            break;
        case HTTP_EVENT_ON_CONNECTED:
            ESP_LOGI(TAG, "HTTP_EVENT_ON_CONNECTED");
            break;
        case HTTP_EVENT_HEADERS_SENT:
            ESP_LOGI(TAG, "HTTP_EVENT_HEADERS_SENT");
            break;
        case HTTP_EVENT_ON_HEADER:
            ESP_LOGI(TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s",
                     evt->header_key, evt->header_value);
            break;
        case HTTP_EVENT_ON_DATA:
            ESP_LOGI(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
            break;
        case HTTP_EVENT_ON_FINISH:
            ESP_LOGI(TAG, "HTTP_EVENT_ON_FINISH");
            break;
        case HTTP_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "HTTP_EVENT_DISCONNECTED");
            break;
        
          default : 
            break;    
        }
    return ESP_OK;
}

void ota_task(void *pvParameter)
{
    ESP_LOGI(TAG, "Starting OTA example task");
    esp_http_client_config_t config = {
        .url = OTA_URL,
        .cert_pem = NULL,
        .buffer_size = 10240,  
        .event_handler = _http_event_handler,
        .keep_alive_enable = true,
         .skip_cert_common_name_check = true,
    };
    esp_https_ota_config_t ota_config = {
        .http_config = &config,
    };

    ESP_LOGI(TAG, "Attempting to download update from %s", config.url);
    esp_err_t ret = esp_https_ota(&ota_config);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "OTA Succeed, Rebooting...");
        char* msg = build_ota_res(50000);
        if(config_on){
        mqtt_publish_to_app(msg);
        free(msg);
        }
    } else {
        char* msg = build_ota_res(50004);
        mqtt_publish_to_app(msg);
        free(msg);
<<<<<<< HEAD
        esp_restart();
        ESP_LOGE(TAG, "Firmware upgrade failed");
    }
=======
        ESP_LOGE(TAG, "Firmware upgrade failed");
    }
    esp_restart();
>>>>>>> b672db7 (first commit)
    while (1) {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

void handle_ota_data(cJSON *value) {
    if (!value) return;

    cJSON *url = cJSON_GetObjectItem(value, "url");
    if (url && cJSON_IsString(url)) {
        if (OTA_URL) {
            free(OTA_URL);  
        }
        OTA_URL = strdup(url->valuestring);  
        ESP_LOGI("OTA_CMD", "OTA URL = %s", OTA_URL);
    }
    cJSON *ver = cJSON_GetObjectItem(value, "version");
    if (ver && cJSON_IsString(ver)) {
        ESP_LOGI("OTA_CMD", "Firmware version = %s", ver->valuestring);
    }
}

<<<<<<< HEAD
void handle_ota_data_mesh(const char *mesh_url){
=======
void handle_ota_data_mesh(char *mesh_url){
>>>>>>> b672db7 (first commit)
    if(OTA_URL){
        free(OTA_URL);
    }
    OTA_URL = strdup(mesh_url);
<<<<<<< HEAD
    printf("Mesh url is : %s \n", mesh_url);
    ESP_LOGI("OTA_MESH_CMD", "Pass url value to OTA : %s\n", OTA_URL);
=======
    ESP_LOGI("OTA_MESH_CMD", "Pass url value to OTA");
>>>>>>> b672db7 (first commit)
}

char *build_ota_res(int error_code)
{
    cJSON *root = cJSON_CreateObject();
    if (!root) return NULL;
    cJSON_AddStringToObject(root, "name", "CmdStartOta");
    cJSON_AddNumberToObject(root, "timestamp", 123456789);
    cJSON_AddNumberToObject(root, "errorCode", error_code);
<<<<<<< HEAD
=======

>>>>>>> b672db7 (first commit)
    char *json_str = cJSON_PrintUnformatted(root);
    if (!json_str) {
        cJSON_Delete(root);
        return NULL;
    }

    cJSON_Delete(root); 
    return json_str;    
}