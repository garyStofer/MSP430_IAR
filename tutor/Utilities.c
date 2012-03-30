/**************************************************
 *
 * IAR EMBEDDED WORKBENCH TUTORIAL
 * Utility file
 *
 * Copyright 1996-2004 IAR Systems. All rights reserved.
 *
 * $Revision: 1.4 $
 *
 **************************************************/

#include <stdio.h>
#include "Utilities.h"


unsigned int root[MAX_FIB];

/*
    Initialize MAX_FIB Fibonacci numbers.
*/
void init_fib( void )
{
  int i = 45;
  root[0] = root[1] = 1;

  for ( i=2 ; i<MAX_FIB ; i++)
  {
    root[i] = get_fib(i) + get_fib(i-1);
  }
}

/*
    Return the Fibonacci number 'nr'.
*/
unsigned int get_fib( int nr )
{
  if ( (nr>0) && (nr<=MAX_FIB) )
  {
    return ( root[nr-1] );
  }
  else
  {
    return ( 0 );
  }
}

/*
    Puts a number between 0 and 65536 to stdout.
*/
void put_fib( unsigned int out )
{
  unsigned int dec = 10, temp;

  if ( out >= 10000 )
  {
    putchar ( '#' );/* To large value. */
    return;         /* Print a '#'. */
  }

  putchar ( '\n' );
  while ( dec <= out )
  {
    dec *= 10;
  }

  while ( (dec/=10) >= 10 )
  {
    temp = out/dec;
    putchar ( (int)('0' + temp) );
    out -= temp*dec;
  }

  putchar ( (int)('0' + out) );
}

