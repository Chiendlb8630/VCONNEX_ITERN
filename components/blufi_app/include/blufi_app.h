
#pragma once
#include "esp_blufi_api.h"
#include "mqtt_client.h"
#include "esp_tls.h"
#include "led_strip.h"
#include "driver/ledc.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"  // cần thiết để nhận EventGroupHandle_t

#define BLUFI_EXAMPLE_TAG "BLUFI_EXAMPLE"
#define BLUFI_INFO(fmt, ...)   ESP_LOGI(BLUFI_EXAMPLE_TAG, fmt, ##__VA_ARGS__)
#define BLUFI_ERROR(fmt, ...)  ESP_LOGE(BLUFI_EXAMPLE_TAG, fmt, ##__VA_ARGS__)
#define CONNECTED_BIT BIT0
#define BLUFI_DONE_BIT BIT1

extern EventGroupHandle_t wifi_event;
extern bool config_on;

void blufi_dh_negotiate_data_handler(uint8_t *data, int len, uint8_t **output_data, int *output_len, bool *need_free);
int blufi_aes_encrypt(uint8_t iv8, uint8_t *crypt_data, int crypt_len);
int blufi_aes_decrypt(uint8_t iv8, uint8_t *crypt_data, int crypt_len);
uint16_t blufi_crc_checksum(uint8_t iv8, uint8_t *data, int len);

int blufi_security_init(void);
void blufi_security_deinit(void);
int esp_blufi_gap_register_callback(void);
esp_err_t esp_blufi_host_init(void);
esp_err_t esp_blufi_host_and_cb_init(esp_blufi_callbacks_t *callbacks);
esp_err_t esp_blufi_host_deinit(void);
esp_err_t esp_blufi_controller_init(void);
esp_err_t esp_blufi_controller_deinit(void);

void ble_mesh_wifi_init(const char *ssid, const char* pass);

void blufi_app_init(void);
// void mess_send_to_app(void);
// void send_device_config_to_app(void);
void wifi_init(void);
void wifi_init_se(void);