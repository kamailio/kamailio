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

#include "security.h"


/* RPC commands */

/* Add blacklist values to database */
void rpc_add_bl(rpc_t *rpc, void *ctx)
{
	char *type;
	char *value;

        if(rpc->scan(ctx, "ss", (char*)(&type), (char*)(&value))<2)
        {
	        rpc->fault(ctx, 0, "Invalid Parameters. Usage: security.add_bl type value\n     Example: security.add_bl user sipvicious");
	}
	else
	{
		if (insert_db(0, type, value) == 0)
		{
			load_data_from_db();
			rpc->rpl_printf(ctx, "Values (%s, %s) inserted into blacklist table", type, value);
		}
		else
		{
			rpc->rpl_printf(ctx, "Error insert values in blacklist table");
		}
	}
}


/* Add whitelist values to database */
void rpc_add_wl(rpc_t *rpc, void *ctx)
{
	char *type;
	char *value;

        if(rpc->scan(ctx, "ss", (char*)(&type), (char*)(&value))<2)
        {
	        rpc->fault(ctx, 0, "Invalid Parameters. Usage: security.add_wl type value\n     Example: security.add_wl user trusted_user");
	}
	else
	{
		if (insert_db(1, type, value) == 0)
		{
			load_data_from_db();
			rpc->rpl_printf(ctx, "Values (%s, %s) inserted into whitelist table", type, value);
		}
		else
		{
			rpc->rpl_printf(ctx, "Error insert values in whitelist table");
		}
	}
}


/* Reload arrays */
void rpc_reload(rpc_t *rpc, void *ctx)
{
	load_data_from_db();
	rpc->rpl_printf(ctx, "Data reloaded");
}


/* Print values */
void rpc_print(rpc_t *rpc, void *ctx)
{
	char *param = NULL;
	int showall = 0;
	int i;
	
	if (rpc->scan(ctx, "s", (char*)(&param))<1)
		showall = 1;
		
	if (!strcmp(param, "dst"))
	{
		rpc->rpl_printf(ctx, "");
		rpc->rpl_printf(ctx, "Destinations");
		rpc->rpl_printf(ctx, "============");
		rpc->rpl_printf(ctx, "[+] Blacklisted");
		rpc->rpl_printf(ctx, "    -----------");
		for (i = 0; i < *nDst; i++)
		{
			rpc->rpl_printf(ctx, "    %04d -> %s", (i+1), dst_list[i]);
		}
	}

	if (showall == 1 || !strcmp(param, "ua"))
	{
		rpc->rpl_printf(ctx, "");
		rpc->rpl_printf(ctx, "User-agent");
		rpc->rpl_printf(ctx, "==========");
		rpc->rpl_printf(ctx, "[+] Blacklisted");
		rpc->rpl_printf(ctx, "    -----------");
		for (i = 0; i < *nblUa; i++)
		{
			rpc->rpl_printf(ctx, "    %04d -> %s", (i+1), bl_ua_list[i]);
		}
		rpc->rpl_printf(ctx, "");
		rpc->rpl_printf(ctx, "[+] Whitelisted");
		rpc->rpl_printf(ctx, "    -----------");
		for (i = 0; i < *nwlUa; i++)
		{
			rpc->rpl_printf(ctx, "    %04d -> %s", (i+1), wl_ua_list[i]);
		}
	}

	if (showall == 1 || !strcmp(param, "country"))
	{
		rpc->rpl_printf(ctx, "");
		rpc->rpl_printf(ctx, "Country");
		rpc->rpl_printf(ctx, "=======");
		rpc->rpl_printf(ctx, "[+] Blacklisted");
		rpc->rpl_printf(ctx, "    -----------");
		for (i = 0; i < *nblCountry; i++)
		{
			rpc->rpl_printf(ctx, "    %04d -> %s", (i+1), bl_country_list[i]);
		}
		rpc->rpl_printf(ctx, "");
		rpc->rpl_printf(ctx, "[+] Whitelisted");
		rpc->rpl_printf(ctx, "    -----------");
		for (i = 0; i < *nwlCountry; i++)
		{
			rpc->rpl_printf(ctx, "    %04d -> %s", (i+1), wl_country_list[i]);
		}
	}

	if (showall == 1 || !strcmp(param, "domain"))
	{
		rpc->rpl_printf(ctx, "");
		rpc->rpl_printf(ctx, "Domain");
		rpc->rpl_printf(ctx, "======");
		rpc->rpl_printf(ctx, "[+] Blacklisted");
		rpc->rpl_printf(ctx, "    -----------");
		for (i = 0; i < *nblDomain; i++)
		{
			rpc->rpl_printf(ctx, "    %04d -> %s", (i+1), bl_domain_list[i]);
		}
		rpc->rpl_printf(ctx, "");
		rpc->rpl_printf(ctx, "[+] Whitelisted");
		rpc->rpl_printf(ctx, "    -----------");
		for (i = 0; i < *nwlDomain; i++)
		{
			rpc->rpl_printf(ctx, "    %04d -> %s", (i+1), wl_domain_list[i]);
		}
	}
	
	if (showall == 1 || !strcmp(param, "user"))
	{
		rpc->rpl_printf(ctx, "");
		rpc->rpl_printf(ctx, "User");
		rpc->rpl_printf(ctx, "====");
		rpc->rpl_printf(ctx, "[+] Blacklisted");
		rpc->rpl_printf(ctx, "    -----------");
		for (i = 0; i < *nblUser; i++)
		{
			rpc->rpl_printf(ctx, "    %04d -> %s", (i+1), bl_user_list[i]);
		}
		rpc->rpl_printf(ctx, "");
		rpc->rpl_printf(ctx, "[+] Whitelisted");
		rpc->rpl_printf(ctx, "    -----------");
		for (i = 0; i < *nwlUser; i++)
		{
			rpc->rpl_printf(ctx, "    %04d -> %s", (i+1), wl_user_list[i]);
		}
	}

	if (showall == 1 || !strcmp(param, "ip"))
	{
		rpc->rpl_printf(ctx, "");
		rpc->rpl_printf(ctx, "IP Address");
		rpc->rpl_printf(ctx, "==========");
		rpc->rpl_printf(ctx, "[+] Blacklisted");
		rpc->rpl_printf(ctx, "    -----------");
		for (i = 0; i < *nblIp; i++)
		{
			rpc->rpl_printf(ctx, "    %04d -> %s", (i+1), bl_ip_list[i]);
		}
		rpc->rpl_printf(ctx, "");
		rpc->rpl_printf(ctx, "[+] Whitelisted");
		rpc->rpl_printf(ctx, "    -----------");
		for (i = 0; i < *nwlIp; i++)
		{
			rpc->rpl_printf(ctx, "    %04d -> %s", (i+1), wl_ip_list[i]);
		}
	}

	rpc->rpl_printf(ctx, "");
}
