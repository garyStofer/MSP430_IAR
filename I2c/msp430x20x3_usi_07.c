#include  <msp430x20x2.h>
#define MAXIM_7300_ADR 0x88   // I2cAddress of the attached port mltiplier
#define MAX_RXTX_BUFF 12
unsigned char MST_Data[MAX_RXTX_BUFF+1];                     // Variable for transmitted data, index 0 is slave address for 7300
unsigned char I2C_Buff[MAX_RXTX_BUFF+1];
unsigned char I2C_State = 0;  
unsigned char I2C_NACK_Err=0;
unsigned char I2C_RxTx_Size =0;

/******************************************************
// USI interrupt service routine
******************************************************/
// This implementation does not generate a a re START condition in a read request but rather rlies on a seperate write 
// to set the subsequent read address in the slave 
#pragma vector = USI_VECTOR
__interrupt void USI_TXRX (void)
{
  static unsigned char tx_ndx;

  
  switch(__even_in_range(I2C_State,10))
    {
      case 0: // Generate Start Condition & send address to slave
              I2C_NACK_Err = 0;
              I2C_State = 2;           // Next interrupt will be for (n)ACK from addrese
              USISRL = 0x00;           // prepare for start condition ..MSB = 0, 
              USICTL0 |= USIGE+USIOE;  // make 0 appear on SDA while SCL high ==> START  condition
              USICTL0 &= ~USIGE;       // SDA is now Low -- switch to clocking mode 
              tx_ndx=0;
              USISRL =  I2C_Buff[tx_ndx++];// 7 bit of address and RW bit in LSB
              USICNT  = (USICNT &0xe0) + 8; // Shift counter = 8, TX Address,  clears Interrupt flag, starts transmitting 
              break;

      case 2: // Receive Ack/Nack bit
              if (I2C_Buff[0] & 0x1) // I2C_Read
                I2C_State = 8;  
              else
                I2C_State = 4;         // Next state: process (n)ACK for address
              USICTL0 &= ~USIOE;       // SDA becomes input on next clock edge 
              USICNT += 1;             // Bit counter = 1, receive (N)Ack bit
              break;

      case 4: // Process Address Ack/Nack & handle data TX
              USICTL0 |= USIOE;        // SDA = output on next Clock edge 
              
              // if NACK or no more data to send; prepare for stop condition by getting SDA low first
              if (USISRL & 0x01 || tx_ndx >= I2C_RxTx_Size) 
              {    
                if (USISRL & 0x01 )     // NACK -- set error flag
                  I2C_NACK_Err++;
                
                I2C_State = 6;          // Next state: STOP
                USISRL = 0x00;
                USICNT +=  1;           // Bit counter = 1, SCL high, SDA low
                break;
              } 
              // Send TX data to slave   
              I2C_State = 2;                // Next state: get (N)Ack
              USISRL = I2C_Buff[tx_ndx++];  // Load data byte 
              USICNT += 8;                  // Bit counter = 8, start TX
              break;

      case 6:// Generate Stop Condition , raise SDA while SCL remains high 
              USISRL = 0x0FF;           // USISRL = 1 to release SDA
              USICTL0 |= USIGE;         // Transparent latch enabled
              USICTL0 &= ~(USIGE+USIOE);// Latch/SDA output disabled
              USICTL1 &= ~USIIFG;       // clear interrupt flag -- this telegram is done 
              I2C_State = 0;            // Reset state machine for next transmission
              break;
           
      // case 8 and 10 are for reads only
      case 8:   // Read 8 bits 
              I2C_State = 10;
              USICTL0 &= ~USIOE;       // SDA becomes input on next clock edge 
              USICNT += 8;             // Bit counter = 8, recieve 8 bits 
              break; 
              
      case  10:  // Store received data byte and send ACK 
              I2C_State = 8;
              I2C_Buff[tx_ndx++] = USISRL ;     // Store data byte 
              USICTL0 |= USIOE;        // SDA = output on next Clock edge 
              USISRL = 0;              // ACK 
              USICNT+=1;
              
              if (tx_ndx > I2C_RxTx_Size ) //receive lenght -- terminate the read 
                  I2C_State = 6; // we can go directly to STOP since we always send a ACK ( 0 level); 
              else
                  I2C_State = 8; // setup for next read 
              break;
    }
}


