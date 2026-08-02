# sensors2shm

Демон Raspberry Pi для чтения ToF-датчиков и передачи результатов в POSIX
shared memory. Текущая конфигурация рассчитана на два VL53L8CX по SPI: это
быстрый вспомогательный слой управления, независимый от ROS 2/SLAM.

## Поддерживаемые датчики

- `l1x` — VL53L1X по I2C, одно расстояние;
- `l5cx` — VL53L5CX по I2C, матрица;
- `l8cx` — VL53L8CX по I2C, матрица;
- `l8cx_spi` — VL53L8CX по SPI, матрица;
- `tcs` — зарезервировано для TCS34725, чтение пока не реализовано.

Для VL53L8CX в этой ветке используется официальный Linux ULD STSW-IMG042
v2.1.0 из `drivers/l8cx_uld`. Он собирается вместе с программой; отдельная
установка драйвера ST не нужна.

## Активная схема двух VL53L8CX

`sensors_config.txt` содержит:

```text
l8cx_spi 22 0 0 vl53l8cx_left
l8cx_spi 23 0 1 vl53l8cx_right
```

Формат строки SPI: `l8cx_spi XSHUT_GPIO SPI_BUS SPI_CS SHM_NAME`.
Номера GPIO — BCM, так как используется `wiringPiSetupGpio()`.

| Датчик | XSHUT | SPI-устройство | shared memory |
| --- | ---: | --- | --- |
| левый | GPIO 22 | `/dev/spidev0.0` | `vl53l8cx_left` |
| правый | GPIO 23 | `/dev/spidev0.1` | `vl53l8cx_right` |

Оба VL53L8CX инициализируются последовательно через XSHUT, работают в
режиме 4×4 с частотой 60 Гц. SPI использует MODE3 (CPOL=1, CPHA=1) и по
умолчанию 1 МГц. Меньшая матрица выбрана намеренно: для аварийной
подстраховки важнее минимальная задержка, чем 8×8 зон.

## Сборка и запуск

Требуются Raspberry Pi с Linux, `gcc`, `make`, `wiringPi`, включённый SPI и
доступ к `/dev/spidev0.0` и `/dev/spidev0.1`. В Raspberry Pi OS SPI можно
включить через `sudo raspi-config` (Interface Options → SPI).

```bash
make
sudo ./background_ranging
```

Для фонового запуска:

```bash
sudo ./background_ranging --daemon
```

Быстрая проверка двух опубликованных кадров:

```bash
python3 read_sensors.py --once
```

В норме скрипт выводит два кадра `VL53L8CX (SPI), 4x4`; в каждой ячейке
показаны `distance_mm/status`. Для непрерывного наблюдения запускайте без
`--once` и закройте ладонью один из датчиков — расстояния соответствующих зон
должны заметно измениться.

Подробности о демоне — в [README_daemon.md](README_daemon.md), о формате
данных — в [README_sensors.md](README_sensors.md).

## Shared memory ABI

Для каждого датчика создаются POSIX объекты `/dev/shm/<SHM_NAME>` и
`/dev/shm/sem.sem_<SHM_NAME>` (POSIX-имя семафора — `/sem_<SHM_NAME>`).
Заголовок содержит Unix-время, тип датчика,
разрешение и формат. Для матриц область всегда имеет размер 200 байт:
8-байтный заголовок, 64 `uint16_t` расстояния и 64 статуса. При 4×4 первые
16 значений содержат актуальные зоны; `resolution == 16`.

Потребитель обязан брать семафор перед чтением. Сервис APC читает
`vl53l8cx_left` и `vl53l8cx_right` напрямую, без ROS 2.
