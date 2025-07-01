/**
 * Copyright (C) 2024 Daniel-Constantin Mierla (asipto.com)
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 *
 * This file is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>


#include "../../core/sr_module.h"
#include "../../core/dprint.h"
#include "../../core/ut.h"
#include "../../core/mod_fix.h"
#include "../../core/kemi.h"
#include "../../core/utils/sruid.h"

#include "ic.h"

MODULE_VERSION

static char *_infdbc_server = NULL;
static int _infdbc_port = 8086;
static char *_infdbc_database = NULL;
static int _infdbc_connected = 0;
static char *_infdbc_user = NULL;
static char *_infdbc_password = NULL;
static char *_infdbc_tags = NULL;

static int w_influxdbc_measure(sip_msg_t *msg, char *pname, char *p2);
static int w_influxdbc_measureend(sip_msg_t *msg, char *p1, char *p2);
static int w_influxdbc_sub(sip_msg_t *msg, char *pname, char *p2);
static int w_influxdbc_subend(sip_msg_t *msg, char *p1, char *p2);
static int w_influxdbc_push(sip_msg_t *msg, char *p1, char *p2);
static int w_influxdbc_long(sip_msg_t *msg, char *pname, char *pvalue);
static int w_influxdbc_string(sip_msg_t *msg, char *pname, char *pvalue);
static int w_influxdbc_double(sip_msg_t *msg, char *pname, char *pvalue);
static int w_influxdbc_division(
		sip_msg_t *msg, char *pname, char *pdividend, char *pdivisor);

static int mod_init(void);
static int child_init(int);
static void mod_destroy(void);

/* clang-format off */
static cmd_export_t cmds[] = {
	{"influxdbc_measure", (cmd_function)w_influxdbc_measure,
			1, fixup_spve_null, fixup_free_spve_null, ANY_ROUTE},
	{"influxdbc_measureend", (cmd_function)w_influxdbc_measureend,
			0, 0, 0, ANY_ROUTE},
	{"influxdbc_sub", (cmd_function)w_influxdbc_sub,
			1, fixup_spve_null, fixup_free_spve_null, ANY_ROUTE},
	{"influxdbc_subend", (cmd_function)w_influxdbc_subend,
			0, 0, 0, ANY_ROUTE},
	{"influxdbc_push", (cmd_function)w_influxdbc_push,
			0, 0, 0, ANY_ROUTE},
	{"influxdbc_long", (cmd_function)w_influxdbc_long,
			2, fixup_spve_igp, fixup_free_spve_igp, ANY_ROUTE},
	{"influxdbc_string", (cmd_function)w_influxdbc_string,
			2, fixup_spve_spve, fixup_free_spve_spve, ANY_ROUTE},
	{"influxdbc_double", (cmd_function)w_influxdbc_double,
			2, fixup_spve_spve, fixup_free_spve_spve, ANY_ROUTE},
	{"influxdbc_division", (cmd_function)w_influxdbc_division,
			3, fixup_sii, fixup_free_sii, ANY_ROUTE},

	{0, 0, 0, 0, 0, 0}
};

static param_export_t params[] = {
	{"server", PARAM_STRING, &_infdbc_server},
	{"database", PARAM_STRING, &_infdbc_database},
	{"port", PARAM_INT, &_infdbc_port},
	{"user", PARAM_STRING, &_infdbc_user},
	{"password", PARAM_STRING, &_infdbc_password},
	{"tags", PARAM_STRING, &_infdbc_tags},

	{0, 0, 0}
};

struct module_exports exports = {
	"influxdbc",	 /* module name */
	DEFAULT_DLFLAGS, /* dlopen flags */
	cmds,			 /* exported functions */
	params,			 /* exported parameters */
	0,				 /* exported rpc functions */
	0,				 /* exported pseudo-variables */
	0,				 /* response handling function */
	mod_init,		 /* module init function */
	child_init,		 /* per child init function */
	mod_destroy		 /* destroy function */
};
/* clang-format on */

/**
 * init module function
 */
static int mod_init(void)
{
	if(_infdbc_server == NULL) {
		LM_ERR("server address not provided\n");
		return -1;
	}
	if(_infdbc_database == NULL) {
		LM_ERR("database name not provided\n");
		return -1;
	}
	return 0;
}

/**
 * @brief Initialize async module children
 */
static int child_init(int rank)
{
	if(rank != PROC_MAIN)
		return 0;

	return 0;
}
/**
 * destroy module function
 */
static void mod_destroy(void)
{
}

/**
 *
 */
static int w_influxdbc_measure(sip_msg_t *msg, char *pname, char *p2)
{
	str sname;

	if(fixup_get_svalue(msg, (gparam_t *)pname, &sname) != 0) {
		LM_ERR("unable to get name parameter\n");
		return -1;
	}
	if(_infdbc_connected == 0) {
		ic_influx_database(_infdbc_server, _infdbc_port, _infdbc_database);
		if(_infdbc_user != NULL && _infdbc_password != NULL) {
			ic_influx_userpw(_infdbc_user, _infdbc_password);
		}
		if(_infdbc_tags != NULL) {
			ic_tags(_infdbc_tags);
		}
		_infdbc_connected = 1;
	}

	ic_measure(sname.s);

	return 1;
}

