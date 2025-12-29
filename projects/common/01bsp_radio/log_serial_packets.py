#!/usr/bin/env python3
"""Log packet metadata from DK board UART output.

The firmware prints one line per accepted packet in the format:
    <len> <seq> <crc_ok> <rssi>

Usage:
    python3 log_serial_packets.py /dev/ttyUSB0 20 --baud 115200

This will create/overwrite ``log_20cm_pkt.log`` and append each parsed line.
Use ``--append`` to keep existing logs.
"""
import argparse
import re
import sys
from typing import Optional

try:
    import serial  # type: ignore
except ImportError as exc:  # pragma: no cover
    print("pyserial is required: pip install pyserial", file=sys.stderr)
    raise

LINE_RE = re.compile(r"^\s*(-?\d+)\s+(\d+)\s+(\d+)\s+(-?\d+)\s*$")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Log UART packet metadata to distance-specific files")
    parser.add_argument("port", help="Serial port (e.g. /dev/ttyUSB0)")
    parser.add_argument("distance_cm", type=int, help="Measurement distance in cm, e.g. 20")
    parser.add_argument("--baud", type=int, default=115200, help="Serial baud rate (default: 115200)")
    parser.add_argument("--output", default=None, help="Explicit output file path; defaults to log_<distance>cm_pkt.log")
    parser.add_argument("--append", action="store_true", help="Append to output file instead of overwriting")
    parser.add_argument("--show", action="store_true", help="Echo parsed lines to stdout as they are logged")
    parser.add_argument("--quiet", action="store_true", help="Suppress status messages")
    return parser.parse_args()


def build_output_path(distance_cm: int, explicit: Optional[str]) -> str:
    if explicit:
        return explicit
    return f"log_{distance_cm}cm_pkt.log"


def main() -> None:
    args = parse_args()
    output_path = build_output_path(args.distance_cm, args.output)
    mode = "a" if args.append else "w"

    if not args.quiet:
        print(f"Opening serial port {args.port} @ {args.baud} baud")
        print(f"Logging to {output_path} (mode={mode})")

    try:
        ser = serial.Serial(args.port, baudrate=args.baud, timeout=1)
    except serial.SerialException as exc:
        print(f"Failed to open serial port {args.port}: {exc}", file=sys.stderr)
        sys.exit(1)

    with ser, open(output_path, mode, encoding="utf-8") as log_file:
        try:
            while True:
                raw = ser.readline()
                if not raw:
                    continue
                try:
                    decoded = raw.decode("utf-8", errors="ignore").strip()
                except Exception:
                    continue
                match = LINE_RE.match(decoded)
                if not match:
                    # optional: show unparsed lines for debugging
                    if args.show and not args.quiet:
                        print(f"[skip] {decoded}")
                    continue
                length, seq, crc_ok, rssi = map(int, match.groups())
                line = f"{length} {seq} {crc_ok} {rssi}\n"
                log_file.write(line)
                log_file.flush()
                if args.show and not args.quiet:
                    print(line.strip())
        except KeyboardInterrupt:
            if not args.quiet:
                print("\nLogging stopped by user")


if __name__ == "__main__":
    main()
