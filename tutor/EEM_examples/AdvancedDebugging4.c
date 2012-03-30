//******************************************************************************
//  MSP-FET430F16x - Trigger Sequencer
//
//  Description;  This demo shows how the State Storage
//  can be used for an instruction trace to see that last
//  eight instructions the CPU has executed
//
//
//  Setup State Storage:
//  - open the State Storage Control menu (Emulator | State Storage Control)
//    - Select: Enable state Storage
//    - Select: Buffer warp around
//    - Trigger action: None
//    - Storage action on : Instruction Fetch
//
//  Open the State Storage Window (Emulator | State Storage Window)
//  Execute the Program -> Go
//  Stop the Program -> Break
//  -> The State Storage window displays the last 8 instructions
//     which have been executed by the CPU
//
//  Note: Try this with Example 2, to see where the function which causes
//        the stack overflow was called.
//
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
