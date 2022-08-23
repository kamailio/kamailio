/*
 * Copyright (C) 2005 Voice Sistem SRL
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * Kamailio is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * Kamailio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 */

/*!
 * \file
 * \brief Kamailio uac :: The SIP UA client module
 * \ingroup uac
 * Module: \ref uac
 */

/*! \defgroup uac The SIP UA Client module
 *
 */


#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include "../../core/sr_module.h"
#include "../../core/dprint.h"
#include "../../core/error.h"
#include "../../core/pvar.h"
#include "../../core/pt.h"
#include "../../core/timer.h"
#include "../../core/mem/mem.h"
#include "../../core/parser/parse_from.h"
#include "../../modules/tm/tm_load.h"
#include "../../modules/tm/t_hooks.h"
#include "../../core/mod_fix.h"
#include "../../core/kemi.h"
#include "../../core/rpc.h"
#include "../../core/rpc_lookup.h"
#include "../../core/rand/kam_rand.h"
#include "../../core/cfg/cfg_struct.h"
#include "../dialog/dlg_load.h"

#include "../rr/api.h"

#include "replace.h"
#include "auth.h"
#include "uac_send.h"
#include "uac_reg.h"
#include "api.h"


MODULE_VERSION


/* local variable used for init */
static char* restore_mode_str = NULL;
static char* auth_username_avp = NULL;
static char* auth_realm_avp = NULL;
static char* auth_password_avp = NULL;
unsigned short restore_from_avp_type;
int_str restore_from_avp_name;
unsigned short restore_to_avp_type;
int_str restore_to_avp_name;
static int uac_restore_dlg = 0;
static int reg_active_param = 1;

/* global param variables */
str rr_from_param = str_init("vsf");
str rr_to_param = str_init("vst");
str uac_passwd = str_init("");
str restore_from_avp = STR_NULL;
str restore_to_avp = STR_NULL;
int restore_mode = UAC_AUTO_RESTORE;
struct tm_binds uac_tmb;
struct rr_binds uac_rrb;
pv_spec_t auth_username_spec;
pv_spec_t auth_realm_spec;
pv_spec_t auth_password_spec;
str uac_default_socket = STR_NULL;
struct socket_info * uac_default_sockinfo = NULL;

str uac_event_callback = STR_NULL;

static int w_replace_from(struct sip_msg* msg, char* p1, char* p2);
static int w_restore_from(struct sip_msg* msg, char* p1, char* p2);
static int w_replace_to(struct sip_msg* msg, char* p1, char* p2);
static int w_restore_to(struct sip_msg* msg, char* p1, char* p2);
static int w_uac_auth(struct sip_msg* msg, char* str, char* str2);
static int w_uac_auth_mode(struct sip_msg* msg, char* pmode, char* str2);
static int w_uac_reg_lookup(struct sip_msg* msg, char* src, char* dst);
static int w_uac_reg_lookup_uri(struct sip_msg* msg, char* src, char* dst);
static int w_uac_reg_status(struct sip_msg* msg, char* src, char* dst);
static int w_uac_reg_request_to(struct sip_msg* msg, char* src, char* mode_s);
static int w_uac_reg_enable(struct sip_msg* msg, char* pfilter, char* pval);
static int w_uac_reg_disable(struct sip_msg* msg, char* pfilter, char* pval);
static int w_uac_reg_refresh(struct sip_msg* msg, char* pluuid, char* p2);
static int mod_init(void);
static void mod_destroy(void);
static int child_init(int rank);

extern int reg_timer_interval;
extern int _uac_reg_gc_interval;
extern int _uac_reg_use_domain;

static pv_export_t mod_pvs[] = {
	{ {"uac_req", sizeof("uac_req")-1}, PVT_OTHER, pv_get_uac_req, pv_set_uac_req,
		pv_parse_uac_req_name, 0, 0, 0 },
	{ {0, 0}, 0, 0, 0, 0, 0, 0, 0 }
};


