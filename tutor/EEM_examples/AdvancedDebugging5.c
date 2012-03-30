//******************************************************************************
//  MSP-FET430F16x - Trace on Variable write
//
//  Description; This demo shows how a variable can
//  be watched in real time, without stoppping the CPU.
//
//  Setup Breakpoint / Trigger:
//  - Open the Breakpoint menu (View | Breakpoints)
//  - In the Breakpoints window, right-click and to open the context menu.
//  - On the context menu, chose New Breakpoint | Conditional...
//    - Breakpoint at: wLoopCounter
//    - Type: Address Bus
//    - Operator : ==
//    - Access : Write
//    - Action : State Storage Trigger
//        Note: Do not check: Break !
//    Other fields: unchanged
//    Close the dialog with OK
//
//  Setup State Storage:
//  - open the State Storage Control menu (Emulator | State Storage Control)
//    - Select: Enable state Storage
//    - Select: Buffer warp around
//    - Storage action on : Triggers
//
//  Execute the Program -> Go
//  Open the State Storage Window (Emulator | State Storage Window)
//  -> push the update Button
//  -> the Trace Buffer inside the MSP430 is read and display
//  -> the information written to the variable wLoopCounter
//     could be displayed without stopping or influencing the program
//     execution
//
//
//                MSP430F149
//             -----------------
//         /|\|              XIN|-
//          | |                 |
//          --|RST          XOUT|-
//            |                 |
//            |             P1.0|-->LED
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
