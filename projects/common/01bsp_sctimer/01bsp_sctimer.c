/**
\brief This program shows the use of the "sctimer" bsp module.

Since the bsp modules for different platforms have the same declaration, you
can use this project with any platform.

Load this program onto your board, and start running. It will enable the sctimer. 
The sctimer is periodic, of period SCTIMER_PERIOD ticks. Each time it elapses:
    - the frame debugpin toggles
    - the error LED toggles

\author Tengfei Chang <tengfei.chang@eecs.berkeley.edu>, April 2017.
*/

#include "stdint.h"
#include "string.h"
#include "board.h"
#include "debugpins.h"
#include "leds.h"
#include "sctimer.h"

//=========================== defines =========================================

#define SCTIMER_PERIOD     16384 // @32kHz = 0.5s

//=========================== variables =======================================

typedef struct {
   uint16_t num_compare;
   uint8_t  led2_counter;
   uint8_t  led3_counter;
   uint8_t  led4_counter;
} app_vars_t;

app_vars_t app_vars;

//=========================== prototypes ======================================

void cb_compare(void);

//=========================== main ============================================

/**
\brief The program starts executing here.
*/
int mote_main(void) {  
   
   // initialize board. 
   board_init();
   
   sctimer_set_callback(cb_compare);
   sctimer_setCompare(sctimer_readCounter()+SCTIMER_PERIOD);
   
   while (1) {
      board_sleep();
   }
}

//=========================== callbacks =======================================

void cb_compare(void) {
   
   // toggle pin
   debugpins_frame_toggle();
   
   // toggle LEDs at their respective intervals
   leds_error_toggle();                // LED1 every 0.5s

   if (++app_vars.led2_counter >= 2) { // LED2 every 1s
      leds_radio_toggle();
      app_vars.led2_counter = 0;
   }

   if (++app_vars.led3_counter >= 3) { // LED3 every 1.5s
      leds_sync_toggle();
      app_vars.led3_counter = 0;
   }

   if (++app_vars.led4_counter >= 4) { // LED4 every 2s
      leds_debug_toggle();
      app_vars.led4_counter = 0;
   }

   // increment counter for bookkeeping
   app_vars.num_compare++;

   // schedule again
   sctimer_setCompare(sctimer_readCounter()+SCTIMER_PERIOD);
}
