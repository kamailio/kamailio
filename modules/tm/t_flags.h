/*
 * $Id$
 */


#ifndef _FLAGS_H
#define _FLAGS_H

#define FL_WHITE	1
#define FL_YELLOW	2
#define FL_GREEN	3
#define FL_RED		4
#define FL_BLUE		5
#define FL_MAGENTA	6
#define FL_BROWN	7
#define FL_BLACK	8



typedef unsigned long tflags_t;

int t_setflag( unsigned int flag );
int t_resetflag( unsigned int flag );
int t_isflagset( unsigned int flag );

#endif
