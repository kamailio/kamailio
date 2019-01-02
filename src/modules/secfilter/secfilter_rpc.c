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


static int get_type(char *ctype)
{
	int type;

	if(!strcmp(ctype, "ua")) {
		type = 0;
	} else if(!strcmp(ctype, "country")) {
		type = 1;
	} else if(!strcmp(ctype, "domain")) {
		type = 2;
	} else if(!strcmp(ctype, "ip")) {
		type = 3;
	} else if(!strcmp(ctype, "user")) {
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
		rpc->fault(ctx, 0,
				"Invalid Parameters. Usage: secfilter.add_dst "
				"number\n     Example: secfilter.add_dst "
				"555123123");
		return;
	}
	text = int2str(number, &data.len);
	data.s = pkg_malloc(data.len * sizeof(char));
	if(!data.s) {
		PKG_MEM_ERROR;
		rpc->rpl_printf(ctx, "Error insert values in the blacklist");
		return;
	}
	memcpy(data.s, text, data.len);
	lock_get(&secf_data->lock);
	if(secf_append_rule(2, 0, &data) == 0) {
		rpc->rpl_printf(ctx,
				"Values (%s) inserted into blacklist destinations", data);
	} else {
		rpc->rpl_printf(ctx, "Error insert values in the blacklist");
	}
	lock_release(&secf_data->lock);
	if(data.s)
		pkg_free(data.s);
}

/* Add blacklist value */
void secf_rpc_add_bl(rpc_t *rpc, void *ctx)
{
	char *ctype = NULL;
	str data = STR_NULL;
	int type;

	if(rpc->scan(ctx, "ss", ctype, &data.s) < 2) {
		rpc->fault(ctx, 0,
				"Invalid Parameters. Usage: secfilter.add_bl type "
				"value\n     Example: secfilter.add_bl user "
				"sipvicious");
		return;
	}
	data.len = strlen(data.s);
	type = get_type(ctype);

	lock_get(&secf_data->lock);
	if(secf_append_rule(0, type, &data) == 0) {
		rpc->rpl_printf(ctx, "Values (%s, %s) inserted into blacklist",
				ctype, data);
	} else {
		rpc->rpl_printf(ctx, "Error insert values in the blacklist");
	}
	lock_release(&secf_data->lock);
}


/* Add whitelist value */
void secf_rpc_add_wl(rpc_t *rpc, void *ctx)
{
	char *ctype = NULL;
	str data = STR_NULL;
	int type;

	if(rpc->scan(ctx, "ss", ctype, &data.s) < 2) {
		rpc->fault(ctx, 0,
				"Invalid Parameters. Usage: secfilter.add_wl type "
				"value\n     Example: secfilter.add_wl user "
				"trusted_user");
		return;
	}
	data.len = strlen(data.s);
	type = get_type(ctype);

	lock_get(&secf_data->lock);
	if(secf_append_rule(1, type, &data) == 0) {
		rpc->rpl_printf(
				ctx, "Values (%s, %s) inserted into whitelist", type, data);
	} else {
		rpc->rpl_printf(ctx, "Error insert values in the whitelist");
	}
	lock_release(&secf_data->lock);
}


/* Reload arrays */
void secf_rpc_reload(rpc_t *rpc, void *ctx)
{
	secf_free_data();

	if(secf_load_db() == -1) {
		LM_ERR("Error loading data from database\n");
		rpc->rpl_printf(ctx, "Error loading data from database");
	} else {
		rpc->rpl_printf(ctx, "Data reloaded");
	}
}


/* Print str_list data */
static void rpc_print_data(rpc_t *rpc, void *ctx, struct str_list *list)
{
	int i = 1;

	while(list) {
		rpc->rpl_printf(ctx, "    %04d -> %.*s", i, list->s.len, list->s.s);
		list = list->next;
		i++;
	}
}


