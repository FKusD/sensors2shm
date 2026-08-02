# Демон sensors2shm

`background_ranging` сначала инициализирует датчики и запускает ranging, а
затем при ключе `--daemon` отделяется от терминала. PID-файл создаётся в
`/run/sensors2shm.pid`; при SIGINT или SIGTERM датчики останавливаются,
shared memory и семафоры освобождаются, PID-файл удаляется.

## Запуск

```bash
cd /path/to/sensors2shm
make
sudo ./background_ranging --daemon
```

Для диагностики запускайте без `--daemon`, чтобы видеть вывод
инициализации:

```bash
sudo ./background_ranging
```

Конфигурация всегда читается из `./sensors_config.txt`, поэтому в unit-файле
нужно указать `WorkingDirectory` репозитория/установочной директории.

## Пример systemd unit

```ini
[Unit]
Description=sensors2shm ranging daemon
After=local-fs.target

[Service]
Type=forking
WorkingDirectory=/opt/sensors2shm
ExecStart=/opt/sensors2shm/background_ranging --daemon
PIDFile=/run/sensors2shm.pid
Restart=on-failure
User=root

[Install]
WantedBy=multi-user.target
```

После установки:

```bash
sudo systemctl daemon-reload
sudo systemctl enable --now sensors2shm.service
sudo systemctl status sensors2shm.service
```

## Диагностика SPI

Проверьте, что SPI включён и оба устройства доступны:

```bash
ls -l /dev/spidev0.0 /dev/spidev0.1
```

Если один VL53L8CX не обнаруживается, сначала проверьте его питание, XSHUT
и CS. Два датчика имеют один и тот же заводской адрес, поэтому их нельзя
инициализировать одновременно без последовательного управления XSHUT.
