#include  <msp430x20x3.h>
#define J_TYPE 1 

#define RESET_2515  0xc0  // Reset Command byte for MCP 2515 
#define READ_2515 0x3   // READ command byte for MCP 2515 CAN BUS controller
#define WRITE_2515 0x2  // Write command byte for mcp 2515 
#define READ_STAT  0xA0  // Read status command byte ..
#define RX_STAT_2515    0xB0  // Read reciver status 
#define RTS_TXb0_2515   0x81  // Request to send for TX buffer 1
#define RTS_TXb1_2515   0x82  // Request to send for TX buffer 2
#define RTS_TXb2_2515   0x83  // Request to send for TX buffer 3
#define LOAD_TXBUF_0_   0x40  // Loading TX Buffer Satrting at at IDH (memmap addr 0x31)
#define LOAD_TXBUF_0    0x41  // Loading TX Buffer Satrting at at D0  (memMap addr 0x36)
#define LOAD_TXBUF_1_   0x42  // Loading TX Buffer Satrting at at IDH (memmap addr 0x41)
#define LOAD_TXBUF_1    0x43  // Loading TX Buffer Satrting at at D0  (memMap addr 0x46)
#define LOAD_TXBUF_2_   0x44  // Loading TX Buffer Satrting at at IDH (memmap addr 0x51)
#define LOAD_TXBUF_2    0x45  // Loading TX Buffer Satrting at at D0  (memMap addr 0x56)

#define CAN_INTE     0x2b
#define CAN_INTF     0x2c
#define TX_B0_CNTRL  0x30
#define TX_B0_SIDH   0x31
#define TX_RTS_CNTRL 0xd
#define CAN_CNTL     0xf
#define CAN_STAT     0xe
#define CNF3         0x28
#define CNF2         0x29
#define CNF1         0x2A
#define EFLG         0x2d
#define T_EC         0x1c // transmit error counter
#define R_EC         0x1d // receive error counter

void 
SPI_Tx1( unsigned char cmd)
{
  
      // send the command 
    USISRL = cmd;
    USICNT = 8;               // start sending 16 bits -- 8 Bit Mode 
    while(!( USICTL1 & USIIFG)) // poll USI interrupt flag until transmission done 
      ;
}

void
SPI_Tx2( unsigned char data1, unsigned char data2)
{
    //send 2 bytes read one byte
    USISRH = data1 ;             // MSB = 0 == write     14..8 = Command 
    USISRL = data2;              // LSB = data to be written 
    USICNT = USI16B+16;          // start sending 16 bits -- 16Bit Mode set
    
    while(!( USICTL1 & USIIFG)) // poll USI interrupt flag until transmission done 
      ;
}
void 
WriteSC( unsigned char commnad)
{  
  P2OUT &= ~0x40 ;           // set CS active (low) 
  SPI_Tx1( commnad);
  P2OUT |= 0x40;             // CS inactive
}
void 
WriteSB(unsigned char addr, unsigned char data)
{
  P2OUT &= ~0x40 ;         
  SPI_Tx2( WRITE_2515,addr);// CANCTRL
  SPI_Tx1(data);            // Configuration mode ABORT
  P2OUT |= 0x40; 
}

unsigned char
ReadSB(unsigned char addr)
{
  P2OUT &= ~0x40 ;      
  SPI_Tx2(READ_2515,addr);    
  SPI_Tx1( 0x0);          // clock in 8 bits
  P2OUT |= 0x40;  
  return USISRL;
}
// Init USI for 16 bit SPI use in master mode. 
// Interrupts are not used, transmitting code polls the interrupt flag during transmissions.
// Clock phase is selected so that raising edge is centered in SDO
// Clock frequency is selected to be 1/2 of SMCLK  -- SMCLK is controlled outside this 
// function through BCSCTL2 

