/*
 *
 * $Id$
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


#ifndef _T_STATS_H
#define _T_STATS_H


extern struct t_stats *cur_stats, *acc_stats;

struct t_stats {
	/* number of server transactions */
	unsigned int transactions;
	/* number of UAC transactions (part of transactions) */
	unsigned int client_transactions;
	/* number of transactions in wait state */
	unsigned int waiting;
	/* number of transactions which completed with this status */
	unsigned int completed_3xx, completed_4xx, completed_5xx, 
		completed_6xx, completed_2xx;
	unsigned int replied_localy;
};

int init_tm_stats(void);

#endif
