/**
\brief Demonstration project for the BMP388 pressure/temperature sensor.

The application periodically samples the sensor through the shared BSP I2C
module and streams compensated readings over UART.
*/

#include "stdio.h"
#include "stdbool.h"
#include "stdint.h"
#include "string.h"

#include "board.h"
#include "i2c.h"
#include "leds.h"
#include "sctimer.h"
#include "uart.h"
#include "bmp388.h"

//=========================== defines =========================================

#define SAMPLE_PERIOD_TICKS   (32768 >> 2)   // 0.5 s @ 32 kHz clock
#define UART_BUFFER_LEN       64

//=========================== variables =======================================

typedef struct {
    volatile bool sample_now;
    volatile bool uart_busy;
    uint8_t uart_buffer[UART_BUFFER_LEN];
    uint8_t uart_len;
    uint8_t uart_index;
    float temperature_c;
    float pressure_pa;
    float altitude_m;
} app_vars_t;

static app_vars_t app_vars;

//=========================== prototypes ======================================

static void cb_compare(void);
static void cb_uart_tx_done(void);
static uint8_t cb_uart_rx(void);
static void schedule_next_sample(void);
static void publish_measurement(void);

//=========================== main ============================================

int mote_main(void) {
    memset(&app_vars, 0, sizeof(app_vars));

    board_init();

    uart_setCallbacks(cb_uart_tx_done, cb_uart_rx);
    uart_enableInterrupts();

    sctimer_set_callback(cb_compare);
    schedule_next_sample();

    i2c_set_addr(BMP388_I2C_ADDR);

    if (!bmp388_init()) {
        // Chip ID mismatch -> blink error LED continuously.
        while (1) {
            leds_error_toggle();
            for (volatile uint32_t delay = 0; delay < 0xfffff; delay++) {
                // busy wait
            }
        }
    }

    while (1) {
        if (!app_vars.sample_now) {
            continue;
        }
        app_vars.sample_now = false;

        bmp388_update_measurement();
        app_vars.temperature_c = bmp388_get_temperature_c();
        app_vars.pressure_pa = bmp388_get_pressure_pa();
    app_vars.altitude_m = bmp388_get_altitude_m(101325.0f);

        publish_measurement();
    }
}

//=========================== callbacks =======================================

static void cb_compare(void) {
    app_vars.sample_now = true;
    schedule_next_sample();
}

static void cb_uart_tx_done(void) {
    app_vars.uart_index++;

    if (app_vars.uart_index < app_vars.uart_len) {
        uart_writeByte(app_vars.uart_buffer[app_vars.uart_index]);
    } else {
        app_vars.uart_busy = false;
    }
}

static uint8_t cb_uart_rx(void) {
    uint8_t byte = uart_readByte();
    uart_writeByte(byte); // echo
    return 0;
}

//=========================== helpers =========================================

static void schedule_next_sample(void) {
    sctimer_setCompare(sctimer_readCounter() + SAMPLE_PERIOD_TICKS);
}

static void publish_measurement(void) {
    if (app_vars.uart_busy) {
        return;
    }

    int len = snprintf((char *)app_vars.uart_buffer,
                       UART_BUFFER_LEN,
                       "T=%.2fC P=%.1fPa Alt=%.2fm\r\n",
                       app_vars.temperature_c,
                       app_vars.pressure_pa,
                       app_vars.altitude_m);

    if (len <= 0) {
        return;
    }

    if (len >= UART_BUFFER_LEN) {
        len = UART_BUFFER_LEN - 1;
        app_vars.uart_buffer[len] = '\n';
    }

    app_vars.uart_busy = true;
    app_vars.uart_len = (uint8_t)len;
    app_vars.uart_index = 0;
    uart_writeByte(app_vars.uart_buffer[0]);
}
