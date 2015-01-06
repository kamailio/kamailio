/*
 * Copyright (C) 2008-2009 1&1 Internet AG
 * Copyright (C) 2001-2003 FhG Fokus
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
 * \brief Module with several utiltity functions related to SIP messages handling
 * \ingroup siputils
 * - Module \ref siputils
 */

/*!
 * \defgroup siputils SIPUTILS :: Various SIP message handling functions
 *
 *
   This module implement various functions and checks related to
   SIP message handling and URI handling.

   It offers some functions related to handle ringing. In a
   parallel forking scenario you get several 183s with SDP. You
   don't want that your customers hear more than one ringtone or
   answer machine in parallel on the phone. So its necessary to
   drop the 183 in this cases and send a 180 instead.

   This module provides a function to answer OPTIONS requests
   which are directed to the server itself. This means an OPTIONS
   request which has the address of the server in the request
   URI, and no username in the URI. The request will be answered
   with a 200 OK which the capabilities of the server.

   To answer OPTIONS request directed to your server is the
   easiest way for is-alive-tests on the SIP (application) layer
   from remote (similar to ICMP echo requests, also known as
   "ping", on the network layer).

 */

#include <assert.h>

#include "../../sr_module.h"
#include "../../mem/mem.h"
#include "../../dprint.h"
#include "../../script_cb.h"
#include "../../locking.h"
#include "../../ut.h"
#include "../../mod_fix.h"
#include "../../error.h"
#include "../../parser/parse_option_tags.h"

#include "ring.h"
#include "options.h"

#include "checks.h"

#include "rpid.h"
#include "siputils.h"

#include "utils.h"
#include "contact_ops.h"
#include "sipops.h"
#include "config.h"

MODULE_VERSION

/* rpid handling defs */
#define DEF_RPID_PREFIX ""
#define DEF_RPID_SUFFIX ";party=calling;id-type=subscriber;screen=yes"
#define DEF_RPID_AVP "$avp(s:rpid)"

/*! Default Remote-Party-ID prefix */
str rpid_prefix = {DEF_RPID_PREFIX, sizeof(DEF_RPID_PREFIX) - 1};
/*! Default Remote-Party-IDD suffix */
str rpid_suffix = {DEF_RPID_SUFFIX, sizeof(DEF_RPID_SUFFIX) - 1};
/*! Definition of AVP containing rpid value */
char* rpid_avp_param = DEF_RPID_AVP;

gen_lock_t *ring_lock = NULL;
unsigned int ring_timeout = 0;
/* for options functionality */
str opt_accept = str_init(ACPT_DEF);
str opt_accept_enc = str_init(ACPT_ENC_DEF);
str opt_accept_lang = str_init(ACPT_LAN_DEF);
str opt_supported = str_init(SUPT_DEF);
/** SL API structure */
sl_api_t opt_slb;

static int mod_init(void);
static void mod_destroy(void);

/* Fixup functions to be defined later */
static int fixup_set_uri(void** param, int param_no);
static int fixup_free_set_uri(void** param, int param_no);
static int fixup_tel2sip(void** param, int param_no);
static int fixup_get_uri_param(void** param, int param_no);
static int free_fixup_get_uri_param(void** param, int param_no);
static int fixup_option(void** param, int param_no);


char *contact_flds_separator = DEFAULT_SEPARATOR;

