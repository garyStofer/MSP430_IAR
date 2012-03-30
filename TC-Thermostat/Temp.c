#include  <msp430x20x3.h>
#define K_TYPE_TC				// Define either J_TYPE_TC or K_TYPE_C
#include "TC_coeff.h"
#define ADC_OFFS_AVG_CNT 24


#define INT_TEMP_SNS_OFFS 267.8 // device dependent 
#define TC_IN_ADC_OFFS_Delta  0 // This is the delta in Offset from the measured ADC_offset via the internally 
								 // shorted in channel (7) and the actual shorted TC input 

#define SET_POINT 200.0
#define HYSTERESIS 1.0

// globals
float TC_Temp;
float Temp;
float Cold_Temp;
float E;
int adc;
int ADC_offs;
unsigned char avg_ndx;

void main(void)
{
  WDTCTL = WDT_MDLY_32;                    // WDT Timer interval mode with SMCLK as clock and 32768 cycles til interrupt --  interval of 32ms at 1hz SMCLK 
  IE1 |= WDTIE;                            // Enable WDT interrupt
  P1DIR = 0xff;                            // P1 all to output direction CSn for 2515
  P1OUT = 0x00;                            // set to low


  P2SEL = 0;                                // select IO 
  P2DIR = 0xff;                             // P2 all output
  P2OUT = 0x0;                             // Set to low
  

   
  // Let the Clock run slow up to here 
  DCOCTL  = CALDCO_8MHZ;
  BCSCTL1 = CALBC1_8MHZ;
  BCSCTL2 = DIVS_3;           // SMCLK = MCKL/8 =~ 1Mhz
  
  //adc stuff
  ADC_offs = avg_ndx =0;
  SD16CTL = SD16REFON +SD16SSEL_1 + SD16DIV_0+ SD16VMIDON;        // 1.2V ref, SMCLK,   SMCLK/1, Ref buffer on ?????????
  SD16CCTL0 = SD16SNGL + SD16IE + SD16DF + SD16OSR_1024 ;   // Single conv, interrupt -- 2's complement 
  SD16INCTL0 = SD16INCH_7 |SD16GAIN_32; ;    // internal shortcut to read ADC_offset, gain 32 , INTDELAY =4th sample
  
  _BIS_SR(GIE + LPM1_bits );                 // Enter LPM1 with interrupt: CPU off, MCLK off, 

  while (1)
  {
	    P1OUT = 0x1;                // This never gets executed because of the LPM1 above , only interrupt routines are running          
		P1OUT =0;
		P1OUT =1;
		P1OUT =0;
	  
  }
  
}

#pragma vector=SD16_VECTOR
__interrupt void SD16ISR(void)
{ 
	

	switch ( SD16INCTL0 & 0x7)	// initial case -- measure offset voltage
	{
		case SD16INCH_7:// Offset
		{
			ADC_offs += (int)SD16MEM0 ;
			if (avg_ndx++ >= ADC_OFFS_AVG_CNT)
			{
				ADC_offs /= avg_ndx;
				avg_ndx=0;
				SD16INCTL0 = SD16INCH_6;	// next measure internal temp
				SD16AE = 0;              	// remove Analog input enable for P1.2 and P1.3 -- makes internal temp meas unstable 
			}
			break;
		}
      /* calc of ADC scale factor for internal temp sensor
      Vtemp = ((int) SD16MEM0 - ADC_offs) * (1.2 / 0xffff)  ;
      Vtemp /= 1.32e-3; // the Temp Coefficient per deg/K
      therefore :
      Vref / maxcount / TempCoeff = ScaleFactor  
      1.2V / 65532    / 1.32e-3   = 13.87183e-3  */

		case SD16INCH_6:  // internal Temp sensor -- Cold Junction temp
		{
		  Cold_Temp =  ((int)SD16MEM0 )  * 13.87183e-3; // in Kelvin
		  // calibration (op-amp) offset is taken out via the number below -- Can not use ADC_offs
		  // because the internal temp sensors has a different offset
		  Cold_Temp -= INT_TEMP_SNS_OFFS;     // convert to deg/C (-273) and  cal out PGA offset 
		
		
		  // calc TC cold junction emf equivalent from Vtemp so it can be added to TC voltage
		  // not done yet, might not have enough flash to hold an other 5th degree poliniomial and coefficients 
		  // so for now, just using the temp and add it to the temp calculated from  the TC -- slight error because 
		  // we are not exactly in the right place of the curve 
	
		 
		  // differential input on P1.2 / P1.3  
		  SD16AE =  SD16AE2 | SD16AE3; // Analog enable for P1.2 and P1.3  
		  SD16INCTL0 = SD16INCH_1 |SD16GAIN_32;  ; // switch to A1 and gain 32(nominal) for next time through  -- This makes the input range +-15mv & input impedance ~150Ko
		  break;
		}
		
		case SD16INCH_1:	// ThermoCouple input 
		{
    
		/* calc of ADC scale factor 
		// ADC NOTEs !!!!!!!!
		// The spec says that the Nominal Gain of 32 corresponds to a typ  gail of 28.35 !!!!!!!!!!!!!!!!
		// Nominal gain of 32 (28.35) has an error of +-5%, and an Offset Error of 1.5% Full Scale Range with up to 100ppm FSR/degC temp coefficient
		// Internal ref is 1.2V +- 5%  
		
		 E = ((int) SD16MEM0 - ADC_offs) * (1.2 / 0xffff)  ;  // multiply by bit value 
		 E /= 28.35 ;   // divide by gain -- Typical gain from spec 
		 E *= 1000 ;   // make E in mV   Therefore: ADCcount *  6.458845e-4;  +-5% */
		
		// remove offset 
		 adc = ((int) SD16MEM0 ) - ADC_offs + TC_IN_ADC_OFFS_Delta;
		 E = adc *6.45e-4;
		   
		// temperature  = d_0 + d_1*E  + d_2*E^2 + ... + d_n*E^n 
		//TC_Temp = E * TC_coeff[1];
		//TC_Temp += E*E * TC_coeff[2] ;
		//TC_Temp += E*E*E * TC_coeff[3] ;
		//TC_Temp += E*E*E*E * TC_coeff[4] ;
		//TC_Temp += E*E*E*E*E * TC_coeff[5] ;   // calc to the fifth degree polinomial for J-type
	
		 
		// starting at 1 -- index 0 -- offset is always 0 
		TC_Temp = 0;
		float EE = E;
		for (int i = 1 ; i <= N_poly; i++ )
		{
		  	TC_Temp += EE * TC_coeff[i];
		 	EE *= E;
		}
		
		Temp = Cold_Temp + TC_Temp;
		
	 	if (Temp < SET_POINT - HYSTERESIS) 
			P2OUT = BIT6;				// Heater on
		else if (Temp > SET_POINT)
			P2OUT = 0;				// Heater off
		
		
		SD16INCTL0 = SD16INCH_6; // ADC input -- go back to measure cold temp again 
		SD16AE = 0;              // remove Analog input enable for P1.2 and P1.3 -- makes internal temp meas unstable 
		break;
		}
	}
}

// Watchdog Timer interrupt service routine
#pragma vector=WDT_VECTOR
__interrupt void watchdog_timer(void)
{
  SD16CCTL0 |= SD16SC;                      // Start SD16 conversion
}
 

