#!/usr/bin/env python3
"""Bidirectional serial bridge for interacting with an nRF52840-DK.

This script forwards user input to the development kit over UART and prints any
messages received from the board. A newline ("\n") is appended automatically to
outgoing messages so they match the firmware examples in this repository.

Example usage:
    python nrf52840_serial_bridge.py --port /dev/ttyACM0 --baud 115200
"""

from __future__ import annotations

import argparse
import sys
import threading
from typing import Tuple

try:
    import serial  # type: ignore
except ImportError as exc:  # pragma: no cover - dependency guard
    print(
        "pyserial is required for nrf52840_serial_bridge.py. Install it with 'pip install pyserial'.",
        file=sys.stderr,
    )
    raise SystemExit(1) from exc

def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Interact with an nRF52840-DK over a serial connection.",
    )
    parser.add_argument(
        "--port",
        required=True,
        help="Serial port connected to the nRF52840-DK (e.g. /dev/ttyACM0 or COM14).",
    )
    parser.add_argument(
        "--baud",
        type=int,
        default=115200,
        help="UART baud rate (default: 115200).",
    )
    parser.add_argument(
        "--newline",
        default="lf",
        choices=("lf", "crlf"),
        help=(
            "Newline sequence appended to outgoing messages. "
            "Use 'lf' for '\n' (default) or 'crlf' for '\r\n'."
        ),
    )
    parser.add_argument(
        "--timeout",
        type=float,
        default=0.5,
        help="Read timeout in seconds for the serial port (default: 0.5).",
    )
    args = parser.parse_args()
    args.newline = {"lf": "\n", "crlf": "\r\n"}[args.newline]
    return args

def open_serial(port: str, baud: int, timeout: float) -> serial.Serial:
    try:
        ser = serial.Serial(port=port, baudrate=baud, timeout=timeout)
    except serial.SerialException as exc:
        print(f"Failed to open serial port {port}: {exc}", file=sys.stderr)
        raise SystemExit(1) from exc
    return ser

def reader_loop(ser: serial.Serial, stop_event: threading.Event) -> None:
    while not stop_event.is_set():
        try:
            raw = ser.readline()
        except serial.SerialException as exc:
            print(f"[RX] Serial read error: {exc}", file=sys.stderr)
            stop_event.set()
            break

        if not raw:
            continue

        message = raw.decode("utf-8", errors="replace").rstrip("\r\n")
        print(f"[RX] {message}")

        if message == "exit":
            print("[RX] Remote requested shutdown (received 'exit').")
            stop_event.set()
            break

def writer_loop(ser: serial.Serial, stop_event: threading.Event, newline: str) -> None:
    try:
        while not stop_event.is_set():
            try:
                message = input("[TX] ")
            except EOFError:
                stop_event.set()
                break

            payload = (message + newline).encode("utf-8")

            try:
                ser.write(payload)
            except serial.SerialException as exc:
                print(f"[TX] Serial write error: {exc}", file=sys.stderr)
                stop_event.set()
                break

            if message == "exit":
                stop_event.set()
                break
    finally:
        stop_event.set()

def main() -> int:
    args = parse_args()
    stop_event = threading.Event()

    ser = open_serial(args.port, args.baud, args.timeout)
    print(
        "Connected to {port} at {baud} baud. Type 'exit' to close the session.".format(
            port=args.port,
            baud=args.baud,
        )
    )

    reader = threading.Thread(target=reader_loop, args=(ser, stop_event), daemon=True)
    reader.start()

    try:
        writer_loop(ser, stop_event, args.newline)
    except KeyboardInterrupt:
        stop_event.set()
    finally:
        stop_event.set()
        reader.join(timeout=1.0)
        ser.close()
        print("Disconnected.")

    return 0

if __name__ == "__main__":
    sys.exit(main())
