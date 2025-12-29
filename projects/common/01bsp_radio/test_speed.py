#!/usr/bin/env python3
"""
Minimal test script to measure packet arrival intervals from the device UART.
Usage: python3 test_speed.py /dev/ttyUSB0 [baud] [duration_seconds]

This script prints per-packet timestamps, computes average interval, min/max and jitter.
It does not require a sequence number in the payload, but if you want to detect lost packets robustly,
add a small sequence counter in the firmware payload (example given in README).
"""
import sys
import time
import serial
from collections import deque


def main():
    if len(sys.argv) < 2:
        print("Usage: {} <serial_port> [baud] [duration_seconds]".format(sys.argv[0]))
        sys.exit(1)
    port = sys.argv[1]
    baud = int(sys.argv[2]) if len(sys.argv) >= 3 else 115200
    duration = int(sys.argv[3]) if len(sys.argv) >= 4 else 30

    try:
        ser = serial.Serial(port, baudrate=baud, timeout=1)
    except Exception as e:
        print("Failed to open serial port {}: {}".format(port, e))
        sys.exit(2)

    print("Measuring for {} seconds on {} @ {} bps".format(duration, port, baud))
    t0 = time.time()
    last_ts = None
    intervals = []
    count = 0
    try:
        while True:
            if time.time() - t0 > duration:
                break
            line = ser.readline()
            if not line:
                continue
            ts = time.time()
            count += 1
            if last_ts is not None:
                intervals.append(ts-last_ts)
            last_ts = ts
            # print short summary per N
            if count % 10 == 0:
                print("%d packets, last interval=%.4fs" % (count, intervals[-1] if intervals else 0.0))
    except KeyboardInterrupt:
        print('\nInterrupted by user')
    finally:
        ser.close()

    if intervals:
        avg = sum(intervals)/len(intervals)
        mn = min(intervals)
        mx = max(intervals)
        jitter = mx - mn
        print('--- results ---')
        print('packets received: %d' % count)
        print('intervals measured: %d' % len(intervals))
        print('average interval: %.6f s' % avg)
        print('min interval: %.6f s' % mn)
        print('max interval: %.6f s' % mx)
        print('jitter (max-min): %.6f s' % jitter)
        print('estimated packet rate: %.2f packets/sec' % (1.0/avg if avg>0 else 0.0))
    else:
        print('No intervals measured (not enough packets)')

if __name__ == '__main__':
    main()
