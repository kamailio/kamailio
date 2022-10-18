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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../core/sr_module.h"
#include "../../core/mem/shm_mem.h"
#include "../../core/rpc_lookup.h"
#include "../../core/strutils.h"
#include "../../lib/srdb1/db.h"
#include "../../core/dprint.h"
#include "../../core/locking.h"
#include "secfilter.h"

MODULE_VERSION

secf_data_p secf_data = NULL;
static gen_lock_t *secf_lock = NULL;
int *secf_stats;
int total_data = 26;

/* Static and shared functions */
static int mod_init(void);
int secf_init_data(void);
static int child_init(int rank);
static int rpc_init(void);
static void free_str_list(struct str_list *l);
static void free_sec_info(secf_info_p info);
void secf_free_data(void);
static void mod_destroy(void);
static int w_check_sqli(str val);
static int check_user(struct sip_msg *msg, int type);
void secf_reset_stats(void);

/* External functions */
static int w_check_ua(struct sip_msg *msg);
static int w_check_from_hdr(struct sip_msg *msg);
static int w_check_to_hdr(struct sip_msg *msg);
static int w_check_contact_hdr(struct sip_msg *msg);
static int w_check_ip(struct sip_msg *msg);
static int w_check_country(struct sip_msg *msg, char *val);
static int w_check_dst(struct sip_msg *msg, char *val);
static int w_check_sqli_all(struct sip_msg *msg);
static int w_check_sqli_hdr(struct sip_msg *msg, char *val);

/* Exported module parameters - default values */
int secf_dst_exact_match = 1;
str secf_db_url = str_init(DEFAULT_RODB_URL);
str secf_table_name = str_init("secfilter");
str secf_action_col = str_init("action");
str secf_type_col = str_init("type");
str secf_data_col = str_init("data");

/* clang-format off */
/* Exported commands */
static cmd_export_t cmds[] = {
		{"secf_check_ua", (cmd_function)w_check_ua, 0, 0, 0, ANY_ROUTE},
		{"secf_check_from_hdr", (cmd_function)w_check_from_hdr, 0, 0, 0,
				ANY_ROUTE},
		{"secf_check_to_hdr", (cmd_function)w_check_to_hdr, 0, 0, 0, ANY_ROUTE},
		{"secf_check_contact_hdr", (cmd_function)w_check_contact_hdr, 0, 0, 0,
				ANY_ROUTE},
		{"secf_check_ip", (cmd_function)w_check_ip, 0, 0, 0, ANY_ROUTE},
		{"secf_check_country", (cmd_function)w_check_country, 1, 0, 0,
				ANY_ROUTE},
		{"secf_check_dst", (cmd_function)w_check_dst, 1, 0, 0, ANY_ROUTE},
		{"secf_check_sqli_all", (cmd_function)w_check_sqli_all, 0, 0, 0,
				ANY_ROUTE},
		{"secf_check_sqli_hdr", (cmd_function)w_check_sqli_hdr, 1, 0, 0,
				ANY_ROUTE},
		{0, 0, 0, 0, 0, 0}};

/* Exported module parameters */
static param_export_t params[] = {{"db_url", PARAM_STRING, &secf_db_url},
		{"table_name", PARAM_STR, &secf_table_name},
		{"action_col", PARAM_STR, &secf_action_col},
		{"type_col", PARAM_STR, &secf_type_col},
		{"data_col", PARAM_STR, &secf_data_col},
		{"dst_exact_match", PARAM_INT, &secf_dst_exact_match}, {0, 0, 0}};

/* Module exports definition */
struct module_exports exports = {
		"secfilter",	 /* module name */
		DEFAULT_DLFLAGS, /* dlopen flags */
		cmds,		 /* exported functions */
		params,		 /* exported parameters */
		0,		 /* RPC method exports */
		0,		 /* exported pseudo-variables */
		0,		 /* response handling function */
		mod_init,	 /* module initialization function */
		child_init,	 /* per-child init function */
		mod_destroy	 /* module destroy function */
};

