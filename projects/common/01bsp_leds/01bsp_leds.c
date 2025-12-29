/**
\brief This is a program which shows how to use the bsp modules for the board
       and leds.

\note: Since the bsp modules for different platforms have the same declaration,
       you can use this project with any platform.

Load this program on your boards. The LEDs should start blinking furiously.

\author Thomas Watteyne <watteyne@eecs.berkeley.edu>, August 2014.
*/

#include "stdint.h"
#include "stdio.h"
// bsp modules required
#include "board.h"
#include "leds.h"

#define LED_DELAY_LOOP   (0x080000UL)   // 实测当前板卡环境下0x080000优雅地依次闪亮

int mote_main(void){
   board_init(); // 初始化开发板
   leds_all_off(); // 确保所有LED熄灭

   while (1) {
      leds_error_on();      // LED1亮
      some_delay();
      leds_error_off();

      leds_sync_on();       // LED2亮
      some_delay();
      leds_sync_off();

      leds_debug_on();      // LED4亮
      some_delay();
      leds_debug_off();

      leds_radio_on();      // LED3亮
      some_delay();
      leds_radio_off();
   }
}

void some_delay(void) {
   volatile uint32_t counter;
   for (counter=0; counter<LED_DELAY_LOOP; counter++);
}

/**
\以下为源代码，用以参考

// int mote_main(void) {uint8_t i;
   
//    board_init(); //初始化开发板
   
//    // error LED functions 灯1
//    leds_error_on();          some_delay(); 
//    leds_error_off();         some_delay(); 
//    leds_error_toggle();      some_delay(); //状态翻转，开关切换
//    leds_error_blink();       some_delay(); //灯1基于toggle切换状态100次 空循环延迟一下

//    // radio LED functions 灯3
//    leds_radio_on();          some_delay(); 
//    leds_radio_off();         some_delay(); 
//    leds_radio_toggle();      some_delay(); 
   
//    // sync LED functions 灯2
//    leds_sync_on();           some_delay(); 
//    leds_sync_off();          some_delay(); 
//    leds_sync_toggle();       some_delay(); 
   
//    // debug LED functions 灯4
//    leds_debug_on();          some_delay(); 
//    leds_debug_off();         some_delay(); 
//    leds_debug_toggle();      some_delay(); 
   
//    // all LED functions
//    leds_all_off();           some_delay(); 
//    leds_all_on();            some_delay(); 
//    leds_all_off();           some_delay(); 
//    leds_all_toggle();        some_delay(); //四个灯依次切换
   
//    // LED increment function
//    leds_all_off();           some_delay();
//    for (i=0;i<9;i++) {
//       leds_increment();      some_delay();
//    }
   
//    // LED circular shift function
//    leds_all_off();           some_delay();
//    leds_error_on();          some_delay();
//    for (i=0;i<9;i++) {
//       leds_circular_shift(); some_delay();//检查引脚状态，寄存器四个bit值左移一位，如果首位取出来放到临时变量到最后一位
//    }
   
//    // reset the board, so the program starts running again
//    board_reset();
   
//    return 0;
// }

// void some_delay(void) {
//    volatile uint16_t delay;
//    for (delay=0xffff;delay>0;delay--);
// }
*/