static cmd_export_t cmds[]={
	{"ring_insert_callid", (cmd_function)ring_insert_callid, 0, ring_fixup,
		0, REQUEST_ROUTE|FAILURE_ROUTE},
	{"options_reply",      (cmd_function)opt_reply,         0, 0,
		0, REQUEST_ROUTE},
	{"is_user",            (cmd_function)is_user,           1, fixup_str_null,
		0, REQUEST_ROUTE|LOCAL_ROUTE},
	{"has_totag", 	       (cmd_function)has_totag,         0, 0,
		0, ANY_ROUTE},
	{"uri_param",          (cmd_function)uri_param_1,       1, fixup_str_null,
		0, REQUEST_ROUTE|LOCAL_ROUTE},
	{"uri_param",          (cmd_function)uri_param_2,       2, fixup_str_str,
		0, REQUEST_ROUTE|LOCAL_ROUTE},
	{"add_uri_param",      (cmd_function)add_uri_param,     1, fixup_str_null,
		0, REQUEST_ROUTE},
	{"get_uri_param",      (cmd_function)get_uri_param,     2, fixup_get_uri_param, 
		free_fixup_get_uri_param, REQUEST_ROUTE|LOCAL_ROUTE},
	{"tel2sip", (cmd_function)tel2sip, 3, fixup_tel2sip, 0,
	 REQUEST_ROUTE|FAILURE_ROUTE|BRANCH_ROUTE|ONREPLY_ROUTE},
	{"is_e164",            (cmd_function)is_e164,           1, fixup_pvar_null,
		fixup_free_pvar_null, REQUEST_ROUTE|FAILURE_ROUTE|LOCAL_ROUTE},
	{"is_uri_user_e164",   (cmd_function)w_is_uri_user_e164,  1, fixup_pvar_null,
		fixup_free_pvar_null, ANY_ROUTE},
	{"encode_contact",     (cmd_function)encode_contact,    2, 0,
		0, REQUEST_ROUTE|ONREPLY_ROUTE},
	{"decode_contact",     (cmd_function)decode_contact,    0, 0,
		0, REQUEST_ROUTE},
	{"decode_contact_header", (cmd_function)decode_contact_header, 0, 0,
		0,REQUEST_ROUTE|FAILURE_ROUTE|ONREPLY_ROUTE},
	{"cmp_uri",  (cmd_function)w_cmp_uri,                   2, fixup_spve_spve,
		0, ANY_ROUTE},
	{"cmp_aor",  (cmd_function)w_cmp_aor,                   2, fixup_spve_spve,
		0, ANY_ROUTE},
	{"is_rpid_user_e164",   (cmd_function)is_rpid_user_e164, 0, 0,
			0, REQUEST_ROUTE},
	{"append_rpid_hf",      (cmd_function)append_rpid_hf,    0, 0,
			0, REQUEST_ROUTE|BRANCH_ROUTE|FAILURE_ROUTE},
	{"append_rpid_hf",      (cmd_function)append_rpid_hf_p,  2, fixup_str_str,
			0, REQUEST_ROUTE|BRANCH_ROUTE|FAILURE_ROUTE},
	{"set_uri_user", (cmd_function)set_uri_user,             2, fixup_set_uri,
		    fixup_free_set_uri,	ANY_ROUTE},
	{"set_uri_host", (cmd_function)set_uri_host,             2, fixup_set_uri,
		    fixup_free_set_uri,	ANY_ROUTE},
	{"bind_siputils",       (cmd_function)bind_siputils,           1, 0,
			0, 0},
	{"is_request",          (cmd_function)w_is_request,            0, 0,
			0, ANY_ROUTE},
	{"is_reply",            (cmd_function)w_is_reply,              0, 0,
			0, ANY_ROUTE},
	{"is_gruu",  (cmd_function)w_is_gruu,                    0, 0,
		0, ANY_ROUTE},
	{"is_gruu",  (cmd_function)w_is_gruu,                    1, fixup_spve_null,
		0, ANY_ROUTE},
	{"is_supported",  (cmd_function)w_is_supported,                    1, fixup_option,
		0, ANY_ROUTE},
	{"is_first_hop",  (cmd_function)w_is_first_hop,                    0, 0,
		0, ANY_ROUTE},
	{"is_tel_number", (cmd_function)is_tel_number,           1, fixup_spve_null,
		0, ANY_ROUTE},
	{"is_numeric", (cmd_function)is_numeric,                 1, fixup_spve_null,
		0, ANY_ROUTE},
	{0,0,0,0,0,0}
};

static param_export_t params[] = {
	{"ring_timeout",            INT_PARAM, &default_siputils_cfg.ring_timeout},
	{"options_accept",          PARAM_STR, &opt_accept},
	{"options_accept_encoding", PARAM_STR, &opt_accept_enc},
	{"options_accept_language", PARAM_STR, &opt_accept_lang},
	{"options_support",         PARAM_STR, &opt_supported},
	{"contact_flds_separator",  PARAM_STRING, &contact_flds_separator},
	{"rpid_prefix",             PARAM_STR, &rpid_prefix  },
	{"rpid_suffix",             PARAM_STR, &rpid_suffix  },
	{"rpid_avp",                PARAM_STRING, &rpid_avp_param },
	{0, 0, 0}
};


struct module_exports exports= {
	"siputils",
	DEFAULT_DLFLAGS, /* dlopen flags */
	cmds,            /* Exported functions */
	params,          /* param exports */
	0,               /* exported statistics */
	0,               /* exported MI functions */
	0,               /* exported pseudo-variables */
	0,               /* extra processes */
	mod_init,        /* initialization function */
	0,               /* Response function */
	mod_destroy,     /* Destroy function */
	0,               /* Child init function */
};


static int mod_init(void)
{
	if(default_siputils_cfg.ring_timeout > 0) {
		ring_init_hashtable();

		ring_lock = lock_alloc();
		assert(ring_lock);
		if (lock_init(ring_lock) == 0) {
			LM_CRIT("cannot initialize lock.\n");
			return -1;
		}
		if (register_script_cb(ring_filter, PRE_SCRIPT_CB|ONREPLY_CB, 0) != 0) {
			LM_ERR("could not insert callback");
			return -1;
		}
	}

	/* bind the SL API */
	if (sl_load_api(&opt_slb)!=0) {
		LM_ERR("cannot bind to SL API\n");
		return -1;
	}
	
	if ( init_rpid_avp(rpid_avp_param)<0 ) {
		LM_ERR("failed to init rpid AVP name\n");
		return -1;
	}

	if(cfg_declare("siputils", siputils_cfg_def, &default_siputils_cfg, cfg_sizeof(siputils), &siputils_cfg)){
		LM_ERR("Fail to declare the configuration\n");
		return -1;
	}

	return 0;
}


