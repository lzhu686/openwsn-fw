/**
\brief This is a program which shows how to use the bsp modules for the board
       and UART.

\note: Since the bsp modules for different platforms have the same declaration,
       you can use this project with any platform.

Load this program on your board. Open a serial terminal client (e.g. PuTTY or
TeraTerm):
- You will read "Hello World!" printed over and over on your terminal client.
- when you enter a character on the client, the board echoes it back (i.e. you
  see the character on the terminal client) and the "ERROR" led blinks.

\author Thomas Watteyne <watteyne@eecs.berkeley.edu>, February 2012
*/

#include "stdint.h"
#include "stdio.h"
#include "string.h"
// bsp modules required
#include "board.h"
#include "uart.h"
#include "sctimer.h"
#include "leds.h"

//=========================== defines =========================================

#define SCTIMER_PERIOD     0xffff // 0xffff@32kHz = 2s
#define APP_MAX_MSG_LEN    128
static const uint8_t defaultMessage[] = "Hello, World!\r\n";

//=========================== variables =======================================

typedef struct {
              uint8_t uart_lastTxByteIndex;
   volatile   uint8_t uartDone;
   volatile   uint8_t uartSendNow;
          uint8_t txBuffer[APP_MAX_MSG_LEN];
   volatile  uint8_t txLength;
          uint8_t rxBuffer[APP_MAX_MSG_LEN];
          uint8_t rxIndex;
   volatile  uint8_t rxLength;
   volatile  uint8_t newMsgReady;
} app_vars_t;

app_vars_t app_vars;

//=========================== prototypes ======================================

void cb_compare(void);
void cb_uartTxDone(void);
uint8_t cb_uartRxCb(void);

//=========================== main ============================================

/**
\brief The program starts executing here.
*/
int mote_main(void) {
   
   // clear local variable
   memset(&app_vars,0,sizeof(app_vars_t));
    
   app_vars.uartSendNow = 1;
   app_vars.txLength    = sizeof(defaultMessage)-1;
   memcpy(app_vars.txBuffer, defaultMessage, app_vars.txLength);
   
   // initialize the board
   board_init();
   
   // setup UART
   uart_setCallbacks(cb_uartTxDone,cb_uartRxCb);
   uart_enableInterrupts();
   
   // setup sctimer
   sctimer_set_callback(cb_compare);
   sctimer_setCompare(sctimer_readCounter()+SCTIMER_PERIOD);
   
   while(1) {
      
      // wait for timer to elapse
      while (app_vars.uartSendNow==0);
      app_vars.uartSendNow = 0;
      
      // send string over UART
      if (app_vars.newMsgReady) {
         if (app_vars.rxLength > 0 && app_vars.rxLength <= APP_MAX_MSG_LEN) {
            memcpy(app_vars.txBuffer, app_vars.rxBuffer, app_vars.rxLength);
            app_vars.txLength = app_vars.rxLength;
         }
         app_vars.newMsgReady = 0;
      }

      if (app_vars.txLength == 0) {
         continue;
      }

      app_vars.uartDone              = 0;
      app_vars.uart_lastTxByteIndex  = 0;
      uart_writeByte(app_vars.txBuffer[app_vars.uart_lastTxByteIndex]);
      while(app_vars.uartDone==0);
   }
}

//=========================== callbacks =======================================

void cb_compare(void) {
   
   // have main "task" send over UART
   app_vars.uartSendNow = 1;
   
   // schedule again
   sctimer_setCompare(sctimer_readCounter()+SCTIMER_PERIOD);
}

void cb_uartTxDone(void) {
   app_vars.uart_lastTxByteIndex++;
   if (app_vars.uart_lastTxByteIndex<app_vars.txLength) {
      uart_writeByte(app_vars.txBuffer[app_vars.uart_lastTxByteIndex]);
   } else {
      app_vars.uartDone = 1;
   }
}

uint8_t cb_uartRxCb(void) {
   uint8_t byte;
   
   // toggle LED
   leds_error_toggle();
   
   // read received byte
   byte = uart_readByte();
   
   if (app_vars.rxIndex < APP_MAX_MSG_LEN) {
      app_vars.rxBuffer[app_vars.rxIndex++] = byte;
   } else {
      app_vars.rxIndex = 0;
   }

   if (byte == '\n') {
      app_vars.rxLength    = app_vars.rxIndex;
      app_vars.rxIndex     = 0;
      app_vars.newMsgReady = 1;
   }

   return 0;
}