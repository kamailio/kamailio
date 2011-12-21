/*
 * $Id$
 *
 * Copyright (C) 2005 Voice Sistem SRL
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * UAC Kamailio-module is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * UAC Kamailio-module is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 *
 * History:
 * ---------
 *  2005-01-31  first version (ramona)
 *  2005-08-12  some TM callbacks replaced with RR callback - more efficient;
 *              (bogdan)
 *  2006-03-02  UAC authentication looks first in AVPs for credential (bogdan)
 *  2006-03-03  the RR parameter is encrypted via XOR with a password
 *              (bogdan)

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
#include "../../modules/tm/tm_load.h"
#include "../../modules/tm/t_hooks.h"
#include "../../mod_fix.h"
#include "../../rpc.h"
#include "../../rpc_lookup.h"
#include "../../cfg/cfg_struct.h"

#include "../rr/api.h"

#include "from.h"
#include "auth.h"
#include "uac_send.h"
#include "uac_reg.h"
#include "api.h"


MODULE_VERSION


/* local variable used for init */
static char* from_restore_mode_str = NULL;
static char* auth_username_avp = NULL;
static char* auth_realm_avp = NULL;
static char* auth_password_avp = NULL;

/* global param variables */
str rr_param = str_init("vsf");
str uac_passwd = str_init("");
int from_restore_mode = FROM_AUTO_RESTORE;
struct tm_binds uac_tmb;
struct rr_binds uac_rrb;
pv_spec_t auth_username_spec;
pv_spec_t auth_realm_spec;
pv_spec_t auth_password_spec;

