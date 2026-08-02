#!/usr/bin/env python3
"""Read and display SensorData frames published by background_ranging.

Default targets are the two VL53L8CX SPI sensors configured in
``sensors_config.txt``.  Run ``python3 read_sensors.py --once`` for a quick
hardware check or omit ``--once`` to watch frames continuously.
"""

import argparse
import mmap
import os
import signal
import struct
import sys
import time

import posix_ipc


HEADER_FORMAT = "<IBBBB"
HEADER_SIZE = struct.calcsize(HEADER_FORMAT)
MATRIX_FRAME_SIZE = HEADER_SIZE + 64 * 2 + 64
STATUS_OFFSET = HEADER_SIZE + 64 * 2
SENSOR_TYPES = {
    0: "VL53L1X",
    1: "VL53L5CX",
    2: "VL53L8CX (I2C)",
    3: "VL53L8CX (SPI)",
    4: "TCS34725",
}


class SensorData:
    def __init__(self, raw):
        if len(raw) < HEADER_SIZE:
            raise ValueError("frame is shorter than the 8-byte header")

        (self.timestamp_sec, self.sensor_type, self.resolution,
         self.data_format, _) = struct.unpack(HEADER_FORMAT, raw[:HEADER_SIZE])

        if self.data_format == 0:
            if len(raw) < 16:
                raise ValueError("single-value frame is truncated")
            self.distance_mm, self.status = struct.unpack("<HB", raw[8:11])
            self.distances = []
            self.statuses = []
            return

        if self.data_format != 1 or self.resolution not in (16, 64):
            raise ValueError(
                f"unsupported matrix header: format={self.data_format}, "
                f"resolution={self.resolution}"
            )
        if len(raw) < MATRIX_FRAME_SIZE:
            raise ValueError("matrix frame is truncated")

        self.distances = list(struct.unpack(
            f"<{self.resolution}H", raw[HEADER_SIZE:HEADER_SIZE + self.resolution * 2]
        ))
        self.statuses = list(raw[STATUS_OFFSET:STATUS_OFFSET + self.resolution])

    @property
    def sensor_name(self):
        return SENSOR_TYPES.get(self.sensor_type, f"unknown({self.sensor_type})")

    def format(self):
        timestamp = time.strftime("%H:%M:%S", time.localtime(self.timestamp_sec))
        if self.data_format == 0:
            return f"[{timestamp}] {self.sensor_name}: {self.distance_mm} mm, status={self.status}"

        width = int(self.resolution ** 0.5)
        rows = []
        for row in range(width):
            cells = []
            for col in range(width):
                zone = row * width + col
                cells.append(f"{self.distances[zone]:4d}/{self.statuses[zone]:3d}")
            rows.append(" ".join(cells))
        return (
            f"[{timestamp}] {self.sensor_name}, {width}x{width}, "
            "cell=distance_mm/status:\n  " + "\n  ".join(rows)
        )


class SensorReader:
    def __init__(self):
        self.handles = {}

    def open(self, name):
        fd = os.open(f"/dev/shm/{name}", os.O_RDONLY)
        try:
            size = os.fstat(fd).st_size
            if size < HEADER_SIZE:
                raise RuntimeError(f"/dev/shm/{name} is only {size} bytes")
            memory = mmap.mmap(fd, size, mmap.MAP_SHARED, mmap.PROT_READ)
            semaphore = posix_ipc.Semaphore(f"/sem_{name}")
            self.handles[name] = (fd, memory, semaphore)
        except Exception:
            os.close(fd)
            raise

    def read(self, name):
        if name not in self.handles:
            self.open(name)
        _, memory, semaphore = self.handles[name]
        locked = False
        try:
            semaphore.acquire(timeout=0.2)
            locked = True
            memory.seek(0)
            header = memory.read(HEADER_SIZE)
            if len(header) != HEADER_SIZE:
                raise RuntimeError("cannot read frame header")
            _, _, _, data_format, _ = struct.unpack(HEADER_FORMAT, header)
            size = 16 if data_format == 0 else MATRIX_FRAME_SIZE
            memory.seek(0)
            raw = memory.read(size)
            return SensorData(raw)
        finally:
            if locked:
                semaphore.release()

    def close(self):
        for fd, memory, semaphore in self.handles.values():
            memory.close()
            os.close(fd)
            semaphore.close()
        self.handles.clear()


def parse_args():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "names", nargs="*", default=["vl53l8cx_left", "vl53l8cx_right"],
        help="shared-memory names (default: vl53l8cx_left vl53l8cx_right)",
    )
    parser.add_argument("--once", action="store_true", help="read one frame and exit")
    parser.add_argument("--interval", type=float, default=0.25,
                        help="refresh period in seconds (default: 0.25)")
    return parser.parse_args()


def main():
    args = parse_args()
    if args.interval <= 0:
        raise SystemExit("--interval must be greater than zero")

    running = True

    def stop(_signal, _frame):
        nonlocal running
        running = False

    signal.signal(signal.SIGINT, stop)
    signal.signal(signal.SIGTERM, stop)
    reader = SensorReader()
    try:
        while running:
            for name in args.names:
                try:
                    print(f"{name}: {reader.read(name).format()}")
                except (OSError, RuntimeError, ValueError, posix_ipc.Error) as error:
                    print(f"{name}: ERROR: {error}", file=sys.stderr)
            if args.once:
                break
            print("-" * 72)
            time.sleep(args.interval)
    finally:
        reader.close()


if __name__ == "__main__":
    main()
