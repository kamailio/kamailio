/*
 * $Id$
 *
 * Registrar time related functions
 */

#ifndef REGTIME_H
#define REGTIME_H

#include <time.h>


extern time_t act_time;


/*
 * Get actual time and store
 * value in act_time
 */
void get_act_time(void);


#endif /* REGTIME_H */
