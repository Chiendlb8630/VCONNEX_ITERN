#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_mac.h"
#include "led_strip.h"
#include "driver/ledc.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_bt.h"
#include "esp_blufi_api.h"
#include "esp_blufi.h"
#include "blufi_app.h"
#include "cJSON.h"
#include "esp_log.h"
#include "esp_tls.h"
#include "esp_netif.h"
#include "mqtt_app.h"
#include "esp_bt_main.h"
#include "esp_ble_mesh_common_api.h"

static const char *TAG = "BLUFI_APP";

void example_event_callback(esp_blufi_cb_event_t event, esp_blufi_cb_param_t *param);
wifi_config_t sta_config;
EventGroupHandle_t wifi_event;
int wifi_retry = 0;

bool config_on = false;
bool gl_sta_connected = false;
bool gl_sta_got_ip = false;
bool ble_is_connected = false;
uint8_t gl_sta_bssid[6];
uint8_t gl_sta_ssid[32];
int gl_sta_ssid_len;
wifi_sta_list_t gl_sta_list;
bool gl_sta_is_connecting = false;
esp_blufi_extra_info_t gl_sta_conn_info;
void switch_blufi_to_mesh(void);
void wifi_init_se();
void ble_mesh_wifi_init(const char *ssid, const char* pass);

static void mess_send_to_app(void){
    const char* mess_send =  "{\"name\":\"CmdGetDeviceID\","
    "\"devT\":3018,"
    "\"devExtAddr\":\"F4CFA24966D0\","
    "\"timeStamp\":1576030822}";

    esp_err_t ret =  esp_blufi_send_custom_data((uint8_t *)mess_send, strlen(mess_send));
    if( ret == ESP_OK){
        ESP_LOGE(TAG, "Send to BLE STACK");
    } else {
        ESP_LOGE(TAG, "Send error");
    }
}

void wifi_connect(void){
    wifi_retry = 0;
    gl_sta_is_connecting = (esp_wifi_connect() == ESP_OK);
}
static void send_device_config_to_app(void)
{
    const char *json_msg =
"{"
    "\"name\": \"CmdSetDeviceConfig\","
    "\"devT\": 3018,"
    "\"devExtAddr\": \"F4CFA24966D0\","
    "\"software_version\": \"3.1.1\","
    "\"errorCode\": 50000,"
    "\"timeStamp\": 1576030822"
"}";

    esp_err_t ret = esp_blufi_send_custom_data((uint8_t *)json_msg, strlen(json_msg));
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "�? g?i JSON cho App: %s", json_msg);
    } else {
        ESP_LOGE(TAG, "G?i JSON th?t b?i: %s", esp_err_to_name(ret));
    }
}

bool wifi_reconnect(void){
    bool ret;
    if(gl_sta_is_connecting && wifi_retry++ < 10){
        BLUFI_INFO("BLUFI WIFI START RECONNECT\n");
        gl_sta_is_connecting = (esp_wifi_connect() == ESP_OK);
        ret = true;
    } else {
        ret = false;
    }
    return ret;
}

static void ip_event_handler(void * arg, esp_event_base_t event_base, 
            int32_t event_id, void* event_data)
{
    wifi_mode_t mode; 
    switch(event_id){
       case IP_EVENT_STA_GOT_IP: {
        esp_blufi_extra_info_t info;
        xEventGroupSetBits(wifi_event, CONNECTED_BIT);
        esp_wifi_get_mode(&mode);
        ESP_LOGI("IP_EVENT", "Got IP event triggered");
        memset(&info, 0, sizeof(esp_blufi_extra_info_t));
        memcpy(info.sta_bssid, gl_sta_bssid, 6);
        info.sta_bssid_set = true;
        info.sta_ssid = gl_sta_ssid;
        info.sta_ssid_len = gl_sta_ssid_len;
        gl_sta_got_ip = true;
        if(config_on){
        mqtt_load_config();
        mqtt_app_start();
        }
        if (ble_is_connected == true) {
            ESP_LOGI("IP_EVENT", "BLE connected? %d", ble_is_connected);
            esp_blufi_send_wifi_conn_report(mode, ESP_BLUFI_STA_CONN_SUCCESS, 0 , NULL);
            mess_send_to_app();
            config_on = false;
        } else {
            BLUFI_INFO("BLUFI BLE is not connected yet\n");
        }
        break;
    } default: 
        break;
    }
    return;
}