static int w_replace_from1(struct sip_msg* msg, char* str, char* str2);
static int w_replace_from2(struct sip_msg* msg, char* str, char* str2);
static int w_restore_from(struct sip_msg* msg,  char* foo, char* bar);
static int w_uac_auth(struct sip_msg* msg, char* str, char* str2);
static int w_uac_reg_lookup(struct sip_msg* msg,  char* src, char* dst);
static int w_uac_reg_request_to(struct sip_msg* msg,  char* src, char* mode_s);
static int fixup_replace_from1(void** param, int param_no);
static int fixup_replace_from2(void** param, int param_no);
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
	{"uac_replace_from",  (cmd_function)w_replace_from2,  2, fixup_replace_from2, 0,
			REQUEST_ROUTE | BRANCH_ROUTE },
	{"uac_replace_from",  (cmd_function)w_replace_from1,  1, fixup_replace_from1, 0,
			REQUEST_ROUTE | BRANCH_ROUTE },
	{"uac_restore_from",  (cmd_function)w_restore_from,   0,                  0, 0,
			REQUEST_ROUTE },
	{"uac_auth",          (cmd_function)w_uac_auth,       0,                  0, 0,
			FAILURE_ROUTE },
	{"uac_req_send",  (cmd_function)uac_req_send,         0,                  0, 0, 
		REQUEST_ROUTE | FAILURE_ROUTE |
		ONREPLY_ROUTE | BRANCH_ROUTE | ERROR_ROUTE | LOCAL_ROUTE},
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
	{"rr_store_param",    STR_PARAM,                &rr_param.s            },
	{"from_restore_mode", STR_PARAM,                &from_restore_mode_str },
	{"from_passwd",       STR_PARAM,                &uac_passwd.s          },
	{"credential",        STR_PARAM|USE_FUNC_PARAM, (void*)&add_credential },
	{"auth_username_avp", STR_PARAM,                &auth_username_avp     },
	{"auth_realm_avp",    STR_PARAM,                &auth_realm_avp        },
	{"auth_password_avp", STR_PARAM,                &auth_password_avp     },
	{"reg_db_url",        STR_PARAM,                &reg_db_url.s          },
	{"reg_contact_addr",  STR_PARAM,                &reg_contact_addr.s    },
	{"reg_timer_interval", INT_PARAM,		&reg_timer_interval	},
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
	if (from_restore_mode_str && *from_restore_mode_str) {
		if (strcasecmp(from_restore_mode_str,"none")==0) {
			from_restore_mode = FROM_NO_RESTORE;
		} else if (strcasecmp(from_restore_mode_str,"manual")==0) {
			from_restore_mode = FROM_MANUAL_RESTORE;
		} else if (strcasecmp(from_restore_mode_str,"auto")==0) {
			from_restore_mode = FROM_AUTO_RESTORE;
		} else {
			LM_ERR("unsupported value '%s' for from_restore_mode\n",from_restore_mode_str);
			goto error;
		}
	}

	rr_param.len = strlen(rr_param.s);
	if (rr_param.len==0 && from_restore_mode!=FROM_NO_RESTORE)
	{
		LM_ERR("rr_store_param cannot be empty if FROM is restoreable\n");
		goto error;
	}

	uac_passwd.len = strlen(uac_passwd.s);

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

	if (from_restore_mode!=FROM_NO_RESTORE) {
		/* load the RR API */
		if (load_rr_api(&uac_rrb)!=0) {
			LM_ERR("can't load RR API\n");
			goto error;
		}

		if (from_restore_mode==FROM_AUTO_RESTORE) {
			/* we need the append_fromtag on in RR */
			if (!uac_rrb.append_fromtag) {
				LM_ERR("'append_fromtag' RR param is not enabled"
					" - required by AUTO restore mode!"
					" Or you should set from_restore_mode param to 'none'\n");
				goto error;
			}
			/* get all requests doing loose route */
			if (uac_rrb.register_rrcb( rr_checker, 0)!=0) {
				LM_ERR("failed to install RR callback\n");
				goto error;
			}
		}
	}

	if(reg_db_url.s!=NULL)
	{
		if(reg_contact_addr.s==NULL)
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
		uac_reg_init_db();
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

	if(reg_db_url.s==NULL)
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

static int fixup_replace_from1(void** param, int param_no)
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


static int fixup_replace_from2(void** param, int param_no)
{
	pv_elem_t *model;
	char *p;
	str s;

	/* convert to str */
	s.s = (char*)*param;
	s.len = strlen(s.s);

	model=NULL;
	if (param_no==1)
	{
		if (s.len)
		{
			/* put " around display name */
			p = (char*)pkg_malloc(s.len+3);
			if (p==0)
			{
				LM_CRIT("no more pkg mem\n");
				return E_OUT_OF_MEM;
			}
			p[0] = '\"';
			memcpy(p+1, s.s, s.len);
			p[s.len+1] = '\"';
			p[s.len+2] = '\0';
			pkg_free(s.s);
			s.s = p;
			s.len += 2;
		}
	}
	if(s.len!=0)
	{
		if(pv_parse_format(&s ,&model)<0)
		{
			LM_ERR("wrong format [%s] for param no %d!\n", s.s, param_no);
			pkg_free(s.s);
			return E_UNSPEC;
		}
	}
	*param = (void*)model;

	return 0;
}



/************************** wrapper functions ******************************/

static int w_restore_from(struct sip_msg *msg,  char* foo, char* bar)
{
	/* safety checks - must be a request */
	if (msg->first_line.type!=SIP_REQUEST) {
		LM_ERR("called for something not request\n");
		return -1;
	}

	return (restore_from(msg,0)==0)?1:-1;
}


static int w_replace_from1(struct sip_msg* msg, char* uri, char* str2)
{
	str uri_s;

	if(pv_printf_s( msg, (pv_elem_p)uri, &uri_s)!=0)
		return -1;
	return (replace_from(msg, 0, &uri_s)==0)?1:-1;
}


static int w_replace_from2(struct sip_msg* msg, char* dsp, char* uri)
{
	str uri_s;
	str dsp_s;

	if (dsp!=NULL)
	{
		if(dsp!=NULL)
			if(pv_printf_s( msg, (pv_elem_p)dsp, &dsp_s)!=0)
				return -1;
	} else {
		dsp_s.s = 0;
		dsp_s.len = 0;
	}

	if(uri!=NULL)
	{
		if(pv_printf_s( msg, (pv_elem_p)uri, &uri_s)!=0)
			return -1;
	}

	return (replace_from(msg, &dsp_s, (uri)?&uri_s:0)==0)?1:-1;
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


int bind_uac(struct uac_binds *uacb)
{
	if (uacb == NULL)
        {
                LM_WARN("bind_uac: Cannot load uac API into a NULL pointer\n");
                return -1;
        }

        uacb->replace_from = replace_from;
        return 0;
}
