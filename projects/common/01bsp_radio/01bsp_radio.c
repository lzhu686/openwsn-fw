/**
\brief This program shows the use of the "radio" bsp module.

Since the bsp modules for different platforms have the same declaration, you
can use this project with any platform.

After loading this program, your board will switch on its radio on frequency
CHANNEL.

While receiving a packet (i.e. from the start of frame event to the end of
frame event), it will turn on its sync LED.

Every TIMER_PERIOD, it will also send a packet containing LENGTH_PACKET bytes
set to ID. While sending a packet (i.e. from the start of frame event to the
end of frame event), it will turn on its error LED.

\author Thomas Watteyne <watteyne@eecs.berkeley.edu>, August 2014.
*/

#include <stdint.h>
#include <string.h>

#include "board.h"
#include "radio.h"
#include "leds.h"
#include "sctimer.h"
#include "uart.h"
#include "i2c.h"
#include "bmx160.h"

//=========================== defines =========================================

#define LENGTH_PACKET       (125+LENGTH_CRC) ///< maximum length is 127 bytes
#define CHANNEL             11              ///< 11=2.405GHz
#define ROLE_SENSOR_NODE    1               ///< set to 1 for sensor TX node, 0 for RX sink node
#define SAMPLE_PERIOD       (32768>>4)      ///< timer ticks @32kHz (~62.5ms)
#define GYRO_BYTES          6               ///< 3 axes * 2 bytes (big endian)
#define RADIO_SEQ_BYTES     1
#define RADIO_PAYLOAD_BYTES (RADIO_SEQ_BYTES + GYRO_BYTES)
#define LEN_PKT_TO_SEND     (RADIO_PAYLOAD_BYTES + LENGTH_CRC)
#define UART_FRAME_LEN      (GYRO_BYTES + 2) ///< raw gyro bytes + CRLF

//=========================== variables =======================================

enum {
    APP_FLAG_START_FRAME = 0x01,
    APP_FLAG_END_FRAME   = 0x02,
    APP_FLAG_TIMER       = 0x04,
};

typedef enum {
    APP_STATE_TX         = 0x01,
    APP_STATE_RX         = 0x02,
} app_state_t;

typedef struct {
    uint8_t              num_startFrame;
    uint8_t              num_endFrame;
    uint8_t              num_timer;
    
    uint8_t              num_rx_startFrame;
    uint8_t              num_rx_endFrame;
} app_dbg_t;

app_dbg_t app_dbg;

typedef struct {
    volatile    uint8_t         uartDone;
                uint8_t         uart_lastTxByteIndex;
                uint8_t         uart_buffer[UART_FRAME_LEN];

                uint8_t         flags;
                app_state_t     state;
                uint8_t         packet[LENGTH_PACKET];
                uint8_t         packet_len;
                uint8_t         tx_seq;
                int8_t          rxpk_rssi;
                uint8_t         rxpk_lqi;
                bool            rxpk_crc;
} app_vars_t;

app_vars_t app_vars;

//=========================== prototypes ======================================

void     cb_startFrame(PORT_TIMER_WIDTH timestamp);
void     cb_endFrame(PORT_TIMER_WIDTH timestamp);
void     cb_timer(void);

void     cb_uart_tx_done(void);
uint8_t  cb_uart_rx(void);

//=========================== main ============================================

