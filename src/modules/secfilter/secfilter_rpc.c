/**
 * Copyright (C) 2018 Jose Luis Verdeguer
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
 *
 */


#include <string.h>

#include "../../core/mem/mem.h"
#include "../../core/ut.h"
#include "secfilter.h"


/* RPC commands */


static int get_type(str ctype)
{
	int type;
	
	if (ctype.len > 64) ctype.len = 64;

	if(!strncmp(ctype.s, "ua", ctype.len)) {
		type = 0;
	} else if(!strncmp(ctype.s, "country", ctype.len)) {
		type = 1;
	} else if(!strncmp(ctype.s, "domain", ctype.len)) {
		type = 2;
	} else if(!strncmp(ctype.s, "ip", ctype.len)) {
		type = 3;
	} else if(!strncmp(ctype.s, "user", ctype.len)) {
		type = 4;
	} else {
		LM_ERR("Invalid type\n");
		return -1;
	}
	
	return type;
}


/* Add blacklist destination value */
void secf_rpc_add_dst(rpc_t *rpc, void *ctx)
{
	int number;
	str data = STR_NULL;
	char *text = NULL;

	if(rpc->scan(ctx, "d", &number) < 1) {
		rpc->fault(ctx, 500,
				"Invalid Parameters. Usage: secfilter.add_dst "
				"number\n     Example: secfilter.add_dst "
				"555123123");
		return;
	}
	text = int2str(number, &data.len);
	data.s = pkg_malloc(data.len * sizeof(char));
	if(!data.s) {
		PKG_MEM_ERROR;
		rpc->fault(ctx, 500, "Error insert values in the blacklist");
		return;
	}
	memcpy(data.s, text, data.len);
	lock_get(&(*secf_data)->lock);
	if(secf_append_rule(2, 0, &data) == 0) {
		rpc->rpl_printf(ctx,
				"Values (%s) inserted into blacklist destinations", data);
	} else {
		rpc->fault(ctx, 500, "Error insert values in the blacklist");
	}
	lock_release(&(*secf_data)->lock);
	if(data.s)
		pkg_free(data.s);
}

/* Add blacklist value */
void secf_rpc_add_bl(rpc_t *rpc, void *ctx)
{
	str ctype = STR_NULL;
	str data = STR_NULL;
	int type;

	if(rpc->scan(ctx, "ss", &ctype, &data) < 2) {
		rpc->fault(ctx, 0,
				"Invalid Parameters. Usage: secfilter.add_bl type "
				"value\n     Example: secfilter.add_bl user "
				"sipvicious");
		return;
	}
	data.len = strlen(data.s);
	ctype.len = strlen(ctype.s);
	type = get_type(ctype);

	lock_get(&(*secf_data)->lock);
	if(secf_append_rule(0, type, &data) == 0) {
		rpc->rpl_printf(ctx, "Values (%.*s, %.*s) inserted into blacklist",
				ctype.len, ctype.s, data.len, data.s);
	} else {
		rpc->fault(ctx, 500, "Error inserting values in the blacklist");
	}
	lock_release(&(*secf_data)->lock);
}


/* Add whitelist value */
void secf_rpc_add_wl(rpc_t *rpc, void *ctx)
{
	str ctype = STR_NULL;
	str data = STR_NULL;
	int type;

	if(rpc->scan(ctx, "ss", &ctype, &data) < 2) {
		rpc->fault(ctx, 0,
				"Invalid Parameters. Usage: secfilter.add_wl type "
				"value\n     Example: secfilter.add_wl user "
				"trusted_user");
		return;
	}
	data.len = strlen(data.s);
	ctype.len = strlen(ctype.s);
	type = get_type(ctype);

	lock_get(&(*secf_data)->lock);
	if(secf_append_rule(1, type, &data) == 0) {
		rpc->rpl_printf(ctx, "Values (%.*s, %.*s) inserted into whitelist",
				ctype.len, ctype.s, data.len, data.s);
	} else {
		rpc->fault(ctx, 500, "Error insert values in the whitelist");
	}
	lock_release(&(*secf_data)->lock);
}


/* Reload arrays */
int rpc_check_reload(rpc_t *rpc, void *ctx)
{
	if(secf_rpc_reload_time == NULL) {
		LM_ERR("not ready for reload\n");
		rpc->fault(ctx, 500, "Not ready for reload");
		return -1;
	}
	if(*secf_rpc_reload_time != 0
			&& *secf_rpc_reload_time > time(NULL) - secf_reload_delta) {
		LM_ERR("ongoing reload\n");
		rpc->fault(ctx, 500, "ongoing reload");
		return -1;
	}
	*secf_rpc_reload_time = time(NULL);
	return 0;
}

void secf_rpc_reload(rpc_t *rpc, void *ctx)
{
	if(rpc_check_reload(rpc, ctx) < 0) {
		return;
	}
	
	if(secf_load_db() == -1) {
		LM_ERR("Error loading data from database\n");
		rpc->fault(ctx, 500, "Error loading data from database");
	} else {
		rpc->rpl_printf(ctx, "Data reloaded");
	}
}