/* Exported functions */
static cmd_export_t cmds[]={
	{"uac_replace_from",  (cmd_function)w_replace_from,  2, fixup_spve_spve, 0,
		REQUEST_ROUTE | BRANCH_ROUTE },
	{"uac_replace_from",  (cmd_function)w_replace_from,  1, fixup_spve_spve, 0,
		REQUEST_ROUTE | BRANCH_ROUTE },
	{"uac_restore_from",  (cmd_function)w_restore_from,  0,		  0, 0,
		REQUEST_ROUTE },
	{"uac_replace_to",  (cmd_function)w_replace_to,  2, fixup_spve_spve, 0,
		REQUEST_ROUTE | BRANCH_ROUTE },
	{"uac_replace_to",  (cmd_function)w_replace_to,  1, fixup_spve_spve, 0,
		REQUEST_ROUTE | BRANCH_ROUTE },
	{"uac_restore_to",  (cmd_function)w_restore_to,  0, 0, 0, REQUEST_ROUTE },
	{"uac_auth",	  (cmd_function)w_uac_auth,       0, 0, 0, FAILURE_ROUTE },
	{"uac_auth",      (cmd_function)w_uac_auth_mode,  1,
			fixup_igp_null, fixup_free_igp_null, FAILURE_ROUTE },
	{"uac_auth_mode", (cmd_function)w_uac_auth_mode,  1,
			fixup_igp_null, fixup_free_igp_null, FAILURE_ROUTE },
	{"uac_req_send",  (cmd_function)w_uac_req_send,   0, 0, 0, ANY_ROUTE},
	{"uac_reg_lookup",  (cmd_function)w_uac_reg_lookup,  2, fixup_spve_pvar,
		fixup_free_spve_pvar, ANY_ROUTE },
	{"uac_reg_lookup_uri", (cmd_function)w_uac_reg_lookup_uri, 2, fixup_spve_pvar,
		fixup_free_spve_pvar, ANY_ROUTE },
	{"uac_reg_status",  (cmd_function)w_uac_reg_status,  1, fixup_spve_null, 0,
		ANY_ROUTE },
	{"uac_reg_request_to",  (cmd_function)w_uac_reg_request_to,  2,
		fixup_spve_igp, fixup_free_spve_igp,
		REQUEST_ROUTE | FAILURE_ROUTE | BRANCH_ROUTE },
	{"uac_reg_enable",   (cmd_function)w_uac_reg_enable,   2, fixup_spve_spve,
		fixup_free_spve_spve, ANY_ROUTE },
	{"uac_reg_disable",  (cmd_function)w_uac_reg_disable,  2, fixup_spve_spve,
		fixup_free_spve_spve, ANY_ROUTE },
	{"uac_reg_refresh",  (cmd_function)w_uac_reg_refresh,  1, fixup_spve_null,
		fixup_free_spve_null, ANY_ROUTE },
	{"bind_uac", (cmd_function)bind_uac,		  1,  0, 0, 0},
	{0,0,0,0,0,0}
};



/* Exported parameters */
static param_export_t params[] = {
	{"rr_from_store_param", PARAM_STR,			&rr_from_param       },
	{"rr_to_store_param",   PARAM_STR,			&rr_to_param       },
	{"restore_mode",	PARAM_STRING,			&restore_mode_str      },
	{"restore_dlg",	 	INT_PARAM,			&uac_restore_dlg       },
	{"restore_passwd",      PARAM_STR,			&uac_passwd	  },
	{"restore_from_avp",	PARAM_STR,			&restore_from_avp },
	{"restore_to_avp",	PARAM_STR,			&restore_to_avp },
	{"credential",		PARAM_STRING|USE_FUNC_PARAM,	(void*)&add_credential },
	{"auth_username_avp",	PARAM_STRING,			&auth_username_avp     },
	{"auth_realm_avp",	PARAM_STRING,			&auth_realm_avp	},
	{"auth_password_avp",	PARAM_STRING,			&auth_password_avp     },
	{"reg_db_url",		PARAM_STR,			&reg_db_url	  },
	{"reg_db_table",	PARAM_STR,			&reg_db_table	},
	{"reg_contact_addr",	PARAM_STR,			&reg_contact_addr    },
	{"reg_timer_interval",	INT_PARAM,			&reg_timer_interval	},
	{"reg_retry_interval",	INT_PARAM,	  		&reg_retry_interval    },
	{"reg_keep_callid",	INT_PARAM,			&reg_keep_callid       },
	{"reg_random_delay",	INT_PARAM,			&reg_random_delay      },
	{"reg_active",	INT_PARAM,			&reg_active_param      },
	{"reg_gc_interval",		INT_PARAM,	&_uac_reg_gc_interval	},
	{"reg_hash_size",	INT_PARAM,		&reg_htable_size      },
	{"reg_use_domain",	PARAM_INT,		&_uac_reg_use_domain  },
	{"default_socket",	PARAM_STR, &uac_default_socket},
	{"event_callback",	PARAM_STR,	&uac_event_callback},
	{0, 0, 0}
};



