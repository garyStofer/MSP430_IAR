//******************************************************************************
//  MSP-FET430F16x - Breakpoint on Variable write
//
//  Description; This demo shows how a breakpoint can be set to a certain
//  Variable.
//
//  Setup Breakpoint:
//  - Open the Breakpoint menu (View | Breakpoints)
//  - In the Breakpoints window, right-click and to open the context menu.
//  - On the context menu, chose New Breakpoint | Conditional...
//    - Breakpoint at: wLoopCounter
//    - Type: Address Bus
//    - Operator : ==
//    - Access : Write
//    - Action : Break
//    Other fields: unchanged
//    Close the dialog by clicking OK and execute the program
//    -> Run
//    -> the CPU will stop on the first write access to the
//       variable wLoopCounter
//
//  Setup Breakpoint to stop on a write to the variable:
//  Setup Breakpoint:
//  - Modify the previously set breakpoint:
//    - Condition - MDB Value : 50
//    - Condition - Operator : ==
//    - Condition - Access : Write
//    Close the dialog by clicking OK, Reset the program and execute the program
//    -> Run
//    -> the CPU will stop after the value 50 has been written to the
//       Variable wLoopCounter
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
//  Texas Instruments, Inc
//  June 2004
//  Built with IAR Embedded Workbench Pro v3.20
//
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
