/* Simple thermostat with hysteresis to control a heater  via K-type thermocoule and a Triac

  Using a msp430F2013 14 pin DIL device  -- Note flash memory completely full --
    if more space is needed the offset measuring code can be eliminated and fixed value used 

Hookup : K- thermo couple on P1.0 amd P1.1
        10K potentiometer wiper to P1.2 
        10K resistor from P1.3 to top of potentiometer
        Gnd to bottom of potentiometer.

Heater control via P2.6 to switching transistor to Opto isolating Triac driver
Supply with 3.3V or Lipo cell via diode drop.
*/

// Configure the max temp the thermostat should regulate to and the hysteresis:
#define Set_point_MAX 200UL   // the setpoint knob all the way up
#define HYSTERESIS 0

#include  <msp430x20x3.h>
#define K_TYPE_TC				// Define either J_TYPE_TC or K_TYPE_C
#include "TC_coeff.h"
#define ADC_OFFS_AVG_CNT 20


#define INT_TEMP_SNS_OFFS 273   // kelvin offset


// globals
float TC_Temp;
float Temp;
float Cold_Temp;
float E;
int adc;

unsigned long Set_pt;  // The setpoint from the knob
long ADC_offs;
unsigned char avg_ndx;


void main(void)
{
  unsigned char * ADC_fix = (unsigned char *)0xbf; // location of VREF calibration value -- See SD16 Errata SDA2 bug  
  
  
  WDTCTL = WDT_MDLY_32;                    // WDT Timer interval mode with SMCLK as clock and 32768 cycles til interrupt --  interval of 32ms at 1hz SMCLK 
  IE1 |= WDTIE;                            // Enable WDT interrupt
  P1DIR = 0x00;                            // P1 all to inputs 
  P1OUT = 0x00;                            // set to low -- no pullups
  P1SEL = BIT3;                            // enable VREF optional  function on P1.3


  P2SEL = 0;                                // select IO 
  P2DIR = 0xff;                             // P2 all output
  P2OUT = 0x0;                             // Set to low

 *ADC_fix = 0x62;   // fix for SD16 Vref bug SDA2, See MSP430F2013_errata page 5. this makes the vref 1.2V  
  
   
  // Let the Clock run slow up to here 
  DCOCTL  = CALDCO_8MHZ;
  BCSCTL1 = CALBC1_8MHZ;
  BCSCTL2 = DIVS_3;           // SMCLK = MCKL/8 =~ 1Mhz
  
  //adc setup stuff
  SD16AE = 0;              	             // remove Analog input enables -- makes internal  meas unstable 
  SD16INCTL0 = SD16INCH_7 |SD16GAIN_32; ;    // internal shortcut to read ADC_offset, gain 32 , INTDELAY =4th sample
  ADC_offs = avg_ndx =0;
  
  SD16CTL = SD16REFON +SD16SSEL_1 + SD16DIV_0 + SD16VMIDON;        // 1.2V ref, SMCLK,   SMCLK/1, Ref buffer on P1.3
  SD16CCTL0 = SD16SNGL + SD16IE+ SD16DF + SD16XOSR;  // Single conv, interrupt -- 2's complement Bipolar, 512 oversampling

  _BIS_SR(GIE + LPM1_bits );                 // Enter LPM1 with interrupt: CPU off, MCLK off, 
  
 

  while (1)
  {
	 // This never gets executed because of the LPM1 above , only interrupt routines are running          
	;	
  }
  
}

