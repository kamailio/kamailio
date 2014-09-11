/*
 * Route & Record-Route module
 *
 * $Id$
 *
 * Copyright (C) 2001-2003 FhG Fokus
 *
 * This file is part of ser, a free SIP server.
 *
 * ser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * For a license to use the ser software under conditions
 * other than those described here, or to purchase support for this
 * software, please contact iptel.org by e-mail at the following addresses:
 *    info@iptel.org
 *
 * ser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */
/* History:
 * --------
 *  2003-03-11  updated to the new module interface (andrei)
 *  2003-03-16  flags export parameter added (janakj)
 *  2003-03-19  all mallocs/frees replaced w/ pkg_malloc/pkg_free (andrei)
 *  2003-04-01  Added record_route with ip address parameter (janakj)
 *  2003-04-14  enable_full_lr parameter introduced (janakj)
 */


#include <stdio.h>
#include <stdlib.h>
#include "rr_mod.h"
#include "../../sr_module.h"
#include "../../ut.h"
#include "../../error.h"
#include "../../mem/mem.h"
#include "loose.h"
#include "record.h"
#include "avp_cookie.h"
#include <sys/types.h> /* for regex */
#include <regex.h>
#include "../../script_cb.h"
#include "../../usr_avp.h"
#include "../../crc.h"
#include "../../select.h"
#include "../domain/domain.h"
#include "../../select_buf.h"

int append_fromtag = 1;
int enable_double_rr = 1; /* Enable using of 2 RR by default */
int enable_full_lr = 0;   /* Disabled by default */
str add_username = STR_NULL;   /* Do not add username by default */
char *cookie_filter = 0;       /* filter cookies restored in loose_route, potential security problem */
str user_part_avp = STR_NULL;  /* AVP identification where user part of Route URI is stored after loose/strict routing */
str next_route_avp = STR_NULL; /* AVP identification where next route (if exists) would be stored after loose/strict routing */
static str crc_secret_str = STR_NULL;
avp_ident_t user_part_avp_ident;
avp_ident_t next_route_avp_ident;
int rr_force_send_socket = 1; /* Force the send socket if 2 RR was added */

fparam_t* fparam_username = NULL;

MODULE_VERSION

static int mod_init(void);

domain_get_did_t dm_get_did = 0;

/*
 * Exported functions
 */
/*
 * I do not want people to use strict routing so it is disabled by default,
 * you should always use loose routing, if you really need strict routing then
 * you can replace the last zeroes with REQUEST_ROUTE to enable strict_route and
 * record_route_strict. Don't do that unless you know what you are really doing !
 * Oh, BTW, have I mentioned already that you shouldn't use strict routing ?
 */
static cmd_export_t cmds[] = {
	{"loose_route",          loose_route,         0, 0,           REQUEST_ROUTE},
	{"record_route",         record_route,        0, 0,           REQUEST_ROUTE},
	{"record_route_preset",  record_route_preset, 1, fixup_str_1, REQUEST_ROUTE},
	{"record_route_strict" , record_route_strict, 0, 0,           0            },
	{"remove_record_route",  remove_record_route, 0, 0,           REQUEST_ROUTE|FAILURE_ROUTE},
	{0, 0, 0, 0, 0}
};


/*
 * Exported parameters
 */
static param_export_t params[] ={
	{"append_fromtag",   PARAM_INT,    &append_fromtag  },
	{"enable_double_rr", PARAM_INT,    &enable_double_rr},
	{"enable_full_lr",   PARAM_INT,    &enable_full_lr  },
#ifdef ENABLE_USER_CHECK
	{"ignore_user",      PARAM_STR,    &i_user     },
#endif
	{"add_username",     PARAM_STR,    &add_username    },
	{"cookie_filter",    PARAM_STRING, &cookie_filter   },
	{"cookie_secret",    PARAM_STR,    &crc_secret_str  },
	{"user_part_avp",    PARAM_STR,    &user_part_avp   },
	{"next_route_avp",   PARAM_STR,    &next_route_avp  },
	{"force_send_socket", PARAM_INT,    &rr_force_send_socket  },
	{0, 0, 0 }
};


struct module_exports exports = {
	"rr",
	cmds,        /* Exported functions */
	0,           /* RPC methods */
	params,      /* Exported parameters */
	mod_init,    /* initialize module */
	0,           /* response function*/
	0,           /* destroy function */
	0,           /* oncancel function */
	0            /* per-child init function */
};


static ABSTRACT_F(select_rrmod)

static int select_rr_avpcookie(str* res, struct select* s, struct sip_msg* msg) {
	str *s2;
	int ret;
	s2 = rr_get_avp_cookies();
	if (s2) {
	    ret = str_to_static_buffer(res, s2);
	    pkg_free(s2);
	    return ret;
	}
	else
	    return -1;
}

static select_row_t rr_select_table[] = {
	{ NULL, SEL_PARAM_STR, STR_STATIC_INIT("rr"), select_rrmod, SEL_PARAM_EXPECTED},
	{ select_rrmod, SEL_PARAM_STR, STR_STATIC_INIT("dialog_cookie"), select_rr_avpcookie, 0},
	{ NULL, SEL_PARAM_INT, STR_NULL, NULL, 0}
};
 

static int mod_init(void)
{
	void* param;

	DBG("rr - initializing\n");
	crc_secret = crcitt_string(crc_secret_str.s, crc_secret_str.len);
	if (cookie_filter && strlen(cookie_filter)) {
		cookie_filter_re = (regex_t*)pkg_malloc(sizeof(regex_t));
		memset(cookie_filter_re, 0, sizeof(regex_t));
		if (regcomp(cookie_filter_re, cookie_filter, REG_EXTENDED|REG_ICASE|REG_NEWLINE) ) {
			LOG(L_ERR, "ERROR: %s : bad cookie_filter regex '%s'\n", exports.name, cookie_filter);
			return E_BAD_RE;
		}
	}

	memset (&user_part_avp_ident, 0, sizeof(avp_ident_t));
	if (user_part_avp.s && user_part_avp.len) {
		if (parse_avp_ident(&user_part_avp, &user_part_avp_ident)!=0) {
			ERR("modparam \"user_part_avp\" : error while parsing\n");
			return E_CFG;
		}
	}
	memset (&next_route_avp_ident, 0, sizeof(avp_ident_t));
	if (next_route_avp.s && next_route_avp.len) {
		if (parse_avp_ident(&next_route_avp, &next_route_avp_ident)!=0) {
			ERR("modparam \"next_route_avp\" : error while parsing\n");
			return E_CFG;
		}
	}
	avp_flag_dialog = register_avpflag(AVP_FLAG_DIALOG_COOKIE);
	if (avp_flag_dialog == 0) {
		LOG(L_ERR, "ERROR: %s: cannot register avpflag \"%s\"\n", exports.name, AVP_FLAG_DIALOG_COOKIE);
		return E_CFG;
	}

	register_select_table(rr_select_table);

	dm_get_did = (domain_get_did_t)find_export("get_did", 0, 0);
	if (!dm_get_did) {
	    DBG("Domain module not found, rr support for multidomain disabled\n");
	}
	
	if (add_username.s) {
		param=(void*)add_username.s;
		if (fixup_var_str_12(&param,1)<0) {
			ERR("rr:mod_init:can't fixup add_username parameter\n");
			return E_CFG;
		}
		fparam_username=(fparam_t*)param;
	}

	return 0;
}