// Sends "size" number of data bytes to the I2C device identified by addr. 
// returns with 0 if size is too large for the transmit buffer 
// returns with negative number if device did not acknowledge the transfer
// returns with number of data bytes transmitted if no erros 
int 
I2C_send( unsigned char addr, unsigned char *data, unsigned char size )
{
    if ( size > MAX_RXTX_BUFF)    // too much data to send 
      return 0;
    
    if ( data!= I2C_Buff )  // copy data to interrupt buffer if not already there
    {
        for ( unsigned char i = 1; i<= size ; i++ )
        {
            I2C_Buff[i] = data[i];
        }
    }
    
    I2C_RxTx_Size = size+1;
    I2C_Buff[0] = addr & 0xFE; // read -- LSB is 0 !
    
    USICTL1 |= USIIFG;         // Set flag and start communication
    while( I2C_State)  // wait until transmission done 
      ;
    if (I2C_NACK_Err)
      return -I2C_NACK_Err;
    else
      return size;
}

// Receives "size" number of bytes from the I2C device identified by addr.
// returns the buffer where the bytes are stored, or
// returns 0 if the decvice addresses did not acknowledge 

unsigned char *
I2C_read(unsigned char addr, unsigned char size)
{
  if ( size > MAX_RXTX_BUFF)  // too much data to send 
    return 0;
  
  I2C_RxTx_Size = size;
  I2C_Buff[0] = addr | 0x1;  // Read -- LSB of Address is true
  
  USICTL1 |= USIIFG;         // Set flag and start communication
  while( I2C_State)          // wait until transmission done 
    ;
  
   if (I2C_NACK_Err)
      return 0;
   else
      return I2C_Buff;
}



void
I2C_Init( void )  // based on a 16Mhz MCLK !
{
  BCSCTL2 = DIVS_1;  // SMCLK = MCKL/2 =~ 8Mhz
    // Universal Serial Interface configuration 
  USICTL0 = USIPE6+USIPE7+USIMST+USISWRST; // Port & USI mode setup
  USICTL1 = USII2C+USIIE;                 // Enable I2C mode & USI interrupt
  USICKCTL = USIDIV_4+USISSEL_2+USICKPL;  // Setup USI clocks: SCL = SMCLK/64 == 8 Mhz/16 ~= 500Khz 
  USICTL0 &= ~USISWRST;                   // Enable USI
  USICTL1 &= ~USIIFG;                     // Clear pending flag
  
}

void 
main(void)
{
    volatile unsigned int i;             // Use volatile to prevent removal
    volatile int foo;
  
    WDTCTL = WDTPW + WDTHOLD;            // Stop watchdog
  
    // setup the internal clock for the calibrated 16 Mhz
    DCOCTL = CALDCO_16MHZ;
    BCSCTL1 = CALBC1_16MHZ;

  
    P1OUT = 0xC0;                        // P1.6 & P1.7 Pullups, others to 0
    P1REN |= 0xC0;                       // P1.6 & P1.7 Pullups
    P1DIR = 0xFF;                        // Unused pins as outputs
    P2OUT = 0;
    P2DIR = 0xFF;
  
    I2C_Init( );   // set USI port charateristics 
    
    _EINT();       // Enable interrupts 

    MST_Data[1] = 0x4; MST_Data[2] = 0xf;// 7300 out of sleep 
    I2C_send( MAXIM_7300_ADR, MST_Data, 2 );

    MST_Data[1] = 0xb; MST_Data[2] = 0x55; MST_Data[3] = 0x55; MST_Data[4] = 0xff; MST_Data[5]=0xff;MST_Data[6]=0xff;
    I2C_send( MAXIM_7300_ADR, MST_Data, 6 );
    

    MST_Data[1] = 0x2c; MST_Data[2] = 0xFF;MST_Data[3] = 0x00;MST_Data[4] = 0xFF;MST_Data[5] = 0xFF;
    I2C_send( MAXIM_7300_ADR, MST_Data, 5 );
    
    for (  foo = 21; foo-- ; foo>0 )
    {
        MST_Data[1] = 0x4c; MST_Data[2] = 0xaa;
        I2C_send( MAXIM_7300_ADR, MST_Data,2 );
        for (i = 64000; i--;i) ;

        MST_Data[1] = 0x4c; MST_Data[2] = 0x55;
        I2C_send( MAXIM_7300_ADR, MST_Data,2 );
        for (i = 64000; i--;i) ;
    }

    // read ports 12...19
    MST_Data[1] = 0x4c;
    I2C_send( MAXIM_7300_ADR, MST_Data, 1 );
    I2C_read( MAXIM_7300_ADR,2);
   
    MST_Data[1] = 0x58;
    I2C_send( MAXIM_7300_ADR, MST_Data, 1 );
    I2C_read( MAXIM_7300_ADR,2);
 
    // put 7300 into standby -- disabling outputs 
    MST_Data[1] = 0x4; MST_Data[2] = 0x0;
    I2C_send( MAXIM_7300_ADR, MST_Data,2 );
}

