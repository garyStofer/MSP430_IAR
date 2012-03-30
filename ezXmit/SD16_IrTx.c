#include <msp430x20x3.h>


// F2013 revision B is used
// For silicon revision C or later
//    comment out the next line
// #define HAS_SDA2_BUG
//DEFC (SDA2_BUG_FIX, 0xBF)

// When BCL9 bug is fixed by TI
//    comment out the next line
#define HAS_BCL9_BUG

//      MSP430F2013
//    ---------------
//   |               |
//   |          P2.6 |---->|----^^^^---Gnd
//   |      (or P1.2)|  IR-LED  330R
//   |      (or P1.6)|
//   |               |   100nF (optional)
//   |          P1.3 |----||----Gnd
//   |         (Vref)|



// # of SMCLK for each bit at 9600b/s
#define BIT_CLKS (104)
// # of SMCLK for each IR pulse
#define PULSE_CLKS (2)

// Global variable, visible to ISR
__no_init int IR_tmp;

void IR_Tx(char dat8)
// Transmit IR at 9600b/s
// First xmit a "start bit"
// Followed by 8 bits of <dat8>
//    LSB first, MSB last
// End with inactive IR ("stop bit")
{
    IR_tmp = dat8 | 0x0300;
    TACCR0 = BIT_CLKS-1;
    TACCTL0 = CCIE;
    TACCR1 = BIT_CLKS-PULSE_CLKS;
    TACCTL1 = OUTMOD_3;    // set-reset mode
    TACTL = TASSEL_2|MC_1|TACLR;
    _BIS_SR(LPM0_bits|GIE);// let ISR do the rest
    TACTL = 0;
    TACCTL0 = 0;
    TACCTL1 = 0;
}

#pragma vector=TIMERA0_VECTOR
__interrupt void IR_ISR (void)    // Timer A0 ISR for IR_Tx()
{
    if (IR_tmp&BIT0)
      TACCTL1 = OUTMOD_5;  // reset mode
    else
      TACCTL1 = OUTMOD_3;  // set-reset mode
    
    if ((IR_tmp = IR_tmp>>1)==0)
      _BIC_SR_IRQ(LPM0_bits|GIE);
}


void IR_Packet(char n, short dat16[])
{
  char i;
  unsigned char * bytes = (unsigned char * ) dat16;

  IR_Tx(0xC0);           // BOF
  for (i=0; i<n*2; i++)
  {
      IR_Tx(bytes[i]); 
  }
  IR_Tx(0xC3);           // EOF
}


int Get_Sd16(int inctl0)
{
  SD16CTL = SD16REFON|    // Internal 1.2V Ref ON
            SD16SSEL_1;  // SMCLK clock sel for ADC  
  
  SD16INCTL0 = inctl0;  // input selection -- gain == 1 , 4th conversion causes interrupt
  
  if ( inctl0 == SD16INCH_4)
    SD16AE =  SD16AE4;      // Analog input enable for P1.1  == A4+ , A4- internally connected to VSS
  else
    SD16AE = 0;             // no external analog inputs 
  
  SD16CCTL0 = SD16OSR_512|   // oversampling rate 512
              SD16SNGL|     // Single Conversion mode
              SD16DF|       // Data Format 1 == Two's complement
              SD16IE|       // Interrupt enable
              SD16SC;       // Start Conversion
  
  _BIS_SR(LPM0_bits|GIE); // sleep until conversion is done and iterrupt wakes the cpu up again
  return (SD16MEM0);
}

#pragma vector = SD16_VECTOR
__interrupt void SD16ISR(void)
{
  _BIC_SR_IRQ(LPM0_bits|GIE);  // change the power mode on the stack so that device keeps running after a 
                               // ADC interrupt 
}

#pragma vector=TIMERA1_VECTOR
__interrupt void Timer_A1(void)
{
  switch(TAIV)
  {
    case 2:  // TACCR1 CCIFG
      break;
    case 10: // TAIFG
      _BIC_SR_IRQ(LPM3_bits|GIE);
      break;
  }
}

void init(void)
// Set ACLK = VLO/2 = ~6kHz
// Set MCLK = SMCLK = DCO = 1MHz
// Set all I/O pins to out low
// Work around silicon bugs
// Select 1 of the 3 alternate pins
//  for TACCTL1 to control IR_Tx
//  Enable the optional external
//  bypass cap for Vref if any
{
    WDTCTL = WDTPW + WDTHOLD;    // No watch dog 
  
    BCSCTL3 = LFXT1S_2;
    BCSCTL1 = CALBC1_1MHZ|DIVA_1;
    DCOCTL = CALDCO_1MHZ;
   
    P1OUT = 0;
    P1DIR = 0xFF;
   
    P2OUT = 0;
    P2DIR = 0xFF;
    
    TACTL = 0;
    TACCTL0 = 0;
    TACCTL1 = 0;
    
    P1SEL |= BIT3;  // Port 1 connections -- connect Vref to p1.3 for capacitor bypass
    
    
#ifdef HAS_BCL9_BUG
    BCSCTL2 = SELM_1|DIVM_3;
    BCSCTL2 = 0;
#endif

#ifdef HAS_SDA2_BUG
    SDA2_BUG_FIX = 0x31;
#endif
  
    P2SEL = BIT6;  // makes timer PW signal appear on P2.6  for IR LED
  
} 
   

#pragma segment="CODE"
#define FLASH_END (unsigned short *) 0xFFE0  
  
void main( void )
{
  short result[4];
  unsigned short * flash;
  
  init();

  flash =   (unsigned short *) __segment_end("CODE"); 

  FCTL1 = FWKEY + WRT;            // Make flash writable
  FCTL2 = FWKEY + FSSEL_3 + FN2;  // Use SCLK, / 4
  FCTL3 = FWKEY ;  
  
  // initial signature to find it easy in debugger
  *flash++ = 0xcafe;
  *flash++ = 0x1eaf;
    
  //flash_space = FLASH_END - (unsigned short) flash_begin; 
  
  while (1)
  {
    //  Read Offset, Temperature, & Vcc
    result[0] = Get_Sd16(SD16INCH_7); // offset
    result[1] = Get_Sd16(SD16INCH_6); // temp
    result[2] = Get_Sd16(SD16INCH_5); // volt
   
    // result[3] = Get_Sd16(SD16INCH_4); // external input   
    //  Turn OFF Vref until next time
    SD16CTL = 0;
       
    if (flash < FLASH_END )
    {
   //   *flash++ = result[0];
      *flash++ = result[1];
   //    *flash++ = result[2];
    }
    //  Send results out
    IR_Packet(3, result);

    //  Set up TA for wakeup
    TACCR0 = 5000;
    TACTL = TASSEL_1|MC_1|TACLR|TAIE;

    //  Go to sleep for a while
    _BIS_SR(LPM3_bits|GIE);

    // when here we are back from timer A sleep

//  Disable TA
    TACTL = 0;
    TACCTL0 = 0;
    TACCTL1 = 0;
  }
}


