# Daemon

`sensors2shm.service` runs `background_ranging` in the foreground so systemd
can track its exit status and retain diagnostics in the journal. Both configured
sensors must initialize. If either sensor stops publishing for 2 seconds, the
process reports the SPI device and last read error, exits unsuccessfully, and
systemd restarts it after 2 seconds.

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

The service accepts only VL53L8CX SPI entries. A failed sensor makes the
service fail and retry; it cannot silently run with just one sensor.
