# Wireless BMX160 streaming experiment

This document explains how to stream BMX160 gyroscope data over 802.15.4 radio
between two DK boards, reuse the existing PyGame visualiser, and evaluate
channel interference effects.

## Firmware roles

The updated source lives in `projects/common/01bsp_radio/01bsp_radio.c`. A
compile-time flag selects whether the board behaves as the **sensor node**
(samples BMX160 and transmits) or the **sink node** (receives packets and emits
UART frames).

```c
#define ROLE_SENSOR_NODE 1   // set to 1 for the board with the BMX160 attached
```

- Build once with `ROLE_SENSOR_NODE 1` and flash it to the DK that connects to
  the gravity sensor daughterboard.
- Build again with `ROLE_SENSOR_NODE 0` and flash the second DK that will act
  as the receiver.

Radio payload format (per packet):

| Byte index | Description                             |
|------------|------------------------------------------|
| 0          | Sequence counter (uint8, wraps at 255)   |
| 1..2       | Gyro X (int16, big endian, raw LSB)      |
| 3..4       | Gyro Y (int16, big endian)               |
| 5..6       | Gyro Z (int16, big endian)               |

The sink node strips the sequence byte and forwards the remaining six bytes to
UART, appending `\r\n`. This is exactly the same framing as the standalone
`01bsp_bmx160` example, so no Python changes are required.

`CHANNEL` still defaults to 11; adjust this macro to test other channels:

```c
#define CHANNEL 11   // change to interfere with neighbours
```

Remember to recompile and flash after changing `ROLE_SENSOR_NODE` or `CHANNEL`.

## Running the PyGame visualiser

The existing visualiser script
`projects/nrf52840_dk/01bsp_bmx160/01bsp_bmx160.py` reads raw gyro bytes and
animates a cube. Use it on the host that connects to the **sink** DK.

1. Install the Python dependencies once:
   ```bash
   python3 -m pip install --user pyserial pygame PyOpenGL
   ```
2. Edit the serial port in the script (default `COM14`) to match your system,
   e.g. `/dev/ttyUSB1` on Linux or `COMx` on Windows.
3. Power both DKs. The sensor node samples at ~16 Hz (62.5 ms period) and sends
   packets automatically.
4. Run the script and verify the virtual board rotates smoothly as you move the
   physical sensor.

If the cube appears “jumpy”, check the UART output from the sink board to make
sure the CRLF-terminated frames are arriving without gaps.

## Channel reuse experiment（中文说明）

为了观察同频干扰对平滑度的影响，可以与周围同学协作：

1. 先在无人干扰的信道（例如 11）上测试，记录 PyGame 窗口的动画流畅度。
2. 询问邻近小组正在使用的信道号，并把 `CHANNEL` 改成相同的数值，再次编译烧录。
3. 多个小组同时发送时，接收端的串口输出可能出现以下现象：
   - `

   - 动画角速度更新不连续，立方体会突然跳动；
   - RSSI（如果额外记录）波动加大。
4. 记录实验环境：人数、各组 payload 格式、是否有人移动或遮挡。将“干净环境”和“干扰环境”的体感差异写入实验报告。

恢复流畅度的常见方法：切换到独占信道、增大发送间隔、或临时让其他小组暂停发送，从而验证问题确实由同频干扰引起。

## Optional logging & analysis

If you want quantitative evidence in addition to visual smoothness, you can
reuse `scripts/log_serial_packets.py` with the sink UART to capture raw frames
per trial, then parse them to compute packet delivery rate or jitter. The log
format remains binary gyro triples per line, so ensure any custom parser
converts the signed 16-bit values the same way the PyGame script does.
