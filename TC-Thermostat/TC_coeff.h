// Coefficients for inverse polinomial to compute temp from voltage. 

#ifdef J_TYPE_TC
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
#ifdef K_TYPE_TC
#define N_poly 9   // this table can be used up to 20mv , i.e.500 deg C
float TC_coeff[N_poly+1]= {  0.0, // d0
                     2.508355E+1, // d1
                     7.860106E-2, // d2
                    -2.503131E-1, // d3
                     8.315270E-2, // d4
                    -1.228034E-2,  // d5
                     9.804036E-4, // d6
                    -4.413030E-5, // d7
                    1.0527734e-06,
                    -1.052755e-08,
                  };
#endif
#endif
