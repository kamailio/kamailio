/* sp-ul_db module
 *
 * Copyright (C) 2007 1&1 Internet AG
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

#ifndef SP_P_USRLOC_CHECK_H
#define SP_P_USRLOC_CHECK_H

#include <time.h>

#include "../../lock_ops.h"

struct check_data {
	int refresh_flag;
	int reconnect_flag;
	gen_lock_t flag_lock;
};

struct check_list_element{
	struct check_data * data;
	struct check_list_element * next;
};

struct check_list_head{
	gen_lock_t list_lock;
	int element_count;
	struct check_list_element * first;
};

int init_list(void);

struct check_data * get_new_element(void);

int must_refresh(struct check_data * element);

int must_reconnect(struct check_data * element);

int set_must_refresh(void);

int set_must_reconnect(void);

void destroy_list(void);

int must_retry(time_t * timer, time_t interval);


#endif
