/*
 * Copyright (C) 2001-2003 FhG Fokus
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * Kamailio is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * Kamailio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */


#ifndef _SL_STATS_H
#define _SL_STATS_H

#include "../../rpc.h"

enum reply_type { RT_1xx = 0,  RT_200, RT_202, RT_2xx,
		  RT_300, RT_301, RT_302, RT_3xx,
		  RT_400, RT_401, RT_403, RT_404, RT_407, 
		  RT_408, RT_483, RT_4xx,
		  RT_500, RT_5xx, 
		  RT_6xx,
		  RT_xxx,
		  RT_END };

struct sl_stats {
	unsigned long err[RT_END];
	unsigned long all_replies;
	unsigned long err_replies;
	unsigned long failures;
	unsigned long filtered_acks;
};

int init_sl_stats(void);
int init_sl_stats_child(void);
void update_sl_stats( int code );
void update_sl_failures( void );
void update_sl_err_replies( void );
void update_sl_filtered_acks( void );
void sl_stats_destroy();

extern rpc_export_t sl_rpc[];

int sl_register_kstats(void);

#endif /* _SL_STATS_H */
