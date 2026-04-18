/*
 * Local module containing OpenSIPg specific functions that don't have enough
 * appplicability to commit them to Kamailio
 *
 * Copyright (C) 2006-2018 Juha Heinanen
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


#include <stdio.h>
#include <arpa/inet.h>
#include "../../core/ut.h"
#include "../../core/sr_module.h"
#include "../../core/usr_avp.h"
#include "../../core/mem/mem.h"
#include "../../core/mem/shm_mem.h"
#include "../../core/dset.h"
#include "../../core/mod_fix.h"
#include "../../core/locking.h"
#include "local.h"
#include "functions.h"


MODULE_VERSION


/* Module management function prototypes */
static int mod_init(void);
static int child_init(int rank);
static void destroy(void);

/* Default constants */
#define DEF_DIVERSION_REASON_AVP 0
#define DEF_CALLEE_CFUNC_AVP 0
#define DEF_CALLEE_CFUNV_AVP 0
#define DEF_CALLEE_CFB_AVP 0
#define DEF_CALLEE_CFNA_AVP 0
#define DEF_CALLEE_CRUNC_AVP 0
#define DEF_CALLEE_CRUNV_AVP 0
#define DEF_CALLEE_CRB_AVP 0
#define DEF_CALLEE_CRNA_AVP 0

/* Module parameter variables */
int callee_cfunc_avp = DEF_CALLEE_CFUNC_AVP;
int callee_cfunv_avp = DEF_CALLEE_CFUNV_AVP;
int callee_cfb_avp = DEF_CALLEE_CFB_AVP;
int callee_cfna_avp = DEF_CALLEE_CFNA_AVP;
int callee_crunc_avp = DEF_CALLEE_CRUNC_AVP;
int callee_crunv_avp = DEF_CALLEE_CRUNV_AVP;
int callee_crb_avp = DEF_CALLEE_CRB_AVP;
int callee_crna_avp = DEF_CALLEE_CRNA_AVP;
int diversion_reason_avp = DEF_DIVERSION_REASON_AVP;

/* Internal variables */

domain_api_t domain_api;

/* Fixup functions to be defined later */
static int cnd_fixup(void** param, int param_no);


/* Exported functions */
static cmd_export_t cmds[] = {
    {"is_domain_sems", (cmd_function)is_domain_sems, 1, fixup_pvar_null,
     fixup_free_pvar_null, REQUEST_ROUTE|FAILURE_ROUTE|BRANCH_ROUTE},
    {"forwarding", (cmd_function)forwarding, 1, cnd_fixup, 0,
     REQUEST_ROUTE | FAILURE_ROUTE},
    {"diverting_on", (cmd_function)diverting_on, 1, cnd_fixup, 0,
     REQUEST_ROUTE | FAILURE_ROUTE},
    {"redirecting", (cmd_function)redirecting, 1, cnd_fixup, 0,
     REQUEST_ROUTE | FAILURE_ROUTE},
    {0, 0, 0, 0, 0, 0}
};


/* Exported parameters */
static param_export_t params[] = {
    {"callee_cfunc_avp", PARAM_INT, &callee_cfunc_avp},
    {"callee_cfunv_avp", PARAM_INT, &callee_cfunv_avp},
    {"callee_cfb_avp", PARAM_INT, &callee_cfb_avp},
    {"callee_cfna_avp", PARAM_INT, &callee_cfna_avp},
    {"callee_crunc_avp", PARAM_INT, &callee_crunc_avp},
    {"callee_crunv_avp", PARAM_INT, &callee_crunv_avp},
    {"callee_crb_avp", PARAM_INT, &callee_crb_avp},
    {"callee_crna_avp", PARAM_INT, &callee_crna_avp},
    {"diversion_reason_avp", PARAM_INT, &diversion_reason_avp},
    {0, 0, 0}
};


/* Module interface */
struct module_exports exports = {
    "local",
    DEFAULT_DLFLAGS, /* dlopen flags */
    cmds,       /* exported functions */
    params,     /* exported parameters */
    0,          /* exported rpc methods */
    0,          /* exported pseudo vars */
    0,          /* response function*/
    mod_init,   /* module initialization function */
    child_init, /* child initialization function */
    destroy     /* destroy function */
};


/* Module initialization function called in each child separately */
static int child_init(int rank)
{
    return 0;
}


/* Module initialization function that is called
  before the main process forks */
static int mod_init(void)
{
    bind_domain_t bind_domain;

    LM_INFO("initializing\n");

    /* Load domain API */
    bind_domain = (bind_domain_t)find_export("bind_domain", 1, 0);
    if (!bind_domain) {
	LM_ERR("can't find domain module\n");
	return -1;
    }
    if (bind_domain(&domain_api) < 0) {
	LM_ERR("cannot load domain api\n");
	return -1;
    }

    /* Check that all pvar module variables have been defined */
    if (callee_cfunc_avp == 0) {
	LM_ERR("callee_cfunc_avp has not been defined\n");
	goto err;
    }
    if (callee_cfunv_avp == 0) {
	LM_ERR("callee_cfunv_avp has not been defined\n");
	goto err;
    }
    if (callee_cfb_avp == 0) {
	LM_ERR("callee_cfb_avp has not been defined\n");
	goto err;
    }
    if (callee_cfna_avp == 0) {
	LM_ERR("callee_cfna_avp has not been defined\n");
	goto err;
    }
    if (callee_crunc_avp == 0) {
	LM_ERR("callee_crunc_avp has not been defined\n");
	goto err;
    }
    if (callee_crunv_avp == 0) {
	LM_ERR("callee_crunv_avp has not been defined\n");
	goto err;
    }
    if (callee_crb_avp == 0) {
	LM_ERR("callee_crb_avp has not been defined\n");
	goto err;
    }
    if (callee_crna_avp == 0) {
	LM_ERR("callee_crna_avp has not been defined\n");
	goto err;
    }
    if (diversion_reason_avp == 0) {
	LM_ERR("diversion_reason_avp has not been defined\n");
	goto err;
    }

    return 0;

err:

    return -1;
}


static void destroy(void)
{
    return;
}


/* Fixup functions */

/* Convert char* condition parameter to condition_t condition */
static int cnd_fixup(void** param, int param_no)
{
	char *condition;

	if (param_no == 1) {
		condition = (char*)*param;
		if (strcmp(condition, "unconditional") == 0) {
			*param = (void*)UNC;
			return 0;
		}
		if (strcmp(condition, "unavailable") == 0) {
			*param = (void*)UNV;
			return 0;
		}
		if (strcmp(condition, "busy") == 0) {
			*param = (void*)B;
			return 0;
		}
		if (strcmp(condition, "no-answer") == 0) {
			*param = (void*)NA;
			return 0;
		}
		LM_ERR("unknown condition\n");
		return E_UNSPEC;
	}

	return 0;
}
