#include "power.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

static const char *TAG = "POWER";

#define INA226_ADDR             0x40 
#define INA226_SHUNT_OHMS       0.1f // R100

#define INA226_REG_CONFIG       0x00
#define INA226_REG_SHUNT_VOLT   0x01
#define INA226_REG_BUS_VOLT     0x02
#define INA226_VAL_LSB_BUS_V    0.00125f
#define INA226_VAL_LSB_SHUNT_V  0.0000025f

// valores y estado
typedef struct {
    float voltage_v;
    float current_a;
    float power_w;
    bool  connected;
} power_measure_internal_t;

static i2c_master_dev_handle_t dev_handle = NULL;
static power_measure_internal_t latest_data = {0};
static SemaphoreHandle_t data_mutex = NULL;

static int16_t read_register_16(uint8_t reg) {
    if (dev_handle == NULL) return 0;
    uint8_t tx_buf[1] = { reg };
    uint8_t rx_buf[2];
    // Usamos _WITHOUT_ABORT para que no reinicie si falla una lectura puntual
    esp_err_t err = i2c_master_transmit_receive(dev_handle, tx_buf, 1, rx_buf, 2, -1);
    if (err != ESP_OK) return 0;
    return (int16_t)((rx_buf[0] << 8) | rx_buf[1]);
}

// tarea periodica
static void power_task(void *arg) {
    while(1) {
        power_measure_internal_t temp = {0};
        
        if (dev_handle != NULL) {
            int16_t raw_bus = read_register_16(INA226_REG_BUS_VOLT);
            int16_t raw_shunt = read_register_16(INA226_REG_SHUNT_VOLT);

            temp.voltage_v = raw_bus * INA226_VAL_LSB_BUS_V;
            float shunt_v = raw_shunt * INA226_VAL_LSB_SHUNT_V;
            temp.current_a = shunt_v / INA226_SHUNT_OHMS;
            temp.power_w = temp.voltage_v * temp.current_a;
            
            if (temp.power_w < 0) temp.power_w = 0;
            
            temp.connected = true;
        }

        // para condiciones de carrera
        if (xSemaphoreTake(data_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            latest_data = temp;
            xSemaphoreGive(data_mutex);
        }

        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

esp_err_t power_init(i2c_master_bus_handle_t bus_handle) {
    data_mutex = xSemaphoreCreateMutex();
    
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = INA226_ADDR,
        .scl_speed_hz = 100000,
    };
    if (i2c_master_bus_add_device(bus_handle, &dev_cfg, &dev_handle) != ESP_OK) {
        return ESP_FAIL;
    }

    // Config PWM media 128 muestras
    uint8_t config_data[3] = { INA226_REG_CONFIG, 0x49, 0x27 }; 
    i2c_master_transmit(dev_handle, config_data, 3, -1);

    xTaskCreate(power_task, "power_task", 2048, NULL, 5, NULL);
    return ESP_OK;
}

float power_get_watts(void) {
    float watts = 0.0f;
    // con el mutex otra vez
    if (data_mutex != NULL) {
        if (xSemaphoreTake(data_mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
            watts = latest_data.power_w;
            xSemaphoreGive(data_mutex);
        }
    }
    return watts;
}