static void wifi_event_handler(void * arg, esp_event_base_t event_base,
                        int32_t event_id, void* event_data)
{
    wifi_event_sta_connected_t * event;
    wifi_event_sta_disconnected_t * disconnected_event;
    wifi_mode_t mode;

    switch (event_id) {
    case WIFI_EVENT_STA_START:
        wifi_connect();
        break;
    case WIFI_EVENT_STA_CONNECTED:
        gl_sta_connected = true;
        gl_sta_is_connecting = false;
        event = (wifi_event_sta_connected_t*) event_data;
        memcpy(gl_sta_bssid, event->bssid, 6);
        memcpy(gl_sta_ssid, event->ssid, event->ssid_len);
        gl_sta_ssid_len = event->ssid_len;
        break;
    case WIFI_EVENT_STA_DISCONNECTED:
        
        if (gl_sta_connected == false && wifi_reconnect() == false) {
            gl_sta_is_connecting = false;
            disconnected_event = (wifi_event_sta_disconnected_t*) event_data;  
        }
        gl_sta_connected = false;
        gl_sta_got_ip = false;
        memset(gl_sta_ssid, 0, 32);
        memset(gl_sta_bssid, 0, 6);
        gl_sta_ssid_len = 0;
        xEventGroupClearBits(wifi_event, CONNECTED_BIT);
        break;
}             
}


esp_blufi_callbacks_t example_callbacks = {
    .event_cb = example_event_callback,
    .negotiate_data_handler = blufi_dh_negotiate_data_handler,
    .encrypt_func = blufi_aes_encrypt,
    .decrypt_func = blufi_aes_decrypt,
    .checksum_func = blufi_crc_checksum,
};

void example_event_callback(esp_blufi_cb_event_t event ,esp_blufi_cb_param_t *param)
{
    switch (event) {
    case ESP_BLUFI_EVENT_INIT_FINISH:
        BLUFI_INFO("BLUFI init finish\n");
        esp_blufi_adv_start();
        break;
    case ESP_BLUFI_EVENT_DEINIT_FINISH:
        BLUFI_INFO("BLUFI deinit finish\n");
        break;
    case ESP_BLUFI_EVENT_BLE_CONNECT:
        BLUFI_INFO("BLUFI ble connect\n");
        ble_is_connected = true;
        esp_blufi_adv_stop();
        blufi_security_init();
        break;
    case ESP_BLUFI_EVENT_BLE_DISCONNECT:
        BLUFI_INFO("BLUFI ble disconnect\n");
        ble_is_connected = false;
        blufi_security_deinit();
        esp_blufi_adv_start();
        break;
    case ESP_BLUFI_EVENT_SET_WIFI_OPMODE:
        BLUFI_INFO("BLUFI Set WIFI opmode %d\n", param->wifi_mode.op_mode);
        ESP_ERROR_CHECK( esp_wifi_set_mode(param->wifi_mode.op_mode) );
        break;
    case ESP_BLUFI_EVENT_REQ_CONNECT_TO_AP:
        BLUFI_INFO("BLUFI request wifi connect to AP\n");
        esp_wifi_disconnect();
        wifi_connect();
        break;
    case ESP_BLUFI_EVENT_REQ_DISCONNECT_FROM_AP:
        BLUFI_INFO("BLUFI request wifi disconnect from AP\n");
        esp_wifi_disconnect();
        break;
    case ESP_BLUFI_EVENT_REPORT_ERROR:
        BLUFI_ERROR("BLUFI report error, error code %d\n", param->report_error.state);
        esp_blufi_send_error_info(param->report_error.state);
        break;

    case ESP_BLUFI_EVENT_RECV_SLAVE_DISCONNECT_BLE:
        BLUFI_INFO("blufi close a gatt connection");
        esp_blufi_disconnect();
        break;

    case ESP_BLUFI_EVENT_DEAUTHENTICATE_STA:
        /* TODO */
        break;

	case ESP_BLUFI_EVENT_RECV_STA_BSSID:
        memcpy(sta_config.sta.bssid, param->sta_bssid.bssid, 6);
        sta_config.sta.bssid_set = 1;
        esp_wifi_set_config(WIFI_IF_STA, &sta_config);
        BLUFI_INFO("Recv STA BSSID %s\n", sta_config.sta.ssid);
        break;

	case ESP_BLUFI_EVENT_RECV_STA_SSID:
        if (param->sta_ssid.ssid_len >= sizeof(sta_config.sta.ssid)/sizeof(sta_config.sta.ssid[0])) {
            esp_blufi_send_error_info(ESP_BLUFI_DATA_FORMAT_ERROR);
            BLUFI_INFO("Invalid STA SSID\n");
            break;
        }
        strncpy((char *)sta_config.sta.ssid, (char *)param->sta_ssid.ssid, param->sta_ssid.ssid_len);
        sta_config.sta.ssid[param->sta_ssid.ssid_len] = '\0';
        esp_wifi_set_config(WIFI_IF_STA, &sta_config);
        BLUFI_INFO("Recv STA SSID %s\n", sta_config.sta.ssid);
        break;

	case ESP_BLUFI_EVENT_RECV_STA_PASSWD:
        if (param->sta_passwd.passwd_len >= sizeof(sta_config.sta.password)/sizeof(sta_config.sta.password[0])) {
            esp_blufi_send_error_info(ESP_BLUFI_DATA_FORMAT_ERROR);
            BLUFI_INFO("Invalid STA PASSWORD\n");
            break;
        }
        strncpy((char *)sta_config.sta.password, (char *)param->sta_passwd.passwd, param->sta_passwd.passwd_len);
        sta_config.sta.password[param->sta_passwd.passwd_len] = '\0';
        sta_config.sta.threshold.authmode = WIFI_AUTH_OPEN;
        esp_wifi_set_config(WIFI_IF_STA, &sta_config);
        BLUFI_INFO("Recv STA PASSWORD %s\n", sta_config.sta.password);
        break;

    case ESP_BLUFI_EVENT_GET_WIFI_LIST:{
        wifi_scan_config_t scanConf = {
            .ssid = NULL,
            .bssid = NULL,
            .channel = 0,
            .show_hidden = false
        };
        esp_err_t ret = esp_wifi_scan_start(&scanConf, true);
        if (ret != ESP_OK) {
            esp_blufi_send_error_info(ESP_BLUFI_WIFI_SCAN_FAIL);
        }
        break;
    }
   case ESP_BLUFI_EVENT_RECV_CUSTOM_DATA: {
    uint8_t *data = param->custom_data.data;
    int len = param->custom_data.data_len;
    parse_data((char*) data, len);
    printf("BLUFI Custom Data : %.*s\n", len, (char *)data);
    xEventGroupSetBits(wifi_event, BLUFI_DONE_BIT);
    send_device_config_to_app();

    break;
    }

    default: 
        break;
    } 
}

