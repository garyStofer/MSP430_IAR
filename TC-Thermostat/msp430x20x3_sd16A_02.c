#include  <msp430x20x3.h>
#define AVERAGE_COUNT 32
// transmit Command address data pair for 7301 port expander , make sure the MSB 
// of the command byte is not set when calling for a write.
void
SPI_send( unsigned char command, unsigned char data )
{
    
    USISRH = command ;        // MSB = 0 == write for 7301 14..8 = Command 
    USISRL = data;            // LSB = data to be written
    P1OUT &= ~0x1 ;           // set CS active (low) 
    USICNT = USI16B+16;       // start sending 16 bits -- 16Bit Mode set
    
    while(!( USICTL1 & USIIFG)) // poll USI interrupt flag until transmission done 
      ;
    P1OUT |= 0x1;               // clear the CS for 7300 -- latches the data 
}



unsigned char 
SPI_read(unsigned char command)
{
  
    command |= 0x80;  // MSB = 1 == read for 7301 14..8 = Command 
    SPI_send( command, 0 ); // latch the read address with the read request bit set
    SPI_send( 0, 0 );       // Clock out the data for the latched address
   
    return USISRL;
}


// Init USI for 16 bit SPI use in master mode. 
// Interrupts are not used, transmitting code polls the interrupt flag during transmissions.
// Clock phase is selected so that raising edge is centered in SDO
// Clock frequency is selected to be 1/2 of SMCLK  -- SMCLK is controlled outside this 
// function through BCSCTL2 

void
SPI_Init( void )  // based on a 16Mhz MCLK and 8Mhz SMCLK !
{
  // Universal Serial Interface configuration 
  USICTL0 = USIPE5+USIPE6+USIPE7+USIMST+USISWRST+USIOE ; // PortIOs & USI Master mode, Hold in Reset, Enable IOs 
  USICTL1 = USICKPH;                              // No  USI interrupts, Set clock phase  
  USICKCTL = USIDIV_1+USISSEL_2 ;                 // Setup USI clocks: SCL = SMCLK/2 == 8 Mhz/2 ~= 4Mhz 
  USICTL0 &= ~USISWRST;                           // Enable USI
  USICTL1 &= ~USIIFG;                             // Clear pending flag
}


// Coefficients for inverse polinomial to compute temp from voltage. 

// temperature  = d_0 + d_1*E  + d_2*E^2 + ... + d_n*E^n 
// where temp is in deg C, E is in mv
// 
// J-Type coeff
/*
float TC_coeff[] = {  0.0, // d0
                     1.978425E+01, // d1
                    -2.001204E-01, // d2
                    1.036969E-02 , // d3
                    -2.549687E-04, // d4
                     3.585153E-06, // d5
                    -5.344285E-08, // d6
                     5.099890E-10  // d7
                  };
*/
// K-Type coeff
float TC_coeff[] = {  0.0, // d0
                     2.508355E+1, // d1
                     7.860106E-2, // d2
                    -2.503131E-1, // d3
                     8.315270E-2, // d4
                    -1.228034E-2, // d5
                     9.804036E-4, // d6
                    -4.413030E-5  // d7
                  };
float TC_Temp;

float Temp;
float C_Temp;
char avg_cnt;
float T_avg = 0;

