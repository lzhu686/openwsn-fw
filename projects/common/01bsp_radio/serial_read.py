#!/usr/bin/env python3
"""
Simple serial reader for OpenWSN/board UART output.
Usage: python3 serial_read.py /dev/ttyUSB0 [baud]
Default baud: 115200

This script prints lines received from the serial port, with timestamps.
It can also measure inter-line intervals to help determine packet rate stability.
"""
import sys
import time
import serial


def main():
    if len(sys.argv) < 2:
        print("Usage: {} <serial_port> [baud]".format(sys.argv[0]))
        sys.exit(1)
    port = sys.argv[1]
    baud = int(sys.argv[2]) if len(sys.argv) >= 3 else 115200

    try:
        ser = serial.Serial(port, baudrate=baud, timeout=1)
    except Exception as e:
        print("Failed to open serial port {}: {}".format(port, e))
        sys.exit(2)

    print("Listening on {} @ {} bps".format(port, baud))
    last_ts = None
    try:
        while True:
            line = ser.readline()
            if not line:
                # no data this loop
                continue
            ts = time.time()
            try:
                decoded = line.decode('utf-8', errors='replace').rstrip('\r\n')
            except Exception:
                decoded = repr(line)
            if last_ts is None:
                delta = 0.0
            else:
                delta = ts - last_ts
            last_ts = ts
            print("{:.6f} +{:.3f}s | {}".format(ts, delta, decoded))
    except KeyboardInterrupt:
        print('\nExiting...')
    finally:
        ser.close()


if __name__ == '__main__':
    main()
