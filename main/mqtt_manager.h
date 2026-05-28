#ifndef MQTT_MANAGER_H
#define MQTT_MANAGER_H

#include "mqtt_client.h"

esp_mqtt_client_handle_t mqtt_manager_init(void);

void mqtt_manager_publish_telemetry(esp_mqtt_client_handle_t client, float power, int mode, int brightness, int start_time, int end_time);

#endif // MQTT_MANAGER_H