/* RPC exported commands */
static const char *rpc_reload_doc[2] = {"Reload values from database", NULL};
static const char *rpc_print_doc[2] = {"Print values from database", NULL};
static const char *rpc_stats_doc[2] = {"Print statistics of blocked and allowed messages", NULL};
static const char *rpc_stats_reset_doc[2] = {"Reset statistics", NULL};
static const char *rpc_add_dst_doc[2] = {
		"Add new values to destination blacklist", NULL};
static const char *rpc_add_bl_doc[2] = {"Add new values to blacklist", NULL};
static const char *rpc_add_wl_doc[2] = {"Add new values to whitelist", NULL};

rpc_export_t secfilter_rpc[] = {
		{"secfilter.reload", secf_rpc_reload, rpc_reload_doc, 0},
		{"secfilter.print", secf_rpc_print, rpc_print_doc, 0},
		{"secfilter.stats", secf_rpc_stats, rpc_stats_doc, 0},
		{"secfilter.stats_reset", secf_rpc_stats_reset, rpc_stats_reset_doc, 0},
		{"secfilter.add_dst", secf_rpc_add_dst, rpc_add_dst_doc, 0},
		{"secfilter.add_bl", secf_rpc_add_bl, rpc_add_bl_doc, 0},
		{"secfilter.add_wl", secf_rpc_add_wl, rpc_add_wl_doc, 0}, {0, 0, 0, 0}};
/* clang-format on */

/***
PREVENT SQL INJECTION
***/

/* External function to search for illegal characters in several headers */
static int w_check_sqli_all(struct sip_msg *msg)
{
	str ua = STR_NULL;
	str name = STR_NULL;
	str user = STR_NULL;
	str domain = STR_NULL;
	int res;
	int retval = 1;

	/* Find SQLi in user-agent header */
	res = secf_get_ua(msg, &ua);
	if(res == 0) {
		if(w_check_sqli(ua) != 1) {
			LM_INFO("Possible SQL injection found in User-agent (%.*s)\n",
					ua.len, ua.s);
			retval = 0;
			goto end_sqli;
		}
	}

	/* Find SQLi in from header */
	res = secf_get_from(msg, &name, &user, &domain);
	if(res == 0) {
		if(name.len > 0) {
			if(w_check_sqli(name) != 1) {
				LM_INFO("Possible SQL injection found in From name (%.*s)\n",
						name.len, name.s);
				retval = 0;
				goto end_sqli;
			}
		}

		if(user.len > 0) {
			if(w_check_sqli(user) != 1) {
				LM_INFO("Possible SQL injection found in From user (%.*s)\n",
						user.len, user.s);
				retval = 0;
				goto end_sqli;
			}
		}

		if(domain.len > 0) {
			if(w_check_sqli(domain) != 1) {
				LM_INFO("Possible SQL injection found in From domain (%.*s)\n",
						domain.len, domain.s);
				retval = 0;
				goto end_sqli;
			}
		}
	}

	/* Find SQLi in to header */
	res = secf_get_to(msg, &name, &user, &domain);
	if(res == 0) {
		if(name.len > 0) {
			if(w_check_sqli(name) != 1) {
				LM_INFO("Possible SQL injection found in To name (%.*s)\n",
						name.len, name.s);
				retval = 0;
				goto end_sqli;
			}
		}

		if(user.len > 0) {
			if(w_check_sqli(user) != 1) {
				LM_INFO("Possible SQL injection found in To user (%.*s)\n",
						user.len, user.s);
				retval = 0;
				goto end_sqli;
			}
		}

		if(domain.len > 0) {
			if(w_check_sqli(domain) != 1) {
				LM_INFO("Possible SQL injection found in To domain (%.*s)\n",
						domain.len, domain.s);
				retval = 0;
				goto end_sqli;
			}
		}
	}

	/* Find SQLi in contact header */
	res = secf_get_contact(msg, &user, &domain);
	if(res == 0) {
		if(user.len > 0) {
			if(w_check_sqli(user) != 1) {
				LM_INFO("Possible SQL injection found in Contact user (%.*s)\n",
						user.len, user.s);
				retval = 0;
				goto end_sqli;
			}
		}

		if(domain.len > 0) {
			if(w_check_sqli(domain) != 1) {
				LM_INFO("Possible SQL injection found in Contact domain "
						"(%.*s)\n",
						domain.len, domain.s);
				retval = 0;
				goto end_sqli;
			}
		}
	}

end_sqli:
	return retval;
}


