#!/usr/bin/env python3
"""
Опрос датчика TCS34725 через программную I2C (SDA=10, SCL=9) с помощью pigpio.
Выводит значения R, G, B, C, Lux, Temp в терминал.
"""
import time
import sys
import pigpio

# --- Настройки для программной I2C ---
TCS34725_I2C_ADDR = 0x29
SDA_PIN = 10
SCL_PIN = 9

# Инициализация pigpio и bit-bang I2C
pi = pigpio.pi()
if not pi.connected:
    print("Ошибка: не удалось подключиться к pigpio daemon. Запустите 'sudo pigpiod'.")
    sys.exit(1)

# Открываем программную I2C на нужных пинах
bb_i2c_handle = pi.bb_i2c_open(SDA_PIN, SCL_PIN, 100000)  # 100kHz
if bb_i2c_handle < 0:
    print(f"Ошибка открытия программной I2C на SDA={SDA_PIN}, SCL={SCL_PIN}")
    pi.stop()
    sys.exit(1)

def tcs34725_write(reg, value):
    pi.bb_i2c_zip(SDA_PIN, [
        4, TCS34725_I2C_ADDR,
        2, 1, (0x80 | reg),
        2, 1, value,
        3, 0
    ])

def tcs34725_read16(reg):
    res = pi.bb_i2c_zip(SDA_PIN, [
        4, TCS34725_I2C_ADDR,
        2, 1, (0x80 | reg),
        4, TCS34725_I2C_ADDR | 1,
        6, 2,
        3, 0
    ])
    if isinstance(res, list) and len(res) == 2:
        return res[0] | (res[1] << 8)
    return 0

def tcs34725_init():
    tcs34725_write(0x00, 0x01)  # ENABLE: Power ON
    time.sleep(0.01)
    tcs34725_write(0x00, 0x03)  # ENABLE: Power ON + ADC EN
    time.sleep(0.01)
    tcs34725_write(0x01, 0xD5)  # ATIME
    tcs34725_write(0x0F, 0x00)  # CONTROL: Gain 1x

def tcs34725_get_raw_data():
    c = tcs34725_read16(0x14)
    r = tcs34725_read16(0x16)
    g = tcs34725_read16(0x18)
    b = tcs34725_read16(0x1A)
    return r, g, b, c

def tcs34725_calculate_lux(r, g, b):
    return 0.136 * r + 1.0 * g - 0.444 * b

def tcs34725_calculate_color_temp(r, g, b):
    if r + g + b == 0:
        return 0
    return (r + g + b) // 3

def main():
    try:
        tcs34725_init()
        print("TCS34725 инициализирован через программную I2C (pigpio)")
        print("Нажмите Ctrl+C для выхода")
        print("-" * 60)
        while True:
            try:
                r, g, b, c = tcs34725_get_raw_data()
                color_temp = tcs34725_calculate_color_temp(r, g, b)
                lux = tcs34725_calculate_lux(r, g, b)
                print(f"TCS34725: R={r}, G={g}, B={b}, C={c}, Temp={color_temp}, Lux={lux:.1f}")
            except Exception as e:
                print(f"TCS34725: Ошибка чтения: {e}")
            time.sleep(0.2)
    except KeyboardInterrupt:
        print("\nЗавершение работы...")
    finally:
        pi.bb_i2c_close(SDA_PIN)
        pi.stop()
        print("Программная I2C закрыта, pigpio остановлен.")

if __name__ == "__main__":
    main() 