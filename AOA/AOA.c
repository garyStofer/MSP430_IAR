//******************************************************************************
//  Angle of attack indicator
// Reads two potentiometer, one on the flaps, one on a flying vane. 
// Displays the angle of the Vane beween level and stall position.
// Has 3 calibration ponts for 0-33%  flaps, 33-66% flaps and 66-100% flaps. 
// indicates Flap position when flap is actuated + 2 seconds 
 
//******************************************************************************
#include  <msp430x20x3.h>

#define FlapSteps 3 // How many points we have for the stall angle calibration  - 
#define NumLEDs 10
#define FLAP_Hysteresis  200		// The sensitivity of how quick the flap display kiks in
#define FLAP_DisplayTime 30

typedef struct tag_flash_mem 
{	unsigned short Flap_low;
	unsigned short Flap_high;
	unsigned short Vane_0deg;
	unsigned short Stall[FlapSteps];
} Persist_data;
// define some symbols at the beginning of each Flash info section 
Persist_data *p_data; 		// pointer to the current persist data segment, either info_B or info_C
Persist_data info_B;		// in flash memory 
Persist_data info_C;		// in flash memory 
Persist_data info_D;		// in flash memory
// Note: Info_A is used for the calibration values and is not to be disturbed 
#pragma DATA_SECTION (info_B , ".infoB");  
#pragma DATA_SECTION (info_C , ".infoC");  
#pragma DATA_SECTION (info_D , ".infoD"); 


 
#define AVG 20
#define I2C_BufferSz 10


#define MAX7313_I2C_ADDR 		0x40  // MAX7315 Address A0-A2 inputs connected to GND = 0x40 according to Table1
#define MAX7313_CONTROL_REG 	0x0f  // Non auto increment 
#define MAX7313_MASTER_PWM		0x0e  // Non auto increment	
#define MAX7713_PORT_CONTROL 	0x02  // Auto Increment,Blink Phase0 port control	 	
#define MAX7313_PORT_SETUP 		0x06  // Auto Increment

#define MAX7313_INDIVID_PWM 	0x10  // Auto Increment

// Globals for I2C ISR
enum  I2Cstate {StartTX,StartRX,TX,RX,RX_end,Stop} ;
int I2C_State ;                     			// State variable
unsigned char MST_Data[I2C_BufferSz] ;          // Array for transmit data
unsigned char n_tx;								// Var for number of bytes to transmit out of the above array	

               
// Globals for ADC readings 
long ADC_Vane, ADC_Flap;
//unsigned short Speed_ADCoffs, Angle_ADCoffs;



// Interrupt service routines 
#pragma vector = USI_VECTOR
interrupt void 
ISR_USI_TXRX (void)
{
	  static unsigned char tx_ndx;	
	  
	  switch(I2C_State)
	  {
		  case StartTX:
		  case StartRX:
		  	tx_ndx = 0;					// Point to address in Data  
		  	USISRL = 0x00;           	// Generate I2c Start Condition...SDA going low while SCK is high
			USICTL0 |= USIGE+USIOE;
			USICTL0 &= ~USIGE;
			USISRL = MST_Data[tx_ndx++]; 	// Set Slave address to be sent, bit0 indicates read or write  
			USICNT = 8; 					// Bit counter = 8, TX Address
#ifdef I2C_RX								// To conserve space, only enable TX if no RX needed 
			if (I2C_State == StartTX)
				I2C_State = TX;
			else
				I2C_State = RX;
			break;
	  	
	  case RX: // setup to receive Address Ack/Nack for address write next
	          USICTL0 &= ~USIOE;      // SDA = input
	          USICNT = 0x01;          // Bit counter = 1, receive one (N)Ack bit
	         
	          while (! (USICTL1&USIIFG)) // wait for ACK/NACK bit to be clocked in
	          ;
	          USICNT=8;					// Clock in 8 data bits from device -- 
	          I2C_State = RX_end;
	          break;
	          
	  case RX_end:
	  		  MST_Data[1] = USISRL;		// store the read device's resposne somewhere
	  		  USISRL = 0;				// send Ack & primne for stop 
	  		  USICTL0 |= USIOE;        	// SDA = output again
	  		  USICNT = 1;      			// Bit counter = 1, SCL high, SDA low
	          I2C_State=Stop;			// terminate with STOP
	          break;
#else
			I2C_State = TX;
			break;
#endif 	  		       	  
	    		
	  case TX: // setup to receive Address Ack/Nack bit next
	          USICTL0 &= ~USIOE;      // SDA = input
	          USICNT = 0x01;          // Bit counter = 1, receive (N)Ack bit
	         
	          while (! (USICTL1&USIIFG)) // wait for ACK/NACK bit to be clocked in
	          ;
	         
	          
	          if (USISRL & 0x01 || tx_ndx > n_tx )  // If NACK or nothing else to transmit
	          {
	          		USISRL = 0x00;		// Prime for stop, clock out a Low, so the stop condition can be generated 	
	          		USICTL0 |= USIOE;   // SDA = output again
	          		USICNT = 1;      	// Bit counter = 1, SCL high, SDA low
	          		I2C_State=Stop;		// terminate with STOP
	          }
	          else 
	          {
	          	USISRL = MST_Data[tx_ndx++];  	// Load next data byte
	          	USICTL0 |= USIOE;        		// SDA = output again
	            USICNT = 8;       				// Bit counter = 8, start TX of next byte
	            // Stay at current state, check ACk again and trsnsmit next byte
	          }

	          break;
	
	   
	      case Stop:// Generate Stop Condition
	          USISRL = 0x0FF;          		// USISRL = 1 to release SDA
	          USICTL0 |= USIGE;        		// Transparent latch enabled
	          USICTL0 &= ~(USIGE+USIOE);	// Latch/SDA output disabled
	          USICTL1 &= ~USIIE;			// Disable Interrupts
	          LPM0_EXIT;               		// Exit active for next transfer
	          break;
	}

}

