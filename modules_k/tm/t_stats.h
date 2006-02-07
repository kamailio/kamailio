/*
 * $Id$
 *
 * Copyright (C) 2006 Voice Sistem
 *
 * This file is part of openser, a free SIP server.
 *
 * openser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * openser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * History:
 * --------
 *  2006-02-07  initial version (bogdan)

 */


#ifndef _T_STATS_H
#define _T_STATS_H

#include "../../statistics.h"

extern int tm_enable_stats;


/* statistic variables */
extern stat_var *rcv_rpls;
extern stat_var *rld_rpls;
extern stat_var *loc_rpls;
extern stat_var *uas_trans;
extern stat_var *uac_trans;
extern stat_var *trans_2xx;
extern stat_var *trans_3xx;
extern stat_var *trans_4xx;
extern stat_var *trans_5xx;
extern stat_var *trans_6xx;
extern stat_var *trans_inuse;


#ifdef STATISTICS
inline static void stats_trans_rpl( int code, int local ) {
	if (tm_enable_stats) {
		if (code>=700) {
			return;
		} else if (code>=600) {
			update_stat( trans_6xx, 1);
		} else if (code>=500) {
			update_stat( trans_5xx, 1);
		} else if (code>=400) {
			update_stat( trans_4xx, 1);
		} else if (code>=300) {
			update_stat( trans_3xx, 1);
		} else if (code>=200) {
			update_stat( trans_2xx, 1);
		}
		if (local)
			update_stat( loc_rpls, 1);
		else
			update_stat( rld_rpls, 1);
	}
}

inline static void stats_trans_new( int local ) {
	if (tm_enable_stats) {
		update_stat( trans_inuse, 1 );
		if (local)
			update_stat( uac_trans, 1 );
		else
			update_stat( uas_trans, 1 );
	}
}
#else
	#define stats_trans_rpl( _code  )  
	#define stats_trans_new( _local )  
#endif

#endif
