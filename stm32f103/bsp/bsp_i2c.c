#include "bsp_i2c.h"

bool bsp_i2c_read(I2C_HandleTypeDef *bus, uint16_t address, uint16_t offset, uint16_t address_bits,
                  uint8_t *data, uint16_t size, uint32_t timeout)
{
    return HAL_I2C_Mem_Read(bus, address, offset,
                            (address_bits == 16u ? I2C_MEMADD_SIZE_16BIT : I2C_MEMADD_SIZE_8BIT),
                            data, size, timeout) == HAL_OK;
}

bool bsp_i2c_write(I2C_HandleTypeDef *bus, uint16_t address, uint16_t offset, uint16_t address_bits,
                   const uint8_t *data, uint16_t size, uint32_t timeout)
{
    return HAL_I2C_Mem_Write(bus, address, offset,
                             (address_bits == 16u ? I2C_MEMADD_SIZE_16BIT : I2C_MEMADD_SIZE_8BIT),
                             (uint8_t *)data, size, timeout) == HAL_OK;
}

bool bsp_i2c_ready(I2C_HandleTypeDef *bus, uint16_t address, uint32_t timeout)
{
    /* timeout 是 HAL 每次探测的时限，共探测 10 次。它和 read/write 的传输时限
     * 是两回事——那个量的是「一次传输最多等多久」，这个量的是「一次探测最多等
     * 多久」——所以由调用方分别给定，不再由这里写死。 */
    return HAL_I2C_IsDeviceReady(bus, address, 10, timeout) == HAL_OK;
}