struct module_exports exports= {
	"uac",           /* module name */
	DEFAULT_DLFLAGS, /* dlopen flags */
	cmds,            /* cmd exports */
	params,          /* param exports */
	0,               /* RPC method exports */
	mod_pvs,         /* pseudo-variables exports */
	0,               /* response handling function */
	mod_init,        /* module initialization function */
	child_init,      /* per-child init function */
	mod_destroy
};


inline static int parse_auth_avp( char *avp_spec, pv_spec_t *avp, char *txt)
{
	str s;
	s.s = avp_spec; s.len = strlen(s.s);
	if (pv_parse_spec(&s, avp)==NULL) {
		LM_ERR("malformed or non AVP %s AVP definition\n",txt);
		return -1;
	}
	return 0;
}


static int mod_init(void)
{
	pv_spec_t avp_spec;
	str host;
	int port, proto;

	if (restore_mode_str && *restore_mode_str) {
		if (strcasecmp(restore_mode_str,"none")==0) {
			restore_mode = UAC_NO_RESTORE;
		} else if (strcasecmp(restore_mode_str,"manual")==0) {
			restore_mode = UAC_MANUAL_RESTORE;
		} else if (strcasecmp(restore_mode_str,"auto")==0) {
			restore_mode = UAC_AUTO_RESTORE;
		} else {
			LM_ERR("unsupported value '%s' for restore_mode\n",  restore_mode_str);
			goto error;
		}
	}

	if ( (rr_from_param.len==0 || rr_to_param.len==0) && restore_mode!=UAC_NO_RESTORE)
	{
		LM_ERR("rr_store_param cannot be empty if FROM is restoreable\n");
		goto error;
	}

	/* parse the auth AVP spesc, if any */
	if ( auth_username_avp || auth_password_avp || auth_realm_avp) {
		if (!auth_username_avp || !auth_password_avp || !auth_realm_avp) {
			LM_ERR("partial definition of auth AVP!");
			goto error;
		}
		if ( parse_auth_avp(auth_realm_avp, &auth_realm_spec, "realm")<0
				|| parse_auth_avp(auth_username_avp, &auth_username_spec, "username")<0
				|| parse_auth_avp(auth_password_avp, &auth_password_spec, "password")<0
		   ) {
			goto error;
		}
	} else {
		memset( &auth_realm_spec, 0, sizeof(pv_spec_t));
		memset( &auth_password_spec, 0, sizeof(pv_spec_t));
		memset( &auth_username_spec, 0, sizeof(pv_spec_t));
	}

	/* load the TM API - FIXME it should be loaded only
	 * if NO_RESTORE and AUTH */
	if (load_tm_api(&uac_tmb)!=0) {
		LM_ERR("can't load TM API\n");
		goto error;
	}

	if (restore_mode!=UAC_NO_RESTORE) {
		/* load the RR API */
		if (load_rr_api(&uac_rrb)!=0) {
			LM_ERR("can't load RR API\n");
			goto error;
		}


		if(restore_from_avp.s) {

			if (pv_parse_spec(&restore_from_avp, &avp_spec)==0	|| avp_spec.type!=PVT_AVP) {
				LM_ERR("malformed or non AVP %.*s AVP definition\n", restore_from_avp.len, restore_from_avp.s);
				return -1;
			}

			if(pv_get_avp_name(0, &avp_spec.pvp, &restore_from_avp_name, &restore_from_avp_type)!=0) {
				LM_ERR("[%.*s]- invalid AVP definition\n", restore_from_avp.len, restore_from_avp.s);
				return -1;
			}

			restore_from_avp_type |= AVP_VAL_STR;

		}

		if(restore_to_avp.s) {

			if (pv_parse_spec(&restore_to_avp, &avp_spec)==0	|| avp_spec.type!=PVT_AVP) {
				LM_ERR("malformed or non AVP %.*s AVP definition\n", restore_to_avp.len, restore_to_avp.s);
				return -1;
			}

			if(pv_get_avp_name(0, &avp_spec.pvp, &restore_to_avp_name, &restore_to_avp_type)!=0) {
				LM_ERR("[%.*s]- invalid AVP definition\n", restore_to_avp.len, restore_to_avp.s);
				return -1;
			}

			restore_to_avp_type |= AVP_VAL_STR;

		}


		if (restore_mode==UAC_AUTO_RESTORE) {
			/* we need the append_fromtag on in RR */

			if (uac_restore_dlg==0) {
				if (!uac_rrb.append_fromtag) {
					LM_ERR("'append_fromtag' RR param is not enabled!"
							" - required by AUTO restore mode\n");
					goto error;
				}
			} else {
				if (uac_init_dlg()!=0) {
					LM_ERR("failed to find dialog API - is dialog module loaded?\n");
					goto error;
				}
			}

			/* get all requests doing loose route */
			if (uac_rrb.register_rrcb( rr_checker, 0)!=0) {
				LM_ERR("failed to install RR callback\n");
				goto error;
			}
		}
	}

	if(reg_db_url.s && reg_db_url.len>=0)
	{
		if(!reg_contact_addr.s || reg_contact_addr.len<=0)
		{
			LM_ERR("contact address parameter not set\n");
			goto error;
		}
		if(reg_active_init(reg_active_param)<0) {
			LM_ERR("failed to init reg active mode\n");
			goto error;
		}
		if(reg_htable_size>14)
			reg_htable_size = 14;
		if(reg_htable_size<2)
			reg_htable_size = 2;

		reg_htable_size = 1<<reg_htable_size;
		if(uac_reg_init_rpc()!=0)
		{
			LM_ERR("failed to register RPC commands\n");
			goto error;
		}
		if(uac_reg_init_ht(reg_htable_size)<0)
		{
			LM_ERR("failed to init reg htable\n");
			goto error;
		}

		register_procs(1);
		/* add child to update local config framework structures */
		cfg_register_child(1);
	}

	if(uac_default_socket.s && uac_default_socket.len > 0) {
		if(parse_phostport(
				   uac_default_socket.s, &host.s, &host.len, &port, &proto)
				!= 0) {
			LM_ERR("bad socket <%.*s>\n", uac_default_socket.len,
					uac_default_socket.s);
			return -1;
		}
		uac_default_sockinfo =
				grep_sock_info(&host, (unsigned short)port, proto);
		if(uac_default_sockinfo == 0) {
			LM_ERR("non-local socket <%.*s>\n", uac_default_socket.len,
					uac_default_socket.s);
			return -1;
		}
		LM_INFO("default uac socket set to <%.*s>\n",
				uac_default_socket.len, uac_default_socket.s);
	}
	init_from_replacer();

	uac_req_init();

	return 0;
error:
	return -1;
}

