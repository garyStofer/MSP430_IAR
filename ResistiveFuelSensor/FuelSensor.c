#include <msp430x20x3.h>
#include <math.h>
// Fuel sensors to work with ISS Fuel Level gauge for 0-5V and Resistive senor 30 to 245Ohms
// Resistors excat  Min and Max values 
#define R_min 33.5		
#define R_max 250
#define R1 1200 		// value of resistor going to 3.3V

// defines for the Pulse With modulator creating the DC for the gauge
#define PW_PER 1000       // PW Periode in Timer clocks
//#define PW_EMPTY 637      // Counts for the gauge to read Empty RH guage
//#define PW_FULL  170      // Counts for the gauge to read Full  RH guage
#define PW_EMPTY 650      // Counts for the gauge to read Empty RH guage
#define PW_FULL  175      // Counts for the gauge to read Full  RH guage
#define PW_RANGE  (PW_EMPTY-PW_FULL)


#pragma vector=WDT_VECTOR
__interrupt void wdt_ISR(void)
{
	// just to bring it out of sleep -- 
	// todo : really not needed anymore could just run it full all the time in main 
    LPM0_EXIT;
}

#pragma vector = SD16_VECTOR
__interrupt void SD16ISR(void)
{
    SD16CCTL0 &= ~SD16IFG;  // Clear the interrupt flag.
    LPM0_EXIT;              // change the power mode on the stack so that device keeps running after rti
}

// ------------ END of ISRs ---------------// 
int Read_ADC(int inctl0)
{
  // See setup of ADC in init();
	if ( inctl0 == SD16INCH_4)
		SD16AE =  SD16AE4;
	else if ( inctl0 == SD16INCH_0)
		SD16AE = SD16AE0;

  SD16INCTL0 = inctl0;    // input selection -- gain == 1 , 4th conversion causes interrupt  max range = +- 0.6V 
  SD16CCTL0 |=  SD16SC;   // Start Conversion
  
  LPM0; // sleep until conversion is done and iterrupt wakes the cpu up again
  return (SD16MEM0);         // Capture the ADC value ;
}


 
void init(void)
{
    // Stop watchdog timer first 
  WDTCTL = WDTPW + WDTHOLD;
  
  P1SEL=0;    // set all pins to digital IO first
  
  // Port 2 is used for Xtal 
  P2SEL= 0xff;   // using P2.6 and P2.7 as watch xtal in''
  P2DIR = 0x80;  // P2.7 = output , P2.6 = input  
  P2OUT = 0;     // low
  P2IE =0;
    
  // Clock selection  1 Mhz 
  // Set ACLK = VLO
  // Set MCLK = SMCLK = DCO = 1MHz
  
  DCOCTL  = CALDCO_16MHZ;  // CAL data for DCO modulator
  BCSCTL1 = CALBC1_16MHZ;  // CAL data for DCO -- ACKL div /1 
  BCSCTL2 = 0;             // reset condition = 0, MCLK = SMCLK = DCO , no dividers
  BCSCTL3 = LFXT1S_0;      // ==  32khz xtal used for watchdog interval timer 
  

  WDTCTL = WDT_ADLY_250; // 250ms, using ACLK = 32Khz  
  IE1 = WDTIE  ;          // enable the maskable interrupt for the watchdog interval timer 
 
  // P1.1 used as analog input             
  // P1.0 used as analog input        
  // P1.2 used for Timer_A2 output        
  // P1.3 used for Vref output (to bypass cap) 
  
  // Port setups
  P1OUT = 0;        // no Pull down for Osc in
  P1REN = 0;        // no Pull up resistor 
  
 
  P1DIR = 0xFc;       // Using P1.0 as A0+ and  P1.1 is analog A4+ input, rest as outputs
  P1IE =0; 				// no interrupts from pins
  
  // ADC setup 
  // Analog input switching is done in Read_ADC since it has to be changed depending on the measurement input
  // in order to do single ended measurments 
 //  SD16AE =  SD16AE4|SD16AE0;    // Analog input enable for P1.1 and P1.0   == A4+ and A0+ , A4- internally connected to VSS

  SD16CTL = SD16REFON|    // Internal 1.2V Ref ON
            SD16SSEL_1 |  // SMCLK clock sel for ADC  
            SD16XDIV_2 ;  // Clock divisor 16 == 1Mhz  if SMCLK is at 16mhz

  //Since we only want to measure positive voltages setup in Unipolar/binary offset dataformat
   SD16CCTL0 = SD16OSR_512|  // oversampling rate 512
              SD16SNGL|     // Single Conversion mode
              SD16IE |      // Interrupt enable
	    //    SD16DF|       // Data Format 1 == Two's complement, ( Bipolar )
  			  SD16UNI ;		// Unipolar data format , 0.6v = 0xffff

          
  // Select alternate funstions done below
  // P1SEL = BIT3;  // Port 1.3 connections -- connect Vref to p1.3 for capacitor bypass
  
  // Timer A setup  so that it produces a PWM signal on Timer_A2.TA1 output with the
  // Frequency controlled by compare register TACCR0 and the pulse with set by the 
  // value in TACCR1, so that a 50% pulse with is achieved when TACCR1 = 1/2 of TCCR0
  // The output signal is generated in hardware, no interrupts are used 
 
  TACCR0 = PW_PER;    // Periode in SMCKL clocks 16Mhz SMCLK == 1.6Khz when TACCR0 = 10000
  TACCR1 = 600;       //  just some initial number for the first time through 
  
  // Compare mode for PWM  with Reset output on reaching threshold in compare register, Set on counter overflow to 0
  // no interrupts are created 
  TACCTL0 = 0;          // Nothing used -- 
  TACCTL1 = OUTMOD_7;   // Reset/Set -- This causes the output to be cleared upon reaching TCCR1 and set when the counter
                        // hits TACCR0 and reloads to 0 agian. This produces a positive pulse that inceases in duration 
                        // With increasing values in TACCR1

  TACTL = TASSEL_2| /* Timer A clock source select: 1 - SmCLK  */
          MC_1|     // Counting mode: up to TACCR0
          TACLR;    // Timer A clear -- clears the count register
  
   
   // Select alternate Pin IO funstions 
   P1SEL = BIT2|BIT3;  // makes Timer_A2.TA1 output appear (PW signal), Port 1.3 connections -- connect Vref to p1.3 for capacitor bypass
 
    
  _BIS_SR(GIE) ; //Enable interrups 
} 
   

