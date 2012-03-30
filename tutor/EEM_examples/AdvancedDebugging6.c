//******************************************************************************
//  MSP-FET430F16x - Trigger Sequencer
//
//  Description; After a delay the function 'DummyStackFill' is called
//  which places 100 Bytes on the Stack
//
//  Setup Triggers for Sequencer:
//  -right-click 'wLoopCounter++;' (line 75)
//    -> select 'Toggle Breakpoint (Conditional)'
//  - right-click 'DummyStackFill();' (line 104)
//    -> select 'Toggle Breakpoint (Conditional)'
//
//  Setup the Trigger Sequecer (Emulator | Sequecer Control)
//  - Enable sequencer
//  - Action: Break
//  - Transition Trigger 0: Trigger on line 104 (or higher value)
//  - Transition Trigger 1: Trigger on line 75  (or lower value)
//  - Transition Trigger 2: byass -->
//  - Click 'Apply'
//
//    Execute the program -> Run
//    -> the CPU will stop only when the instruction 'wLoopCounter++;' is
//       executed after the function DummyStackFill() was called.
//       This means that the led blinks several times before the execution
//       stops.
//       Note: the instruction 'wLoopCounter++;' is executed several times
//             before the break stops the program execution
//
//                   MSP430
//             -----------------
//         /|\|              XIN|-
//          | |                 |
//          --|RST          XOUT|-
//            |                 |
//            |             P1.0|-->LED (169)
//            |             P5.1|-->LED (449)
//
//
//
//  Texas Instruments, Inc
//  June 2004
//  Built with IAR Embedded Workbench Pro v3.20
//
//******************************************************************************


#include  <msp430x16x.h>

#define TRUE  (1==1)
#define FALSE (!TRUE)

// Variable Defintions

volatile unsigned int wLoopCounter = 0;             // Processing Loop Counter

// Function Prototypes
void Delay(unsigned int Value);

void main(void)
{
  WDTCTL = WDTPW + WDTHOLD;   // Stop watchdog timer
  P1OUT  = 0x00;              // init output
  P1DIR |= 0x01;              // Set P1.0 to output direction
  P5OUT  = 0x00;              // init output
  P5DIR |= 0x02;              // Set P5.1 to output direction

  TACTL = TASSEL_2 + MC_1;    // Run to CCR0 with SMCLK
  TACCR0 = 1000;              //
  TACCTL0 = CCIE;             // Enable CCR0 int.

  _EINT();                    // Enable interrupts

  while(TRUE)
  {
     wLoopCounter++;
     P1OUT ^= 0x01;           // Toggle P1.0 using exclusive-OR
     P5OUT ^= 0x02;           // Toggle P5.1 using exclusive-OR
     Delay(10000);
  }
}

void Delay(unsigned int Value)
{
   volatile unsigned int j = 0;

   for(j=Value; j>0; j--);
}


void DummyStackFill(void)
{
       volatile unsigned int k[100]=0; // Local Variable is stored on the Stack
                                       // -> 100 words are pushed to the Stack
}


#pragma vector=TIMERA0_VECTOR
__interrupt void isr_TACCR0()
{
   static   unsigned int j=0;    // Entrance counter
   j++;
   if (j>5000)
   {
       DummyStackFill();
       j = 0;
   }
}