#pragma vector = SD16_VECTOR
interrupt void 
SD16ISR(void)
{
  SD16CCTL0 &= ~SD16IFG;    // Clear the interrupt flag since we are not reading SD16MEM0 in here 
  LPM0_EXIT; 				// Leave CPU running upon ISR exit
}

 
unsigned short  
ReadADC(int chan)
{
   SD16INCTL0 = chan ; 			// input selection -- gain == 1 , 4th conversion causes interrupt
   SD16CCTL0 |= SD16SC;       	// Start Conversion
   LPM0;  						// sleep CPU until conversion is done and ISR wakes the cpu up again
   return (SD16MEM0);
}

void 
I2c_Init( )
{
	// USI setup for I2C
	USICTL0 = USIPE6+USIPE7+USIMST+USISWRST;	// Port & USI mode setup
	USICTL1 = USII2C;                			// Enable I2C mode 
// TODO change clock to faster i2C speed	
	USICKCTL = USIDIV_3+USISSEL_2+USICKPL;		// Setup USI clocks: SCL = SMCLK/5 (~250kHz)
	USICTL0 &= ~USISWRST;                		// Enable USI
	USICTL1 &= ~USIIFG;							// Clear Interrupt Flag
}



// TODO limit the number of bytes to be sent or recived to max buffer size 
void 
I2c_Transmit(unsigned char n) 
{
	n_tx=n;
	I2C_State = StartTX;    			// Entry into Interrupt SM 
	USICTL1 |= (USIIE + USIIFG);	// Enable interrupt and set interrupt flag to start IC2 ISR
	LPM0;                   		// CPU off, await USI transmit complete -- (exits with LPM0_EXIT)
}

void MAX7315_Led(unsigned short leds)
{
	unsigned char *cp;
	
	
	cp  = (unsigned char *) &leds;
	/* turns LEDS on/off*/
	MST_Data[0] = MAX7313_I2C_ADDR; // initialize with I2C address of device to talk to
	MST_Data[1] = MAX7713_PORT_CONTROL;	  
	MST_Data[2] = *cp++;	
	MST_Data[3] = *cp;         
	I2c_Transmit(3);
}

void MAX7315_Dim(unsigned char dim)
{
	/* dims the LEDS in 16 steps
	 * use both, matser and O8 PWM sets to get the most dynamic change in 16 steps 
	 */
	dim &= 0xf;
	dim |= dim<<4;
	if (dim < 0x10)		// minimal brightness -- Below this the LEDs come on full again. 
		dim = 0x10;
	
	MST_Data[0] = MAX7313_I2C_ADDR; // initialize with I2C address of device to talk to
	// Configure Master/O8 pwm, O8 output PWM used via extrnal circuitry to dimm the LEDS
	// All values with "F" in the low nibble produce a static Low (active)Output regardless of the high nibble
	// All values with "0" in the high nibble produce a static Low (active) Output regardless of the low nibble 
	MST_Data[1] = MAX7313_MASTER_PWM;
	MST_Data[2] = dim;     
	I2c_Transmit(2);
}

