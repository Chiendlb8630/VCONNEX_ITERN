#pragma once 

#include "mqtt_client.h"
#include "esp_tls.h"

typedef struct{
    char broker[128];
    char username[64];
    char password[64];
    char mqttsub[128];
    char mqttpub[128];
}device_config_t;

extern device_config_t mqtt_conf;
extern bool mqtt_on;
extern esp_mqtt_client_config_t mqtt_cfg;
extern esp_mqtt_client_handle_t client;

void mqtt_app_start(void);
void parse_data(const char *data, size_t len);
void mqtt_publish_to_app(const char *msg);
char *build_status_message(void);
void mqtt_save_config(const char *broker, const char *user, const char *pass, const char *pub_topic, const char *sub_topic);
void mqtt_load_config();
void mqtt_stop(void);