/**************************************************
 *
 * IAR EMBEDDED WORKBENCH TUTORIAL
 * Utility header file
 *
 * Copyright 1996-2004 IAR Systems. All rights reserved.
 *
 * $Revision: 1.3 $
 *
 **************************************************/

#define MAX_FIB 10

//-----------------------------------------------------------
// when running the tutorials using the DLIB libs you have
// to use the unbuffered __putchar function instead of the
// buffered putchar. In this case 'activate' the lines
// below.
#if 0
#include <yfuns.h>
#define putchar __putchar
#endif
//-----------------------------------------------------------

void init_fib( void );
unsigned int get_fib( int nr );
void put_fib( unsigned int out );

