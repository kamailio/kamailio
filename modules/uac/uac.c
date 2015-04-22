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


#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include "../../sr_module.h"
#include "../../dprint.h"
#include "../../error.h"
#include "../../pvar.h"
#include "../../pt.h"
#include "../../timer.h"
#include "../../mem/mem.h"
#include "../../parser/parse_from.h"
#include "../../modules/tm/tm_load.h"
#include "../../modules/tm/t_hooks.h"
#include "../../mod_fix.h"
#include "../../rpc.h"
#include "../../rpc_lookup.h"
#include "../../cfg/cfg_struct.h"
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
struct dlg_binds dlg_api;

static int w_replace_from(struct sip_msg* msg, char* p1, char* p2);
static int w_restore_from(struct sip_msg* msg);
static int w_replace_to(struct sip_msg* msg, char* p1, char* p2);
static int w_restore_to(struct sip_msg* msg);
static int w_uac_auth(struct sip_msg* msg, char* str, char* str2);
static int w_uac_reg_lookup(struct sip_msg* msg,  char* src, char* dst);
static int w_uac_reg_request_to(struct sip_msg* msg,  char* src, char* mode_s);
static int fixup_replace_uri(void** param, int param_no);
static int mod_init(void);
static void mod_destroy(void);
static int child_init(int rank);

extern int reg_timer_interval;

static pv_export_t mod_pvs[] = {
	{ {"uac_req", sizeof("uac_req")-1}, PVT_OTHER, pv_get_uac_req, pv_set_uac_req,
		pv_parse_uac_req_name, 0, 0, 0 },
	{ {0, 0}, 0, 0, 0, 0, 0, 0, 0 }
};


/* Exported functions */
static cmd_export_t cmds[]={
	{"uac_replace_from",  (cmd_function)w_replace_from,  2, fixup_replace_uri, 0,
			REQUEST_ROUTE | BRANCH_ROUTE },
	{"uac_replace_from",  (cmd_function)w_replace_from,  1, fixup_replace_uri, 0,
			REQUEST_ROUTE | BRANCH_ROUTE },
	{"uac_restore_from",  (cmd_function)w_restore_from,  0,                  0, 0,
			REQUEST_ROUTE },
	{"uac_replace_to",  (cmd_function)w_replace_to,  2, fixup_replace_uri, 0,
		 	REQUEST_ROUTE | BRANCH_ROUTE },
	{"uac_replace_to",  (cmd_function)w_replace_to,  1, fixup_replace_uri, 0,
			REQUEST_ROUTE | BRANCH_ROUTE },
	{"uac_restore_to",  (cmd_function)w_restore_to,  0, 0, 0,
			REQUEST_ROUTE },
	{"uac_auth",          (cmd_function)w_uac_auth,       0,                  0, 0,
			FAILURE_ROUTE },
	{"uac_req_send",  (cmd_function)w_uac_req_send,       0,                  0, 0,
			ANY_ROUTE},
	{"uac_reg_lookup",  (cmd_function)w_uac_reg_lookup,  2, fixup_pvar_pvar,
		fixup_free_pvar_pvar, ANY_ROUTE },
	{"uac_reg_request_to",  (cmd_function)w_uac_reg_request_to,  2, fixup_pvar_uint, fixup_free_pvar_uint,
		REQUEST_ROUTE | FAILURE_ROUTE | BRANCH_ROUTE },
	{"bind_uac", (cmd_function)bind_uac,                  1,                  0, 0,
		0},
	{0,0,0,0,0,0}
};



/* Exported parameters */
static param_export_t params[] = {
	{"rr_from_store_param", PARAM_STR,				&rr_from_param       },
	{"rr_to_store_param",   PARAM_STR,				&rr_to_param       },
	{"restore_mode",        PARAM_STRING,				&restore_mode_str      },
	{"restore_dlg",         INT_PARAM,				&uac_restore_dlg       },
	{"restore_passwd",      PARAM_STR,				&uac_passwd          },
	{"restore_from_avp",	PARAM_STR,				&restore_from_avp },
	{"restore_to_avp",		PARAM_STR,				&restore_to_avp },
	{"credential",        PARAM_STRING|USE_FUNC_PARAM, (void*)&add_credential },
	{"auth_username_avp", PARAM_STRING,                &auth_username_avp     },
	{"auth_realm_avp",    PARAM_STRING,                &auth_realm_avp        },
	{"auth_password_avp", PARAM_STRING,                &auth_password_avp     },
	{"reg_db_url",        PARAM_STR,                &reg_db_url          },
	{"reg_db_table",      PARAM_STR,                &reg_db_table        },
	{"reg_contact_addr",  PARAM_STR,                &reg_contact_addr    },
	{"reg_timer_interval", INT_PARAM,		&reg_timer_interval	},
	{"reg_retry_interval",INT_PARAM,                &reg_retry_interval    },
	{0, 0, 0}
};



struct module_exports exports= {
	"uac",
	DEFAULT_DLFLAGS, /* dlopen flags */
	cmds,       /* exported functions */
	params,     /* param exports */
	0,          /* exported statistics */
	0,          /* exported MI functions */
	mod_pvs,    /* exported pseudo-variables */
	0,          /* extra processes */
	mod_init,   /* module initialization function */
	0,
	mod_destroy,
	child_init  /* per-child init function */
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

