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
		       
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "../../dprint.h"

#include "ht_api.h"
#include "api.h"
#include "ht_dmq.h"

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

	if (ht->dmqreplicate>0 && ht_dmq_replicate_action(HT_DMQ_SET_CELL, hname, name, type, val, mode)!=0) {
		LM_ERR("dmq relication failed\n");
	}

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
	if (ht->dmqreplicate>0 && ht_dmq_replicate_action(HT_DMQ_DEL_CELL, hname, name, 0, NULL, 0)!=0) {
		LM_ERR("dmq relication failed\n");
	}
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
	if (ht->dmqreplicate>0 && ht_dmq_replicate_action(HT_DMQ_SET_CELL_EXPIRE, hname, name, type, val, 0)!=0) {
		LM_ERR("dmq relication failed\n");
	}
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
	int_str isval;
	ht = ht_get_table(hname);
	if(ht==NULL)
		return -1;
	if (ht->dmqreplicate>0) {
		isval.s.s = sre->s;
		isval.s.len = sre->len;
		if (ht_dmq_replicate_action(HT_DMQ_RM_CELL_RE, hname, NULL, AVP_VAL_STR, &isval, mode)!=0) {
			LM_ERR("dmq relication failed\n");
		}
	}
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

