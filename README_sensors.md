# Программы чтения данных датчиков

Этот каталог содержит программы для чтения данных с датчиков расстояния VL53L1X и VL53L5CX из shared memory.

## Файлы

- `back_ranging.c` - Основная программа для работы с датчиками (записывает данные в shared memory)
- `read_sensors.py` - Python программа для чтения данных из shared memory
- `read_sensors.c` - C программа для чтения данных из shared memory
- `Makefile` - Makefile для компиляции C программы
- `sensors_config.txt` - Конфигурационный файл датчиков

## Структура данных в shared memory

Данные датчиков хранятся в структуре `SensorData`, которая поддерживает как одиночные измерения (VL53L1X), так и матричные (VL53L5CX):

```c
typedef struct {
    uint32_t timestamp;      // Временная метка (Unix timestamp)
    uint8_t sensor_type;     // Тип датчика (0=VL53L1X, 1=VL53L5CX, 2=TCS34725)
    uint8_t resolution;      // Разрешение (1 для одиночного, 16 для 4x4, 64 для 8x8)
    uint8_t data_format;     // Формат данных (0=одиночное, 1=матрица)
    uint8_t reserved;        // Зарезервировано
    
    union {
        // Одиночное измерение (VL53L1X, TCS34725)
        struct {
            uint16_t distance_mm;    // Расстояние в мм
            uint8_t status;          // Статус измерения
            uint8_t reserved[5];     // Зарезервировано
        } single;
        
        // Матричное измерение (VL53L5CX)
        struct {
            uint16_t distances[64];  // Массив расстояний (максимум 8x8)
            uint8_t statuses[64];    // Массив статусов
        } matrix;
    } data;
} SensorData;
```

### Размеры данных:
- **Одиночное измерение**: 16 байт (8 байт заголовка + 8 байт данных)
- **Матрица 4x4**: 56 байт (8 байт заголовка + 16*2 + 16 = 56 байт)
- **Матрица 8x8**: 200 байт (8 байт заголовка + 64*2 + 64 = 200 байт)

## Использование

### 1. Запуск основной программы

```bash
# Компиляция
make

# Запуск основной программы (записывает данные в shared memory)
./back_ranging
```

### 2. Чтение данных (Python)

```bash
# Установка прав на выполнение
chmod +x read_sensors.py

# Запуск
python3 read_sensors.py

# Или через make
make run_python
```

### 3. Чтение данных (C)

```bash
# Компиляция
make read_sensors

# Запуск
./read_sensors

# Или через make
make run_c
```

## Настройка

### Изменение списка датчиков

В файлах `read_sensors.py` и `read_sensors.c` измените массив `sensor_names`:

```python
# Python
sensor_names = [
    "vl53l1x_left",
    "vl53l5cx_left",
    "vl53l1x_right"
]
```

```c
// C
const char* sensor_names[] = {
    "vl53l1x_left",
    "vl53l5cx_left",
    "vl53l1x_right"
};
```

### Изменение интервала обновления

- Python: измените `update_interval` в функции `main()`
- C: измените `sleep(1)` в основном цикле

## Пример вывода

```
Программа чтения данных датчиков запущена
Нажмите Ctrl+C для остановки
------------------------------------------------------------
vl53l1x_left: [14:30:25] VL53L1X: Distance=150mm, Status=0
vl53l5cx_left: [14:30:25] VL53L5CX: Matrix 16 zones: Zone0=200mm(0) Zone1=195mm(0) Zone2=210mm(0) Zone3=205mm(0) ...
------------------------------------------------------------
vl53l1x_left: [14:30:26] VL53L1X: Distance=148mm, Status=0
vl53l5cx_left: [14:30:26] VL53L5CX: Matrix 16 zones: Zone0=202mm(0) Zone1=198mm(0) Zone2=208mm(0) Zone3=203mm(0) ...
------------------------------------------------------------
```

## Статусы датчиков

- `0` - Успешное измерение
- `1` - Сигнал слишком слабый
- `2` - Сигнал слишком сильный
- `3` - Множественные цели
- `4` - Нет цели
- `5` - Ошибка измерения

## Устранение неполадок

### Shared memory не найден

Убедитесь, что основная программа `back_ranging` запущена и создала shared memory сегменты.

### Ошибки компиляции

Для C программы может потребоваться установка библиотеки `librt`:

```bash
sudo apt-get install libc6-dev
```

### Права доступа

Убедитесь, что у пользователя есть права на чтение `/dev/shm/`.

## Очистка

```bash
# Удаление скомпилированных файлов
make clean

# Удаление shared memory сегментов (если основная программа не запущена)
sudo rm -f /dev/shm/vl53l1x_*
sudo rm -f /dev/shm/vl53l5cx_*
``` 