			memset(&dlg_api, 0, sizeof(struct dlg_binds));
			if (uac_restore_dlg==0 || load_dlg_api(&dlg_api)!=0) {
				if (!uac_rrb.append_fromtag) {
					LM_ERR("'append_fromtag' RR param is not enabled!"
						" - required by AUTO restore mode\n");
					goto error;
				}
				if (uac_restore_dlg!=0)
					LM_DBG("failed to find dialog API - is dialog module loaded?\n");
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



/************************** fixup functions ******************************/

static int fixup_replace_uri(void** param, int param_no)
{
	pv_elem_t *model;
	str s;

	model=NULL;
	s.s = (char*)(*param); s.len = strlen(s.s);
	if(pv_parse_format(&s, &model)<0)
	{
		LM_ERR("wrong format[%s]!\n",(char*)(*param));
		return E_UNSPEC;
	}
	if (model==NULL)
	{
		LM_ERR("empty parameter!\n");
		return E_UNSPEC;
	}
	*param = (void*)model;

	return 0;
}

/************************** wrapper functions ******************************/

static int w_restore_from(struct sip_msg *msg)
{
	/* safety checks - must be a request */
	if (msg->first_line.type!=SIP_REQUEST) {
		LM_ERR("called for something not request\n");
		return -1;
	}

	return (restore_uri(msg,&rr_from_param,&restore_from_avp,1)==0)?1:-1;
}


int w_replace_from(struct sip_msg* msg, char* p1, char* p2)
{
	str uri_s;
	str dsp_s;
	str *uri = NULL;
	str *dsp = NULL;

	if (p2==NULL) {
		 p2 = p1;
		 p1 = NULL;
		 dsp = NULL;
	}

	/* p1 display , p2 uri */

	if ( p1!=NULL ) {
		if(pv_printf_s( msg, (pv_elem_p)p1, &dsp_s)!=0)
			return -1;
		dsp = &dsp_s;
	}

	/* compute the URI string; if empty string -> make it NULL */
	if (pv_printf_s( msg, (pv_elem_p)p2, &uri_s)!=0)
		return -1;
	uri = uri_s.len?&uri_s:NULL;

	if (parse_from_header(msg)<0 ) {
		LM_ERR("failed to find/parse FROM hdr\n");
		return -1;
	}

	LM_DBG("dsp=%p (len=%d) , uri=%p (len=%d)\n",dsp,dsp?dsp->len:0,uri,uri?uri->len:0);

	return (replace_uri(msg, dsp, uri, msg->from, &rr_from_param, &restore_from_avp, 1)==0)?1:-1;

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

static int w_restore_to(struct sip_msg *msg)
{
	/* safety checks - must be a request */
	if (msg->first_line.type!=SIP_REQUEST) {
		LM_ERR("called for something not request\n");
		return -1;
	}

	return (restore_uri(msg,&rr_to_param,&restore_to_avp,0)==0)?1:-1;
}


static int w_replace_to(struct sip_msg* msg, char* p1, char* p2)
{
	str uri_s;
	str dsp_s;
	str *uri = NULL;
	str *dsp = NULL;

	if (p2==NULL) {
		p2 = p1;
		p1 = NULL;
		dsp = NULL;
	}

	 /* p1 display , p2 uri */

	if( p1!=NULL ) {
		if(pv_printf_s( msg, (pv_elem_p)p1, &dsp_s)!=0)
			return -1;
		dsp = &dsp_s;
	}

	/* compute the URI string; if empty string -> make it NULL */
	if (pv_printf_s( msg, (pv_elem_p)p2, &uri_s)!=0)
		return -1;
	uri = uri_s.len?&uri_s:NULL;

	/* parse TO hdr */
	if ( msg->to==0 && (parse_headers(msg,HDR_TO_F,0)!=0 || msg->to==0) ) {
		LM_ERR("failed to parse TO hdr\n");
		return -1;
	}

	LM_DBG("dsp=%p (len=%d) , uri=%p (len=%d)\n",dsp,dsp?dsp->len:0,uri,uri?uri->len:0);

	return (replace_uri(msg, dsp, uri, msg->to, &rr_to_param, &restore_to_avp, 0)==0)?1:-1;

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


static int w_uac_reg_lookup(struct sip_msg* msg,  char* src, char* dst)
{
	pv_spec_t *spv;
	pv_spec_t *dpv;
	pv_value_t val;

	spv = (pv_spec_t*)src;
	dpv = (pv_spec_t*)dst;
	if(pv_get_spec_value(msg, spv, &val) != 0)
	{
		LM_ERR("cannot get src uri value\n");
		return -1;
	}

	if (!(val.flags & PV_VAL_STR))
	{
	    LM_ERR("src pv value is not string\n");
	    return -1;
	}
	return uac_reg_lookup(msg, &val.rs, dpv, 0);
}


static int w_uac_reg_request_to(struct sip_msg* msg, char* src, char* mode_s)
{
	pv_spec_t *spv;
	pv_value_t val;
	unsigned int mode;

	mode = (unsigned int)(long)mode_s;

	spv = (pv_spec_t*)src;
	if(pv_get_spec_value(msg, spv, &val) != 0)
	{
		LM_ERR("cannot get src uri value\n");
		return -1;
	}

	if (!(val.flags & PV_VAL_STR))
	{
	    LM_ERR("src pv value is not string\n");
	    return -1;
	}

	if (mode > 1)
	{
		LM_ERR("invalid mode\n");
		return -1;
	}

	return uac_reg_request_to(msg, &val.rs, mode);
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
