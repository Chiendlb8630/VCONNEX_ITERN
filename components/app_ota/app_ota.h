#pragma once
#include "cJSON.h"

extern bool ota_IsRunning;
void handle_ota_data(cJSON *value);
void ota_task(void *pvParameter);
<<<<<<< HEAD
void handle_ota_data_mesh(const char *mesh_url);
=======
void handle_ota_data_mesh(char *mesh_url);
>>>>>>> b672db7 (first commit)
