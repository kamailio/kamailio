/*
 *
 * $Id$
 *
 *
 *
 * Copyright (C) 2001-2003 Fhg Fokus
 *
 * This file is part of ser, a free SIP server.
 *
 * ser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * For a license to use the ser software under conditions
 * other than those described here, or to purchase support for this
 * software, please contact iptel.org by e-mail at the following addresses:
 *    info@iptel.org
 *
 * ser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
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
	unsigned long err[RT_END];
	unsigned long failures;
};

int init_sl_stats(void);
void update_sl_stats( int code );
void update_sl_failures( void );
void sl_stats_destroy();

#endif
