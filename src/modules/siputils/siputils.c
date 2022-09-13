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
 *  This module implement various functions and checks related to
 *  SIP message handling and URI handling.
 *
 *  This module provides a function to answer OPTIONS requests
 *  which are directed to the server itself. This means an OPTIONS
 *  request which has the address of the server in the request
 *  URI, and no username in the URI. The request will be answered
 *  with a 200 OK which the capabilities of the server.
 *
 *  To answer OPTIONS request directed to your server is the
 *  easiest way for is-alive-tests on the SIP (application) layer
 *  from remote (similar to ICMP echo requests, also known as
 *  "ping", on the network layer).

*/

#include <assert.h>

#include "../../core/sr_module.h"
#include "../../core/mem/mem.h"
#include "../../core/dprint.h"
#include "../../core/script_cb.h"
#include "../../core/locking.h"
#include "../../core/ut.h"
#include "../../core/mod_fix.h"
#include "../../core/error.h"
#include "../../core/kemi.h"
#include "../../core/parser/parse_option_tags.h"
#include "../../core/parser/parse_uri.h"
#include "../../core/parser/parse_date.h"

#include "options.h"

#include "checks.h"

#include "rpid.h"
#include "siputils.h"

#include "utils.h"
#include "contact_ops.h"
#include "sipops.h"
#include "config.h"
#include "chargingvector.h"

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

/* for options functionality */
str opt_accept = str_init(ACPT_DEF);
str opt_accept_enc = str_init(ACPT_ENC_DEF);
str opt_accept_lang = str_init(ACPT_LAN_DEF);
str opt_supported = str_init(SUPT_DEF);
/** SL API structure */
sl_api_t opt_slb;

static int mod_init(void);
static void mod_destroy(void);

static int w_contact_param_encode(sip_msg_t *msg, char *pnparam, char *psaddr);
static int w_contact_param_decode(sip_msg_t *msg, char *pnparam, char *p2);
static int w_contact_param_decode_ruri(sip_msg_t *msg, char *pnparam, char *p2);
static int w_contact_param_rm(sip_msg_t *msg, char *pnparam, char *p2);

static int w_hdr_date_check(sip_msg_t *msg, char *ptdiff, char *p2);

/* Fixup functions to be defined later */
static int fixup_set_uri(void** param, int param_no);
static int fixup_free_set_uri(void** param, int param_no);
static int fixup_tel2sip(void** param, int param_no);
static int fixup_get_uri_param(void** param, int param_no);
static int free_fixup_get_uri_param(void** param, int param_no);
static int fixup_option(void** param, int param_no);

static int ki_is_gruu(sip_msg_t *msg);

char *contact_flds_separator = DEFAULT_SEPARATOR;

