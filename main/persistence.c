#include "persistence.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"

static const char *TAG = "PERSISTENCE";

#define NVS_NAMESPACE "storage"

// Claves
#define KEY_MODE   "p_mode"
#define KEY_BRIGHT "p_bright"
#define KEY_START  "p_start"
#define KEY_END    "p_end"

esp_err_t persistence_save_mode(uint8_t mode) {
    nvs_handle_t my_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &my_handle);
    if (err == ESP_OK) {
        err = nvs_set_u8(my_handle, KEY_MODE, mode);
        if (err == ESP_OK) err = nvs_commit(my_handle);
        nvs_close(my_handle);
        ESP_LOGI(TAG, "Modo guardado: %d", mode);
    }
    return err;
}

uint8_t persistence_load_mode(void) {
    nvs_handle_t my_handle;
    uint8_t mode = 1; // Valor por defecto
    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &my_handle) == ESP_OK) {
        nvs_get_u8(my_handle, KEY_MODE, &mode);
        nvs_close(my_handle);
    }
    return mode;
}

esp_err_t persistence_save_brightness(uint8_t brightness) {
    nvs_handle_t my_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &my_handle);
    if (err == ESP_OK) {
        err = nvs_set_u8(my_handle, KEY_BRIGHT, brightness);
        if (err == ESP_OK) err = nvs_commit(my_handle);
        nvs_close(my_handle);
        ESP_LOGI(TAG, "Brillo guardado: %d", brightness);
    }
    return err;
}

uint8_t persistence_load_brightness(void) {
    nvs_handle_t my_handle;
    uint8_t brightness = 50; // Valor por defecto
    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &my_handle) == ESP_OK) {
        nvs_get_u8(my_handle, KEY_BRIGHT, &brightness);
        nvs_close(my_handle);
    }
    return brightness;
}

esp_err_t persistence_save_start_time(int start_time) {
    nvs_handle_t my_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &my_handle);
    if (err == ESP_OK) {
        err = nvs_set_i32(my_handle, KEY_START, start_time); // Solo KEY_START
        if (err == ESP_OK) {
            err = nvs_commit(my_handle);
        }
        nvs_close(my_handle);
        
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "Hora de inicio guardada: %d", start_time);
        } else {
            ESP_LOGE(TAG, "Error guardando hora de inicio (%s)", esp_err_to_name(err));
        }
    }
    return err;
}

esp_err_t persistence_save_end_time(int end_time) {
    nvs_handle_t my_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &my_handle);
    if (err == ESP_OK) {
        err = nvs_set_i32(my_handle, KEY_END, end_time); // Solo KEY_END
        if (err == ESP_OK) {
            err = nvs_commit(my_handle);
        }
        nvs_close(my_handle);
        
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "Hora de fin guardada: %d", end_time);
        } else {
            ESP_LOGE(TAG, "Error guardando hora de fin (%s)", esp_err_to_name(err));
        }
    }
    return err;
}

int persistence_load_start_time(void) {
    nvs_handle_t my_handle;
    int32_t val = 21; // Default
    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &my_handle) == ESP_OK) {
        nvs_get_i32(my_handle, KEY_START, &val);
        nvs_close(my_handle);
    }
    return (int)val;
}

int persistence_load_end_time(void) {
    nvs_handle_t my_handle;
    int32_t val = 22; // Default
    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &my_handle) == ESP_OK) {
        nvs_get_i32(my_handle, KEY_END, &val);
        nvs_close(my_handle);
    }
    return (int)val;
}