static int child_init(int rank)
{
	int pid;

	if (rank!=PROC_MAIN)
		return 0;

	if(!reg_db_url.s || reg_db_url.len<=0)
		return 0;

	pid=fork_process(PROC_TIMER, "TIMER UAC REG", 1);
	if (pid<0)
	{
		LM_ERR("failed to register timer routine as process\n");
		return -1;
	}
	if (pid==0){
		/* child */
		/* initialize the config framework */
		if (cfg_child_init())
			return -1;

		uac_reg_load_db();
		LM_DBG("run initial uac registration routine\n");
		uac_reg_timer(0);
		for(;;){
			/* update the local config framework structures */
			cfg_update();

			sleep(reg_timer_interval);
			uac_reg_timer(get_ticks());
		}
	}
	/* parent */
	return 0;
}

static void mod_destroy(void)
{
	destroy_credentials();
}


/************************** wrapper functions ******************************/

static int ki_restore_from(struct sip_msg *msg)
{
	/* safety checks - must be a request */
	if (msg->first_line.type!=SIP_REQUEST) {
		LM_ERR("called for something not request\n");
		return -1;
	}

	return (restore_uri(msg,&rr_from_param,&restore_from_avp,1)==0)?1:-1;
}

static int w_restore_from(struct sip_msg *msg, char *p1, char *p2)
{
	return ki_restore_from(msg);
}

