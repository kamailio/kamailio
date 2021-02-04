/*
 * Copyright (C) 2006 Voice Sistem S.R.L.
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

/*!
 * \file
 * \brief Kamailio Presence_XML :: Core
 * \ingroup presence_xml
 */

/*!
 * \defgroup presence_xml Presence_xml :: This module implements a range
 *   of XML-based SIP event packages for presence
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libxml/parser.h>
#include <time.h>

#include "../../core/sr_module.h"
#include "../../core/dprint.h"
#include "../../core/str.h"
#include "../../core/ut.h"
#include "../../core/parser/msg_parser.h"
#include "../../core/parser/parse_uri.h"
#include "../../core/mem/mem.h"
#include "../presence/bind_presence.h"
#include "../presence/hash.h"
#include "../presence/notify.h"
#include "../xcap_client/xcap_functions.h"
#include "../../modules/sl/sl.h"
#include "../../core/mod_fix.h"
#include "../../core/kemi.h"
#include "pidf.h"
#include "add_events.h"
#include "presence_xml.h"
#include "pres_check.h"
#include "api.h"

MODULE_VERSION

#define S_TABLE_VERSION 4

/** module functions */

static int mod_init(void);
static int child_init(int);
static void destroy(void);
static int pxml_add_xcap_server(modparam_t type, void *val);
static int shm_copy_xcap_list(void);
static void free_xs_list(xcap_serv_t *xs_list, int mem_type);
static int xcap_doc_updated(int doc_type, str xid, char *doc);

static int fixup_presxml_check(void **param, int param_no);
static int w_presxml_check_basic(
		sip_msg_t *msg, char *presentity_uri, char *status);
static int w_presxml_check_activities(
		sip_msg_t *msg, char *presentity_uri, char *activities);

/** module variables ***/
presence_api_t psapi = {0};

/* Module parameter variables */
str pxml_xcap_table = str_init("xcap");
static str pxml_db_url = str_init(DEFAULT_DB_URL);
int pxml_force_active = 0;
int pxml_force_dummy_presence = 0;
int pxml_integrated_xcap_server = 0;
xcap_serv_t *xs_list = NULL;
int pxml_disable_presence = 0;
int pxml_disable_winfo = 0;
int pxml_disable_bla = 1;
int pxml_disable_xcapdiff = 0;
static int pxml_passive_mode = 0;
str xcapauth_userdel_reason = str_init("probation");

int pxml_force_single_body = 0;
str pxml_single_body_priorities = str_init("Available|Ringing|On the Phone");
str pxml_single_body_lookup_element = str_init("note");

/** SL API structure */
sl_api_t slb;

/* database connection */
db1_con_t *pxml_db = NULL;
db_func_t pxml_dbf;

/* functions imported from xcap_client module */

xcapGetNewDoc_t xcap_GetNewDoc;

/* clang-format off */
static cmd_export_t cmds[]={
	{ "pres_check_basic",		(cmd_function)w_presxml_check_basic, 2,
		fixup_presxml_check, 0, ANY_ROUTE},
	{ "pres_check_activities",	(cmd_function)w_presxml_check_activities, 2,
		fixup_presxml_check, 0, ANY_ROUTE},
	{ "bind_presence_xml",		(cmd_function)bind_presence_xml, 1,
		0, 0, 0},
	{ 0, 0, 0, 0, 0, 0}
};
/* clang-format on */

