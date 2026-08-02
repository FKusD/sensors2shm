# Чтение данных sensors2shm

`background_ranging` публикует один POSIX shared-memory сегмент и один
именованный семафор на каждый датчик. Имена задаются последним полем
`sensors_config.txt`.

Для текущей пары VL53L8CX используются:

```text
/dev/shm/vl53l8cx_left   /dev/shm/sem.sem_vl53l8cx_left
/dev/shm/vl53l8cx_right  /dev/shm/sem.sem_vl53l8cx_right
```

## Двоичный формат

Первые восемь байт — little-endian заголовок `<IBBBB`:

| Поле | Тип | Значение |
| --- | --- | --- |
| `timestamp_sec` | `uint32_t` | Unix-время записи |
| `sensor_type` | `uint8_t` | 0 L1X, 1 L5CX, 2 L8CX I2C, 3 L8CX SPI, 4 TCS |
| `resolution` | `uint8_t` | 1, 16 (4×4) или 64 (8×8) |
| `data_format` | `uint8_t` | 0 — одиночное, 1 — матрица |
| `reserved` | `uint8_t` | зарезервировано |

У матрицы после заголовка расположены 64 `uint16_t` расстояния в мм, затем
64 `uint8_t` статуса. Сегмент матрицы всегда 200 байт, в том числе при 4×4:
потребитель использует только первые `resolution` значений. Это важно для
совместимости с `fdrive_8_SMSK.py`.

## Безопасное чтение в Python

Перед чтением необходимо захватить одноимённый семафор. Упрощённый пример:

```python
import mmap, os, struct
import posix_ipc

name = "vl53l8cx_left"
fd = os.open(f"/dev/shm/{name}", os.O_RDONLY)
memory = mmap.mmap(fd, 200, mmap.MAP_SHARED, mmap.PROT_READ)
sem = posix_ipc.Semaphore(f"/sem_{name}")

sem.acquire(timeout=0.1)
try:
    raw = memory[:200]
finally:
    sem.release()

timestamp, sensor_type, resolution, data_format, _ = struct.unpack("<IBBBB", raw[:8])
assert sensor_type == 3 and data_format == 1 and resolution == 16
distances = struct.unpack("<16H", raw[8:40])
statuses = list(raw[136:152])
```

Статусы начинаются с фиксированного смещения 136, а не сразу после 16
расстояний: в ABI зарезервировано место под полную 8×8 матрицу. Значение
`255` обозначает зону без валидного измерения в обработчике APC.

## Актуальная геометрия для APC

`fdrive_8_SMSK.py` берёт четыре зоны с индексами `11, 10, 9, 8` из каждого
датчика и сопоставляет левую/правую стороны коридора. Он безопасно останавливает
машину, если хотя бы один кадр имеет разрешение, отличное от 16.
