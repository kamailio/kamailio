/**
 * Copyright (C) 2023 Daniel-Constantin Mierla (asipto.com)
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
#include <math.h>

#include "../../core/sr_module.h"
#include "../../core/dprint.h"
#include "../../core/pvar.h"
#include "../../core/mod_fix.h"
#include "../../core/lvalue.h"


MODULE_VERSION

static int w_math_pow(sip_msg_t *msg, char *v1, char *v2, char *r);
static int w_math_log(sip_msg_t *msg, char *v1, char *r);
static int fixup_math_p2(void **param, int param_no);
static int fixup_math_p3(void **param, int param_no);


/* clang-format off */
static cmd_export_t cmds[]={
	{"math_pow", (cmd_function)w_math_pow, 3, fixup_math_p3,
		0, ANY_ROUTE},
	{"math_log", (cmd_function)w_math_log, 2, fixup_math_p2,
		0, ANY_ROUTE},

	{0, 0, 0, 0, 0, 0}
};

struct module_exports exports = {
	"math",
	DEFAULT_DLFLAGS, /* dlopen flags */
	cmds,
	0,
	0,              /* exported RPC methods */
	0,              /* exported pseudo-variables */
	0,              /* response function */
	0,              /* module initialization function */
	0,              /* per child init function */
	0               /* destroy function */
};
/* clang-format on */


/**
 *
 */
static int w_math_pow(sip_msg_t *msg, char *v1, char *v2, char *r)
{
	int vi1 = 0;
	int vi2 = 0;
	pv_spec_t *dst;
	pv_value_t val = {0};

	if(fixup_get_ivalue(msg, (gparam_t*)v1, &vi1)<0) {
		LM_ERR("failed to get first parameter value\n");
		return -1;
	}
	if(fixup_get_ivalue(msg, (gparam_t*)v2, &vi2)<0) {
		LM_ERR("failed to get second parameter value\n");
		return -1;
	}

	dst = (pv_spec_t *)r;
	if(dst->setf==NULL) {
		LM_ERR("target pv is not writable\n");
		return -1;
	}

	val.ri = (long)pow((double)vi1, (double)vi2);
	val.flags = PV_TYPE_INT|PV_VAL_INT;

	dst->setf(msg, &dst->pvp, (int)EQ_T, &val);

	return 1;
}

/**
 *
 */
static int w_math_log(sip_msg_t *msg, char *v1, char *r)
{
	int vi1 = 0;
	pv_spec_t *dst;
	pv_value_t val = {0};

	if(fixup_get_ivalue(msg, (gparam_t*)v1, &vi1)<0) {
		LM_ERR("failed to get first parameter value\n");
		return -1;
	}

	dst = (pv_spec_t *)r;
	if(dst->setf==NULL) {
		LM_ERR("target pv is not writable\n");
		return -1;
	}

	val.ri = (long)log((double)vi1);
	val.flags = PV_TYPE_INT|PV_VAL_INT;

	dst->setf(msg, &dst->pvp, (int)EQ_T, &val);

	return 1;
}

/**
 *
 */
static int fixup_math_p2(void **param, int param_no)
{
	if (param_no==1) {
		return fixup_igp_igp(param, param_no);
	} else if (param_no==2) {
		return fixup_pvar_null(param, 1);
	}
	return 0;
}

/**
 *
 */
static int fixup_math_p3(void **param, int param_no)
{
	if (param_no==1 || param_no==2) {
		return fixup_igp_igp(param, param_no);
	} else if (param_no==3) {
		return fixup_pvar_null(param, 1);
	}
	return 0;
}