int ki_replace_from(sip_msg_t *msg, str *pdsp, str *puri)
{
	str *uri = NULL;
	str *dsp = NULL;

	dsp = pdsp;
	uri = (puri && puri->len) ? puri : NULL;

	if(parse_from_header(msg) < 0) {
		LM_ERR("failed to find/parse FROM hdr\n");
		return -1;
	}

	LM_DBG("dsp=%p (len=%d) , uri=%p (len=%d)\n", dsp, dsp ? dsp->len : 0, uri,
			uri ? uri->len : 0);

	return (replace_uri(msg, dsp, uri, msg->from, &rr_from_param,
					&restore_from_avp, 1)==0)? 1 : -1;
}

static int ki_replace_from_uri(sip_msg_t* msg, str* puri)
{
	return ki_replace_from(msg, NULL, puri);
}

int w_replace_from(struct sip_msg* msg, char* p1, char* p2)
{
	str uri_s;
	str dsp_s;
	str *dsp = NULL;

	if (p2==NULL) {
		p2 = p1;
		p1 = NULL;
		dsp = NULL;
	}

	/* p1 display , p2 uri */
	if(p1 != NULL) {
		if(fixup_get_svalue(msg, (gparam_t *)p1, &dsp_s) < 0) {
			LM_ERR("cannot get the display name value\n");
			return -1;
		}
		dsp = &dsp_s;
	}

	/* compute the URI string; if empty string -> make it NULL */
	if(fixup_get_svalue(msg, (gparam_t *)p2, &uri_s) < 0) {
		LM_ERR("cannot get the uri value\n");
		return -1;
	}
	return ki_replace_from(msg, dsp, &uri_s);
}

int replace_from_api(sip_msg_t *msg, str* pd, str* pu)
{
	str *uri;
	str *dsp;
	if (parse_from_header(msg)<0 ) {
		LM_ERR("failed to find/parse FROM hdr\n");
		return -1;
	}

	uri = (pu!=NULL && pu->len>0)?pu:NULL;
	dsp = (pd!=NULL && pd->len>0)?pd:NULL;

	LM_DBG("dsp=%p (len=%d) , uri=%p (len=%d)\n", dsp, dsp?dsp->len:0,
			uri, uri?uri->len:0);

	return replace_uri(msg, dsp, uri, msg->from, &rr_from_param, &restore_from_avp, 1);
}

static int ki_restore_to(struct sip_msg *msg)
{
	/* safety checks - must be a request */
	if (msg->first_line.type!=SIP_REQUEST) {
		LM_ERR("called for something not request\n");
		return -1;
	}

	return (restore_uri(msg,&rr_to_param,&restore_to_avp,0)==0)?1:-1;
}

static int w_restore_to(struct sip_msg *msg, char *p1, char *p2)
{
	return ki_restore_to(msg);
}

static int ki_replace_to(sip_msg_t* msg, str* pdsp, str* puri)
{
	str *uri = NULL;
	str *dsp = NULL;

	dsp = pdsp;
	uri = (puri && puri->len) ? puri : NULL;

	/* parse TO hdr */
	if ( msg->to==0 && (parse_headers(msg,HDR_TO_F,0)!=0 || msg->to==0) ) {
		LM_ERR("failed to parse TO hdr\n");
		return -1;
	}

	LM_DBG("dsp=%p (len=%d) , uri=%p (len=%d)\n",
			dsp, dsp?dsp->len:0, uri, uri?uri->len:0);

	return (replace_uri(msg, dsp, uri, msg->to, &rr_to_param,
				&restore_to_avp, 0)==0)?1:-1;
}

static int ki_replace_to_uri(sip_msg_t* msg, str* puri)
{
	return ki_replace_to(msg, NULL, puri);
}

static int w_replace_to(struct sip_msg* msg, char* p1, char* p2)
{
	str uri_s;
	str dsp_s;
	str *dsp = NULL;

	if (p2==NULL) {
		p2 = p1;
		p1 = NULL;
		dsp = NULL;
	}

	/* p1 display , p2 uri */
	if(p1 != NULL) {
		if(fixup_get_svalue(msg, (gparam_t *)p1, &dsp_s) < 0) {
			LM_ERR("cannot get the display name value\n");
			return -1;
		}
		dsp = &dsp_s;
	}

	/* compute the URI string; if empty string -> make it NULL */
	if(fixup_get_svalue(msg, (gparam_t *)p2, &uri_s) < 0) {
		LM_ERR("cannot get the uri value\n");
		return -1;
	}
	return ki_replace_to(msg, dsp, &uri_s);
}