/**
 *
 */
static int ki_influxdbc_measure(sip_msg_t *msg, str *name)
{
	ic_measure(name->s);

	return 1;
}

/**
 *
 */
static int w_influxdbc_measureend(sip_msg_t *msg, char *p1, char *p2)
{
	ic_measureend();

	return 1;
}

/**
 *
 */
static int ki_influxdbc_measureend(sip_msg_t *msg)
{
	ic_measureend();

	return 1;
}

/**
 *
 */
static int w_influxdbc_sub(sip_msg_t *msg, char *pname, char *p2)
{
	str sname;

	if(fixup_get_svalue(msg, (gparam_t *)pname, &sname) != 0) {
		LM_ERR("unable to get name parameter\n");
		return -1;
	}

	ic_sub(sname.s);

	return 1;
}

/**
 *
 */
static int w_influxdbc_subend(sip_msg_t *msg, char *p1, char *p2)
{
	ic_subend();

	return 1;
}

/**
 *
 */
static int w_influxdbc_push(sip_msg_t *msg, char *p1, char *p2)
{
	ic_push();

	return 1;
}

/**
 *
 */
static int ki_influxdbc_push(sip_msg_t *msg)
{
	ic_push();

	return 1;
}

/**
 *
 */
static int w_influxdbc_long(sip_msg_t *msg, char *pname, char *pvalue)
{
	str sname;
	int ival;

	if(fixup_get_svalue(msg, (gparam_t *)pname, &sname) != 0) {
		LM_ERR("unable to get name parameter\n");
		return -1;
	}
	if(fixup_get_ivalue(msg, (gparam_t *)pvalue, &ival) != 0) {
		LM_ERR("unable to get value parameter\n");
		return -1;
	}

	ic_long(sname.s, ival);

	return 1;
}

/**
 *
 */
static int ki_influxdbc_long(sip_msg_t *msg, str *name, int val)
{
	ic_long(name->s, val);

	return 1;
}

/**
 *
 */
static int w_influxdbc_string(sip_msg_t *msg, char *pname, char *pvalue)
{
	str sname;
	str sval;

	if(fixup_get_svalue(msg, (gparam_t *)pname, &sname) != 0) {
		LM_ERR("unable to get name parameter\n");
		return -1;
	}
	if(fixup_get_svalue(msg, (gparam_t *)pvalue, &sval) != 0) {
		LM_ERR("unable to get value parameter\n");
		return -1;
	}

	ic_string(sname.s, sval.s);

	return 1;
}


/**
 *
 */
static int w_influxdbc_double(sip_msg_t *msg, char *pname, char *pvalue)
{
	str sname;
	str sval;
	double dval = 0.0;

	if(fixup_get_svalue(msg, (gparam_t *)pname, &sname) != 0) {
		LM_ERR("unable to get name parameter\n");
		return -1;
	}
	if(fixup_get_svalue(msg, (gparam_t *)pvalue, &sval) != 0) {
		LM_ERR("unable to get value parameter\n");
		return -1;
	}

	dval = strtod(sval.s, NULL);

	ic_double(sname.s, dval);

	return 1;
}


/**
 *
 */
static int w_influxdbc_division(
		sip_msg_t *msg, char *pname, char *pdividend, char *pdivisor)
{
	str sname;
	int idividend;
	int idivisor;

	if(fixup_get_svalue(msg, (gparam_t *)pname, &sname) != 0) {
		LM_ERR("unable to get name parameter\n");
		return -1;
	}
	if(fixup_get_ivalue(msg, (gparam_t *)pdividend, &idividend) != 0) {
		LM_ERR("unable to get dividend parameter\n");
		return -1;
	}
	if(fixup_get_ivalue(msg, (gparam_t *)pdivisor, &idivisor) != 0) {
		LM_ERR("unable to get divisor parameter\n");
		return -1;
	}
	if(idivisor == 0) {
		ic_double(sname.s, 0.0);
		return 1;
	}

	ic_double(sname.s, (double)idividend / (double)idivisor);

	return 1;
}

/**
 *
 */
/* clang-format off */
static sr_kemi_t sr_kemi_influxdbc_exports[] = {
    { str_init("influxdbc"), str_init("measure"),
        SR_KEMIP_INT, ki_influxdbc_measure,
        { SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
            SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
    },
    { str_init("influxdbc"), str_init("measureend"),
        SR_KEMIP_INT, ki_influxdbc_measureend,
        { SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
            SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
    },
    { str_init("influxdbc"), str_init("ic_long"),
        SR_KEMIP_INT, ki_influxdbc_long,
        { SR_KEMIP_STR, SR_KEMIP_INT, SR_KEMIP_NONE,
            SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
    },
    { str_init("influxdbc"), str_init("push"),
        SR_KEMIP_INT, ki_influxdbc_push,
        { SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
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
	sr_kemi_modules_add(sr_kemi_influxdbc_exports);
	return 0;
}
