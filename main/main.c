// app_main.c

#include <stdio.h>
#include "esp_log.h"
#include "nvs_flash.h"
#include "mqtt_client.h"

// Hardware
#include "i2c_init.h"
#include "lamp.h"
#include "oled_ui.h" 
#include "input.h"
#include "power.h"
#include "persistence.h"
#include "esp_pm.h"         // ahorro de energía

// Conectividad
#include "wifi_manager.h"
#include "mqtt_manager.h"
#include "schedule.h"

static const char *TAG = "MAIN";
#define INTERVAL_MS 5 * 1000    // Intervalo MQTT

// configuración ahorro energía
void power_save_init(void) {
#if CONFIG_PM_ENABLE
    esp_pm_config_t pm_config = {
        .max_freq_mhz = 240, // Frecuencia máxima
        .min_freq_mhz = 80,  // Bajar a 80MHz
        .light_sleep_enable = false // NO usar Light Sleep
    };
    ESP_ERROR_CHECK(esp_pm_configure(&pm_config));
    ESP_LOGI(TAG, "Gestión de energía (PM) configurada.");
#endif
}

void app_main(void)
{
    esp_err_t ret;
    esp_mqtt_client_handle_t mqtt_client = NULL;
    
    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    i2c_master_bus_handle_t bus_handle = i2c_init();
    if (!bus_handle) {
        ESP_LOGE(TAG, "Error crítico: No hay bus I2C");
        return;
    }

    lamp_init();
    oled_ui_init(bus_handle);
    
    input_init();

    power_init(bus_handle);
    
    // Wi-Fi y MQTT
    ESP_LOGI(TAG, "Iniciando gestor Wi-Fi y Portal de Configuración...");
    bool is_sta_connected = (wifi_manager_init() == ESP_OK); // Llama a la función que gestiona STA/AP
    
    if (is_sta_connected) {
        mqtt_client = mqtt_manager_init();
    } else {
        ESP_LOGW(TAG, "Sin Wi-Fi, modo AP/Configuración activo.");
    }
    

    lamp_set_mode(persistence_load_mode());

    lamp_set_brightness(persistence_load_brightness());

    schedule_set_start_time(persistence_load_start_time());

    schedule_set_end_time(persistence_load_end_time());

    oled_ui_set_mode_main();

    power_save_init();

    // valores anteriores
    // antes static?
    int last_modo = -1;
    int last_brillo = -1;
    int last_inicio = -1;
    int last_fin = -1;
    float last_consumo = -1.0; 

    ESP_LOGI(TAG, "VERSION OTA 1.0.3");

    while(1) {
        // valores actuales
        int modo_actual = lamp_get_mode();
        int brillo_actual = lamp_get_brightness();
        int hora_inicio = schedule_get_start_time();
        int hora_fin = schedule_get_end_time();
        float consumo = power_get_watts(); //

        // comprobar cambios
        bool hay_cambios = (modo_actual != last_modo)       ||
                           (brillo_actual != last_brillo)   ||
                           (hora_inicio != last_inicio)     ||
                           (hora_fin != last_fin)           ||
                           (consumo > last_consumo + 0.1)   ||
                           (consumo < last_consumo - 0.1);

        // Si hay cambios, enviamos y actualizamos
        if (hay_cambios && mqtt_client != NULL && is_sta_connected) {
            
            ESP_LOGI(TAG, "Cambio detectado. Enviando telemetría...");
            mqtt_manager_publish_telemetry(mqtt_client, consumo, modo_actual, brillo_actual, hora_inicio, hora_fin);

            last_modo = modo_actual;
            last_brillo = brillo_actual;
            last_inicio = hora_inicio;
            last_fin = hora_fin;
            last_consumo = consumo;
        }
        
        vTaskDelay(pdMS_TO_TICKS(INTERVAL_MS));
    }
}