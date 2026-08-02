# VL53L8CX shared-memory frame

Each configured SPI sensor creates:

- `/dev/shm/<name>` — 200-byte frame;
- `/dev/shm/sem.sem_<name>` — POSIX semaphore (`/sem_<name>`).

The frame is little-endian:

| Offset | Type | Meaning |
| ---: | --- | --- |
| 0 | `uint32_t` | Unix timestamp, seconds |
| 4 | `uint8_t` | sensor type, always `3` (VL53L8CX SPI) |
| 5 | `uint8_t` | resolution, always `16` (4×4) |
| 6 | `uint8_t` | format, always `1` (matrix) |
| 8 | `uint16_t[64]` | distances in millimetres; first 16 are valid |
| 136 | `uint8_t[64]` | target statuses; first 16 are valid |

Readers must acquire `/sem_<name>` before reading a frame.
`read_sensors.py --once` is a convenient diagnostic client.
