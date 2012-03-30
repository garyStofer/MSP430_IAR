#include <msp430x20x3.h>
#include <math.h>
// Fuel sensors to work with ISS Fuel Level gauge for 0-5V 

// Global variable, visible to ISR
unsigned short  isr_cnt;
short  F_Osc; 

// The actual Frequency at the 555's output when the sensor is completely submerged or dry
//#define F_FULL 4614 // * 4 = Sensor Frequency when full
//#define F_EMPTY 6696 // * 4 = Sensor Frequency when empty  // empty count RH sensor =6706
#define F_FULL 4719 // * 4 = Sensor Frequency when full
#define F_EMPTY 7321 
#define F_RANGE  (F_EMPTY-F_FULL)

// defines for the Pulse With modulator creating the DC for the gauge
#define PW_PER 1000       // PW Periode in Timer clocks
//#define PW_EMPTY 637      // Counts for the gauge to read Empty RH guage
//#define PW_FULL  170      // Counts for the gauge to read Full  RH guage
#define PW_EMPTY 650      // Counts for the gauge to read Empty RH guage
#define PW_FULL  170      // Counts for the gauge to read Full  RH guage
#define PW_RANGE  (PW_EMPTY-PW_FULL)


#pragma vector=WDT_VECTOR
__interrupt void wdt_ISR(void)
{
    // SR is cleared, Only NMI interrupts can interrupt this 
    F_Osc = isr_cnt; // capture the frequncy counter and reset
    isr_cnt = 0;
    LPM0_EXIT;
}

//
// PORT 1 Interrupt Service Routine -- 
// making an edge counter with p1 interrupt and watchdog timer 
// Counting endges on P1.0 input -- Oscillator frequency from fuel sensor capacitor
//

#pragma vector=PORT1_VECTOR
__interrupt void port1_ISR (void)
{
  isr_cnt++;
  P1IFG = 0;      // clear the interrupt flag 
}


// ------------ END of ISRs ---------------// 
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
  
   
  WDTCTL = WDT_ADLY_250; // using ACLK = 32Khz  
  IE1 = WDTIE  ;          // enable the maskable interrupt for the watchdog interval timer 
 
  // P1.1 used as analog input             
  // P1.0 used for Fuel cap osc. in        
  // P1.2 used for Timer_A2 output        
  // P1.3 used for Vref output (to bypass cap) 
  
  // Port setups
  P1OUT = 0;        // no Pull down for Osc in
  P1REN = 0;        // no Pull up resistor 
  
  
  P1DIR = 0xFc;       // Using P1.0 as input to count freq. from Sensing element, P1.1 is analog A4+ input
  P1IE  = BIT0;       // Enables interrupt from P1.0 
  P1IES = 0xff;       // high to low tansition
  P1IFG = 0x00;       // clear interrupt flag

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
   P1SEL = BIT2;//|BIT3;  // makes Timer_A2.TA1 output appear (PW signal), Port 1.3 connections -- connect Vref to p1.3 for capacitor bypass
 
    
  _BIS_SR(GIE) ; //Enable interrups 
} 
   


// vars here as globals for ease of debugging

volatile float factorFull;
volatile unsigned short  tacc1_tmp;
int DF;

void main( void )
{

  init();

  while (1)
  { 
      //  Go to sleep until WDT interval timer wakes up - 
      //  P1 Interrupt counts edges on P1.0 in the mean time 
      // _BIS_SR(LPM3_bits); 

    LPM0; // sleep until watchdog timer interval is up 

 
    DF= F_EMPTY - F_Osc;  // the difference between empty 
   
    if (DF <0) 
      DF =0;     // make sure df is not more than max range 
    else if ( DF > F_RANGE) 
      DF = (F_RANGE);
    
// Df = (F_RANGE/8)*4;   // for testing -- 
// Df =F_RANGE;
    factorFull =  1.0 - ( DF / (float) F_RANGE ); // the factor of full 1.0 == full 

       // PWM claculation
    tacc1_tmp = factorFull *  PW_RANGE; 
    tacc1_tmp += PW_FULL;
    TACCR1 = tacc1_tmp  ; // program the new counter value for the PWM

    P1OUT ^= BIT5; // blink led
  }
}

