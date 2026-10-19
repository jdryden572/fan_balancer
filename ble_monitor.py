#!/usr/bin/env python3
"""Read FanBalancer output over BLE (Nordic UART Service).

Setup:  python3 -m venv .venv && .venv/bin/pip install bleak
Run:    .venv/bin/python ble_monitor.py [--log ble.log] [--name FanBalancer]

Prints each line to stdout and appends it to the log file. Reconnects
automatically if the board restarts (e.g. BOOT button) or drops the link.
"""
import argparse
import asyncio
import sys
from datetime import datetime

from bleak import BleakClient, BleakScanner

# Must match NUS_TX_UUID in fan_balancer.ino
NUS_TX_UUID = "6E400003-B5A0-F393-E254-63E1E7C0B2E8"


async def run(name, log_path):
    buf = ""
    log = open(log_path, "a", buffering=1)

    def emit(msg):
        line = f"{datetime.now():%H:%M:%S} {msg}"
        print(line, flush=True)
        log.write(line + "\n")

    def on_data(_, data: bytearray):
        nonlocal buf
        buf += data.decode(errors="replace")
        while "\n" in buf:
            line, buf = buf.split("\n", 1)
            emit(line.rstrip("\r"))

    while True:
        emit(f"[scanning for {name}]")
        dev = await BleakScanner.find_device_by_name(name, timeout=10)
        if dev is None:
            continue
        disconnected = asyncio.Event()
        try:
            async with BleakClient(dev, disconnected_callback=lambda _: disconnected.set()) as c:
                emit("[connected]")
                buf = ""
                await c.start_notify(NUS_TX_UUID, on_data)
                await disconnected.wait()
        except Exception as e:
            emit(f"[error: {e}]")
        emit("[disconnected]")
        await asyncio.sleep(1)


if __name__ == "__main__":
    ap = argparse.ArgumentParser()
    ap.add_argument("--name", default="FanBalancer")
    ap.add_argument("--log", default="ble.log")
    args = ap.parse_args()
    try:
        asyncio.run(run(args.name, args.log))
    except KeyboardInterrupt:
        sys.exit(0)