/* External function to search for illegal characters in some header */
static int w_check_sqli_hdr(struct sip_msg *msg, char *cval)
{
	str val;
	val.s = cval;
	val.len = strlen(cval);

	return w_check_sqli(val);
}


/* Search for illegal characters */
static int w_check_sqli(str val)
{
	char *cval;
	int res = 1;

	cval = (char *)pkg_malloc(val.len + 1);
	if(cval == NULL) {
		LM_CRIT("Cannot allocate pkg memory\n");
		return -2;
	}
	memset(cval, 0, val.len + 1);
	memcpy(cval, val.s, val.len);

	if(strstr(cval, "'") || strstr(cval, "\"") || strstr(cval, "--")
			|| strstr(cval, "%27") || strstr(cval, "%24")
			|| strstr(cval, "%60")) {
		/* Illegal characters found */
		lock_get(secf_lock);
		secf_stats[BL_SQL]++;
		lock_release(secf_lock);
		res = -1;
		goto end;
	}

end:
	if(cval)
		pkg_free(cval);

	return res;
}


/***
BLACKLIST AND WHITELIST
***/

/* Check if the current destination is allowed */
static int w_check_dst(struct sip_msg *msg, char *val)
{
	str dst;
	struct str_list *list;

	dst.s = val;
	dst.len = strlen(val);

	lock_get(&secf_data->lock);
	list = secf_data->bl.dst;
	while(list) {
		if(secf_dst_exact_match == 1) {
			/* Exact match */
			if(list->s.len == dst.len) {
				if(cmpi_str(&list->s, &dst) == 0) {
					lock_get(secf_lock);
					secf_stats[BL_DST]++;
					lock_release(secf_lock);
					lock_release(&secf_data->lock);
					return -2;
				}
			}
		} else {
			/* Any match */
			if(dst.len > list->s.len)
				dst.len = list->s.len;
			if(cmpi_str(&list->s, &dst) == 0) {
				lock_get(secf_lock);
				secf_stats[BL_DST]++;
				lock_release(secf_lock);
				lock_release(&secf_data->lock);
				return -2;
			}
		}
		list = list->next;
	}
	lock_release(&secf_data->lock);

	return 1;
}


/* Check if the current user-agent is allowed
Return codes:
 2 = user-agent whitelisted
 1 = not found
-1 = error
-2 = user-agent blacklisted
*/
static int w_check_ua(struct sip_msg *msg)
{
	int res, len;
	str ua;
	struct str_list *list;

	res = secf_get_ua(msg, &ua);
	if(res != 0)
		return res;

	len = ua.len;

	/* User-agent whitelisted */
	lock_get(&secf_data->lock);
	list = secf_data->wl.ua;
	while(list) {
		if(ua.len > list->s.len)
			ua.len = list->s.len;
		res = cmpi_str(&list->s, &ua);
		if(res == 0) {
			lock_get(secf_lock);
			secf_stats[WL_UA]++;
			lock_release(secf_lock);
			lock_release(&secf_data->lock);
			return 2;
		}
		list = list->next;
		ua.len = len;
	}

	/* User-agent blacklisted */
	list = secf_data->bl.ua;
	while(list) {
		if(ua.len > list->s.len)
			ua.len = list->s.len;
		res = cmpi_str(&list->s, &ua);
		if(res == 0) {
			lock_get(secf_lock);
			secf_stats[BL_UA]++;
			lock_release(secf_lock);
			return -2;
			lock_release(&secf_data->lock);
		}
		list = list->next;
		ua.len = len;
	}
	lock_release(&secf_data->lock);
	
	return 1;
}


/* Check if the current from user is allowed */
static int w_check_from_hdr(struct sip_msg *msg)
{
	return check_user(msg, 1);
}


