/* 
 * $Id$ 
 */

#ifndef __TIMER_NEW__
#define __TIMER_NEW__

#include "cache.h"

int  init_timer_new  (const char* _table);
void timer_new       (cache_t* _c);
void close_timer_new (void);

#endif