static void mod_destroy(void)
{
	if (ring_lock) {
		lock_destroy(ring_lock);
		lock_dealloc((void *)ring_lock);
		ring_lock = NULL;
	}

	ring_destroy_hashtable();
}


/*!
 * \brief Bind function for the SIPUTILS API
 * \param api binded API
 * \return 0 on success, -1 on failure
 */
int bind_siputils(siputils_api_t* api)
{
	if (!api) {
		LM_ERR("invalid parameter value\n");
		return -1;
	}

	get_rpid_avp( &api->rpid_avp, &api->rpid_avp_type );
	api->has_totag = has_totag;
	api->is_uri_user_e164 = is_uri_user_e164;

	return 0;
}

/*
 * Fix set_uri_* function params: uri (writable pvar) and value (pvar)
 */
static int fixup_set_uri(void** param, int param_no)
{
    if (param_no == 1) {
	if (fixup_pvar_null(param, 1) != 0) {
	    LM_ERR("failed to fixup uri pvar\n");
	    return -1;
	}
	if (((pv_spec_t *)(*param))->setf == NULL) {
	    LM_ERR("uri pvar is not writeble\n");
	    return -1;
	}
	return 0;
    }

    if (param_no == 2) {
	return fixup_pvar_null(param, 1);
    }

    LM_ERR("invalid parameter number <%d>\n", param_no);
    return -1;
}

/*
 * Free set_uri_* params.
 */
static int fixup_free_set_uri(void** param, int param_no)
{
    return fixup_free_pvar_null(param, 1);
}


/*
 * Fix tel2sip function params: uri and hostpart pvars and
 * result writable pvar.
 */
static int fixup_tel2sip(void** param, int param_no)
{
    if ((param_no == 1) || (param_no == 2)) {
	if (fixup_var_str_12(param, 1) < 0) {
	    LM_ERR("failed to fixup uri or hostpart pvar\n");
	    return -1;
	}
	return 0;
    }

    if (param_no == 3) {
	if (fixup_pvar_null(param, 1) != 0) {
	    LM_ERR("failed to fixup result pvar\n");
	    return -1;
	}
	if (((pv_spec_t *)(*param))->setf == NULL) {
	    LM_ERR("result pvar is not writeble\n");
	    return -1;
	}
	return 0;
    }

    LM_ERR("invalid parameter number <%d>\n", param_no);
    return -1;
}

/* */
static int fixup_get_uri_param(void** param, int param_no) {
	if (param_no == 1) {
		return fixup_str_null(param, 1);
	}
	if (param_no == 2) {
		if (fixup_pvar_null(param, 1) != 0) {
			LM_ERR("failed to fixup result pvar\n");
			return -1;
		}
		if (((pv_spec_t *)(*param))->setf == NULL) {
			LM_ERR("result pvar is not writeble\n");
			return -1;
		}
		return 0;
	}

	LM_ERR("invalid parameter number <%d>\n", param_no);
	return -1;
}

/* */
static int free_fixup_get_uri_param(void** param, int param_no) {
	if (param_no == 1) {
		LM_WARN("free function has not been defined for spve\n");
		return 0;
	}
	if (param_no == 2) {
		return fixup_free_pvar_null(param, 1);
	}
	LM_ERR("invalid parameter number <%d>\n", param_no);
	return -1;
}

/* */
static int fixup_option(void** param, int param_no) {

    char *option;
    unsigned int option_len, res;

    option = (char *)*param;
    option_len = strlen(option);

    if (param_no != 1) {
	LM_ERR("invalid parameter number <%d>\n", param_no);
	return -1;
    }

    switch (option_len) {
    case 4:
	if (strncasecmp(option, "path", 4) == 0)
	    res = F_OPTION_TAG_PATH;
	else if (strncasecmp(option, "gruu", 4) == 0)
	    res = F_OPTION_TAG_GRUU;
	else {
	    LM_ERR("unknown option <%s>\n", option);
	    return -1;
	}
	break;
    case 5:
	if (strncasecmp(option, "timer", 5) == 0)
	    res = F_OPTION_TAG_TIMER;
	else {
	    LM_ERR("unknown option <%s>\n", option);
	    return -1;
	}
	break;
    case 6:
	if (strncasecmp(option, "100rel", 6) == 0)
	    res = F_OPTION_TAG_100REL;
	else {
	    LM_ERR("unknown option <%s>\n", option);
	    return -1;
	}
	break;
    case 8:
	if (strncasecmp(option, "outbound", 8) == 0)
	    res = F_OPTION_TAG_OUTBOUND;
	else {
	    LM_ERR("unknown option <%s>\n", option);
	    return -1;
	}
	break;
    case 9:
	if (strncasecmp(option, "eventlist", 9) == 0)
	    res = F_OPTION_TAG_EVENTLIST;
	else {
	    LM_ERR("unknown option <%s>\n", option);
	    return -1;
	}
	break;
    default:
	LM_ERR("unknown option <%s>\n", option);
	return -1;
    }

    *param = (void *)(long)res;
    return 0;
}