/* Check if the current to user is allowed */
static int w_check_to_hdr(struct sip_msg *msg)
{
	return check_user(msg, 2);
}


/* Check if the current contact user is allowed */
static int w_check_contact_hdr(struct sip_msg *msg)
{
	return check_user(msg, 3);
}


/* 
Check if the current user is allowed 

Return codes:
 4 = name whitelisted
 3 = domain whitelisted
 2 = user whitelisted
 1 = not found
-1 = error
-2 = user blacklisted
-3 = domain blacklisted
-4 = name blacklisted
*/
static int check_user(struct sip_msg *msg, int type)
{
	str name = STR_NULL;
	str user = STR_NULL;
	str domain = STR_NULL;
	int res = 0;
	int nlen, ulen, dlen;
	struct str_list *list = NULL;

	switch(type) {
		case 1:
			res = secf_get_from(msg, &name, &user, &domain);
			break;
		case 2:
			res = secf_get_to(msg, &name, &user, &domain);
			break;
		case 3:
			res = secf_get_contact(msg, &user, &domain);
			break;
		default:
			return -1;
	}
	if(res != 0) {
		return res;
	}
	
	if (user.s == NULL || domain.s == NULL) {
		return -1;
	}

	nlen = name.len;
	ulen = user.len;
	dlen = domain.len;

	/* User whitelisted */
	lock_get(&secf_data->lock);
	list = secf_data->wl.user;
	while(list) {
		if(name.len > list->s.len)
			name.len = list->s.len;
		if (name.s != NULL) {
			res = cmpi_str(&list->s, &name);
			if(res == 0) {
				lock_get(secf_lock);
				switch(type) {
					case 1:
						secf_stats[WL_FNAME]++;
						break;
					case 2:
						secf_stats[WL_TNAME]++;
						break;
					case 3:
						secf_stats[WL_CNAME]++;
						break;
				}
				lock_release(secf_lock);
				lock_release(&secf_data->lock);
				return 4;
			}
		}
		if(user.len > list->s.len)
			user.len = list->s.len;
		res = cmpi_str(&list->s, &user);
		if(res == 0) {
			lock_get(secf_lock);
			switch(type) {
				case 1:
					secf_stats[WL_FUSER]++;
					break;
				case 2:
					secf_stats[WL_TUSER]++;
					break;
				case 3:
					secf_stats[WL_CUSER]++;
					break;
			}
			lock_release(secf_lock);
			lock_release(&secf_data->lock);
			return 2;
		}
		list = list->next;
		name.len = nlen;
		user.len = ulen;
	}
	/* User blacklisted */
	list = secf_data->bl.user;
	while(list) {
		if(name.len > list->s.len)
			name.len = list->s.len;
		if (name.s != NULL) {
			res = cmpi_str(&list->s, &name);
			if(res == 0) {
				lock_get(secf_lock);
				switch(type) {
					case 1:
						secf_stats[BL_FNAME]++;
						break;
					case 2:
						secf_stats[BL_TNAME]++;
						break;
					case 3:
						secf_stats[BL_CNAME]++;
						break;
				}
				lock_release(secf_lock);
				lock_release(&secf_data->lock);
				return -4;
			}
		}
		if(user.len > list->s.len)
			user.len = list->s.len;
		res = cmpi_str(&list->s, &user);
		if(res == 0) {
			lock_get(secf_lock);
			switch(type) {
				case 1:
					secf_stats[BL_FUSER]++;
					break;
				case 2:
					secf_stats[BL_TUSER]++;
					break;
				case 3:
					secf_stats[BL_CUSER]++;
					break;
			}
			lock_release(secf_lock);
			lock_release(&secf_data->lock);
			return -2;
		}
		list = list->next;
		name.len = nlen;
		user.len = ulen;
	}

	/* Domain whitelisted */
	list = secf_data->wl.domain;
	while(list) {
		if(domain.len > list->s.len)
			domain.len = list->s.len;
		res = cmpi_str(&list->s, &domain);
		if(res == 0) {
			lock_get(secf_lock);
			switch(type) {
				case 1:
					secf_stats[WL_FDOMAIN]++;
					break;
				case 2:
					secf_stats[WL_TDOMAIN]++;
					break;
				case 3:
					secf_stats[WL_CDOMAIN]++;
					break;
			}
			lock_release(secf_lock);
			lock_release(&secf_data->lock);
			return 3;
		}
		list = list->next;
		domain.len = dlen;
	}
	/* Domain blacklisted */
	list = secf_data->bl.domain;
	while(list) {
		if(domain.len > list->s.len)
			domain.len = list->s.len;
		res = cmpi_str(&list->s, &domain);
		if(res == 0) {
			lock_get(secf_lock);
			switch(type) {
				case 1:
					secf_stats[BL_FDOMAIN]++;
					break;
				case 2:
					secf_stats[BL_TDOMAIN]++;
					break;
				case 3:
					secf_stats[BL_CDOMAIN]++;
					break;
			}
			lock_release(secf_lock);
			lock_release(&secf_data->lock);
			return -3;
		}
		list = list->next;
		domain.len = dlen;
	}
	lock_release(&secf_data->lock);

	return 1;
}


