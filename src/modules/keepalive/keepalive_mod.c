/**
 * keepalive module - remote destinations probing
 *
 * Copyright (C) 2017 Guillaume Bour <guillaume@bour.cc>
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

/*! \file
 * \ingroup keepalive
 * \brief Keepalive :: Send keepalives
 */

/*! \defgroup keepalive Keepalive :: Probing remote gateways by sending keepalives
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>

#include "../../core/mod_fix.h"
#include "../../core/kemi.h"

#include "../tm/tm_load.h"
#include "../dispatcher/api.h"

#include "keepalive.h"
#include "api.h"

MODULE_VERSION


static int mod_init(void);
static void mod_destroy(void);
static int ka_mod_add_destination(modparam_t type, void *val);
int ka_init_rpc(void);
int ka_alloc_destinations_list();
static int w_cmd_is_alive(struct sip_msg *msg, char *str1, char *str2);
static int fixup_add_destination(void** param, int param_no);
static int w_add_destination(sip_msg_t *msg, char *uri, char *owner);
static int w_del_destination(sip_msg_t *msg, char *uri, char *owner);


extern struct tm_binds tmb;

int ka_ping_interval = 30;
ka_destinations_list_t *ka_destinations_list = NULL;
str ka_ping_from = str_init("sip:keepalive@kamailio.org");
int ka_counter_del = 5;



static cmd_export_t cmds[] = {
	{"ka_is_alive", (cmd_function)w_cmd_is_alive, 1,
			fixup_spve_null, 0, ANY_ROUTE},
	// internal API
	{"ka_add_destination", (cmd_function)w_add_destination, 2,
		fixup_add_destination, 0, REQUEST_ROUTE|BRANCH_ROUTE|ONREPLY_ROUTE},
	{"ka_del_destination", (cmd_function)w_del_destination, 2,
		fixup_add_destination, 0, ANY_ROUTE},
	{"bind_keepalive", (cmd_function)bind_keepalive, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0}
};


static param_export_t params[] = {
	{"ping_interval", PARAM_INT, &ka_ping_interval},
	{"destination", PARAM_STRING | USE_FUNC_PARAM,
				(void *)ka_mod_add_destination},
	{"ping_from", PARAM_STRING,	&ka_ping_from},
	{"delete_counter", PARAM_INT,	&ka_counter_del},
	{0, 0, 0}
};


/** module exports */
struct module_exports exports = {
	"keepalive",     /* module name */
	DEFAULT_DLFLAGS, /* dlopen flags */
	cmds,            /* cmd (cfg function) exports */
	params,          /* param exports */
	0,               /* RPC method exports */
	0,               /* pseudo-variables exports */
	0,               /* response handling function */
	mod_init,        /* module init function */
	0,               /* per-child init function */
	mod_destroy      /* module destroy function */
};


/**
 * Module initialization
 */
static int mod_init(void)
{
	LM_INFO("Initializing keepalive module\n");

	if(load_tm_api(&tmb) == -1) {
		LM_ERR("could not load the TM-functions - please load tm module\n");
		return -1;
	}

	if(ka_init_rpc() < 0) {
		LM_ERR("failed to register RPC commands\n");
		return -1;
	}

	if(ka_alloc_destinations_list() < 0)
		return -1;

	return 0;
}

/*! \brief
 * destroy function
 */
static void mod_destroy(void)
{
	if(ka_destinations_list){
		lock_release(ka_destinations_list->lock);
		lock_dealloc(ka_destinations_list->lock);
	}
}


/*! \brief
 * parses string to dispatcher dst flags set
 * returns <0 on failure or int with flag on success.
 */
int ka_parse_flags(char *flag_str, int flag_len)
{
	return 0;
}


static int fixup_add_destination(void** param, int param_no)
{
	if (param_no == 1 || param_no == 2) {
		return fixup_spve_all(param, param_no);
	}

	return 0;
}
/*!
* @function w_add_destination
* @abstract adds given sip uri in allocated destination stack as named ka_alloc_destinations_list
* wrapper for ka_add_dest
* @param msg sip message
* @param uri given uri
* @param owner given owner name
*
* @result 1 successful  , -1 fail
*/
static int w_add_destination(sip_msg_t *msg, char *uri, char *owner)
{
	str suri ={0,0};
	str sowner={0,0};
	if(fixup_get_svalue(msg, (gparam_t*)uri, &suri)!=0) {
		LM_ERR("unable to get uri string\n");
		return -1;
	}
	if(fixup_get_svalue(msg, (gparam_t*)owner, &sowner)!=0) {
		LM_ERR("unable to get owner regex\n");
		return -1;
	}

	return ka_add_dest(&suri, &sowner, 0, ka_ping_interval, 0, 0);
}