/* Print values */
void secf_rpc_print(rpc_t *rpc, void *ctx)
{
	void *handle;
	void *dsth, *dstbh;
	void *uah, *uabh, *uawh;
	void *ch, *cbh, *cwh;
	void *dh, *dbh, *dwh;
	void *iph, *ipbh, *ipwh;
	void *usrh, *usrbh, *usrwh;
	struct str_list *list;

	str param = STR_NULL;
	int showall = 0;

	if(rpc->scan(ctx, "s", &param) < 1)
		showall = 1;
		
	param.len = strlen(param.s);

	/* Create empty structure and obtain its handle */
	if (rpc->add(ctx, "{", &handle) < 0) return;

	if(showall == 1 || !strncmp(param.s, "dst", param.len)) {
		if (rpc->struct_add(handle, "{", "Destinations", &dsth) < 0)
		{
			rpc->fault(ctx, 500, "Internal error creating inner struct");
			return;
		}

		if (rpc->struct_add(dsth, "{", "Blacklisted", &dstbh) < 0)
		{
			rpc->fault(ctx, 500, "Internal error creating inner struct");
			return;
		}

		list = (*secf_data)->bl.dst;
		while(list) {
			if (rpc->struct_add(dstbh, "S", "Value", &list->s) < 0) {
				rpc->fault(ctx, 500, "Internal error creating inner struct");
				return;
			}
			list = list->next;
		}
	}

	if(showall == 1 || !strncmp(param.s, "ua", param.len)) {
		if (rpc->struct_add(handle, "{", "User-Agent", &uah) < 0)
		{
			rpc->fault(ctx, 500, "Internal error creating inner struct");
			return;
		}

		if (rpc->struct_add(uah, "{", "Blacklisted", &uabh) < 0)
		{
			rpc->fault(ctx, 500, "Internal error creating inner struct");
			return;
		}

		if (rpc->struct_add(uah, "{", "Whitelisted", &uawh) < 0)
		{
			rpc->fault(ctx, 500, "Internal error creating inner struct");
			return;
		}

		list = (*secf_data)->bl.ua;
		while(list) {
			if (rpc->struct_add(uabh, "S", "Value", &list->s.s) < 0) {
				rpc->fault(ctx, 500, "Internal error creating inner struct");
				return;
			}
			list = list->next;
		}

		list = (*secf_data)->wl.ua;
		while(list) {
			if (rpc->struct_add(uawh, "S", "Value", &list->s) < 0) {
				rpc->fault(ctx, 500, "Internal error creating inner struct");
				return;
			}
			list = list->next;
		}
	}

	if(showall == 1 || !strncmp(param.s, "country", param.len)) {
		if (rpc->struct_add(handle, "{", "Country", &ch) < 0)
		{
			rpc->fault(ctx, 500, "Internal error creating inner struct");
			return;
		}

		if (rpc->struct_add(ch, "{", "Blacklisted", &cbh) < 0)
		{
			rpc->fault(ctx, 500, "Internal error creating inner struct");
			return;
		}

		if (rpc->struct_add(ch, "{", "Whitelisted", &cwh) < 0)
		{
			rpc->fault(ctx, 500, "Internal error creating inner struct");
			return;
		}

		list = (*secf_data)->bl.country;
		while(list) {
			if (rpc->struct_add(cbh, "S", "Value", &list->s) < 0) {
				rpc->fault(ctx, 500, "Internal error creating inner struct");
				return;
			}
			list = list->next;
		}

		list = (*secf_data)->wl.country;
		while(list) {
			if (rpc->struct_add(cwh, "S", "Value", &list->s) < 0) {
				rpc->fault(ctx, 500, "Internal error creating inner struct");
				return;
			}
			list = list->next;
		}
	}

	if(showall == 1 || !strncmp(param.s, "domain", param.len)) {
		if (rpc->struct_add(handle, "{", "Domain", &dh) < 0)
		{
			rpc->fault(ctx, 500, "Internal error creating inner struct");
			return;
		}

		if (rpc->struct_add(dh, "{", "Blacklisted", &dbh) < 0)
		{
			rpc->fault(ctx, 500, "Internal error creating inner struct");
			return;
		}

		if (rpc->struct_add(dh, "{", "Whitelisted", &dwh) < 0)
		{
			rpc->fault(ctx, 500, "Internal error creating inner struct");
			return;
		}

		list = (*secf_data)->bl.domain;
		while(list) {
			if (rpc->struct_add(dbh, "S", "Value", &list->s) < 0) {
				rpc->fault(ctx, 500, "Internal error creating inner struct");
				return;
			}
			list = list->next;
		}

		list = (*secf_data)->wl.domain;
		while(list) {
			if (rpc->struct_add(dwh, "S", "Value", &list->s) < 0) {
				rpc->fault(ctx, 500, "Internal error creating inner struct");
				return;
			}
			list = list->next;
		}
	}

	if(showall == 1 || !strncmp(param.s, "ip", param.len)) {
		if (rpc->struct_add(handle, "{", "IP-Address", &iph) < 0)
		{
			rpc->fault(ctx, 500, "Internal error creating inner struct");
			return;
		}

		if (rpc->struct_add(iph, "{", "Blacklisted", &ipbh) < 0)
		{
			rpc->fault(ctx, 500, "Internal error creating inner struct");
			return;
		}

		if (rpc->struct_add(iph, "{", "Whitelisted", &ipwh) < 0)
		{
			rpc->fault(ctx, 500, "Internal error creating inner struct");
			return;
		}

		list = (*secf_data)->bl.ip;
		while(list) {
			if (rpc->struct_add(ipbh, "S", "Value", &list->s) < 0) {
				rpc->fault(ctx, 500, "Internal error creating inner struct");
				return;
			}
			list = list->next;
		}

		list = (*secf_data)->wl.ip;
		while(list) {
			if (rpc->struct_add(ipwh, "S", "Value", &list->s) < 0) {
				rpc->fault(ctx, 500, "Internal error creating inner struct");
				return;
			}
			list = list->next;
		}
	}

	if(showall == 1 || !strncmp(param.s, "user", param.len)) {
		if (rpc->struct_add(handle, "{", "Username", &usrh) < 0)
		{
			rpc->fault(ctx, 500, "Internal error creating inner struct");
			return;
		}

		if (rpc->struct_add(usrh, "{", "Blacklisted", &usrbh) < 0)
		{
			rpc->fault(ctx, 500, "Internal error creating inner struct");
			return;
		}

		if (rpc->struct_add(usrh, "{", "Whitelisted", &usrwh) < 0)
		{
			rpc->fault(ctx, 500, "Internal error creating inner struct");
			return;
		}

		list = (*secf_data)->bl.user;
		while(list) {
			if (rpc->struct_add(usrbh, "S", "Value", &list->s) < 0) {
				rpc->fault(ctx, 500, "Internal error creating inner struct");
				return;
			}
			list = list->next;
		}

		list = (*secf_data)->wl.user;
		while(list) {
			if (rpc->struct_add(usrwh, "S", "Value", &list->s) < 0) {
				rpc->fault(ctx, 500, "Internal error creating inner struct");
				return;
			}
			list = list->next;
		}
	}
}