/* Check if the current IP is allowed

Return codes:
 2 = IP address whitelisted
 1 = not found
-1 = error
-2 = IP address blacklisted
*/
static int w_check_ip(struct sip_msg *msg)
{
	int res, len;
	str ip;
	struct str_list *list;

	if(msg == NULL)
		return -1;

	ip.s = ip_addr2a(&msg->rcv.src_ip);
	ip.len = strlen(ip.s);

	len = ip.len;

	/* IP address whitelisted */
	lock_get(&secf_data->lock);
	list = secf_data->wl.ip;
	while(list) {
		if(ip.len > list->s.len)
			ip.len = list->s.len;
		res = cmpi_str(&list->s, &ip);
		if(res == 0) {
			lock_get(secf_lock);
			secf_stats[WL_IP]++;
			lock_release(secf_lock);
			lock_release(&secf_data->lock);
			return 2;
		}
		list = list->next;
		ip.len = len;
	}
	/* IP address blacklisted */
	list = secf_data->bl.ip;
	while(list) {
		if(ip.len > list->s.len)
			ip.len = list->s.len;
		res = cmpi_str(&list->s, &ip);
		if(res == 0) {
			lock_get(secf_lock);
			secf_stats[BL_IP]++;
			lock_release(secf_lock);
			lock_release(&secf_data->lock);
			return -2;
		}
		list = list->next;
		ip.len = len;
	}
	lock_release(&secf_data->lock);

	return 1;
}


/* Check if the current country is allowed

Return codes:
 2 = Country whitelisted
 1 = not found
-2 = Country blacklisted
*/
static int w_check_country(struct sip_msg *msg, char *val)
{
	int res, len;
	str country;
	struct str_list *list;

	country.s = val;
	country.len = strlen(val);

	len = country.len;

	/* Country whitelisted */
	lock_get(&secf_data->lock);
	list = secf_data->wl.country;
	while(list) {
		if(country.len > list->s.len)
			country.len = list->s.len;
		res = cmpi_str(&list->s, &country);
		if(res == 0) {
			lock_get(secf_lock);
			secf_stats[WL_COUNTRY]++;
			lock_release(secf_lock);
			lock_release(&secf_data->lock);
			return 2;
		}
		list = list->next;
		country.len = len;
	}
	/* Country blacklisted */
	list = secf_data->bl.country;
	while(list) {
		if(country.len > list->s.len)
			country.len = list->s.len;
		res = cmpi_str(&list->s, &country);
		if(res == 0) {
			lock_get(secf_lock);
			secf_stats[BL_COUNTRY]++;
			lock_release(secf_lock);
			lock_release(&secf_data->lock);
			return -2;
		}
		list = list->next;
		country.len = len;
	}
	lock_release(&secf_data->lock);

	return 1;
}


void secf_reset_stats(void)
{
	lock_get(secf_lock);
	memset(secf_stats, 0, total_data * sizeof(int));
	lock_release(secf_lock);
}


/***
INIT AND DESTROY FUNCTIONS
***/