/*!
 *
 */
static int ki_add_destination(sip_msg_t *msg, str *uri, str *owner)
{
	if(ka_alloc_destinations_list() < 0)
		return -1;

	return ka_add_dest(uri, owner, 0, ka_ping_interval, 0, 0);
}

/*!
* @function w_del_destination_f
* @abstract deletes given sip uri in allocated destination stack as named ka_alloc_destinations_list
* wrapper for ka_del_destination
* @param msg sip message
* @param uri given uri
* @param owner given owner name, not using now
*
* @result 1 successful  , -1 fail
*/
static int w_del_destination(sip_msg_t *msg, char *uri, char *owner)
{
	str suri ={0,0};
	str sowner={0,0};
	if(fixup_get_svalue(msg, (gparam_t*)uri, &suri)!=0) {
		LM_ERR("unable to get uri string\n");
		return -1;
	}
	if(fixup_get_svalue(msg, (gparam_t*)owner, &sowner)!=0) {
		LM_ERR("unable to get owner regex\n");
		return -1;
	}

	return ka_del_destination(&suri, &sowner);
}

/*!
 *
 */
static int ki_del_destination(sip_msg_t *msg, str *uri, str *owner)
{
	return ka_del_destination(uri, owner);
}

/*
 * Function callback executer per module param "destination".
 * Is just a wrapper to ka_add_dest() api function
 */
static int ka_mod_add_destination(modparam_t type, void *val)
{
	if(ka_alloc_destinations_list() < 0)
		return -1;

	str dest = {val, strlen(val)};
	str owner = str_init("_params");
	LM_DBG("adding destination %.*s\n", dest.len, dest.s);

	return ka_add_dest(&dest, &owner, 0, ka_ping_interval, 0, 0);
}

/*
 * Allocate global variable *ka_destination_list* if not already done
 * WHY:  when specifying static destinations as module param, ka_mod_add_destination() is
 *       executed BEFORE mod_init()
 */
int ka_alloc_destinations_list()
{
	if(ka_destinations_list != NULL) {
		LM_DBG("ka_destinations_list already allocated\n");
		return 1;
	}

	ka_destinations_list = (ka_destinations_list_t *)shm_malloc(
			sizeof(ka_destinations_list_t));
	if(ka_destinations_list == NULL) {
		LM_ERR("no more memory.\n");
		return -1;
	}

	ka_destinations_list->lock = lock_alloc();
	if(!ka_destinations_list->lock) {
		LM_ERR("Couldnt allocate Lock \n");
		return -1;
	}
	return 0;
}

static int ki_is_alive(sip_msg_t *msg, str *dest)
{
	ka_state state = ka_destination_state(dest);
	// must not return 0, as it stops dialplan execution
	if(state == KA_STATE_UNKNOWN) {
		return KA_STATE_UP;
	}

	return state;
}

static int w_cmd_is_alive(struct sip_msg *msg, char *str1, char *str2)
{
	str dest = STR_NULL;

	if(fixup_get_svalue(msg, (gparam_t*)str1, &dest)!=0) {
		LM_ERR("failed to get dest parameter\n");
		return -1;
	}
	return ki_is_alive(msg, &dest);
}

/**
 *
 */
/* clang-format off */
static sr_kemi_t sr_kemi_keepalive_exports[] = {
	{ str_init("keepalive"), str_init("is_alive"),
		SR_KEMIP_INT, ki_is_alive,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("keepalive"), str_init("add_destination"),
		SR_KEMIP_INT, ki_add_destination,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("keepalive"), str_init("del_destination"),
		SR_KEMIP_INT, ki_del_destination,
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
	sr_kemi_modules_add(sr_kemi_keepalive_exports);
	return 0;
}
