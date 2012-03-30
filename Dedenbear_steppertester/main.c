
#include "msp430x20x3.h"
#include "stdio.h"

// Define the bits for the 4 switches 
#define SW_FWD 0x80
#define SW_REV 0x20
#define SW_FWD_SLOW 0x40
#define SW_REV_SLOW 0x10


static unsigned  motor_off = 0;

// WDT Interrupt Service Routine
//
#pragma vector=WDT_VECTOR
__interrupt void wdt_ISR(void)
{
 // P1IFG = 0;
 // P1IE = 0xf;
}


//
// Timer_A Initialization
//

void timerA_Init(unsigned int rate, unsigned int lenght)
{

  P1IE  = 0x00;  // P4..7 interrupt enable off -- no buttons while motor is running
  P1IFG = 0;     // Clear kbd interrupt status 
  
   
  motor_off = lenght; 
  // Initialize Compare Register 0
  TACCR0 = rate;

  // Configure Capture/Control Register 0
  // Capture Mode: Off (0)
  // Input Select: CCIxA (0)
  // Synchronize: No (0)
  // Mode: Compare (0)
  // Output Mode: OUT bit (0)
  // Interrupt Enable: On (1)
  TACCTL0 = CCIE;

  // Configure Timer A Control Register
  // Clock source: SMCLK (TASSEL_2)
  // Input divider: /1   (0)
  // Mode Control: Up (MC_1)
  // Clear: Yes (TACLR)
  // Interrupt Enable: No (0)
  TACTL = TASSEL_2 + MC_1 + TACLR +ID_2;
}

//
// PORT 1 Interrupt Service Routine
//
#pragma vector=PORT1_VECTOR
__interrupt void port1_ISR (void)
{
   // although the interrupt is generated on the low to high transition there 
  // is a chance of the contact bouncing on the way to close, therefore wait here 
  // until the button is open again -- 
  // As soon as the motor (timer_A) is running 
  // interrupts for the KBD are turned off, therefore acting as a long debounce time 
  // for the swithes and this wait here then should not be necessary 
   while (( P1IN &0xf0 )!= 0xf0) // switch pulls low, 
    ;
   
   if (P1IFG & SW_REV || P1IFG & SW_REV_SLOW )   // interrupt flag is 
     P1OUT = 0xF2;         // set direction bit -- keep high bits on for pull up
   else 
     P1OUT = 0xF0;


    // timer interrupts at 100us -- 2 interrupts needed to make one cycle  
   if (P1IFG & SW_FWD_SLOW || P1IFG & SW_REV_SLOW )
    timerA_Init(1200, 540);// 1.6khz, 800 pulses
   else 
     timerA_Init(550, 540);// 1.6khz, 800 pulses


}



//
//  Timer A0 Interrupt Service Routine
//
#pragma vector=TIMERA0_VECTOR
__interrupt void timerA0_ISR(void)
{
 
   P1OUT ^= 0x1;  // toggle p1.0 -- the led -- create pulse for stepper
 /*
   if ( motor_off < 20 )  // ramp down before stop
   {
     TACCR0 += 10;
   }
   else
   {
      if (TACCR0 > 200 ) // 5khz // ramp up from start
        TACCR0 -= 1;
   }
 */
   
  if (--motor_off == 0) // turn off pulses 
  {  
     TACCTL0 = 0;   // turn off this interrupt 
     P1OUT &= 0xf0; // clear the clock  and direction on end 

     // turn keybd interrupts back on
     P1IFG = 0;   // Clear interrupt status 
     P1IE  = 0xf0;  // P4..7 interrupt enable the buttons 
  }
  
}

void main(void)
{
//   pointer for port1
//   volatile unsigned char * const  port1= (volatile unsigned char *) 0x21;
  

  /*
#define WDTIS0             interval select 00=clk/32768, 01= clk/8192
#define WDTIS1             "" 02=clk/512, 03=clk/64
#define WDTSSEL           ClockSel 0=SMCLK, 1=ACLK
#define WDTCNTCL          resets the counter 
#define WDTTMSEL          modeselect 1==interval, 0 = watchdog
#define WDTNMI            Mode select for RST/NMI pin 1=NMI
#define WDTNMIES          Edge select for NMI pin 1=falling
#define WDTHOLD           Stops the counter 
  */ 
 // Stop watchdog timer first 
  WDTCTL = WDTPW + WDTHOLD;
 // WDTCTL = WDTPW + WDTCNTCL+WDTTMSEL; // interval mode, smCLK/32768 =~ 4.096 ms at 8mhz SMCLK
  WDTCTL = WDT_ADLY_250;
 // not using the wathdog timer 
 // IE1 |= WDTIE  ; // enable the maskable interrupt for the watchdog interval timer 


  // Port1 setup 
  P1SEL = 0;     // all IO 
  P1DIR = 0x0f;  // Set P1.0..3 to output direction, P14..7 is input
  P1REN = 0xf0;  // P4..7 resistor enable 
  P1OUT = 0xf0;  // P4..7 high so that pullup 
    
  // setup the internal clock for the calibrated 16 Mhz
  DCOCTL = CALDCO_16MHZ;
  BCSCTL1 = CALBC1_16MHZ;
  BCSCTL2 = DIVS_1;  // SMCLK = MCKL/2 =~ 8Mhz
  //   BCSCTL2 = 0 after reset SMClk and MClk dividers at x1 , DCO used for both 
  //   BCSCTL3 = 5 after reset, deals with Xtal osc - Not applicable 
  
  // Configure P1.4..7 to generate an interrupt on High to low edge
  P1IES = 0x00;  // P4..7 interrupt low to high edge select
  P1IE  = 0xf0;  // P4..7 interrupt enable
  P1IFG = 0x00;  // clear interrupt flag 
 
  _BIS_SR(GIE) ; // enable interrups 
 

  while(1)
  {

  }
    ; //park;

}