/* Initialize data */
int secf_init_data(void)
{
	secf_data = (secf_data_p)shm_malloc(sizeof(secf_data_t));
	if(!secf_data) {
		SHM_MEM_ERROR;
		return -1;
	}
	memset(secf_data, 0, sizeof(secf_data_t));

	secf_stats = shm_malloc(total_data * sizeof(int));
	memset(secf_stats, 0, total_data * sizeof(int));
	
	if(secf_dst_exact_match != 0)
		secf_dst_exact_match = 1;

	return 0;
}


/* Module init function */
static int mod_init(void)
{
	LM_DBG("SECFILTER module init\n");
	/* Init data to store database values */
	if(secf_init_data() == -1)
		return -1;
	/* Init RPC */
	if(rpc_init() < 0)
		return -1;
	/* Init locks */
	if(lock_init(&secf_data->lock) == 0) {
		LM_CRIT("cannot initialize lock.\n");
		return -1;
	}
	secf_lock = lock_alloc();
	if (!secf_lock) {
		LM_CRIT("cannot allocate memory for lock.\n");
		return -1;
	}
	if (lock_init(secf_lock) == 0) {
		LM_CRIT("cannot initialize lock.\n");
		return -1;
	}
	/* Init database connection and check version */
	if(secf_init_db() == -1)
		return -1;
	/* Load data from database */
	if(secf_load_db() == -1) {
		LM_ERR("Error loading data from database\n");
		return -1;
	}

	return 0;
}


/* Module child init function */
static int child_init(int rank)
{
	if(rank == PROC_INIT || rank == PROC_MAIN || rank == PROC_TCP_MAIN)
		return 0; /* do nothing for the main process */

	return 0;
}


/* Module destroy function */
static void mod_destroy(void)
{
	LM_DBG("SECFILTER module destroy\n");
	if(!secf_data)
		return;

	/* Free shared data */
	secf_free_data();
	/* Destroy lock */
	lock_destroy(&secf_data->lock);
	shm_free(secf_data);
	secf_data = NULL;

	if (secf_lock) {
		lock_destroy(secf_lock);
		lock_dealloc((void *)secf_lock);
		secf_lock = NULL;
	}
}


/* RPC init */
static int rpc_init(void)
{
	/* Register RPC commands */
	if(rpc_register_array(secfilter_rpc) != 0) {
		LM_ERR("failed to register RPC commands\n");
		return -1;
	}
	return 0;
}

/* Free shared data */
static void free_str_list(struct str_list *l)
{
	struct str_list *i;
	while(l) {
		i = l->next;
		LM_DBG("free '%.*s'[%p] next:'%p'\n", l->s.len, l->s.s, l, i);
		shm_free(l->s.s);
		shm_free(l);
		l = i;
	}
}


static void free_sec_info(secf_info_p info)
{
	LM_DBG("freeing ua[%p]\n", info->ua);
	free_str_list(info->ua);
	LM_DBG("freeing country[%p]\n", info->country);
	free_str_list(info->country);
	LM_DBG("freeing domain[%p]\n", info->domain);
	free_str_list(info->domain);
	LM_DBG("freeing user[%p]\n", info->user);
	free_str_list(info->user);
	LM_DBG("freeing ip[%p]\n", info->ip);
	free_str_list(info->ip);
	LM_DBG("freeing dst[%p]\n", info->dst);
	free_str_list(info->dst);
	LM_DBG("zeroed info[%p]\n", info);
	memset(info, 0, sizeof(secf_info_t));
}


void secf_free_data(void)
{
	lock_get(&secf_data->lock);

	LM_DBG("freeing wl\n");
	free_sec_info(&secf_data->wl);
	memset(&secf_data->wl_last, 0, sizeof(secf_info_t));
	LM_DBG("so, ua[%p] should be NULL\n", secf_data->wl.ua);

	LM_DBG("freeing bl\n");
	free_sec_info(&secf_data->bl);
	memset(&secf_data->bl_last, 0, sizeof(secf_info_t));
	LM_DBG("so, ua[%p] should be NULL\n", secf_data->bl.ua);

	lock_release(&secf_data->lock);
}