int replace_to_api(sip_msg_t *msg, str* pd, str* pu)
{
	str *uri;
	str *dsp;
	if ( msg->to==0 && (parse_headers(msg,HDR_TO_F,0)!=0 || msg->to==0) ) {
		LM_ERR("failed to find/parse TO hdr\n");
		return -1;
	}

	uri = (pu!=NULL && pu->len>0)?pu:NULL;
	dsp = (pd!=NULL && pd->len>0)?pd:NULL;

	LM_DBG("dsp=%p (len=%d) , uri=%p (len=%d)\n", dsp, dsp?dsp->len:0,
			uri, uri?uri->len:0);

	return replace_uri(msg, dsp, uri, msg->to, &rr_to_param, &restore_to_avp, 0);
}


static int w_uac_auth(struct sip_msg* msg, char* str, char* str2)
{
	return (uac_auth(msg)==0)?1:-1;
}

static int ki_uac_auth(struct sip_msg* msg)
{
	return (uac_auth(msg)==0)?1:-1;
}

static int w_uac_auth_mode(struct sip_msg* msg, char* pmode, char* str2)
{
	int imode = 0;

	if(fixup_get_ivalue(msg, (gparam_t*)pmode, &imode)<0) {
		LM_ERR("failed to get the mode parameter\n");
		return -1;
	}
	return (uac_auth_mode(msg, imode)==0)?1:-1;
}

static int ki_uac_auth_mode(sip_msg_t* msg, int mode)
{
	return (uac_auth_mode(msg, mode)==0)?1:-1;
}

static int w_uac_reg_lookup(struct sip_msg* msg, char* src, char* dst)
{
	pv_spec_t *dpv;
	str sval;

	if(fixup_get_svalue(msg, (gparam_t*)src, &sval)<0) {
		LM_ERR("cannot get the uuid parameter\n");
		return -1;
	}

	dpv = (pv_spec_t*)dst;

	return uac_reg_lookup(msg, &sval, dpv, 0);
}

static int ki_uac_reg_lookup(sip_msg_t* msg, str* userid, str* sdst)
{
	pv_spec_t *dpv = NULL;
	dpv = pv_cache_get(sdst);
	if(dpv==NULL) {
		LM_ERR("cannot get pv spec for [%.*s]\n", sdst->len, sdst->s);
		return -1;
	}
	return uac_reg_lookup(msg, userid, dpv, 0);
}

static int w_uac_reg_lookup_uri(struct sip_msg* msg, char* src, char* dst)
{
	pv_spec_t *dpv;
	str sval;

	if(fixup_get_svalue(msg, (gparam_t*)src, &sval)<0) {
		LM_ERR("cannot get the uuid parameter\n");
		return -1;
	}

	dpv = (pv_spec_t*)dst;

	return uac_reg_lookup(msg, &sval, dpv, 1);
}

static int ki_uac_reg_lookup_uri(sip_msg_t* msg, str* suri, str* sdst)
{
	pv_spec_t *dpv = NULL;
	dpv = pv_cache_get(sdst);
	if(dpv==NULL) {
		LM_ERR("cannot get pv spec for [%.*s]\n", sdst->len, sdst->s);
		return -1;
	}
	return uac_reg_lookup(msg, suri, dpv, 1);
}

static int w_uac_reg_status(struct sip_msg* msg, char* src, char* p2)
{
	str sval;

	if(fixup_get_svalue(msg, (gparam_t*)src, &sval)<0) {
		LM_ERR("cannot get the uuid parameter\n");
		return -1;
	}

	return uac_reg_status(msg, &sval, 0);
}

static int ki_uac_reg_status(sip_msg_t *msg, str *sruuid)
{
	return uac_reg_status(msg, sruuid, 0);
}

static int w_uac_reg_enable(struct sip_msg* msg, char* pfilter, char* pval)
{
	str sfilter;
	str sval;

	if(fixup_get_svalue(msg, (gparam_t*)pfilter, &sfilter)<0) {
		LM_ERR("cannot get the filter parameter\n");
		return -1;
	}
	if(fixup_get_svalue(msg, (gparam_t*)pval, &sval)<0) {
		LM_ERR("cannot get the value parameter\n");
		return -1;
	}
	return uac_reg_enable(msg, &sfilter, &sval);
}

