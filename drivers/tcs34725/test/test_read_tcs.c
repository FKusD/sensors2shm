#include "driver_tcs34725_basic.h"
#include <stdio.h>
#include <stdint.h>

int main(void)
{
    uint8_t res;
    uint16_t red, green, blue, clear;

    // Инициализация датчика
    res = tcs34725_basic_init();
    if (res != 0)
    {
        printf("Ошибка инициализации TCS34725\n");
        return 1;
    }

    // Три раза читаем значения
    for (int i = 0; i < 3; i++)
    {
        tcs34725_interface_delay_ms(1000); // Задержка 1 секунда
        res = tcs34725_basic_read(&red, &green, &blue, &clear);
        if (res != 0)
        {
            printf("Ошибка чтения данных\n");
            tcs34725_basic_deinit();
            return 1;
        }
        printf("red: %u, green: %u, blue: %u, clear: %u\n", red, green, blue, clear);
    }

    // Завершение работы
    tcs34725_basic_deinit();
    return 0;
}
