
#include "msp430x20x3.h"
#include "stdio.h"

// Define the bits for the 4 switches 
#define SW_FWD 0x80
#define SW_REV 0x40
#define SW_FAST 0x20
#define SW_SLOW 0x10

#define MAX_RATE 5000  // motor can not go faster than this
#define MIN_RATE 60000

// Tables that contain the stepp sequence 
/*
static const unsigned char fullstepHalfPower[] =
{
  0x1, 0x4, 0x2, 0x8, 0x1, 0x4, 0x2, 0x8
};
static const unsigned char fullstepFullPower[] =
{
  0xa, 0x9, 0x5, 0x6, 0xA, 0x9, 0x5, 0x6
};
*/
static const unsigned char half_step[] = 
{
  0xA, 0x8, 0x9, 0x1, 0x5, 0x4, 0x6, 0x2
};

// useing a strcut for bitfield with a 3 bit with so that the code can just
// increment or decrememt without having to check for overflow/underflow
struct { unsigned ndx :3; }vector;

// WDT Interrupt Service Routine
//
#pragma vector=WDT_VECTOR
__interrupt void wdt_ISR(void)
{
    static unsigned int entered =0;
    entered++;
}
//
// PORT 1 Interrupt Service Routine
//
#pragma vector=PORT1_VECTOR
__interrupt void port1_ISR (void)
{
}


//
// Timer_A Initialization
//
static unsigned int theRate ;
void timerA_Init(unsigned int rate)
{
  theRate = rate;
  // Initialize Compare Register 0
  TACCR0 = theRate;

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
  TACTL = TASSEL_2 + MC_1 + TACLR;
}

//
//  Timer A0 Interrupt Service Routine
//
#pragma vector=TIMERA0_VECTOR
__interrupt void timerA0_ISR(void)
{
  unsigned char c ;
  static unsigned char motor_off = 0;
  
  c = ~P1IN; // Switch pulls low, so invert 
 
  if (c & SW_FWD )  // go one way 
  {
    vector.ndx++;
    motor_off = 0;
  }
  if (c & SW_REV ) // or the other way -- Both buttons pressed cancels each other for no movement
  {
    vector.ndx--;
    motor_off = 0;
  }
  // set motor coils 
  if (! motor_off )
   P1OUT = half_step[vector.ndx] |0xf0; // keep upper nibble on for pull up resistors
  else
   P1OUT = 0xf0;
  

  // change the rate of stepps 
  if ( c & SW_FAST || c & SW_SLOW )
  {
    if ( c & SW_FAST )
    {
      if (theRate > MAX_RATE) // limit -- too fast 
      {
        theRate-= theRate/1000;
        TACCR0 = theRate;
      }
    }
    else
    {
      if (theRate < MIN_RATE)
      {
        theRate += (theRate/1000);
        TACCR0 = theRate;
      }
    }
  }
  
  if ((c & SW_FAST ) && (c & SW_SLOW ))  // both switches together mean to release the motor coil
    motor_off = 1;
}


void main(void)
{
 // unsigned char c,x;
 // int u ;
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
  WDTCTL = WDTPW + WDTCNTCL+WDTTMSEL; // interval mode, smCLK/32768 =~ 4.096 ms at 8mhz SMCLK

  // not using the wathdog timer 
 // IE1 |= WDTIE  ; // enable the maskable interrupt for the watchdog interval timer 


  // Port1 setup 
  P1SEL = 0;     // all IO 
  P1DIR = 0x0f;  // Set P1.0..3 to output direction, P14..7 is input
  P1REN = 0xf0;  // P4..7 resistor enable 
  P1OUT = 0xf0;  // P4..7 high so that pullup 
    
  // Configure P1.4..7 to generate an interrupt on High to low edge
  // P1IES = 0xf0;  // P4..7 interrupt high to low edge select
  // P1IE  = 0x00;  // P4..7 interrupt enable
  // P1IFG = 0x00;  // clear interrupt flag 

  // setup the internal clock for the calibrated 16 Mhz
  DCOCTL = CALDCO_16MHZ;
  BCSCTL1 = CALBC1_16MHZ;
  BCSCTL2 = DIVS_1;  // SMCLK = MCKL/2 =~ 8Mhz
 //   BCSCTL2 = 0 after reset SMClk and MClk dividers at x1 , DCO used for both 
 //   BCSCTL3 = 5 after reset, deals with Xtal osc - Not applicable 
  // start at 0 index
  
  
  vector.ndx = 0;
  timerA_Init( 12000 );  
  
  _BIS_SR(GIE) ; // enable interrups 
 

  while(1) ; //park;

}