/* clang-format off */
static param_export_t params[]={
	{ "db_url",		PARAM_STR, &pxml_db_url},
	{ "xcap_table",		PARAM_STR, &pxml_xcap_table},
	{ "force_active",	INT_PARAM, &pxml_force_active },
	{ "integrated_xcap_server", INT_PARAM, &pxml_integrated_xcap_server},
	{ "xcap_server",     	PARAM_STRING|USE_FUNC_PARAM,(void*)pxml_add_xcap_server},
	{ "disable_presence",	INT_PARAM, &pxml_disable_presence },
	{ "disable_winfo",		INT_PARAM, &pxml_disable_winfo },
	{ "disable_bla",		INT_PARAM, &pxml_disable_bla },
	{ "disable_xcapdiff",	INT_PARAM, &pxml_disable_xcapdiff },
	{ "passive_mode",		INT_PARAM, &pxml_passive_mode },
	{ "xcapauth_userdel_reason", PARAM_STR, &xcapauth_userdel_reason},
	{ "force_dummy_presence",       INT_PARAM, &pxml_force_dummy_presence },
	{ "force_presence_single_body", INT_PARAM, &pxml_force_single_body },
	{ "presence_single_body_priorities",  PARAM_STR, &pxml_single_body_priorities },
	{ "presence_single_body_lookup_element", PARAM_STR, &pxml_single_body_lookup_element },
	{ 0, 0, 0}
};
/* clang-format on */

/** module exports */
/* clang-format off */
struct module_exports exports= {
	"presence_xml",		/* module name */
	DEFAULT_DLFLAGS,	/* dlopen flags */
	cmds,				/* exported functions */
	params,				/* exported parameters */
	0,					/* RPC method exports */
	0,					/* exported pseudo-variables */
	0,					/* response handling function */
	mod_init,			/* module initialization function */
	child_init,			/* per-child init function */
	destroy				/* module destroy function */
};
/* clang-format on */

/**
 * init module function
 */
static int mod_init(void)
{
	if(pxml_passive_mode == 1) {
		return 0;
	}

	LM_DBG("db_url=%s (len=%d addr=%p)\n", ZSW(pxml_db_url.s),
			pxml_db_url.len, pxml_db_url.s);

	/* bind the SL API */
	if(sl_load_api(&slb) != 0) {
		LM_ERR("cannot bind to SL API\n");
		return -1;
	}

	if(presence_load_api(&psapi) != 0) {
		LM_ERR("cannot bind to presence api\n");
		return -1;
	}

	if(psapi.add_event == NULL || psapi.update_watchers_status == NULL) {
		LM_ERR("requited presence api not available\n");
		return -1;
	}
	if(xml_add_events() < 0) {
		LM_ERR("adding xml events\n");
		return -1;
	}

	if(pxml_force_active == 0) {
		/* binding to mysql module  */
		if(db_bind_mod(&pxml_db_url, &pxml_dbf)) {
			LM_ERR("Database module not found\n");
			return -1;
		}

		if(!DB_CAPABILITY(pxml_dbf, DB_CAP_ALL)) {
			LM_ERR("Database module does not implement all functions"
				   " needed by the module\n");
			return -1;
		}

		pxml_db = pxml_dbf.init(&pxml_db_url);
		if(!pxml_db) {
			LM_ERR("while connecting to database\n");
			return -1;
		}

		if(db_check_table_version(
				   &pxml_dbf, pxml_db, &pxml_xcap_table, S_TABLE_VERSION)
				< 0) {
			DB_TABLE_VERSION_ERROR(pxml_xcap_table);
			goto dberror;
		}
		if(!pxml_integrated_xcap_server) {
			xcap_api_t xcap_api;
			bind_xcap_t bind_xcap;

			/* bind xcap */
			bind_xcap = (bind_xcap_t)find_export("bind_xcap", 1, 0);
			if(!bind_xcap) {
				LM_ERR("Can't bind xcap_client\n");
				goto dberror;
			}

			if(bind_xcap(&xcap_api) < 0) {
				LM_ERR("Can't bind xcap_api\n");
				goto dberror;
			}
			xcap_GetNewDoc = xcap_api.getNewDoc;
			if(xcap_GetNewDoc == NULL) {
				LM_ERR("can't import getNewDoc from xcap_client module\n");
				goto dberror;
			}

			if(xcap_api.register_xcb(PRES_RULES, xcap_doc_updated) < 0) {
				LM_ERR("registering xcap callback function\n");
				goto dberror;
			}
		}
	}

	if(shm_copy_xcap_list() < 0) {
		LM_ERR("copying xcap server list in share memory\n");
		return -1;
	}

	if(pxml_db)
		pxml_dbf.close(pxml_db);
	pxml_db = NULL;

	return 0;

dberror:
	pxml_dbf.close(pxml_db);
	pxml_db = NULL;
	return -1;
}

