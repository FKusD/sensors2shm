# sensors2shm

Minimal daemon for two VL53L8CX sensors over SPI. It publishes 4×4 ranging
frames at 60 Hz into POSIX shared memory; it does not contain I²C, VL53L1X or
VL53L5CX support.

## Hardware

Both modules share SPI0 MOSI (GPIO10), MISO (GPIO9), SCLK (GPIO11), 3.3 V and
GND. The modules use separate chip selects configured by:

```ini
dtoverlay=spi0-2cs,cs0_pin=22,cs1_pin=23
```

| Sensor | NCS | SPI device | shared memory |
| --- | --- | --- | --- |
| left | GPIO22 | `/dev/spidev0.0` | `vl53l8cx_left` |
| right | GPIO23 | `/dev/spidev0.1` | `vl53l8cx_right` |

`LPn` and `SPI_I2C_N` are held HIGH at 3.3 V in hardware. The daemon never
changes them. SPI runs in mode 3 at 1 MHz by default.

## Configuration

`sensors_config.txt` accepts only this format:

```text
l8cx_spi <spi_bus> <spi_cs> <shm_name>
```

The default configuration contains both sensors. To diagnose one sensor,
temporarily comment out the other line and restart the service.

## Build and run

```bash
make
sudo systemctl restart sensors2shm.service
uv run read_sensors.py --once
```

The systemd unit starts `background_ranging --daemon` and keeps it running.
Do not launch a second manual instance while the service is active.

See [README_sensors.md](README_sensors.md) for the shared-memory format.
