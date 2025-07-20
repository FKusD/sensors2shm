#include "driver_tcs34725_interface.h"
#include <stdio.h>
#include <stdint.h>
#include <stdarg.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>
#include <time.h>

#define I2C_DEV_PATH "/dev/i2c-1"

static int i2c_fd = -1;

uint8_t tcs34725_interface_iic_init(void)
{
    i2c_fd = open(I2C_DEV_PATH, O_RDWR);
    if (i2c_fd < 0)
    {
        perror("I2C open");
        return 1;
    }
    return 0;
}

uint8_t tcs34725_interface_iic_deinit(void)
{
    if (i2c_fd >= 0)
    {
        close(i2c_fd);
        i2c_fd = -1;
    }
    return 0;
}

uint8_t tcs34725_interface_iic_read(uint8_t addr, uint8_t reg, uint8_t *buf, uint16_t len)
{
    if (i2c_fd < 0)
        return 1;
    if (ioctl(i2c_fd, I2C_SLAVE, addr) < 0)
        return 1;
    // TCS34725 требует, чтобы был выставлен бит 7 (COMMAND) в адресе регистра
    uint8_t reg_addr = reg | 0x80;
    if (write(i2c_fd, &reg_addr, 1) != 1)
        return 1;
    if (read(i2c_fd, buf, len) != len)
        return 1;
    return 0;
}

uint8_t tcs34725_interface_iic_write(uint8_t addr, uint8_t reg, uint8_t *buf, uint16_t len)
{
    if (i2c_fd < 0)
        return 1;
    if (ioctl(i2c_fd, I2C_SLAVE, addr) < 0)
        return 1;
    uint8_t data[len + 1];
    data[0] = reg | 0x80;
    for (uint16_t i = 0; i < len; i++)
        data[i + 1] = buf[i];
    if (write(i2c_fd, data, len + 1) != (len + 1))
        return 1;
    return 0;
}

void tcs34725_interface_delay_ms(uint32_t ms)
{
    usleep(ms * 1000);
}

void tcs34725_interface_debug_print(const char *const fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
}