static int child_init(int rank)
{
	LM_DBG("[%d]  pid [%d]\n", rank, getpid());

	if(pxml_passive_mode == 1) {
		return 0;
	}

	if(rank == PROC_INIT || rank == PROC_MAIN || rank == PROC_TCP_MAIN)
		return 0; /* do nothing for the main process */

	if(pxml_force_active == 0) {
		if(pxml_db)
			return 0;
		pxml_db = pxml_dbf.init(&pxml_db_url);
		if(pxml_db == NULL) {
			LM_ERR("while connecting database\n");
			return -1;
		}
		if(pxml_dbf.use_table(pxml_db, &pxml_xcap_table) < 0) {
			LM_ERR("in use_table SQL operation\n");
			return -1;
		}
	}

	LM_DBG("child %d: Database connection opened successfully\n", rank);

	return 0;
}

static void destroy(void)
{
	LM_DBG("start\n");
	if(pxml_db && pxml_dbf.close)
		pxml_dbf.close(pxml_db);

	free_xs_list(xs_list, SHM_MEM_TYPE);

	return;
}

static int pxml_add_xcap_server(modparam_t type, void *val)
{
	xcap_serv_t *xs;
	int size;
	char *serv_addr = (char *)val;
	char *sep = NULL;
	unsigned int port = 80;
	str serv_addr_str;

	serv_addr_str.s = serv_addr;
	serv_addr_str.len = strlen(serv_addr);

	sep = strchr(serv_addr, ':');
	if(sep) {
		char *sep2 = NULL;
		str port_str;

		sep2 = strchr(sep + 1, ':');
		if(sep2)
			sep = sep2;


		port_str.s = sep + 1;
		port_str.len = serv_addr_str.len - (port_str.s - serv_addr);

		if(str2int(&port_str, &port) < 0) {
			LM_ERR("while converting string to int\n");
			goto error;
		}
		if(port < 1 || port > 65535) {
			LM_ERR("wrong port number\n");
			goto error;
		}
		*sep = '\0';
		serv_addr_str.len = sep - serv_addr;
	}

	size = sizeof(xcap_serv_t) + (serv_addr_str.len + 1) * sizeof(char);
	xs = (xcap_serv_t *)pkg_malloc(size);
	if(xs == NULL) {
		ERR_MEM(PKG_MEM_STR);
	}
	memset(xs, 0, size);
	size = sizeof(xcap_serv_t);

	xs->addr = (char *)xs + size;
	strcpy(xs->addr, serv_addr);

	xs->port = port;
	/* check for duplicates */
	xs->next = xs_list;
	xs_list = xs;
	return 0;

error:
	free_xs_list(xs_list, PKG_MEM_TYPE);
	return -1;
}

static int shm_copy_xcap_list(void)
{
	xcap_serv_t *xs, *shm_xs, *prev_xs;
	int size;

	xs = xs_list;
	if(xs == NULL) {
		if(pxml_force_active == 0 && !pxml_integrated_xcap_server) {
			LM_ERR("no xcap_server parameter set\n");
			return -1;
		}
		return 0;
	}
	xs_list = NULL;
	size = sizeof(xcap_serv_t);

	while(xs) {
		size += (strlen(xs->addr) + 1) * sizeof(char);
		shm_xs = (xcap_serv_t *)shm_malloc(size);
		if(shm_xs == NULL) {
			ERR_MEM(SHARE_MEM);
		}
		memset(shm_xs, 0, size);
		size = sizeof(xcap_serv_t);

		shm_xs->addr = (char *)shm_xs + size;
		strcpy(shm_xs->addr, xs->addr);
		shm_xs->port = xs->port;
		shm_xs->next = xs_list;
		xs_list = shm_xs;

		prev_xs = xs;
		xs = xs->next;

		pkg_free(prev_xs);
	}
	return 0;

error:
	free_xs_list(xs_list, SHM_MEM_TYPE);
	return -1;
}

