#include "mqtt_manager.h"
#include "esp_log.h"
#include "cJSON.h"  
#include "lamp.h"
#include <string.h> 
#include <stdio.h>
#include "schedule.h"
#include "ota.h"

static const char *TAG = "MQTT_MANAGER";

#define TB_BROKER_URI "mqtt://129.151.232.248:25565"
#define TB_ACCESS_TOKEN "klh54um68i6q1u9wsmh1" 
#define TB_TELEMETRY_TOPIC "v1/devices/me/telemetry"
#define TB_RPC_TOPIC "v1/devices/me/rpc/request/+"

void process_rpc_request(const char *payload) {
    cJSON *root = cJSON_Parse(payload);
    if (root == NULL) {
        ESP_LOGE(TAG, "JSON RPC inválido");
        return;
    }

    cJSON *method = cJSON_GetObjectItem(root, "method");
    cJSON *params = cJSON_GetObjectItem(root, "params");

    if (cJSON_IsString(method)) {
        if (strcmp(method->valuestring, "setBrightness") == 0 || strcmp(method->valuestring, "setBrightness()") == 0) {
            if (cJSON_IsNumber(params)) {
                int val = params->valueint;
                ESP_LOGI(TAG, "RPC: Cambiar Brillo a %d", val);
                lamp_set_brightness((uint8_t)val);
            }else{
                uint8_t current_bri = lamp_get_brightness();
                int next_bri = current_bri + 10;
                if (next_bri > 100) next_bri = 0;
                lamp_set_brightness((uint8_t)next_bri);
            }
        } 
        else if (strcmp(method->valuestring, "set_start_time") == 0 || strcmp(method->valuestring, "set_start_time()") == 0) {
            if (cJSON_IsNumber(params)) {
                int val = params->valueint;
                if (val >= 0 && val <= 23) {
                    schedule_set_start_time(val);
                    ESP_LOGI(TAG, "RPC: Cambiar Hora de inicio a %d", val);    
                }
            }    
        } 
        else if (strcmp(method->valuestring, "set_end_time") == 0 || strcmp(method->valuestring, "set_end_time()") == 0) {
            if (cJSON_IsNumber(params)) {
                int val = params->valueint;
                if (val >= 0 && val <= 23) {
                    schedule_set_end_time(val);
                    ESP_LOGI(TAG, "RPC: Cambiar Hora de fin a %d", val);    
                }
            }    
        } 
        else if (strcmp(method->valuestring, "setMode") == 0 || strcmp(method->valuestring, "setMode()") == 0) {
            if (cJSON_IsNumber(params)) {
                int val = params->valueint;
                ESP_LOGI(TAG, "RPC Telegram: Modo directo a %d", val);
                lamp_set_mode((uint8_t)val);
            } 
            else {
                uint8_t current_mode = lamp_get_mode();
                uint8_t next_mode = current_mode + 1;
                if (next_mode > 3) {
                    next_mode = 1;
                }
                
                ESP_LOGI(TAG, "RPC Botón: Ciclando modo a %d", next_mode);
                lamp_set_mode(next_mode);
            }
        }
        else if (strcmp(method->valuestring, "setLedState") == 0) {
             bool state = cJSON_IsTrue(params);
             ESP_LOGI(TAG, "RPC: LED State -> %s", state ? "ON" : "OFF");
             lamp_set_brightness(state ? 100 : 0); 
        }
        else if (strcmp(method->valuestring, "set_start_time_right") == 0) {
            int time = schedule_get_start_time();
            time = (time + 1) % 24;
            schedule_set_start_time(time);
            ESP_LOGI(TAG, "RPC: Estableciendo tiempo de inicio a %d", time);
        }
        else if (strcmp(method->valuestring, "set_start_time_left") == 0) {
            int time = schedule_get_start_time();
            time = (time - 1 + 24) % 24;
            schedule_set_start_time(time);
            ESP_LOGI(TAG, "RPC: Estableciendo tiempo de inicio a %d", time);
        }
        else if (strcmp(method->valuestring, "set_end_time_right") == 0) {
            int time = schedule_get_end_time();
            time = (time + 1) % 24;
            schedule_set_end_time(time);
            ESP_LOGI(TAG, "RPC: Estableciendo tiempo de fin a %d", time);
        }
        else if (strcmp(method->valuestring, "set_end_time_left") == 0) {
            int time = schedule_get_end_time();
            time = (time - 1 + 24) % 24;
            schedule_set_end_time(time);
            ESP_LOGI(TAG, "RPC: Estableciendo tiempo de fin a %d", time);
        }
    }
    cJSON_Delete(root);
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
    esp_mqtt_event_handle_t event = event_data;
    
    switch ((esp_mqtt_event_id_t)event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT Conectado. Suscribiendo a RPC y Atributos...");
            esp_mqtt_client_subscribe(event->client, TB_RPC_TOPIC, 0);
            esp_mqtt_client_subscribe(event->client, "v1/devices/me/attributes", 0);
            break;
            
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGW(TAG, "MQTT Desconectado");
            break;

        case MQTT_EVENT_DATA:
            if (strncmp(event->topic, "v1/devices/me/rpc/request/", 26) == 0) {
                ESP_LOGI(TAG, "Mensaje RPC recibido");
                char *payload = strndup(event->data, event->data_len);
                if (payload) {
                    process_rpc_request(payload);
                    free(payload);
                }
            }
            else if (strncmp(event->topic, "v1/devices/me/attributes", 24) == 0) {
                
                char *payload = strndup(event->data, event->data_len);
                ESP_LOGW(TAG, "JSON: %s", payload); // Para ver qué llega

                cJSON *root = cJSON_Parse(payload);
                if (root) {
                    cJSON *fw_ver = cJSON_GetObjectItem(root, "fw_version");
                    cJSON *fw_title = cJSON_GetObjectItem(root, "fw_title");
                    
                    if (!fw_ver && cJSON_HasObjectItem(root, "shared")) {
                        cJSON *shared = cJSON_GetObjectItem(root, "shared");
                        fw_ver = cJSON_GetObjectItem(shared, "fw_version");
                        fw_title = cJSON_GetObjectItem(shared, "fw_title");
                    }

                    if (fw_ver && fw_title) {
                        char *nube_version = fw_ver->valuestring;
                        char *nube_title = fw_title->valuestring;

                        ESP_LOGI(TAG, "Verificando: Actual [%s] vs Nube [%s]", FIRMWARE_VERSION, nube_version);

                        if (strcmp(nube_version, FIRMWARE_VERSION) != 0) {
                            ESP_LOGW(TAG, "¡Versión nueva! Construyendo URL y actualizando...");
                            
                            char full_url[512];
                            snprintf(full_url, sizeof(full_url), 
                                     "http://129.151.232.248:8080/api/v1/%s/firmware?title=%s&version=%s", 
                                     TB_ACCESS_TOKEN, nube_title, nube_version);

                            // Lanza la OTA con la URL fabricada
                            ota_start(full_url);
                        } else {
                            ESP_LOGI(TAG, "Firmware ya actualizado.");
                        }
                    } 
                    cJSON_Delete(root);
                }
                free(payload);       
            }
            break;
            
        case MQTT_EVENT_ERROR:
            ESP_LOGE(TAG, "MQTT Error");
            break;
        default:
            break;
    }
}

esp_mqtt_client_handle_t mqtt_manager_init(void) {
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = TB_BROKER_URI,
        .credentials.username = TB_ACCESS_TOKEN,
    };

    ESP_LOGI(TAG, "Inicializando cliente MQTT...");
    esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);
    
    if (client == NULL) {
        ESP_LOGE(TAG, "Fallo al crear cliente MQTT");
        return NULL;
    }

    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(client);
    
    return client;
}

void mqtt_manager_publish_telemetry(esp_mqtt_client_handle_t client, float power, int mode, int brightness, int start_time, int end_time) {
    if (client == NULL) return;

    char payload[256];
    snprintf(payload, sizeof(payload), "{\"power\": %.2f, \"mode\": %d, \"brightness\": %d, \"start_time\": %d, \"end_time\": %d}", 
             power, mode, brightness, start_time, end_time);
    
    esp_mqtt_client_publish(client, TB_TELEMETRY_TOPIC, payload, 0, 1, 0);
}