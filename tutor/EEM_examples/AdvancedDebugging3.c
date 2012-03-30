//******************************************************************************
//  MSP-FET430F16x - Breakpoint on write access to Flash
//
//  Description;  This demo shows how a breakpoint can be set
//  to stop the program execution if the program tries to write
//  data to the flash.
//
//  Setup Breakpoint:
//  - Open the Breakpoint menu (View | Breakpoints)
//  - In the Breakpoints window, right-click and to open the context menu.
//  - On the context menu, chose New Breakpoint | Range...
//    - Start: 0x1000
//    - Range Delimiter: End Value
//                       0xFFFF
//    - Type : Address
//    - Access : Write
//    - Action : Break
//    - Action when: Inside Range
//    Close the dialog by clicking OK and execute the program
//    -> Run
//    -> the CPU will stop after the first write access to the
//       Flash ( = 'wDataInFlash++' instruction)
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

__no_init unsigned int wDataInFlash @ 0xF000;       // locate in Flash

// Function Prototypes
void Delay(unsigned int Value);

void main(void)
{
  unsigned int i = 0;

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
     i++;
     if ( i == 50)
     {
        wDataInFlash++;       // this should create a Break - no Write in Flash
                              // allowed
        _NOP();               // This NOP is used to show that it really was
                              // 'wDataInFlash++;' that caused the break
                              // - PC stops here
     }
     if ( i == 100)
     {
        i = 0;
     }
  }
}

void Delay(unsigned int Value)
{
   volatile unsigned int j = 0;

   for(j=Value; j>0; j--);
}
