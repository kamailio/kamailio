/**
 *
 * Copyright (C) 2008 Elena-Ramona Modroiu (asipto.com)
 *
 * This file is part of kamailio, a free SIP server.
 *
 * Kamailio is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * Kamailio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */
		       
#ifndef _HT_MOD_API_H_
#define _HT_MOD_API_H_

#include "../../sr_module.h"
#include "../../usr_avp.h"

typedef int (*ht_api_set_cell_f)(str *hname, str *name, int type,
		int_str *val, int mode);
typedef int (*ht_api_del_cell_f)(str *hname, str *name);

typedef int (*ht_api_set_cell_expire_f)(str *hname, str *name,
		int type, int_str *val);
typedef int (*ht_api_get_cell_expire_f)(str *hname, str *name,
		unsigned int *val);

typedef int (*ht_api_rm_cell_re_f)(str *hname, str *sre, int mode);
typedef int (*ht_api_count_cells_re_f)(str *hname, str *sre, int mode);

typedef struct htable_api {
	ht_api_set_cell_f set;
	ht_api_del_cell_f rm;
	ht_api_set_cell_expire_f set_expire;
	ht_api_get_cell_expire_f get_expire;
	ht_api_rm_cell_re_f rm_re;
	ht_api_count_cells_re_f count_re;
} htable_api_t;

typedef int (*bind_htable_f)(htable_api_t* api);
int bind_htable(htable_api_t* api);

/**
 * @brief Load the Sanity API
 */
static inline int htable_load_api(htable_api_t *api)
{
	bind_htable_f bindhtable;

	bindhtable = (bind_htable_f)find_export("bind_htable", 0, 0);
	if(bindhtable == 0) {
		LM_ERR("cannot find bind_htable\n");
		return -1;
	}
	if(bindhtable(api)<0)
	{
		LM_ERR("cannot bind htable api\n");
		return -1;
	}
	return 0;
}

#endif