/**
\brief The program starts executing here.
*/
int mote_main(void) {
    // clear local variables
    memset(&app_vars,0,sizeof(app_vars_t));

    // initialize board
    board_init();

    // setup UART
    uart_setCallbacks(cb_uart_tx_done,cb_uart_rx);
    uart_enableInterrupts();

    app_vars.uartDone = 1;
    app_vars.tx_seq   = 0;

#if ROLE_SENSOR_NODE
    // configure BMX160 sensor (gyro at 3200Hz, +/-1000°/s like standalone example)
    i2c_set_addr(BMX160_ADDR);
    (void)bmx160_who_am_i();
    bmx160_acc_config(0x0c);
    bmx160_gyr_config(0x0d);
    bmx160_mag_config(0x0b);
    bmx160_acc_range(0x8);
    bmx160_gyr_range(0x1);

    // start periodic sampler timer
    sctimer_set_callback(cb_timer);
    sctimer_setCompare(sctimer_readCounter()+SAMPLE_PERIOD);
    sctimer_enable();

    // kick off first sample immediately
    app_vars.flags |= APP_FLAG_TIMER;
#endif

    // add callback functions radio
    radio_setStartFrameCb(cb_startFrame);
    radio_setEndFrameCb(cb_endFrame);

    // prepare radio
    radio_rfOn();
    radio_setFrequency(CHANNEL, FREQ_RX);

    radio_rxEnable();
    radio_rxNow();
    app_vars.state = APP_STATE_RX;

    while (1) {

        // sleep while waiting for at least one of the flags to be set
        while (app_vars.flags==0x00) {
            board_sleep();
        }

        // handle and clear every flag
        while (app_vars.flags) {


            //==== APP_FLAG_START_FRAME (TX or RX)

            if (app_vars.flags & APP_FLAG_START_FRAME) {
                // start of frame

                switch (app_vars.state) {
                    case APP_STATE_RX:
                        // started receiving a packet

                        // led
                        leds_error_on();
                        break;
                    case APP_STATE_TX:
                        // started sending a packet

                        // led
                        leds_sync_on();
                    break;
                }

                // clear flag
                app_vars.flags &= ~APP_FLAG_START_FRAME;
            }

            //==== APP_FLAG_END_FRAME (TX or RX)

            if (app_vars.flags & APP_FLAG_END_FRAME) {
                // end of frame

                switch (app_vars.state) {

                    case APP_STATE_RX:

                        // done receiving a packet
                        app_vars.packet_len = sizeof(app_vars.packet);

                        // get packet from radio
                        radio_getReceivedFrame(
                            app_vars.packet,
                            &app_vars.packet_len,
                            sizeof(app_vars.packet),
                            &app_vars.rxpk_rssi,
                            &app_vars.rxpk_lqi,
                            &app_vars.rxpk_crc
                        );
#if !ROLE_SENSOR_NODE
                        if (app_vars.rxpk_crc && app_vars.packet_len >= LEN_PKT_TO_SEND) {
                            if (app_vars.uartDone) {
                                memcpy(&app_vars.uart_buffer[0], &app_vars.packet[1], GYRO_BYTES);
                                app_vars.uart_buffer[GYRO_BYTES]     = '\r';
                                app_vars.uart_buffer[GYRO_BYTES + 1] = '\n';

                                app_vars.uartDone             = 0;
                                app_vars.uart_lastTxByteIndex = 0;
                                uart_writeByte(app_vars.uart_buffer[app_vars.uart_lastTxByteIndex]);
                            }
                        }
#endif

                        // led
                        leds_error_off();
                        break;
                    case APP_STATE_TX:
                        // done sending a packet

                        // switch to RX mode
                        radio_rxEnable();
                        radio_rxNow();
                        app_vars.state = APP_STATE_RX;

                        // led
                        leds_sync_off();
                        break;
                }
                // clear flag
                app_vars.flags &= ~APP_FLAG_END_FRAME;
            }

            //==== APP_FLAG_TIMER

            if (app_vars.flags & APP_FLAG_TIMER) {
                // timer fired

#if ROLE_SENSOR_NODE
                if (app_vars.state==APP_STATE_RX) {
                    int16_t gyr_x;
                    int16_t gyr_y;
                    int16_t gyr_z;

                    // stop listening during transmission
                    radio_rfOff();

                    // read latest gyro sample
                    bmx160_read_9dof_data();
                    gyr_x = bmx160_read_gyr_x();
                    gyr_y = bmx160_read_gyr_y();
                    gyr_z = bmx160_read_gyr_z();

                    app_vars.packet_len = LEN_PKT_TO_SEND;
                    app_vars.packet[0]  = app_vars.tx_seq;
                    app_vars.packet[1]  = (uint8_t)((gyr_x >> 8) & 0xff);
                    app_vars.packet[2]  = (uint8_t)(gyr_x & 0xff);
                    app_vars.packet[3]  = (uint8_t)((gyr_y >> 8) & 0xff);
                    app_vars.packet[4]  = (uint8_t)(gyr_y & 0xff);
                    app_vars.packet[5]  = (uint8_t)((gyr_z >> 8) & 0xff);
                    app_vars.packet[6]  = (uint8_t)(gyr_z & 0xff);

                    // start transmitting packet with payload (sequence + gyro data)
                    radio_loadPacket(app_vars.packet,RADIO_PAYLOAD_BYTES);
                    radio_txEnable();
                    radio_txNow();

                    app_vars.tx_seq++;

                    app_vars.state = APP_STATE_TX;
                }
#endif

                // clear flag
                app_vars.flags &= ~APP_FLAG_TIMER;
            }
        }
    }
}

//=========================== callbacks =======================================

void cb_startFrame(PORT_TIMER_WIDTH timestamp) {
    // set flag
    app_vars.flags |= APP_FLAG_START_FRAME;

    // update debug stats
    app_dbg.num_startFrame++;

    if (app_vars.state == APP_STATE_RX) {
        app_dbg.num_rx_startFrame++;
    }
}

void cb_endFrame(PORT_TIMER_WIDTH timestamp) {
    // set flag
    app_vars.flags |= APP_FLAG_END_FRAME;

    // update debug stats
    app_dbg.num_endFrame++;

    if (app_vars.state == APP_STATE_RX) {
        app_dbg.num_rx_endFrame++;
    }
}

void cb_timer(void) {
#if ROLE_SENSOR_NODE
    // set flag
    app_vars.flags |= APP_FLAG_TIMER;

    // update debug stats
    app_dbg.num_timer++;

    sctimer_setCompare(sctimer_readCounter()+SAMPLE_PERIOD);
#endif
}

void cb_uart_tx_done(void) {
    app_vars.uart_lastTxByteIndex++;
    if (app_vars.uart_lastTxByteIndex<UART_FRAME_LEN) {
        uart_writeByte(app_vars.uart_buffer[app_vars.uart_lastTxByteIndex]);
    } else {
        app_vars.uartDone = 1;
    }
}

uint8_t cb_uart_rx(void) {
    uint8_t byte;

    // toggle LED
    leds_error_toggle();

    // read received byte
    byte = uart_readByte();

    // echo that byte over serial
    uart_writeByte(byte);

    return 0;
}
