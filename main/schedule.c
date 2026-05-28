#include "schedule.h"
#include "lamp.h"
#include <time.h>
#include <sys/time.h>
#include "esp_sntp.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "persistence.h"
#include "schedule.h"

static const char *TAG = "SCHEDULE";

// Variables globales para guardar las horas configuradas
// -1 indica que no está configurada
static int g_start_hour = -1;
static int g_end_hour = -1;

void schedule_set_start_time(int hour) {
    if (hour >= 0 && hour <= 23) {
        g_start_hour = hour;
        ESP_LOGI(TAG, "Hora de encendido programada: %02d:00", hour);
    } else {
        ESP_LOGE(TAG, "Hora de inicio inválida (0-23)");
    }
    persistence_save_start_time(hour);
}

void schedule_set_end_time(int hour) {
    if (hour >= 0 && hour <= 23) {
        g_end_hour = hour;
        ESP_LOGI(TAG, "Hora de apagado programada: %02d:00", hour);
    } else {
        ESP_LOGE(TAG, "Hora de fin inválida (0-23)");
    }
    persistence_save_end_time(hour);
}

int schedule_get_start_time(){
    return g_start_hour;
}

int schedule_get_end_time(){
    return g_end_hour;
}

static void schedule_task(void *arg) {
    time_t now;
    struct tm timeinfo;
    bool time_synced = false;

    // Esperar sincronización
    while (!time_synced) {
        time(&now);
        localtime_r(&now, &timeinfo);
        if (timeinfo.tm_year > (2020 - 1900)) {
            time_synced = true;
            ESP_LOGI(TAG, "Hora sincronizada. Verificando estado inicial...");
        } else {
            vTaskDelay(pdMS_TO_TICKS(2000));
        }
    }

    while (1) {
        time(&now);
        localtime_r(&now, &timeinfo);
        int current_h = timeinfo.tm_hour;

        // comprobar si está dentro del horario
        bool should_be_on = false;

        if (g_start_hour != -1 && g_end_hour != -1) {
            if (g_start_hour < g_end_hour) {
                if (current_h >= g_start_hour && current_h < g_end_hour) {
                    should_be_on = true;
                }
            } else {
                if (current_h >= g_start_hour || current_h < g_end_hour) {
                    should_be_on = true;
                }
            }
        }
        
        static int last_order = -1; // -1: nada, 0: apagado, 1: encendido

        if (should_be_on && last_order != 1) {
            ESP_LOGI(TAG, "Dentro del horario activo: Encendiendo.");
            lamp_turn_on();
            last_order = 1;
        } 
        else if (!should_be_on && last_order != 0) {
            ESP_LOGI(TAG, "Fuera del horario activo: Apagando.");
            lamp_turn_off();
            last_order = 0;
        }
        // actualizamos cada 30 sec
        vTaskDelay(pdMS_TO_TICKS(30000));
    }
}

void schedule_init(void) {
    ESP_LOGI(TAG, "Iniciando servicio de horario...");

    // nos conectamos al servidor de tiempo
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "pool.ntp.org");
    esp_sntp_init();

    // Configurar Zona Horaria
    setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1);
    tzset();

    xTaskCreate(schedule_task, "schedule_task", 4096, NULL, 5, NULL);
}