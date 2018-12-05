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

#include "../../core/sr_module.h"
#include "../../core/mem/shm_mem.h"
#include "../../core/rpc_lookup.h"
#include "../../lib/srdb1/db.h"
#include "security.h"

MODULE_VERSION

static int mod_init(void);
static int child_init(int rank);
static void mod_destroy(void);

/* External functions */
static int w_check_ua(struct sip_msg* msg, char *val);
static int w_check_domain(struct sip_msg* msg, char *val);
static int w_check_country(struct sip_msg* msg, char *val);
static int w_check_user(struct sip_msg* msg, char *val);
static int w_check_ip(struct sip_msg* msg, char *val);
static int w_check_dst(struct sip_msg* msg, char *val);
static int w_check_sqli(struct sip_msg* msg);

/* Database variables */
db_func_t db_funcs;       /* Database API functions */
db1_con_t* db_handle=0;   /* Database connection handle */

/* Exported module parameters - default values */
int dst_exact_match = 1;
str db_url = {NULL, 0};
str table_name = str_init("security");
str action_col = str_init("action");
str type_col = str_init("type");
str data_col = str_init("data");

/* Exported commands */
static cmd_export_t cmds[]={
        {"check_ua",      (cmd_function)w_check_ua     , 1, 0, 0, ANY_ROUTE},
        {"check_domain",  (cmd_function)w_check_domain , 1, 0, 0, ANY_ROUTE},
        {"check_country", (cmd_function)w_check_country, 1, 0, 0, ANY_ROUTE},
        {"check_user",    (cmd_function)w_check_user   , 1, 0, 0, ANY_ROUTE},
        {"check_ip",      (cmd_function)w_check_ip     , 1, 0, 0, ANY_ROUTE},
        {"check_dst",     (cmd_function)w_check_dst    , 1, 0, 0, ANY_ROUTE},
        {"check_sqli",    (cmd_function)w_check_sqli   , 0, 0, 0, ANY_ROUTE},
        {0, 0, 0, 0, 0, 0}
};

/* Exported module parameters */
static param_export_t params[]={
        {"db_url",          PARAM_STRING, &db_url},
        {"table_name",      PARAM_STR, &table_name },
        {"action_col",      PARAM_STR, &action_col },
        {"type_col",        PARAM_STR, &type_col },
        {"data_col",        PARAM_STR, &data_col },
        {"dst_exact_match", PARAM_INT, &dst_exact_match },
        {0, 0, 0}
};

/* Module exports definition */
struct module_exports exports={
	"security",		/* module name */
	DEFAULT_DLFLAGS,	/* dlopen flags */
	cmds,			/* exported functions */
	params,			/* exported parameters */
	0,			/* RPC method exports */
	0,			/* exported pseudo-variables */
	0,			/* response handling function */
	mod_init,		/* module initialization function */
	child_init,		/* per-child init function */
	mod_destroy		/* module destroy function */
};

/* RPC exported commands */
static const char *rpc_reload_doc[2] = {
	"Reload values from database", NULL};

static const char *rpc_print_doc[2] = {
	"Print values from database", NULL};

static const char *rpc_add_bl_doc[2] = {
	"Add new values to blacklist", NULL};

static const char *rpc_add_wl_doc[2] = {
	"Add new values to whitelist", NULL};

rpc_export_t security_rpc[] = {
        { "security.reload",  rpc_reload, rpc_reload_doc, 0},
        { "security.print",   rpc_print, rpc_print_doc, 0},
        { "security.add_bl",  rpc_add_bl, rpc_add_bl_doc, 0},
        { "security.add_wl",  rpc_add_wl, rpc_add_wl_doc, 0},
        { 0, 0, 0, 0}
};


/* Prevent SQL injection */
static int w_check_sqli(struct sip_msg *msg)
{
	if (check_sqli_ua(msg) == 0)      return 0;
	if (check_sqli_to(msg) == 0)      return 0;
	if (check_sqli_from(msg) == 0)    return 0;
	if (check_sqli_contact(msg) == 0) return 0;

	return 1;
}


/* Check if the current user-agent is allowed */
static int w_check_dst(struct sip_msg *msg, char *val)
{
	int i;

	uppercase(val);

	if (dst_exact_match == 0)
	{
		for (i = 0; i < *nDst; i++)
		{
			/* Find any match */
			if (strstr(val, dst_list[i])) return -1;
		}
	}
	else
	{
		for (i = 0; i < *nDst; i++)
		{
			/* Find an exact match */
			if (strlen(val) == strlen(dst_list[i]) && strcmp(val, dst_list[i])) return -1;
		}
	}
	
	return 1;
}


/* Check if the current user-agent is allowed */
static int w_check_ua(struct sip_msg *msg, char *val)
{
	int i;

	uppercase(val);

	for (i = 0; i < *nwlUa; i++)
	{
		/* User-agent whitelisted */
		if (strstr(val, wl_ua_list[i])) return 2;
	}
	for (i = 0; i < *nblUa; i++)
	{
		/* User-agent blacklisted */
		if (strstr(val, bl_ua_list[i])) return -1;
	}
	
	return 1;
}


/* Check if the current domain is allowed */
static int w_check_domain(struct sip_msg *msg, char *val)
{
	int i;
	
	uppercase(val);

	for (i = 0; i < *nwlDomain; i++)
	{
		/* Domain whitelisted */
		if (strstr(val, wl_domain_list[i])) return 2;
	}
	for (i = 0; i < *nblDomain; i++)
	{
		/* Domain blacklisted */
		if (strstr(val, bl_domain_list[i])) return -1;
	}

	return 1;
}


