# Daemon

`sensors2shm.service` starts `background_ranging --daemon` and uses
`/run/sensors2shm.pid` to track the forked process.

Install or update the unit from this repository:

```bash
sudo install -m 644 sensors2shm.service /etc/systemd/system/sensors2shm.service
sudo systemctl daemon-reload
sudo systemctl restart sensors2shm.service
```

Useful commands:

```bash
sudo systemctl restart sensors2shm.service
systemctl status sensors2shm.service
journalctl -u sensors2shm.service --no-pager -n 50
```

The daemon accepts only VL53L8CX SPI entries. If only one sensor initializes,
it continues publishing that sensor while the system log records the failed
SPI device and initialization stage.
