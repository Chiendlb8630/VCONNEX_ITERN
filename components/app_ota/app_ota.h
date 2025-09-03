#pragma once
#include "cJSON.h"

extern bool ota_IsRunning;
void handle_ota_data(cJSON *value);
void ota_task(void *pvParameter);
void handle_ota_data_mesh(const char *mesh_url);
