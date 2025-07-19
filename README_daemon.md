# Sensors2SHM Daemon

Демон для работы с датчиками VL53L1X, VL53L5CX и TCS34725 с использованием shared memory.

## Описание

Программа `background_ranging` была модифицирована для работы в режиме демона. Демон автоматически отключается от терминала **ПОСЛЕ инициализации и запуска всех датчиков** и продолжает работу в фоновом режиме.

## Особенности демона

- **Демонизация после инициализации** - датчики инициализируются и запускаются до отключения от терминала
- **PID файл** - создается файл `/var/run/sensors2shm.pid` для отслеживания процесса
- **Корректное завершение** - обработка сигналов SIGINT и SIGTERM
- **Проверка дублирования** - предотвращает запуск нескольких экземпляров
- **Без логирования** - отсутствие записи в файлы для защиты SD карты Raspberry Pi

## Компиляция

```bash
# Компилируем программу
make

# Или вручную:
gcc -o background_ranging background_ranging.c -lwiringPi -lpthread -lrt
```

## Использование

### Запуск в режиме демона

```bash
# Запуск демона
./background_ranging --daemon

# Или через скрипт управления
./sensors2shm.sh start
```

### Запуск в обычном режиме

```bash
# Запуск с выводом в терминал
./background_ranging
```

### Управление демоном

Используйте скрипт `sensors2shm.sh` для управления демоном:

```bash
# Запуск демона
./sensors2shm.sh start

# Остановка демона
./sensors2shm.sh stop

# Перезапуск демона
./sensors2shm.sh restart

# Проверка статуса
./sensors2shm.sh status
```

## Файлы демона

- **PID файл**: `/var/run/sensors2shm.pid` - содержит PID процесса демона
- **Конфигурация**: `./sensors_config.txt` - конфигурация датчиков

## Права доступа

Для корректной работы демона могут потребоваться права root:

```bash
# Создание директории для PID файлов
sudo mkdir -p /var/run

# Установка прав доступа для PID файла
sudo chown root:root /var/run/sensors2shm.pid
sudo chmod 644 /var/run/sensors2shm.pid
```

## Автозапуск

Для автоматического запуска демона при загрузке системы создайте systemd сервис:

### Создание systemd сервиса

Создайте файл `/etc/systemd/system/sensors2shm.service`:

```ini
[Unit]
Description=Sensors2SHM Daemon
After=network.target

[Service]
Type=forking
ExecStart=/path/to/sensors2shm.sh start
ExecStop=/path/to/sensors2shm.sh stop
Restart=always
User=root
WorkingDirectory=/path/to/sensors2shm

[Install]
WantedBy=multi-user.target
```

### Активация сервиса

```bash
# Перезагрузка systemd
sudo systemctl daemon-reload

# Включение автозапуска
sudo systemctl enable sensors2shm

# Запуск сервиса
sudo systemctl start sensors2shm

# Проверка статуса
sudo systemctl status sensors2shm
```

## Мониторинг

### Проверка процессов

```bash
# Проверка PID файла
cat /var/run/sensors2shm.pid

# Проверка процесса
ps aux | grep background_ranging

# Проверка shared memory
ls -la /dev/shm/ | grep sensor
```

## Устранение неполадок

### Демон не запускается

1. Проверьте права доступа к файлам
2. Убедитесь, что I2C включен: `sudo raspi-config`
3. Проверьте подключение датчиков: `i2cdetect -y 1`
4. Запустите в обычном режиме для отладки: `./background_ranging`

### Демон завершается

1. Проверьте конфигурацию датчиков в `sensors_config.txt`
2. Убедитесь, что GPIO пины не заняты другими процессами
3. Проверьте доступность I2C шины

### Проблемы с shared memory

```bash
# Очистка shared memory
sudo rm -f /dev/shm/sensor_*

# Проверка семафоров
ls -la /dev/shm/ | grep sem
```

## Сигналы

Демон обрабатывает следующие сигналы:

- **SIGINT** (Ctrl+C) - корректное завершение
- **SIGTERM** - корректное завершение
- **SIGHUP** - перезагрузка конфигурации (не реализовано)

## Безопасность

- Демон работает от имени root для доступа к GPIO и I2C
- PID файл защищен правами доступа
- Проверка дублирования предотвращает запуск нескольких экземпляров
- Корректное освобождение ресурсов при завершении
- **Отсутствие логирования для защиты SD карты Raspberry Pi** 