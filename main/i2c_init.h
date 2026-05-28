#ifndef I2C_INIT_H
#define I2C_INIT_H
#include "driver/i2c_master.h"
#include "esp_err.h"

#define I2C_MASTER_SDA_IO 21
#define I2C_MASTER_SCL_IO 22

i2c_master_bus_handle_t i2c_init(void);
#endif // I2C_MANAGER_H