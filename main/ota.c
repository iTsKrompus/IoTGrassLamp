#include "ota.h"
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_http_client.h"
#include "esp_https_ota.h"
#include "esp_crt_bundle.h"
#include "esp_wifi.h"

static const char *TAG = "OTA_UPDATE";


void ota_task(void *pvParameter) {
    char *url = (char *)pvParameter; // Recibimos la URL
    esp_wifi_set_ps(WIFI_PS_NONE);
    ESP_LOGW(TAG, "Iniciando actualización automática desde: %s", url);

    esp_http_client_config_t config = {
        .url = url,
        .timeout_ms = 10000,
        .keep_alive_enable = true,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .skip_cert_common_name_check = true, // Seguridad relajada para evitar líos
        .cert_pem = NULL, 
    };

    esp_https_ota_config_t ota_config = {
        .http_config = &config,
    };

    esp_err_t ret = esp_https_ota(&ota_config);

    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Actualización Éxitosa. Reiniciando...");
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        esp_restart();
    } else {
        ESP_LOGE(TAG, "Fallo OTA. Error: %d", ret);
    }
    
    free(url); // Liberamos la memoria del texto
    vTaskDelete(NULL);
}


void ota_start(const char *url)
{
    if (url == NULL) return;

    char *url_copy = strdup(url);
    xTaskCreate(ota_task, "ota_task", 8192, url_copy, 5, NULL);
}