#pragma vector=SD16_VECTOR
__interrupt void SD16ISR(void)
{ 
   float E;
   P1OUT = 0;
  // Vtemp = ((int) SD16MEM0 - ADC_offs) * (1.2 / 0xffff)  ;
  // Vtemp /= 1.32e-3; // the Temp Coefficient per deg/K
    // therefore :
  // Vref / maxcount / TempCoeff = ScaleFactor  
  // 1.2V / 65532    / 1.32e-3   = 13.87183e-3  
 
  if (SD16INCTL0 == SD16INCH_6 )  // internal Temp sensor -- Cold Junction temp
  {
    C_Temp = ((int) SD16MEM0 ) * 13.87183e-3;
    // calibration (op-amp) offset is taken out via the number below
    C_Temp -= 270;     // convert to deg/C (-273) and  PGA offset 
  
    
    // calc TC cold junction emf equivalent from Vtemp so it can be added to TC voltage
 // not done yet, might not have enough flash to hold an other 5th degree poliniomial and coefficients 
 // so for now, just using the temp and add it to the temp calculated from  the TC -- slight error because 
 // we are not exactly in the right place of the curve 
   
    // differential input on P1.2 / P1.3  
    SD16AE =  SD16AE2 | SD16AE3; // Analog enable for P1.2 and P1.3  
    SD16INCTL0 = SD16INCH_1 |SD16GAIN_32;  ; // switch to A1 and gain 32 for next time through 
    return;
  }
  // else 
  
  // E = ((int) SD16MEM0 - ADC_offs) * (1.2 / 0xffff)  ;  // multiply by bit value 
  // E /= 32.00 ;   // divide by gain 
  // E *= 1000 ;   // make E in mV
  
   E = ((int) SD16MEM0 ) * 5.722133211e-4;

// temperature  = d_0 + d_1*E  + d_2*E^2 + ... + d_n*E^n 
   TC_Temp = E * TC_coeff[1];
   TC_Temp += E*E * TC_coeff[2] ;
   TC_Temp += E*E*E * TC_coeff[3] ;
   TC_Temp += E*E*E*E * TC_coeff[4] ;
   TC_Temp += E*E*E*E*E * TC_coeff[5] ;   // calc to the fifth degree polinomial
   TC_Temp += E*E*E*E*E*E * TC_coeff[6] ;
   TC_Temp += E*E*E*E*E*E*E * TC_coeff[7] ; 
   
   // remove offset here
   // J_TEMP += offset ;
   TC_Temp -= 2.0;  // adc offset compensation
  // TC_Temp *= 1.085; // PGA gain compensation 
   Temp = C_Temp + TC_Temp;
  
   Temp *=1.08;
   SD16INCTL0 = SD16INCH_6; // go back to measure internal temp again 
   SD16AE = 0;              // remove Analog input enable for P1.2 and P1.3 -- makes internal temp meas unstable 
   
   // average multiple readings before sending out 
/*   T_avg += Temp;
   if (avg_cnt++ >=1 )
   {
     Temp = T_avg/avg_cnt  * 1.08;  
     T_avg = 0;
     avg_cnt = 0;
   }
   */
}

// Watchdog Timer interrupt service routine
#pragma vector=WDT_VECTOR
__interrupt void watchdog_timer(void)
{
//  P1OUT = 1;
  SD16CCTL0 |= SD16SC;                      // Start SD16 conversion
}
 


void main(void)
{
   DCOCTL = CALDCO_16MHZ;
   BCSCTL1 = CALBC1_16MHZ;
   BCSCTL2 = DIVS_3;           // SMCLK = MCKL/8 =~ 2Mhz


  WDTCTL = WDT_MDLY_32;                     // WDT Timer interval --  4ms 
  IE1 |= WDTIE;                             // Enable WDT interrupt
  P1DIR = 0xff;                            // P1.0 to output direction
  P1OUT = 0;

  SPI_Init();
  SPI_read(0x45);
  
  // 1.2V ref, SMCLK
  SD16CTL = SD16REFON +SD16SSEL_1 + SD16DIV_3;        // 1.2V ref, SMCLK,   SMCLK /8

  SD16CCTL0 = SD16SNGL + SD16IE + SD16DF ;//  + SD16OSR_512 ;   // Single conv, interrupt -- 2's complement 
 
  SD16INCTL0 = SD16INCH_6;                  // initial internal temp
 
  _BIS_SR(GIE +LPM0_bits );                 // Enter LPM0 with interrupt
}