void 
MAX7315_Init( unsigned char dim )
{
	MST_Data[0] = MAX7313_I2C_ADDR; // initialize with I2C address of device to talk to
	
	// Configure LED ports as outputs
	MST_Data[1] = MAX7313_PORT_SETUP;
	MST_Data[2] = 0x00;        // P1-p7 outputs
	MST_Data[3] = 0x00;        // P8-p15 outputs
	I2c_Transmit(3);
	
	// Configure Control register, no global intens control, O8 regular output, used for eternal dimmer
	MST_Data[1] = MAX7313_CONTROL_REG;	
	MST_Data[2] = 0x00;    
	I2c_Transmit(2);
		
	// Configure P0-P7 outputs as static outputs, no PWM 
	/*  This is the default power up configuration -- No need to program it
	MST_Data[1] = MAX7313_INDIVID_PWM;	
	MST_Data[2] = 0xFF;     // Set individual output PWM to disable PWM
	MST_Data[3] = 0xff;     // External circuitry used to dimm the LED's 
	MST_Data[4] = 0xff;     
	MST_Data[5] = 0xff; 
	MST_Data[6] = 0xFF;     // Set individual output PWM to disable PWM
	MST_Data[7] = 0xff;     // External circuitry used to dimm the LED's 
	MST_Data[8] = 0xff;     
	MST_Data[9] = 0xff; 
	I2c_Transmit(9);
	*/
	MAX7315_Dim(dim); 			// brightness 
	MAX7315_Led(0x0000); 		// all on initially
}

#ifdef I2C_RX
/* MAX7315_Receive_inputs() 
 * Abbreviated I2c communication, only a single byte can be read per transfer, no clock streching by the addressed device is 
 * allowed. 
 */
unsigned char 
MAX7315_Receive_inputs()
{	
    // first write command address , pointing to register 0 for subsequent read 
	MST_Data[0] = MAX7315_I2C_ADDR; 
	MST_Data[1] = 0x00; 			// Address of input reg
	I2c_Transmit(1);
	
	MST_Data[0] |= 0x1; 			// This is the "read" I2C address now
	I2C_State = StartRX;    		// Entry into Interrupt SM 
	USICTL1 |= (USIIE + USIIFG);	// Enable interrupt and set interrupt flag to start IC2 ISR
	LPM0; 
	return MST_Data[1]; 
}	
#endif  


void 
Flash_EraseOld_and_Reset( void) 
{
	
	FCTL1 = FWKEY + ERASE;   	// Make eraseable
	FCTL3 = FWKEY ; 		 	// undoo the lock 	
	p_data->Flap_low =0xffff;	// This starts the erase of the section, takes ~12ms 	
	
	FCTL3 = FWKEY +  LOCK;     // Lock all segments 
	FCTL1 = FWKEY ;            // remove Write and Erase bits 
	
	WDTCTL = WDTPW + 0x3; 		// This will create an Watchdog reset almost immediatly... 
}	

