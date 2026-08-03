#!/usr/bin/env python3
"""Display 4x4 VL53L8CX SPI frames from POSIX shared memory."""

import argparse
import mmap
import os
import struct
import time

import posix_ipc


HEADER_FORMAT = "<IBBBB"
HEADER_SIZE = struct.calcsize(HEADER_FORMAT)
RESOLUTION = 16
FRAME_SIZE = HEADER_SIZE + 64 * 2 + 64
SENSOR_TYPE_VL53L8CX_SPI = 3
DISTANCES_OFFSET = HEADER_SIZE
STATUSES_OFFSET = DISTANCES_OFFSET + 64 * 2


def read_frame(name: str) -> tuple[int, list[int], list[int]]:
    fd = os.open(f"/dev/shm/{name}", os.O_RDONLY)
    try:
        memory = mmap.mmap(fd, FRAME_SIZE, mmap.MAP_SHARED, mmap.PROT_READ)
        semaphore = posix_ipc.Semaphore(f"/sem_{name}")
        try:
            semaphore.acquire(timeout=0.2)
            try:
                memory.seek(0)
                raw = memory.read(FRAME_SIZE)
            finally:
                semaphore.release()
        finally:
            semaphore.close()
            memory.close()
    finally:
        os.close(fd)

    timestamp, sensor_type, resolution, data_format, _ = struct.unpack_from(
        HEADER_FORMAT, raw
    )
    if (sensor_type, resolution, data_format) != (
        SENSOR_TYPE_VL53L8CX_SPI,
        RESOLUTION,
        1,
    ):
        raise ValueError(
            f"unexpected frame header: type={sensor_type}, "
            f"resolution={resolution}, format={data_format}"
        )
    distances = list(struct.unpack_from("<16H", raw, DISTANCES_OFFSET))
    # The C frame always reserves 64 distance slots, even for a 4x4 frame.
    statuses = list(raw[STATUSES_OFFSET : STATUSES_OFFSET + RESOLUTION])
    return timestamp, distances, statuses


def print_frame(name: str) -> None:
    timestamp, distances, statuses = read_frame(name)
    label = time.strftime("%H:%M:%S", time.localtime(timestamp))
    print(f"{name} [{label}] VL53L8CX SPI 4x4:")
    for row in range(4):
        offset = row * 4
        cells = [
            f"{distances[offset + column]:4d}/{statuses[offset + column]:3d}"
            for column in range(4)
        ]
        print("  " + " ".join(cells))


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "names",
        nargs="*",
        default=["vl53l8cx_left", "vl53l8cx_right"],
        help="shared-memory names",
    )
    parser.add_argument("--once", action="store_true", help="print one frame")
    parser.add_argument("--interval", type=float, default=0.25)
    args = parser.parse_args()

    while True:
        for name in args.names:
            try:
                print_frame(name)
            except (OSError, ValueError, posix_ipc.Error) as error:
                print(f"{name}: ERROR: {error}")
        if args.once:
            return
        time.sleep(args.interval)


if __name__ == "__main__":
    main()