/* Check if the current country is allowed */
static int w_check_country(struct sip_msg *msg, char *val)
{
	int i;
	
	uppercase(val);

	for (i = 0; i < *nwlCountry; i++)
	{
		/* Country whitelisted */
		if (strstr(val, wl_country_list[i])) return 2;
	}
	for (i = 0; i < *nblCountry; i++)
	{
		/* Country blacklisted */
		if (strstr(val, bl_country_list[i])) return -1;
	}

	return 1;
}


/* Check if the current user is allowed */
static int w_check_user(struct sip_msg *msg, char *val)
{
	int i;
	
	uppercase(val);

	for (i = 0; i < *nwlUser; i++)
	{
		/* User whitelisted */
		if (strstr(val, wl_user_list[i])) return 2;
	}
	for (i = 0; i < *nblUser; i++)
	{
		/* User blacklisted */
		if (strstr(val, bl_user_list[i])) return -1;
	}

	return 1;
}


/* Check if the current IP is allowed */
static int w_check_ip(struct sip_msg *msg, char *val)
{
	int i;
	
	uppercase(val);

	for (i = 0; i < *nwlIp; i++)
	{
		/* IP address whitelisted */
		if (strstr(val, wl_ip_list[i])) return 2;
	}
	for (i = 0; i < *nblIp; i++)
	{
		/* IP address blacklisted */
		if (strstr(val, bl_ip_list[i])) return -1;
	}

	return 1;
}


/* toUppercase */
void uppercase(char *sPtr)
{
	while(*sPtr != '\0')
	{
		*sPtr = toupper((unsigned char) *sPtr);
		++sPtr;
	}
}


/* Module init function */
static int mod_init(void)
{
	int i;

	LM_INFO("SECURITY module init\n");

	/* Register RPC commands */
        if (rpc_register_array(security_rpc) != 0)
        {
                LM_ERR("failed to register RPC commands\n");
                return -1;
        }
        LM_INFO("RPC commands registered\n");

	nblUa      = (int *)shm_malloc(sizeof(int));
	nblCountry = (int *)shm_malloc(sizeof(int));
	nblDomain  = (int *)shm_malloc(sizeof(int));
	nblUser    = (int *)shm_malloc(sizeof(int));
	nblIp      = (int *)shm_malloc(sizeof(int));

	nwlUa      = (int *)shm_malloc(sizeof(int));
	nwlCountry = (int *)shm_malloc(sizeof(int));
	nwlDomain  = (int *)shm_malloc(sizeof(int));
	nwlUser    = (int *)shm_malloc(sizeof(int));
	nwlIp      = (int *)shm_malloc(sizeof(int));

	nDst       = (int *)shm_malloc(sizeof(int));

	*bl_ua_list      = shm_malloc(1024*sizeof(char *));
	*bl_country_list = shm_malloc(1024*sizeof(char *));
	*bl_domain_list  = shm_malloc(1024*sizeof(char *));
	*bl_user_list    = shm_malloc(1024*sizeof(char *));
	*bl_ip_list      = shm_malloc(1024*sizeof(char *));

	*wl_ua_list      = shm_malloc(1024*sizeof(char *));
	*wl_country_list = shm_malloc(1024*sizeof(char *));
	*wl_domain_list  = shm_malloc(1024*sizeof(char *));
	*wl_user_list    = shm_malloc(1024*sizeof(char *));
	*wl_ip_list      = shm_malloc(1024*sizeof(char *));

	*dst_list        = shm_malloc(1024*sizeof(char *));

	for (i = 0; i < 1024; i++)
	{
		bl_ua_list[i]      = (char *)shm_malloc(255*sizeof(char));
		bl_country_list[i] = (char *)shm_malloc(255*sizeof(char));
		bl_domain_list[i]  = (char *)shm_malloc(255*sizeof(char));
		bl_user_list[i]    = (char *)shm_malloc(255*sizeof(char));
		bl_ip_list[i]      = (char *)shm_malloc(255*sizeof(char));

		wl_ua_list[i]      = (char *)shm_malloc(255*sizeof(char));
		wl_country_list[i] = (char *)shm_malloc(255*sizeof(char));
		wl_domain_list[i]  = (char *)shm_malloc(255*sizeof(char));
		wl_user_list[i]    = (char *)shm_malloc(255*sizeof(char));
		wl_ip_list[i]      = (char *)shm_malloc(255*sizeof(char));

		dst_list[i]        = (char *)shm_malloc(255*sizeof(char));
	}

        if (dst_exact_match != 0)
        	dst_exact_match = 1;

        if (init_db() == -1) return -1;
        	
	load_data_from_db();

	return 0;
}


/* Module child init function */
static int child_init(int rank)
{
        if (rank==PROC_INIT || rank==PROC_MAIN || rank==PROC_TCP_MAIN)
                return 0; /* do nothing for the main process */

	return 1;
}


/* Module destroy function */
static void mod_destroy(void)
{
	LM_INFO("SECURITY module destroy\n");

	if (nblUa)      shm_free(nblUa);
	if (nblCountry) shm_free(nblCountry);
	if (nblDomain)  shm_free(nblDomain);
	if (nblUser)    shm_free(nblUser);
	if (nblIp)      shm_free(nblIp);

	if (nwlUa)      shm_free(nwlUa);
	if (nwlCountry) shm_free(nwlCountry);
	if (nwlDomain)  shm_free(nwlDomain);
	if (nwlUser)    shm_free(nwlUser);
	if (nwlIp)      shm_free(nwlIp);

	if (nDst)       shm_free(nDst);
}