static void free_xs_list(xcap_serv_t *xsl, int mem_type)
{
	xcap_serv_t *xs, *prev_xs;

	xs = xsl;

	while(xs) {
		prev_xs = xs;
		xs = xs->next;
		if(mem_type & SHM_MEM_TYPE)
			shm_free(prev_xs);
		else
			pkg_free(prev_xs);
	}
	xsl = NULL;
}

static int xcap_doc_updated(int doc_type, str xid, char *doc)
{
	pres_ev_t ev;
	str rules_doc;

	/* call updating watchers */
	ev.name.s = "presence";
	ev.name.len = PRES_LEN;

	rules_doc.s = doc;
	rules_doc.len = strlen(doc);

	if(psapi.update_watchers_status(&xid, &ev, &rules_doc) < 0) {
		LM_ERR("updating watchers in presence\n");
		return -1;
	}
	return 0;
}

int bind_presence_xml(struct presence_xml_binds *pxb)
{
	if(pxb == NULL) {
		LM_WARN("bind_presence_xml: Cannot load presence_xml API into a NULL "
				"pointer\n");
		return -1;
	}

	pxb->pres_check_basic = presxml_check_basic;
	pxb->pres_check_activities = presxml_check_activities;
	return 0;
}

static int fixup_presxml_check(void **param, int param_no)
{
	if(param_no == 1) {
		return fixup_spve_null(param, 1);
	} else if(param_no == 2) {
		return fixup_spve_null(param, 1);
	}
	return 0;
}

static int w_presxml_check_basic(
		sip_msg_t *msg, char *presentity_uri, char *status)
{
	str uri, basic;

	if(fixup_get_svalue(msg, (gparam_p)presentity_uri, &uri) != 0) {
		LM_ERR("invalid presentity uri parameter\n");
		return -1;
	}

	if(fixup_get_svalue(msg, (gparam_p)status, &basic) != 0) {
		LM_ERR("invalid status parameter\n");
		return -1;
	}

	return presxml_check_basic(msg, uri, basic);
}

static int ki_presxml_check_basic(sip_msg_t *msg, str *pres_uri, str *status)
{
	if(pres_uri == NULL || status == NULL) {
		return -1;
	}
	return presxml_check_basic(msg, *pres_uri, *status);
}

static int w_presxml_check_activities(
		sip_msg_t *msg, char *presentity_uri, char *activity)
{
	str uri, act;

	if(fixup_get_svalue(msg, (gparam_p)presentity_uri, &uri) != 0) {
		LM_ERR("invalid presentity uri parameter\n");
		return -1;
	}

	if(fixup_get_svalue(msg, (gparam_p)activity, &act) != 0) {
		LM_ERR("invalid activity parameter\n");
		return -1;
	}

	return presxml_check_activities(msg, uri, act);
}

static int ki_presxml_check_activities(
		sip_msg_t *msg, str *pres_uri, str *activity)
{
	if(pres_uri == NULL || activity == NULL) {
		return -1;
	}
	return presxml_check_activities(msg, *pres_uri, *activity);
}

/**
 *
 */
/* clang-format off */
static sr_kemi_t sr_kemi_presence_xml_exports[] = {
	{ str_init("presence_xml"), str_init("pres_check_basic"),
		SR_KEMIP_INT, ki_presxml_check_basic,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("presence_xml"), str_init("pres_check_activities"),
		SR_KEMIP_INT, ki_presxml_check_activities,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},

	{ {0, 0}, {0, 0}, 0, NULL, { 0, 0, 0, 0, 0, 0 } }
};
/* clang-format on */

/**
 *
 */
int mod_register(char *path, int *dlflags, void *p1, void *p2)
{
	sr_kemi_modules_add(sr_kemi_presence_xml_exports);
	return 0;
}