#pragma vector=SD16_VECTOR
__interrupt void SD16ISR(void)
{ 
	

	switch ( SD16INCTL0 & 0x7)	// Swicth on the input selection of the ADC
	{
            	// ThermoCouple input 
		case SD16INCH_0:	
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
		 adc = ((int) SD16MEM0 ) - ADC_offs;
                 //E = adc *6.45884e-4;
		 E = adc *6.48e-4;  // sligltly modified for device dependent gain difference at X32 pga setting
           
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
                
                if ( Temp > Set_pt + HYSTERESIS )
                  P2OUT = 0;				// Heater off
                else if (Temp < Set_pt)
                  P2OUT = BIT6;				// Heater on

		
	        // setpoint next
                SD16INCTL0 = SD16INCH_1; 
		break;
		}
                
                // SET Point
              	case SD16INCH_1:	
		{
                  // Single ended reading of pot connected to Vref/2 volt.
                  // Vref/2 and above ( 0.6V) is 0x7fff , while 0V reads as 0x0000
                  
                  
                  adc = ((int) SD16MEM0 ) ;
                  if ( adc < 0 )        // can be a small negative number due to offset and noise.
                    Set_pt = 0;
                  else
                     Set_pt = (Set_point_MAX * adc) / 30185; // 30185 = max adc reading with 5k series to a 4.4k pot

                  // Next measure Cold junction temp 
		  SD16INCTL0 = SD16INCH_6  ; // switch to internal temp sensor inputs next
		  break;
                }
                
                /* calc of ADC scale factor for internal temp sensor
                Vtemp = ((int) SD16MEM0 - ADC_offs) * (1.2 / 0xffff)  ;
                Vtemp /= 1.32e-3; // the Temp Coefficient per deg/K
                therefore :
                Vref / maxcount / TempCoeff = ScaleFactor  
                1.2V / 65535    / 1.32e-3   = 13.87183e-3  */
                
                //  Cold Junction temp -- internal Temp sensor -- 
                case SD16INCH_6: 
		{
         		  
                  Cold_Temp =  ((int)SD16MEM0 )  * 13.87183e-3; // in Kelvin
		  // calibration (op-amp) offset is taken out via the number below -- Can not use ADC_offs
		  // because the internal temp sensors has a different offset
		  Cold_Temp -= INT_TEMP_SNS_OFFS;     // convert to deg/C (-273) and  cal out PGA offset 
		
		
		  // calc TC cold junction emf equivalent from Vtemp so it can be added to TC voltage
		  // not done yet, might not have enough flash to hold an other 5th degree poliniomial and coefficients 
		  // so for now, just using the temp and add it to the temp calculated from  the TC -- slight error because 
		  // we are not exactly in the right place of the curve 
	
		 
		  // Next Measure 
                  SD16INCTL0 = SD16INCH_0; 
		  break;
		}
                
                
               //  internal offset adc count only executed once on startup
                case SD16INCH_7:
		{
                    ADC_offs += (int)SD16MEM0 ;
                    if (avg_ndx++ >= ADC_OFFS_AVG_CNT)
                    {
                            ADC_offs /= avg_ndx;
                            avg_ndx=0;
                            SD16INCTL0 = SD16INCH_0;	// next measure TC
                            
                    }
                    break;
		}
                
              default:
                  SD16INCTL0 =SD16INCH_0;   // all others back to TC reading
                  break;
   /*   
                // unused inputs -- trapped just in case
                case SD16INCH_2:	
		case SD16INCH_3:	
                case SD16INCH_4:	
                case SD16INCH_5:
                case SD16INCH_7:	  
                {
                
                  SD16INCTL0 =SD16INCH_0;
                  break;
                }
   */               
	}
 
        
 // SETUP for next reading follows
        

        SD16AE = 0;              	// remove Analog input enables -- makes internal  meas unstable 
        switch ( SD16INCTL0 & 0x7)	// Swicth on the input selection of the ADC
	{
           case SD16INCH_0: // TC 	
              SD16AE =  SD16AE0 | SD16AE1;  // Analog enable for P1.0 and P1.1  for next measrmenents
              SD16INCTL0 = SD16INCH_0 | SD16GAIN_32;    // gain 32(nominal) for next time through  -- This makes the input range +-15mv & input impedance ~150Ko
              break;
              
           case SD16INCH_1: // Setpoint
               SD16AE =  SD16AE2;  // Analog enable for P1.2 , A1+  
               SD16INCTL0=SD16INCH_1;  // resets gain and interrupt delay bits  
               break;
     /*          
           case SD16INCH_2: // not used
           case SD16INCH_3: // not used
           case SD16INCH_4: // not used 
                break;
           case SD16INCH_5: // not used // VCC/11
           case SD16INCH_6: // internal temp sensor
               SD16AE = 0;    
               break;
      */          
          
          case SD16INCH_7: // internal shortcut for offset meaurment 
                SD16INCTL0 = SD16INCH_7|SD16GAIN_32; 
                break;
                
                
        }
}

// Watchdog Timer interrupt service routine
#pragma vector=WDT_VECTOR
__interrupt void watchdog_timer(void)
{
  SD16CCTL0 |= SD16SC;                      // Start SD16 conversion
}
 