void wifi_init(void)
{
    
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_t *sta_netif = esp_netif_create_default_wifi_sta();
    assert(sta_netif);
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &ip_event_handler, NULL));

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK( esp_wifi_init(&cfg) );
    ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK( esp_wifi_start() );
}

void wifi_init_se(){
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK( esp_wifi_init(&cfg));
    ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK( esp_wifi_start());
}

void ble_mesh_wifi_init(const char *ssid, const char* pass){
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK( esp_wifi_init(&cfg) );

    wifi_config_t wifi_config = {0};
    strncpy((char *)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid) - 1);
    strncpy((char *)wifi_config.sta.password, pass, sizeof(wifi_config.sta.password) - 1);

    ESP_LOGI(TAG, "Connecting to SSID:%s PASS:%s", ssid, pass);

    ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK( esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );
    ESP_ERROR_CHECK( esp_wifi_start() );
    ESP_ERROR_CHECK( esp_wifi_connect());
}

void blufi_app_init(void)
{
    esp_err_t ret;
    ret = esp_blufi_controller_init();
    if (ret) {
        BLUFI_ERROR("%s BLUFI controller init failed: %s\n", __func__, esp_err_to_name(ret));
        return;
    }

    ret = esp_blufi_host_and_cb_init(&example_callbacks);
    if (ret) {
        BLUFI_ERROR("%s initialise failed: %s\n", __func__, esp_err_to_name(ret));
        return;
    }
    BLUFI_INFO("BLUFI VERSION %04x\n", esp_blufi_get_version());
}

void switch_blufi_to_mesh(){
    esp_err_t err = esp_bluedroid_deinit();
    if (err != ESP_OK) {
        ESP_LOGE("APP", "Failed to disable bluedroid: %s", esp_err_to_name(err));
    }
    err = esp_bluedroid_disable();
    if (err != ESP_OK) {
        ESP_LOGE("APP", "Failed to disable bluedroid: %s", esp_err_to_name(err));
    }
    err = esp_bluedroid_deinit();
    if (err != ESP_OK) {
        ESP_LOGE("APP", "Failed to de-initialize bluedroid: %s", esp_err_to_name(err));
    }
    err = esp_bt_controller_disable();
    if (err != ESP_OK) {
        ESP_LOGE("APP", "Failed to disable BT controller: %s", esp_err_to_name(err));
    }
    err = esp_bt_controller_deinit();
    if (err != ESP_OK) {
        ESP_LOGE("APP", "Failed to de-initialize BT controller: %s", esp_err_to_name(err));
    }
}