void
SPI_Init( void )  // based on a 16Mhz MCLK and 2Mhz SMCLK !
{
  // Universal Serial Interface configuration 
  USICTL0 = USIPE5+USIPE6+USIPE7+USIMST+USISWRST+USIOE ; // PortIOs & USI Master mode, Hold in Reset, Enable IOs 
  USICTL1 = USICKPH;                              // No  USI interrupts, Set clock phase  
  USICKCTL = USIDIV_0+USISSEL_2 ;                 // Setup USI clocks: SCL = SMCLK = 2mHZ
  USICTL0 &= ~USISWRST;                           // Enable USI
  USICTL1 &= ~USIIFG;                             // Clear pending flag
  SPI_Tx1(0);                                     // get the interface started -- nobody is listening
}

void main(void)
{
  WDTCTL = WDT_MDLY_32;                     // WDT Timer interval --  4ms 
  IE1 |= WDTIE;                             // Enable WDT interrupt
  P1DIR = 0xff;                            // P1 all to output direction CSn for 2515
  P1OUT = 0x00;                            // set to
  P1OUT = 0x0;

  P2SEL = 0;                                // select IO 
  P2DIR = 0xff;                             // P2 all output
  P2OUT = 0x40;                             // P2.6 high for CSn on 2515

  SPI_Init();
  
  WriteSC(RESET_2515);   // The reset function takes some time in the chip -- placed here so that it executes before 
                          // switching the clock to 16Mhz 
   
  // Let the Clock run slow up to here so that the 2515 can initialize first ( 128 clocks at 8mhz)
  DCOCTL = CALDCO_16MHZ;
  BCSCTL1 = CALBC1_16MHZ;
  BCSCTL2 = DIVS_3;           // SMCLK = MCKL/8 =~ 2Mhz
  
  // set the BAUDrate and Bit-time counters
  // SETUP for BaudRate 250Kbd at 8mhz xtal start addr 0x28 = CNF3
  // TQ = 2*(BRP=1)/FOsc, = 2*(0+1)/8mhz = 250nS. At 16TQ per Bit BAUD = 250Kd
  WriteSB(CNF3,0x87);              // CNF3  CLCKOUT/SOF = SOF, PHSeg2 = 7+1 == 0x87
  WriteSB(CNF2,0xA1);              // CNF2: BLTMODE=1,PHSEG1=4+1, PROPSEG= 1+1;
  WriteSB(CNF1,0x41);              // CNF1: SJW=1 (2TQ),BRP=0 // Slowed down the baudrate to 125kbd
  
 
  WriteSB( CAN_CNTL,0x08);            // Normal mode , ONE-SHOT  mode -- no retries

  // prepare a CAN gram 
  P2OUT &= ~0x40 ;       
  SPI_Tx2( WRITE_2515,TX_B0_SIDH);   // Set address for TXbuf0
  SPI_Tx2( 0xaa,0xa0);          // SIDH, SIDL
  SPI_Tx2( 0x00,0x00);         // EID8, EID0
  SPI_Tx1( 0x0);         // Not RTR, datalength in chars = 0
  SPI_Tx2(0xaa,0xaa);        // the data to send  
  SPI_Tx2(0xaa,0xaa);        // the data to send 
  SPI_Tx2(0xaa,0xaa);        // the data to send 
  P2OUT |= 0x40;    
  
  ReadSB(TX_B0_CNTRL);
  WriteSB(CAN_INTF,0);
  WriteSB(TX_B0_CNTRL, 0x8);  // or send single byte command code instead 

  ReadSB(TX_B0_CNTRL);// x50 = ABTF and TXERR
  ReadSB(CAN_INTF);   // MERRE Message error interrupt = 80 
  ReadSB(EFLG);       // 0x15 = TRANSMIT Error passive Flag, TXWARN and EWARN
  ReadSB(T_EC);
  ReadSB(R_EC);
  
  // 1.2V ref, SMCLK
  SD16CTL = SD16REFON +SD16SSEL_1 + SD16DIV_3;        // 1.2V ref, SMCLK,   SMCLK /8

  SD16CCTL0 = SD16SNGL + SD16IE + SD16DF ;//  + SD16OSR_512 ;   // Single conv, interrupt -- 2's complement 
 
  SD16INCTL0 = SD16INCH_6;                  // initial internal temp
 
  _BIS_SR(GIE +LPM0_bits );                 // Enter LPM0 with interrupt
}
// Coefficients for inverse polinomial to compute temp from voltage. 