/* Print values */
void secf_rpc_print(rpc_t *rpc, void *ctx)
{
	char *param = NULL;
	int showall = 0;

	if(rpc->scan(ctx, "s", (char *)(&param)) < 1)
		showall = 1;

	if(!strcmp(param, "dst")) {
		rpc->rpl_printf(ctx, "");
		rpc->rpl_printf(ctx, "Destinations");
		rpc->rpl_printf(ctx, "============");
		rpc->rpl_printf(ctx, "[+] Blacklisted");
		rpc->rpl_printf(ctx, "    -----------");
		rpc_print_data(rpc, ctx, secf_data->bl.dst);
	}

	if(showall == 1 || !strcmp(param, "ua")) {
		rpc->rpl_printf(ctx, "");
		rpc->rpl_printf(ctx, "User-agent");
		rpc->rpl_printf(ctx, "==========");
		rpc->rpl_printf(ctx, "[+] Blacklisted");
		rpc->rpl_printf(ctx, "    -----------");
		rpc_print_data(rpc, ctx, secf_data->bl.ua);
		rpc->rpl_printf(ctx, "");
		rpc->rpl_printf(ctx, "[+] Whitelisted");
		rpc->rpl_printf(ctx, "    -----------");
		rpc_print_data(rpc, ctx, secf_data->wl.ua);
	}

	if(showall == 1 || !strcmp(param, "country")) {
		rpc->rpl_printf(ctx, "");
		rpc->rpl_printf(ctx, "Country");
		rpc->rpl_printf(ctx, "=======");
		rpc->rpl_printf(ctx, "[+] Blacklisted");
		rpc->rpl_printf(ctx, "    -----------");
		rpc_print_data(rpc, ctx, secf_data->bl.country);
		rpc->rpl_printf(ctx, "");
		rpc->rpl_printf(ctx, "[+] Whitelisted");
		rpc->rpl_printf(ctx, "    -----------");
		rpc_print_data(rpc, ctx, secf_data->wl.country);
	}

	if(showall == 1 || !strcmp(param, "domain")) {
		rpc->rpl_printf(ctx, "");
		rpc->rpl_printf(ctx, "Domain");
		rpc->rpl_printf(ctx, "======");
		rpc->rpl_printf(ctx, "[+] Blacklisted");
		rpc->rpl_printf(ctx, "    -----------");
		rpc_print_data(rpc, ctx, secf_data->bl.domain);
		rpc->rpl_printf(ctx, "");
		rpc->rpl_printf(ctx, "[+] Whitelisted");
		rpc->rpl_printf(ctx, "    -----------");
		rpc_print_data(rpc, ctx, secf_data->wl.domain);
	}

	if(showall == 1 || !strcmp(param, "ip")) {
		rpc->rpl_printf(ctx, "");
		rpc->rpl_printf(ctx, "IP Address");
		rpc->rpl_printf(ctx, "==========");
		rpc->rpl_printf(ctx, "[+] Blacklisted");
		rpc->rpl_printf(ctx, "    -----------");
		rpc_print_data(rpc, ctx, secf_data->bl.ip);
		rpc->rpl_printf(ctx, "");
		rpc->rpl_printf(ctx, "[+] Whitelisted");
		rpc->rpl_printf(ctx, "    -----------");
		rpc_print_data(rpc, ctx, secf_data->wl.ip);
	}

	if(showall == 1 || !strcmp(param, "user")) {
		rpc->rpl_printf(ctx, "");
		rpc->rpl_printf(ctx, "User");
		rpc->rpl_printf(ctx, "====");
		rpc->rpl_printf(ctx, "[+] Blacklisted");
		rpc->rpl_printf(ctx, "    -----------");
		rpc_print_data(rpc, ctx, secf_data->bl.user);
		rpc->rpl_printf(ctx, "");
		rpc->rpl_printf(ctx, "[+] Whitelisted");
		rpc->rpl_printf(ctx, "    -----------");
		rpc_print_data(rpc, ctx, secf_data->wl.user);
	}

	rpc->rpl_printf(ctx, "");
}
