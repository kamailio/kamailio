/**
 * $Id$
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
		       
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "../../dprint.h"

#include "ht_api.h"
#include "api.h"

/**
 *
 */
int ht_api_set_cell(str *hname, str *name, int type,
		int_str *val, int mode)
{
	ht_t* ht;
	ht = ht_get_table(hname);
	if(ht==NULL)
		return -1;
	return ht_set_cell(ht, name, type, val, mode);
}

/**
 *
 */
int ht_api_del_cell(str *hname, str *name)
{
	ht_t* ht;
	ht = ht_get_table(hname);
	if(ht==NULL)
		return -1;
	return ht_del_cell(ht, name);
}

/**
 *
 */
int ht_api_set_cell_expire(str *hname, str *name,
		int type, int_str *val)
{
	ht_t* ht;
	ht = ht_get_table(hname);
	if(ht==NULL)
		return -1;
	return ht_set_cell_expire(ht, name, type, val);
}

/**
 *
 */
int ht_api_get_cell_expire(str *hname, str *name,
		unsigned int *val)
{
	ht_t* ht;
	ht = ht_get_table(hname);
	if(ht==NULL)
		return -1;
	return ht_get_cell_expire(ht, name, val);
}

/**
 *
 */
int ht_api_rm_cell_re(str *hname, str *sre, int mode)
{
	ht_t* ht;
	ht = ht_get_table(hname);
	if(ht==NULL)
		return -1;
	if(ht_rm_cell_re(sre, ht, mode /* 0 - name; 1 - value */)<0)
		return -1;
	return 0;
}

/**
 *
 */
int ht_api_count_cells_re(str *hname, str *sre, int mode)
{
	ht_t* ht;
	ht = ht_get_table(hname);
	if(ht==NULL)
		return -1;
	if(ht_count_cells_re(sre, ht, mode /* 0 - name; 1 - value */)<0)
		return -1;
	return 0;
}

/**
 *
 */
int bind_htable(htable_api_t* api)
{
	if (!api) {
		ERR("Invalid parameter value\n");
		return -1;
	}
	api->set = ht_api_set_cell;
	api->rm  = ht_api_del_cell;
	api->set_expire = ht_api_set_cell_expire;
	api->get_expire = ht_api_get_cell_expire;
	api->rm_re    = ht_api_rm_cell_re;
	api->count_re = ht_api_count_cells_re;
	return 0;
}

