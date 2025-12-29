Serial tools for testing OpenWSN/device UART output

Files added:
- `scripts/serial_read.py` — simple timestamped serial reader. Usage:

  python3 scripts/serial_read.py /dev/ttyUSB0 [baud]

  Default baud: 115200

- `scripts/test_speed.py` — collects packet arrival intervals and prints average/min/max/jitter.

  python3 scripts/test_speed.py /dev/ttyUSB0 [baud] [duration_seconds]

Notes and recommended experiments

1) Verify custom payload
   - Build and flash the modified firmware (`projects/common/01bsp_radio/01bsp_radio.c`) on one board.
   - Plug the board into your host and determine the serial device (e.g. `/dev/ttyUSB0`).
   - Run:

     python3 scripts/serial_read.py /dev/ttyUSB0 115200

   - You should see UART lines printed; the periodic transmitted packet will contain the custom payload `HELLO_NODE_A` (then padded bytes). Adjust `custom_payload` in the C file if you want different text.

2) Increase sending speed
   - The send rate is controlled by `TIMER_PERIOD` in `01bsp_radio.c`. Smaller `TIMER_PERIOD` -> more frequent sends.
   - Example: reduce `TIMER_PERIOD` to half for twice the sending rate, rebuild and flash.
   - Use `scripts/test_speed.py` to measure inter-packet intervals and estimate packet rate. Try increasing speed until you observe drops or large jitter.
   - If you want robust lost-packet detection, modify the firmware payload to include a small sequence counter (example snippet in the next section).

3) Robust sequence-number payload (recommended for automatic loss detection)
   - In the timer handler, instead of copying a fixed string, prepend a 2-byte sequence counter and then the message. Update `LEN_PKT_TO_SEND` accordingly.
   - Example in C (pseudocode):

     static uint16_t seq = 0;
     char *msg = "HELLO";
     uint8_t i = 0;
     app_vars.packet[i++] = (seq >> 8) & 0xff;
     app_vars.packet[i++] = seq & 0xff;
     memcpy(&app_vars.packet[i], msg, strlen(msg));
     i += strlen(msg);
     // pad rest
     while (i<app_vars.packet_len) app_vars.packet[i++] = ID;
     seq++;

   - On the host, parse the first two bytes as a sequence and detect gaps.

4) Change channel and test reception
   - To change the channel, update the `#define CHANNEL` in `01bsp_radio.c` and rebuild/flash for one device.
   - On the other device, keep its `CHANNEL` unchanged (or set to the new channel to test receiving). Observe LED activity and serial output when packets are received.

Dependencies
- These scripts use `pyserial`. Install with:

  pip3 install pyserial