// vars here as globals for ease of debugging
volatile unsigned short  tacc1_tmp;


void main( void )
{  
	unsigned short result[3];
	
	static float V_bat;
	float V_Res;
	static float R_Res;

	
	float factorFull;

	
	result[0]= result[1] = result[2] = 0;
	
	init();
	//  Read Offset Vcc 
	result[0] = Read_ADC(SD16INCH_7); // offset
	//  Read V-Battery -- for debugging only 
	result[1] = Read_ADC(SD16INCH_4); // volt 12.1K - 470o divider (0.6v == 0xffff) 1 Bit == 244.8615 e-6 volt
	result[1] -=   result[0];         // remove PGA offset  
    V_bat = result[1] * (244.8615e-6);   // convert to voltage 

  while (1)
  { 
	//  Go to sleep until WDT interval timer wakes up - 
	LPM0; // sleep until watchdog timer interval is up -- every 16 or 250ms as setup in init()
	
	
	result[2] = Read_ADC(SD16INCH_0);		// read the Fuel level resistor
	result[2] -=  result[0];    			// remove PGA offset
	V_Res = result[2] * 9.155e-6; 			// convert to volatage, 0.6V = 0xffff counts  == 9.155uv per count
	R_Res = V_Res * R1/ (3.30 -V_Res); 	// resitor is high for full tank
	
	
	factorFull =  (R_Res - R_min)/ (R_max-R_min);
	
	if (factorFull > 1.0 )
	factorFull = 1.0;
	if (factorFull < 0 )
		factorFull = 0;
		
	// for testing of the guage set factor here to fixed value
	// factorFull =  0.5; // the factor of full 1.0 == full
	//factorFull = 0;
	// PWM claculation
	tacc1_tmp = (1.0 - factorFull) *  PW_RANGE; 
	tacc1_tmp += PW_FULL;
	TACCR1 = tacc1_tmp  ; // program the new counter value for the PWM
	
//	P1OUT ^= BIT5; // blink led
  }
}