#ifdef J_TYPE
// temperature  = d_0 + d_1*E  + d_2*E^2 + ... + d_n*E^n 
// where temp is in deg C, E is in mv

#define N_poly  7
float TC_coeff[N_poly+1] = {  0.0, // d0
                     1.978425E+01, // d1
                    -2.001204E-01, // d2
                    1.036969E-02 , // d3
                    -2.549687E-04, // d4
                     3.585153E-06, // d5
                    -5.344285E-08, // d6
                     5.099890E-10  // d7
                  };
#else   // K-Type coeff
#define N_poly 7
float TC_coeff[N_poly+1]= {  0.0, // d0
                     2.508355E+1, // d1
                     7.860106E-2, // d2
                    -2.503131E-1, // d3
                     8.315270E-2, // d4
                    -1.228034E-2, // d5
                     9.804036E-4, // d6
                    -4.413030E-5, // d7
                  };
#endif
float TC_Temp;
float Temp;
float C_Temp;
float E;
int adc;

float
pow( float x, int n)
{
    float acc = x;
    
    while(--n )
      acc = acc * x;

    return acc;
}

#pragma vector=SD16_VECTOR
__interrupt void SD16ISR(void)
{ 
      /* calc of ADC scale factor for internal temp sensor
      Vtemp = ((int) SD16MEM0 - ADC_offs) * (1.2 / 0xffff)  ;
      Vtemp /= 1.32e-3; // the Temp Coefficient per deg/K
      therefore :
      Vref / maxcount / TempCoeff = ScaleFactor  
      1.2V / 65532    / 1.32e-3   = 13.87183e-3  */

    if (SD16INCTL0 == SD16INCH_6 )  // internal Temp sensor -- Cold Junction temp
    {
      C_Temp =  ((int)SD16MEM0 ) * 13.87183e-3;
      // calibration (op-amp) offset is taken out via the number below
      C_Temp -= 268.80;     // convert to deg/C (-273) and  cal out PGA offset 
    
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
    
    /* calc of ADC scale factor 
     E = ((int) SD16MEM0 - ADC_offs) * (1.2 / 0xffff)  ;  // multiply by bit value 
     E /= 32.00 ;   // divide by gain 
     E *= 1000 ;   // make E in mV   Therefore: ADCcount *  5.722133211e-4; */
    
    // remove offset 
     adc = ((int) SD16MEM0 );
     adc+=60; //  PGA offset
     E = adc * 5.722133211e-4;
     E *= 1.10; // PGA gain error ? 
     TC_Temp =0;
   
    // temperature  = d_0 + d_1*E  + d_2*E^2 + ... + d_n*E^n 
    //TC_Temp = E * TC_coeff[1];
    //TC_Temp += E*E * TC_coeff[2] ;
    //TC_Temp += E*E*E * TC_coeff[3] ;
    //TC_Temp += E*E*E*E * TC_coeff[4] ;
    //TC_Temp += E*E*E*E*E * TC_coeff[5] ;   // calc to the fifth degree polinomial for J-type
    //TC_Temp += E*E*E*E*E*E * TC_coeff[6] ;
    //TC_Temp += E*E*E*E*E*E*E * TC_coeff[7] ; 
    
    // starting at 1 -- index 0 -- offset is always 0 
    for (int i = 1; i <= N_poly; i++ )
    {
      TC_Temp += pow(E,i) * TC_coeff[i];
    }
    
    Temp = C_Temp + TC_Temp;
    
    SD16INCTL0 = SD16INCH_6; // ADC input -- go back to measure internal temp again 
    SD16AE = 0;              // remove Analog input enable for P1.2 and P1.3 -- makes internal temp meas unstable 

}

// Watchdog Timer interrupt service routine
#pragma vector=WDT_VECTOR
__interrupt void watchdog_timer(void)
{
  SD16CCTL0 |= SD16SC;                      // Start SD16 conversion
}
 

