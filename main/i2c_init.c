#include "i2c_init.h"
#include "esp_log.h"

static const char *TAG = "I2C_INIT";

i2c_master_bus_handle_t i2c_init(void)
{
    ESP_LOGI(TAG, "Inicializando I2C Master en SDA=%d, SCL=%d", I2C_MASTER_SDA_IO, I2C_MASTER_SCL_IO);

    i2c_master_bus_handle_t i2c_bus = NULL;
    i2c_master_bus_config_t bus_config = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = 0, // Puerto 0
        .sda_io_num = I2C_MASTER_SDA_IO,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .flags.enable_internal_pullup = true,
        .glitch_ignore_cnt = 7,
    };

    esp_err_t err = i2c_new_master_bus(&bus_config, &i2c_bus);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error al crear bus I2C: %s", esp_err_to_name(err));
        return NULL;
    }

    return i2c_bus;
}