/**************************************************
 *
 * IAR EMBEDDED WORKBENCH TUTORIAL
 * C tutorial. Print the Fibonacci numbers.
 *
 * Copyright 1996-2004 IAR Systems. All rights reserved.
 *
 * $Revision: 1.6 $
 *
 **************************************************/

#include "Tutor.h"

int call_count;


/*
    Increase the 'call_count' variable by one.
*/
void next_counter(void)
{
  call_count += 1;      /* from d_f_p */
}


/*
    Increase the 'call_count' variable.
    Get and print the associated Fibonacci number.
*/
void do_foreground_process(void)
{
  unsigned int fib;
  next_counter();
  fib = get_fib( call_count );
  put_fib( fib );
}


/*
    Main program.
    Prints the Fibonacci numbers.
*/
void main(void)
{
  call_count=0;

  init_fib();

  while( call_count < MAX_FIB )
  {
    do_foreground_process();
  }
}