static cmd_export_t cmds[]={
	{"options_reply",      (cmd_function)opt_reply,         0, 0,
		0, REQUEST_ROUTE},
	{"is_user",            (cmd_function)is_user,           1, fixup_spve_null,
		0, REQUEST_ROUTE|LOCAL_ROUTE},
	{"has_totag", 	       (cmd_function)w_has_totag,         0, 0,
		0, ANY_ROUTE},
	{"uri_param",          (cmd_function)uri_param_1,       1, fixup_spve_null,
		0, REQUEST_ROUTE|BRANCH_ROUTE|FAILURE_ROUTE},
	{"uri_param",          (cmd_function)uri_param_2,       2, fixup_spve_spve,
		0, REQUEST_ROUTE|BRANCH_ROUTE|FAILURE_ROUTE},
	{"uri_param_any",      (cmd_function)w_uri_param_any,   1, fixup_spve_null,
		0, REQUEST_ROUTE|BRANCH_ROUTE|FAILURE_ROUTE},
	{"add_uri_param",      (cmd_function)add_uri_param,     1, fixup_str_null,
		0, REQUEST_ROUTE},
	{"get_uri_param",      (cmd_function)get_uri_param,     2, fixup_get_uri_param,
		free_fixup_get_uri_param, REQUEST_ROUTE|LOCAL_ROUTE},
	{"uri_param_rm",       (cmd_function)w_uri_param_rm,    1, fixup_spve_null,
		0, REQUEST_ROUTE|BRANCH_ROUTE|FAILURE_ROUTE},
	{"tel2sip", (cmd_function)tel2sip, 3, fixup_tel2sip, 0,
		REQUEST_ROUTE|FAILURE_ROUTE|BRANCH_ROUTE|ONREPLY_ROUTE},
	{"is_uri",            (cmd_function)is_uri,           1, fixup_spve_null,
		fixup_free_spve_null, ANY_ROUTE},
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
	{"cmp_hdr_name",  (cmd_function)w_cmp_hdr_name,         2, fixup_spve_spve,
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
	{"is_request",          (cmd_function)w_is_request,            0, 0,
		0, ANY_ROUTE},
	{"is_reply",            (cmd_function)w_is_reply,              0, 0,
		0, ANY_ROUTE},
	{"is_gruu",  (cmd_function)w_is_gruu,                    0, 0,
		0, ANY_ROUTE},
	{"is_gruu",  (cmd_function)w_is_gruu,                    1, fixup_spve_null,
		0, ANY_ROUTE},
	{"is_supported",  (cmd_function)w_is_supported,          1, fixup_option,
		0, ANY_ROUTE},
	{"is_first_hop",  (cmd_function)w_is_first_hop,          0, 0,
		0, ANY_ROUTE},
	{"is_first_hop",  (cmd_function)w_is_first_hop,          1, fixup_igp_null,
		fixup_free_igp_null, ANY_ROUTE},
	{"is_tel_number", (cmd_function)is_tel_number,           1, fixup_spve_null,
		0, ANY_ROUTE},
	{"is_numeric", (cmd_function)is_numeric,                 1, fixup_spve_null,
		0, ANY_ROUTE},
	{"is_alphanum", (cmd_function)ksr_is_alphanum,               1, fixup_spve_null,
		0, ANY_ROUTE},
	{"is_alphanumex", (cmd_function)ksr_is_alphanumex,           2, fixup_spve_spve,
		0, ANY_ROUTE},
	{"sip_p_charging_vector", (cmd_function)sip_handle_pcv,  1, fixup_spve_null,
		fixup_free_spve_null, ANY_ROUTE},
	{"contact_param_encode",      (cmd_function)w_contact_param_encode,    2,
		fixup_spve_spve, fixup_free_spve_spve, REQUEST_ROUTE|ONREPLY_ROUTE},
	{"contact_param_decode",      (cmd_function)w_contact_param_decode,    1,
		fixup_spve_null, fixup_free_spve_null, REQUEST_ROUTE|ONREPLY_ROUTE},
	{"contact_param_decode_ruri", (cmd_function)w_contact_param_decode_ruri, 1,
		fixup_spve_null, fixup_free_spve_null, REQUEST_ROUTE},
	{"contact_param_rm",      (cmd_function)w_contact_param_rm,    1,
		fixup_spve_null, fixup_free_spve_null, REQUEST_ROUTE|ONREPLY_ROUTE},
	{"hdr_date_check",  (cmd_function)w_hdr_date_check,      1, fixup_igp_null,
		fixup_free_igp_null, ANY_ROUTE},

	{"bind_siputils",       (cmd_function)bind_siputils,           1, 0,
		0, 0},

	{0,0,0,0,0,0}
};

static param_export_t params[] = {
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


static pv_export_t mod_pvs[] =  {
	{ {"pcv", (sizeof("pcv")-1)}, PVT_OTHER, pv_get_charging_vector,
		0, pv_parse_charging_vector_name, 0, 0, 0},

	{ {0, 0}, 0, 0, 0, 0, 0, 0, 0 }
};

struct module_exports exports= {
	"siputils",      /* module name */
	DEFAULT_DLFLAGS, /* dlopen flags */
	cmds,            /* exported functions */
	params,          /* param exports */
	0,               /* exported RPC functions */
	mod_pvs,         /* exported pseudo-variables */
	0,               /* response function */
	mod_init,        /* initialization function */
	0,               /* child init function */
	mod_destroy      /* destroy function */
};


static int mod_init(void)
{
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
	api->has_totag = w_has_totag;
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

static int w_contact_param_encode(sip_msg_t *msg, char *pnparam, char *psaddr)
{
	str nparam = STR_NULL;
	str saddr = STR_NULL;

	if(fixup_get_svalue(msg, (gparam_t*)pnparam, &nparam)<0) {
		LM_ERR("failed to get p1\n");
		return -1;
	}

	if(fixup_get_svalue(msg, (gparam_t*)psaddr, &saddr)<0) {
		LM_ERR("failed to get p1\n");
		return -1;
	}
	return ki_contact_param_encode(msg, &nparam, &saddr);
}

static int w_contact_param_decode(sip_msg_t *msg, char *pnparam, char *p2)
{
	str nparam = STR_NULL;

	if(fixup_get_svalue(msg, (gparam_t*)pnparam, &nparam)<0) {
		LM_ERR("failed to get p1\n");
		return -1;
	}

	return ki_contact_param_decode(msg, &nparam);
}

static int w_contact_param_decode_ruri(sip_msg_t *msg, char *pnparam, char *p3)
{
	str nparam = STR_NULL;

	if(fixup_get_svalue(msg, (gparam_t*)pnparam, &nparam)<0) {
		LM_ERR("failed to get p1\n");
		return -1;
	}

	return ki_contact_param_decode_ruri(msg, &nparam);
}

static int w_contact_param_rm(sip_msg_t *msg, char *pnparam, char *p2)
{
	str nparam = STR_NULL;

	if(fixup_get_svalue(msg, (gparam_t*)pnparam, &nparam)<0) {
		LM_ERR("failed to get p1\n");
		return -1;
	}

	return ki_contact_param_rm(msg, &nparam);
}

/*
 * Check if pseudo variable contains a valid uri
 */
static int ki_is_uri(sip_msg_t* msg, str* suri)
{
	sip_uri_t turi;

	if(suri==NULL || suri->s==NULL || suri->len<=0) {
		return -1;
	}
	if(parse_uri(suri->s, suri->len, &turi)!=0) {
		return -1;
	}
	return 1;
}

/*
 * Check date header value with time difference
 */
static int ki_hdr_date_check(sip_msg_t* msg, int tdiff)
{
	time_t tnow, tmsg;

	if ((!msg->date) && (parse_headers(msg, HDR_DATE_F, 0) == -1)) {
		LM_ERR("failed parsing Date header\n");
		return -1;
	}
	if (!msg->date) {
		LM_ERR("Date header field is not found\n");
		return -1;
	}
	if ((!(msg->date)->parsed) && (parse_date_header(msg) < 0)) {
		LM_ERR("failed parsing DATE body\n");
		return -1;
	}

#ifdef HAVE_TIMEGM
	tmsg=timegm(&get_date(msg)->date);
#else
	tmsg=_timegm(&get_date(msg)->date);
#endif
	if (tmsg < 0) {
		LM_ERR("timegm error\n");
		return -2;
	}

	if ((tnow=time(0)) < 0) {
		LM_ERR("time error %s\n", strerror(errno));
		return -3;
	}

	if (tnow > tmsg + tdiff) {
		LM_ERR("autdated date header value (%ld sec)\n", tnow - tmsg + tdiff);
		return -4;
	} else {
		LM_ERR("Date header value OK\n");
	}

	return 1;

}

/**
 *
 */
static int w_hdr_date_check(sip_msg_t *msg, char *ptdiff, char *p2)
{
	int tdiff = 0;

	if(fixup_get_ivalue(msg, (gparam_t*)ptdiff, &tdiff)<0) {
		LM_ERR("failed to get time diff parameter\n");
		return -1;
	}
	return ki_hdr_date_check(msg, tdiff);
}

/**
 *
 */
/* clang-format off */
static sr_kemi_t sr_kemi_siputils_exports[] = {
	{ str_init("siputils"), str_init("has_totag"),
		SR_KEMIP_INT, has_totag,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("siputils"), str_init("is_request"),
		SR_KEMIP_INT, is_request,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("siputils"), str_init("is_reply"),
		SR_KEMIP_INT, is_reply,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("siputils"), str_init("is_first_hop"),
		SR_KEMIP_INT, is_first_hop,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("siputils"), str_init("is_first_hop_mode"),
		SR_KEMIP_INT, is_first_hop_mode,
		{ SR_KEMIP_INT, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("siputils"), str_init("is_uri"),
		SR_KEMIP_INT, ki_is_uri,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("siputils"), str_init("is_user"),
		SR_KEMIP_INT, ki_is_user,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("siputils"), str_init("uri_param"),
		SR_KEMIP_INT, ki_uri_param,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("siputils"), str_init("uri_param_value"),
		SR_KEMIP_INT, ki_uri_param_value,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("siputils"), str_init("uri_param_any"),
		SR_KEMIP_INT, ki_uri_param_any,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("siputils"), str_init("uri_param_rm"),
		SR_KEMIP_INT, ki_uri_param_rm,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("siputils"), str_init("is_tel_number"),
		SR_KEMIP_INT, ki_is_tel_number,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("siputils"), str_init("is_numeric"),
		SR_KEMIP_INT, ki_is_numeric,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("siputils"), str_init("is_alphanum"),
		SR_KEMIP_INT, ki_is_alphanum,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("siputils"), str_init("is_alphanumex"),
		SR_KEMIP_INT, ki_is_alphanumex,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("siputils"), str_init("options_reply"),
		SR_KEMIP_INT, ki_opt_reply,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("siputils"), str_init("encode_contact"),
		SR_KEMIP_INT, ki_encode_contact,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("siputils"), str_init("decode_contact"),
		SR_KEMIP_INT, ki_decode_contact,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("siputils"), str_init("decode_contact_header"),
		SR_KEMIP_INT, ki_decode_contact_header,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("siputils"), str_init("contact_param_encode"),
		SR_KEMIP_INT, ki_contact_param_encode,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("siputils"), str_init("contact_param_decode"),
		SR_KEMIP_INT, ki_contact_param_decode,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("siputils"), str_init("contact_param_decode_ruri"),
		SR_KEMIP_INT, ki_contact_param_decode_ruri,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("siputils"), str_init("contact_param_rm"),
		SR_KEMIP_INT, ki_contact_param_rm,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("siputils"), str_init("hdr_date_check"),
		SR_KEMIP_INT, ki_hdr_date_check,
		{ SR_KEMIP_INT, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("siputils"), str_init("cmp_uri"),
		SR_KEMIP_INT, ki_cmp_uri,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("siputils"), str_init("cmp_aor"),
		SR_KEMIP_INT, ki_cmp_aor,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("siputils"), str_init("cmp_hdr_name"),
		SR_KEMIP_INT, ki_cmp_hdr_name,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("siputils"), str_init("is_gruu"),
		SR_KEMIP_INT, ki_is_gruu,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},

	{ {0, 0}, {0, 0}, 0, NULL, { 0, 0, 0, 0, 0, 0 } }
};
/* clang-format on */

static int ki_is_gruu(sip_msg_t *msg) {
	return w_is_gruu(msg, NULL, NULL);
}

/**
 *
 */
int mod_register(char *path, int *dlflags, void *p1, void *p2)
{
	sr_kemi_modules_add(sr_kemi_siputils_exports);
	return 0;
}