void
main(void)
{
	unsigned short i;
	unsigned char flying_cal = 0;	
	unsigned short led =0;
	unsigned char dim =5;			// init to medium brightness
	unsigned short min = 0xffff;
	unsigned short max = 0;
	unsigned short flap_pos;
	unsigned short flap_range1_led;
	unsigned short flap_hyst; 
	unsigned short flap_range_stall;
	unsigned char flap_disp_time ;
	unsigned short stall_range;
	unsigned short stall_ndx;			// 
	unsigned short tmp_stall[3];
		
	WDTCTL = WDTPW + WDTHOLD;                 // Stop watchdog timer
	
	// ports 
	P1OUT =0;	// led off 
	P1DIR =0x01; //P1.0 is output  
	
	P2SEL= 0x00; 	// Must do this first before swtiching to input. excessive current otherwise
	P2OUT= 0xff;
	P2DIR = 0x00;	// p2.6 & p2.7 input
	P2REN = 0xD0; 	// Pullup on P2.6 and P2.7 -- used as switch inputs 

/* // Could Trap CPU if DCO cal data is erase
   if (CALBC1_16MHZ ==0xFF || CALDCO_16MHZ == 0xFF)                                     
    	while(1);                          // If calibration constants erased not execute, trap CPU!!
*/
	// Turn on ADC ref  first, before we switch to faster clock, so it has time to stabilize 
	BCSCTL2 = DIVS_3;      // MCLK = DCO, SMCLK = DCO/8  (== 2mhz)
	BCSCTL3 = 0;      	   // No 32khz xtal connected LXT1 and VLO not used 
	// In anticipation of MCLK at 16mhz and SMCL at 2mhz, set the ADC clock to 1Mhz:
	SD16CTL = SD16REFON + SD16SSEL_1 + SD16DIV_1 ;  // ADC 1.2V ref, ADC clk source SMCLK/2 ( ==1mhz) 
	SD16CCTL0 = 	SD16OSR_512|   	// oversampling rate 
	          		SD16SNGL|     	// Single Conversion mode
			  		SD16UNI |		// UNidirectional ADC -- yields an unsigned short result.
	          		SD16IE;       	// Interrupt enable
	
	// rest of ADC setup: 
	// Unipolar setup: 1 ADC bit is (Vref/2) / 65535,  Bipolar 2's complement setup: 1 ADC bit is Vref / 65535  
	// SD16AE = 0;                        		  // No differential analog inputs - single ended only. 
	// SD16INCTL0 = SD16INCH_4;                  // SD16INCH_4 == This is A4 on Pin P1.1 
	// SD16INCTL0 = SD16INCH_1;   // speed voltage  on A1+ Unipolar input, P1.2 pin 4 
	// SD16INCTL0 = SD16INCH_2;   // Angle voltage  on A2+ Unipolar input, p1.4, pin 6   
	// P1SEL |= BIT3;  // Port 1 connections -- connect Vref to p1.3 for capacitor bypass Vref  measured 1.17464

	_BIS_SR(GIE);
	// Init i2C and turn all LEDS on while we are waiting for power to stabilize before switching to faster clock
	// Provides a visual selftest of the LEDs 
	I2c_Init();
	MAX7315_Init( dim );

	// Allow the power rail to stabalize before swithing to fast clock ... CPU fails when switching under 3.3V
	for ( i=0; i < 0xffff; i++ )		;		// delay before switching CPU clock for VCC to rise above 3.3V 
		  
	// DCO set for 16Mhz  -- After sufficient delay above
	DCOCTL  = CALDCO_16MHZ;  // CAL data for DCO modulator
	BCSCTL1 = CALBC1_16MHZ;  // CAL data for DCO -- ACKL div /1 
	// Flash clock setup 
	FCTL2 = FWKEY + FSSEL_3 + FN2;  // SCLK/5, == 400khz,  Single write is 75us, Section erase ~= 12ms	
	
	// find the location of the persistant data,  keying on the fact that the low point of the flap pot can never be 0xffff
	p_data = ( info_B.Flap_low != 0xffff )? &info_B : &info_C; 	
	
	// Check switches for ground calibration entry  
	if (P2IN == 0x00 )		// Both buttons pressed, entering cal mode
	{
		Persist_data *p_data_to =  (p_data == &info_B) ? &info_C : &info_B;
		led = 0;
		
		while ( P2IN != 0xC0 ) 	// Hold until both buttons  released 
		;
		do  	
		{
			MAX7315_Led(led ^= 0x0387);	// toggle  LEDS
			for ( ADC_Vane=ADC_Flap=i=0; i<128; i++ )	
			{	
				ADC_Vane += ReadADC(SD16INCH_1);
				ADC_Flap += ReadADC(SD16INCH_4);
				
			}
			ADC_Vane /= i;
			ADC_Flap /= i; 
			
			if ( ADC_Flap < min )	
				min = ADC_Flap;
			if (ADC_Flap > max )	
				max = ADC_Flap; 
			
			
		} while (P2IN != 0x80 );						// exit with save when upper button is pressed
		
		// Flap min & max positions recorded   
		// write new values to the flash info segment we are currently not pointing to, copiying the other values 
		// Then erase the current ( now old) info segment and reboot via watchdog reset, so that the new values are getting picked up
		FCTL1 = FWKEY + WRT;    // Make flash writable
		FCTL3 = FWKEY ;  		// unlock write to everything but InfoA segment A frite to LOCKA toggles the lock !!
		
		p_data_to->Flap_low = min;
		p_data_to->Flap_high = max;
		p_data_to->Vane_0deg = ADC_Vane;
		
		// copy the non modified values to new location 
		for (i=0; i < FlapSteps; i++ )
		{
			p_data_to->Stall[i] = p_data->Stall[i];
		}
		Flash_EraseOld_and_Reset( ); 		// Never returns from this call, reset/reboots
	}
	
	// initialize tmp in case we go into flying cal and don't collect all the stall steps 
	tmp_stall[0]=p_data->Stall[0];
	tmp_stall[1]=p_data->Stall[1];
	tmp_stall[2]=p_data->Stall[2];
	
	// calculate flap range 
	flap_range1_led  = ((p_data->Flap_high - p_data->Flap_low) / NumLEDs) + 1;	//rounding up 
	flap_range_stall = (p_data->Flap_high - p_data->Flap_low) / FlapSteps;	
	flap_hyst = flap_range1_led / 3; 
	flap_disp_time= 0;
	flap_pos = p_data->Flap_low;		// set it to low so that Flap diaplsy comes on initially during boot up
		

	// this is the main loop 
	while ( 1 ) 
	{
		// check buttons for mode
		if (P2IN == 00 )
			flying_cal = 1;
		
		if (! flying_cal)		// Dimming
		{
			if (! (P2IN& 0x40)) // the switches
			{
				if (dim < 0xf)
					MAX7315_Dim(++dim);
			}
			else if (! (P2IN &0x80) )
			{
				if (dim > 1) 
					MAX7315_Dim(--dim);	
			}
		}	
		
		// averageing 
		for (ADC_Vane=ADC_Flap=i=0; i<AVG; i++ ) 	
		{
			ADC_Vane += ReadADC(SD16INCH_1);
			ADC_Flap += ReadADC(SD16INCH_4);
		}
		   	   		
		ADC_Vane /= i;
		ADC_Flap /= i; 
		
/*	   	if (ADC_Flap > p_data->Flap_high) // limit the flap pos to legal values
	   		ADC_Flap = p_data->Flap_high;
	   	else*/
	    if (ADC_Flap < p_data->Flap_low)
	   		ADC_Flap = p_data->Flap_low;
	   	
	   	if (ADC_Vane < p_data->Vane_0deg)	// limit the Vane reading 
	   		ADC_Vane = p_data->Vane_0deg;
	   		
/* for now don't do the flap position lookup -- lock to single flap position 			
		stall_ndx = (ADC_Flap - p_data->Flap_low) / flap_range_stall;	
		if (stall_ndx >= FlapSteps )
				stall_ndx = FlapSteps-1;
*/	
stall_ndx=0;	// lock to single flap position -- remove this later 		

/*  for now disable the flap position display 	 
	    // check if flap position and display if it has changed
		if (ADC_Flap > flap_pos + flap_hyst  || ADC_Flap < flap_pos - flap_hyst  )  
		{
			flap_disp_time = FLAP_DisplayTime; // in loop times -- needs to change when changing averaging etc..
			flap_pos = ADC_Flap;
		}
 */
flap_disp_time =0;	// dont display flaps
				 
		if ( flap_disp_time )	// Flap position display mode
		{	
			flap_disp_time--;
			led = (ADC_Flap - p_data->Flap_low) / flap_range1_led ;
			if (led > NumLEDs-1 )
				led = NumLEDs-1;
			
			led = 0xffff << led;	// Can only shift a 0 in,  hence the following inversion
			led = ~led;
		}
		else  // Normal or Flying cal mode
		{	
		   	if (flying_cal )
			{
				led = 0xfffe << stall_ndx; // show which one is beeing calibrated  
				
				if (P2IN == 0x80 ) 
				{ 
					Persist_data *p_data_to =  (p_data == &info_B) ? &info_C : &info_B;
					
					FCTL1 = FWKEY + WRT;    // Make flash writable
					FCTL3 = FWKEY ;  		// unlock write to everything but InfoA segment A frite to LOCKA toggles the lock !!
			
					p_data_to->Flap_low = p_data->Flap_low;
					p_data_to->Flap_high = p_data->Flap_high;
					p_data_to->Vane_0deg = p_data->Vane_0deg;
					p_data_to->Stall[0] = tmp_stall[0];
					p_data_to->Stall[1] = tmp_stall[1];
					p_data_to->Stall[2] = tmp_stall[2];
					Flash_EraseOld_and_Reset( ); 					// Only way out of flying cal mode is via reboot/reset
	
				}
				else if ( P2IN == 0x40 )
				{
					tmp_stall[stall_ndx] = ADC_Vane;
				}// record vane and flap  reading in temp var until save button is pressed
			}
			else	// normal mode   
			{
		 	
				stall_range = ((p_data->Stall[stall_ndx] - p_data->Vane_0deg) / NumLEDs) +1;  // roundig up
				led = (ADC_Vane - p_data->Vane_0deg)/stall_range;
				if (led > NumLEDs ) 		//  limit to number of leds
					led = NumLEDs;
				// make bar graph display, leaving lowest (green) LED always on
				led = 0xfffe << led;			
			}
			
			
		}	

		MAX7315_Led(led);   // set LED 
		
		P1OUT ^= 0x1;	// toggle led 
		

	}
 

}


