/*
 *
 * $Id$
 *
 *
 */

#ifndef _SL_STATS_H
#define _SL_STATS_H

enum reply_type { RT_200, RT_202, RT_2xx,
	RT_300, RT_301, RT_302, RT_3xx,
	RT_400, RT_401, RT_403, RT_404, RT_407, 
		RT_408, RT_483, RT_4xx,
	RT_500, RT_5xx, 
	RT_6xx,
	RT_xxx,
	RT_END };


struct sl_stats {
	unsigned int err[RT_END];
	unsigned int failures;
};

int init_sl_stats(void);
void update_sl_stats( int code );
void update_sl_failures( void );
void sl_stats_destroy();

#endif