/* Print stats */
void secf_rpc_stats(rpc_t *rpc, void *ctx)
{
	void *handle;
	void *bh;
	void *wh;
	void *oh;

	/* Create empty structure and obtain its handle */
	if (rpc->add(ctx, "{", &handle) < 0) return;

	/* Create branchesempty structure and obtain its handle */
	if (rpc->struct_add(handle, "{", "Blacklist", &bh) < 0)
	{
		rpc->fault(ctx, 500, "Internal error creating inner struct");
		return;
	}

	if (rpc->struct_add(handle, "{", "Whitelist", &wh) < 0)
	{
		rpc->fault(ctx, 500, "Internal error creating inner struct");
		return;
	}

	if (rpc->struct_add(handle, "{", "Other", &oh) < 0)
	{
		rpc->fault(ctx, 500, "Internal error creating inner struct");
		return;
	}

	/* Fill-in the structure */
	if (rpc->struct_add(bh, "dddddddddddd", "User-Agent", secf_stats[BL_UA],
			"Country", secf_stats[BL_COUNTRY],
			"From-Domain", secf_stats[BL_FDOMAIN],
			"To-Domain", secf_stats[BL_TDOMAIN],
			"Contact-Domain", secf_stats[BL_CDOMAIN],
			"IP-Address", secf_stats[BL_IP],
			"From-Name", secf_stats[BL_FNAME],
			"To-Name", secf_stats[BL_TNAME],
			"Contact-Name", secf_stats[BL_CNAME],
			"From-User", secf_stats[BL_FUSER],
			"To-User", secf_stats[BL_TUSER],
			"Contact-User", secf_stats[BL_CUSER]) < 0) {
		rpc->fault(ctx, 500, "Internal error creating inner struct");
		return;
	}

	if (rpc->struct_add(wh, "dddddddddddd", "User-Agent", secf_stats[WL_UA],
			"Country", secf_stats[WL_COUNTRY],
			"From-Domain", secf_stats[WL_FDOMAIN],
			"To-Domain", secf_stats[WL_TDOMAIN],
			"Contact-Domain", secf_stats[WL_CDOMAIN],
			"IP-Address", secf_stats[WL_IP],
			"From-Name", secf_stats[WL_FNAME],
			"To-Name", secf_stats[WL_TNAME],
			"Contact-Name", secf_stats[WL_CNAME],
			"From-User", secf_stats[WL_FUSER],
			"To-User", secf_stats[WL_TUSER],
			"Contact-User", secf_stats[WL_CUSER]) < 0) {
		rpc->fault(ctx, 500, "Internal error creating inner struct");
		return;
	}

	if (rpc->struct_add(oh, "dd", "Destination", secf_stats[BL_DST],
			"SQL-Injection", secf_stats[BL_SQL]) < 0) {
		rpc->fault(ctx, 500, "Internal error creating inner struct");
		return;
	}
}

/* Reset stats */
void secf_rpc_stats_reset(rpc_t *rpc, void *ctx)
{
	secf_reset_stats();
	rpc->rpl_printf(ctx, "The statistics has been reset");
}