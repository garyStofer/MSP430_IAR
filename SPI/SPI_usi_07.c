#include  <msp430x20x2.h>

/******************************************************
// USI interrupt service routine -- not used for SPI 
******************************************************/
#pragma vector = USI_VECTOR
__interrupt void USI_TXRX (void)  // gets invoked when count register is 0 
{
}


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

void 
main(void)
{
    volatile unsigned int i;             // Use volatile to prevent removal
    volatile int foo;
    volatile unsigned char c;
  
    WDTCTL = WDTPW + WDTHOLD;            // Stop watchdog
  
    // setup the internal clock for the calibrated 16 Mhz
    DCOCTL = CALDCO_16MHZ;
    BCSCTL1 = CALBC1_16MHZ;
    BCSCTL2 = DIVS_1;  // SMCLK = MCKL/2 =~ 8Mhz
  
    P1OUT = 0x1;                        // output P1.1 to high -- disbale CS on 7301
    P1REN = 0x00;                       // no Pullups
    P1DIR = 0xFF;                       // pins as outputs
  
   
    P2OUT = 0;
    P2REN = 0x00;
    P2DIR = 0xFF;
  
    SPI_Init( );   // set USI port charateristics 
    
    // _EINT();       // Enable interrupts -- not used for SPI port 
   
    SPI_send( 0x4,0x0 );  // into sleep
    SPI_send( 0x4,0xf );  // out of slepp 

// port config 
    SPI_send( 0xb, 0x55 ); // P12..P15 outputs
    SPI_send( 0xc, 0x55 ); // P16..P19 outputs 
    SPI_send( 0xd, 0xff ); // P20..P23 inputs
    SPI_send( 0xe, 0xFF ); // p24..P28 inputs
    SPI_send( 0xf, 0xFF ); // P29..P31 inputs
    
    
    SPI_send( 0x4c,0x0 ); // clear outputs P12..19

 //   MST_Data[1] = 0x2c; MST_Data[2] = 0xFF;MST_Data[3] = 0x00;MST_Data[4] = 0xFF;MST_Data[5] = 0xFF;
   SPI_send(0x2c, 0xff ); // Single Bit address P12 = on
   SPI_send(0x2d, 0xff ); // Single Bit address P13 = off 
   SPI_send(0x2e, 0xff ); // single 
   SPI_send(0x2f, 0xff);  // ""
    
   SPI_send(0x2c, 0x0 ); // Single Bit address P12 = on
   SPI_send(0x2d, 0x0 ); // Single Bit address P13 = off 
   SPI_send(0x2e, 0x0 ); // single 
   SPI_send(0x2f, 0x0);  // "" 
   
    for (  foo = 21; foo-- ; foo  )
    {
        SPI_send( 0x4c,0xaa );
        for (i = 64000; i--;i) ;

        SPI_send( 0x4c,0x55 );
        for (i = 64000; i--;i) ;
    }
    
    // read ports 12...19
    c = SPI_read( 0x4c);
 
    // read ports 24..31
    c = SPI_read( 0x58);

    // put 7300 into standby -- disabling outputs 
    SPI_send( 0x4,0 );

}

