/*
 * $Id$
 *
 * Usrloc time related functions
 */

#include "utime.h"


time_t act_time;


void get_act_time(void)
{

	act_time = time(0);
}