static int w_uac_reg_disable(struct sip_msg* msg, char* pfilter, char* pval)
{
	str sfilter;
	str sval;

	if(fixup_get_svalue(msg, (gparam_t*)pfilter, &sfilter)<0) {
		LM_ERR("cannot get the filter parameter\n");
		return -1;
	}
	if(fixup_get_svalue(msg, (gparam_t*)pval, &sval)<0) {
		LM_ERR("cannot get the value parameter\n");
		return -1;
	}
	return uac_reg_disable(msg, &sfilter, &sval);
}

static int w_uac_reg_refresh(struct sip_msg* msg, char* pluuid, char* p2)
{
	str sluuid;

	if(fixup_get_svalue(msg, (gparam_t*)pluuid, &sluuid)<0) {
		LM_ERR("cannot get the local uuid parameter\n");
		return -1;
	}
	return uac_reg_refresh(msg, &sluuid);
}

static int w_uac_reg_request_to(struct sip_msg* msg, char* src, char* pmode)
{
	str sval;
	int imode;

	if(fixup_get_svalue(msg, (gparam_t*)src, &sval)<0) {
		LM_ERR("cannot get the uuid parameter\n");
		return -1;
	}
	if(fixup_get_ivalue(msg, (gparam_t*)pmode, &imode)<0) {
		LM_ERR("cannot get the mode parameter\n");
		return -1;
	}

	if (imode > (UACREG_REQTO_MASK_USER|UACREG_REQTO_MASK_AUTH)) {
		LM_ERR("invalid mode\n");
		return -1;
	}

	return uac_reg_request_to(msg, &sval, (unsigned int)imode);
}

static int ki_uac_reg_request_to(sip_msg_t *msg, str *userid, int imode)
{
	if (imode > 1) {
		LM_ERR("invalid mode\n");
		return -1;
	}

	return uac_reg_request_to(msg, userid, (unsigned int)imode);
}

int bind_uac(uac_api_t *uacb)
{
	if (uacb == NULL) {
		LM_WARN("bind_uac: Cannot load uac API into a NULL pointer\n");
		return -1;
	}

	memset(uacb, 0, sizeof(uac_api_t));
	uacb->replace_from = replace_from_api;
	uacb->replace_to = replace_to_api;
	uacb->req_send = uac_req_send;
	return 0;
}

/**
 *
 */
/* clang-format off */
static sr_kemi_t sr_kemi_uac_exports[] = {
	{ str_init("uac"), str_init("uac_auth"),
		SR_KEMIP_INT, ki_uac_auth,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("uac"), str_init("uac_auth_mode"),
		SR_KEMIP_INT, ki_uac_auth_mode,
		{ SR_KEMIP_INT, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("uac"), str_init("uac_req_send"),
		SR_KEMIP_INT, ki_uac_req_send,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("uac"), str_init("uac_replace_from_uri"),
		SR_KEMIP_INT, ki_replace_from_uri,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("uac"), str_init("uac_replace_from"),
		SR_KEMIP_INT, ki_replace_from,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("uac"), str_init("uac_restore_from"),
		SR_KEMIP_INT, ki_restore_from,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("uac"), str_init("uac_replace_to_uri"),
		SR_KEMIP_INT, ki_replace_to_uri,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("uac"), str_init("uac_replace_to"),
		SR_KEMIP_INT, ki_replace_to,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("uac"), str_init("uac_restore_to"),
		SR_KEMIP_INT, ki_restore_to,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("uac"), str_init("uac_reg_lookup"),
		SR_KEMIP_INT, ki_uac_reg_lookup,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("uac"), str_init("uac_reg_lookup_uri"),
		SR_KEMIP_INT, ki_uac_reg_lookup_uri,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("uac"), str_init("uac_reg_status"),
		SR_KEMIP_INT, ki_uac_reg_status,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("uac"), str_init("uac_reg_request_to"),
		SR_KEMIP_INT, ki_uac_reg_request_to,
		{ SR_KEMIP_STR, SR_KEMIP_INT, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("uac"), str_init("uac_reg_enable"),
		SR_KEMIP_INT, uac_reg_enable,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("uac"), str_init("uac_reg_disable"),
		SR_KEMIP_INT, uac_reg_disable,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("uac"), str_init("uac_reg_refresh"),
		SR_KEMIP_INT, uac_reg_refresh,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
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
	sr_kemi_modules_add(sr_kemi_uac_exports);
	return 0;
}
