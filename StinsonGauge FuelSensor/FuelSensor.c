#include <msp430x20x3.h>
// Global variable, visible to ISR
unsigned short  isr_cnt;
short  F_Osc; 


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

//  P1OUT |= BIT2; // see if out works 
  
  //  Resistor divisor 12.1K- 470 Ohms -- 1 bit == 489.723e-6 
  // ADC setup 
  SD16AE =  SD16AE4;      // Analog input enable for P1.1  == A4+ , A4- internally connected to VSS
  SD16CTL = SD16REFON|    // Internal 1.2V Ref ON
            SD16SSEL_1 |  // SMCLK clock sel for ADC  
            SD16XDIV_2 ;  // Clock divisor 16 == 1Mhz  if SMCLK is at 16mhz

  SD16CCTL0 = SD16OSR_512|  // oversampling rate 512
              SD16SNGL|     // Single Conversion mode
              SD16DF|       // Data Format 1 == Two's complement, ( Bipolar )
              SD16IE;       // Interrupt enable
          
  // Select alternate funstions 
  P1SEL = BIT3;  // Port 1.3 connections -- connect Vref to p1.3 for capacitor bypass
  
  
  // Timer A setup  so that it produces a PWM signal on Timer_A2.TA1 output with the
  // Frequency controlled by compare register TACCR0 and the pulse with set by the 
  // value in TACCR1, so that a 50% pulse with is achieved when TACCR1 = 1/2 of TCCR0
  // The output signal is generated in hardware, no interrupts are used 
 
  TACCR0 = 1000;    // Periode in SMCKL clocks 16Mhz SMCLK == 1.6Khz when TACCR0 = 10000
  TACCR1 = 300;     // 300 == 30% duty cycle 
  
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
   
#define F_MIN 20000/4 // Sensor Frequency when full
#define F_MAX 30000/4 // Sensor Frequency when empty  

// Following are the 6 calibration pouints for the gauge
// The  number represents the error factor at that point, 
// for example when the reading should show 1/8 (125) the PWM needs to but out 2.21 times that for the gauge to read 1/8 full
#define CalPt0    4.0   // assumed value
#define CalPt125  2.21 
#define CalPt250  1.84
#define CalPt500  1.284
#define CalPt750  1.08
#define CalPt1000 1.0

// vars here as globals for ease of debugging
short Delta_F;
unsigned short  tacc1_tmp;
float x;
unsigned short yy;

void main( void )
{
  unsigned short result[3];
  
  static float V_bat;
  int Gauage_V_comp;
  

  init();
 
  
  //  Read Offset Vcc 
  result[0] = Read_ADC(SD16INCH_7); // offset
  
  while (1)
  {
      //  Go to sleep until WDT interval timer wakes up - 
      //  P1 Interrupt counts edges on P1.0 in the mean time 
      // _BIS_SR(LPM3_bits); 

    LPM0; // sleep until watchdog timer interval is up 

    Delta_F = F_MAX - F_Osc; 
    
    // PWM claculation
    // TACCR1 =( F_Osc - F_MAX ) / ( (F_MAX-F_MIN) / TACCR0);
    // have to make sure that PW value stays within 0 and TACCR0
    
    if (Delta_F < 0 )
      Delta_F = 0;
        
    //  Read V-Battery
    result[1] = Read_ADC(SD16INCH_4); // volt 12.1K - 470o divider (0.6v == 0x7fff) 1 Bit == 489.723 e-6 volt
    result[1] -=   result[0];         // remove PGA offset  
    
    V_bat = result[1] * (484.0e-6);   // convert to voltage 
    
    // Calibration and Vbat compensation. 
    // Set analog gain so that gauge reads full at 14V Vbat with a PW modulation of 100% ( == 3.3V after RC integrator) 
    // at 14v fullscale reading is obtained by Full (100%) PWM  - or TACCR1 == TACRR0 or 1000cts
    // at 10V fullscale reading is obtained by 71.4% PWM   10/14 = 71.4% (ohmic gauage error)                     
 

   // TACCR1 = Delta_F / (F_MAX - F_MIN /TACCR0); // scale to 1000 counts of PW modulator 
   // or 
   
    // cast to long to promote the math to long to prevent overflow 
    // tacc1_tmp 0 == empty , 999 = full 
    tacc1_tmp = (long) Delta_F * TACCR0 /(F_MAX-F_MIN);  // Figure the percentage PWM from Frequency ( 100% = TACCR0 Counts) 
 
   // Calibration to gauge scale errors using 6 calibration points and linear approximation between the points 
   x = 1.0;
   if (tacc1_tmp <= 125)
         x = CalPt0 -(((CalPt0-CalPt125)/125) * tacc1_tmp);
   else if (tacc1_tmp <=250) 
       x = CalPt125 - (((CalPt125-CalPt250)/ 125) * (tacc1_tmp -125)) ;
   else if (tacc1_tmp <= 500 )
       x = CalPt250 - (((CalPt250-CalPt500)/250) * (tacc1_tmp -250));
   else if ( tacc1_tmp <=750 )
      x = CalPt500 - (((CalPt500-CalPt750)/250) * (tacc1_tmp - 500 )); 
   else 
       x = CalPt750 - (((CalPt750-CalPt1000)/250) * (tacc1_tmp - 750 ));


   // Correct the PWM so the gauge reading matches the level                  
   tacc1_tmp *= x; 
   
   // need to calibrate to resistive behavior of gauage, Voltage comp, based on a 100%PW at  14V Cal point 
   tacc1_tmp =  V_bat / 14.0 * tacc1_tmp ; 
   
   TACCR1 = tacc1_tmp  ; // program the new couter calue for the PWM
   P1OUT ^= BIT5; // blink led
   
  }
}

