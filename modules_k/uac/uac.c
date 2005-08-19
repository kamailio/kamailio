/*
 * $Id$
 *
 * Copyright (C) 2005 Voice Sistem SRL
 *
 * This file is part of openser, a free SIP server.
 *
 * UAC OpenSER-module is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * UAC OpenSER-module is distributed in the hope that it will be useful,
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
 */


#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "../../sr_module.h"
#include "../../dprint.h"
#include "../../error.h"
#include "../../items.h"
#include "../../mem/mem.h"
#include "../tm/tm_load.h"
#include "../tm/t_hooks.h"
#include "../rr/api.h"

#include "from.h"
#include "auth.h"


MODULE_VERSION


/* local variable used for init */
static char* from_restore_mode_str = NULL;

/* global param variables */
str rr_param = {"vsf",3};
int from_restore_mode = FROM_NO_RESTORE;
struct tm_binds uac_tmb;
struct rr_binds uac_rrb;

static int w_replace_from1(struct sip_msg* msg, char* str, char* str2);
static int w_replace_from2(struct sip_msg* msg, char* str, char* str2);
static int w_restore_from(struct sip_msg* msg,  char* foo, char* bar);
static int w_uac_auth(struct sip_msg* msg, char* str, char* str2);
static int fixup_replace_from1(void** param, int param_no);
static int fixup_replace_from2(void** param, int param_no);
static int mod_init(void);
static void mod_destroy();


/* Exported functions */
static cmd_export_t cmds[]={
	{"uac_replace_from",  w_replace_from2,  2, fixup_replace_from2,
			REQUEST_ROUTE },
	{"uac_replace_from",  w_replace_from1,  1, fixup_replace_from1,
			REQUEST_ROUTE },
	{"uac_restore_from",  w_restore_from,   0,                  0,
			REQUEST_ROUTE },
	{"uac_auth",          w_uac_auth,       0,                  0,
			FAILURE_ROUTE },
	{0,0,0,0,0}
};



/* Exported parameters */
static param_export_t params[] = {
	{"rr_store_param",    STR_PARAM,                &rr_param.s            },
	{"from_restore_mode", STR_PARAM,                &from_restore_mode_str },
	{"credential",        STR_PARAM|USE_FUNC_PARAM, &add_credential        },
	{0, 0, 0}
};



struct module_exports exports= {
	"uac",
	cmds,       /* exported functions */
	params,     /* param exports */
	mod_init,   /* module initialization function */
	(response_function) 0,
	mod_destroy,
	0,
	0  /* per-child init function */
};




static int mod_init(void)
{
	LOG(L_INFO,"UAC - initializing\n");

	if (from_restore_mode_str && *from_restore_mode_str) {
		if (strcasecmp(from_restore_mode_str,"none")==0) {
			from_restore_mode = FROM_NO_RESTORE;
		} else if (strcasecmp(from_restore_mode_str,"manual")==0) {
			from_restore_mode = FROM_MANUAL_RESTORE;
		} else if (strcasecmp(from_restore_mode_str,"auto")==0) {
			from_restore_mode = FROM_AUTO_RESTORE;
		} else {
			LOG(L_ERR,"ERROR:uac:mod_init: unsupported value '%s' for "
				"from_restore_mode\n",from_restore_mode_str);
			goto error;
		}
	}

	if (from_restore_mode!=FROM_NO_RESTORE &&
			from_restore_mode!=FROM_AUTO_RESTORE &&
			from_restore_mode!=FROM_MANUAL_RESTORE )
	{
		LOG(L_ERR,"ERROR:uac:mod_init: invalid (%d) restore_from mode\n",
			from_restore_mode);
	}

	rr_param.len = strlen(rr_param.s);
	if (rr_param.len==0 && from_restore_mode!=FROM_NO_RESTORE)
	{
		LOG(L_ERR,"ERROR:uac:mod_init: rr_store_param cannot be empty "
			"if FROM is restoreable\n");
		goto error;
	}

	if (from_restore_mode!=FROM_NO_RESTORE) {
		/* load the TM API */
		if (load_tm_api(&uac_tmb)!=0) {
			LOG(L_ERR, "ERROR:uac:mod_init: can't load TM API\n");
			goto error;
		}

		/* load the RR API */
		if (load_rr_api(&uac_rrb)!=0) {
			LOG(L_ERR, "ERROR:uac:mod_init: can't load RR API\n");
			goto error;
		}

		if (from_restore_mode==FROM_AUTO_RESTORE) {
			/* get all requests doing loose route */
			if (uac_rrb.register_rrcb( rr_checker, 0)!=0) {
				LOG(L_ERR,"ERROR:uac:mod_init: failed to install "
					"RR callback\n");
				goto error;
			}
		}
	}

	init_from_replacer();

	return 0;
error:
	return -1;
}


static void mod_destroy()
{
	destroy_credentials();
}



/************************** fixup functions ******************************/

static int fixup_replace_from1(void** param, int param_no)
{
	xl_elem_t *model;

	model=NULL;
	if (param_no==1)
	{
		if(xl_parse_format((char*)(*param),&model,XL_DISABLE_COLORS)<0)
		{
			LOG(L_ERR, "uac:fixup_replace_from1: ERROR: wrong format[%s]!\n",
				(char*)(*param));
			return E_UNSPEC;
		}
	}
	*param = (void*)model;

	return 0;
}


static int fixup_replace_from2(void** param, int param_no)
{
	xl_elem_t *model;
	char *p;
	str s;

	/* convert to str */
	s.s = (char*)*param;
	s.len = strlen(s.s);
	if (s.len==0)
		s.s = 0;

	model=NULL;
	if (param_no==1)
	{
		if (s.len)
		{
			/* put " to display name */
			p = (char*)pkg_malloc(s.len+3);
			if (p==0)
			{
				LOG(L_CRIT,"ERROR:uac:fixup_replace_from2: no more pkg mem\n");
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
	if(s.s!=0)
	{
		if(xl_parse_format(s.s,&model,XL_DISABLE_COLORS)<0)
		{
			LOG(L_ERR,
				"uac:fixup_replace_from2: ERROR: wrong format[%s]!\n", s.s);
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
		LOG(L_ERR,"ERROR:uac:w_restore_from: called for something "
			"not request\n");
		return -1;
	}

	return (restore_from(msg,0)==0)?1:-1;
}


#define UAC_URI_SIZE	512
static char uac_uri_buf[UAC_URI_SIZE];
static char uac_dsp_buf[UAC_URI_SIZE];

static int w_replace_from1(struct sip_msg* msg, char* uri, char* str2)
{
	str uri_s;

	if(uri==NULL)
		return -1;
	
	uri_s.len = UAC_URI_SIZE;
	uri_s.s = uac_uri_buf;

	if(xl_printf(msg, (xl_elem_p)uri, uri_s.s, &uri_s.len)!=0)
		return -1;
	return (replace_from(msg, 0, &uri_s)==0)?1:-1;
}


static int w_replace_from2(struct sip_msg* msg, char* dsp, char* uri)
{
	str uri_s;
	str dsp_s;
	
	if(uri==NULL && dsp==NULL)
		return -1;

	uri_s.len = UAC_URI_SIZE;
	uri_s.s   = uac_uri_buf;
	dsp_s.len = UAC_URI_SIZE;
	dsp_s.s   = uac_dsp_buf;

	if(uri!=NULL)
		if(xl_printf(msg, (xl_elem_p)uri, uri_s.s, &uri_s.len)!=0)
			return -1;
	if(dsp!=NULL)
		if(xl_printf(msg, (xl_elem_p)dsp, dsp_s.s, &dsp_s.len)!=0)
			return -1;
	return (replace_from(msg, (dsp)?&dsp_s:0, (uri)?&uri_s:0)==0)?1:-1;
}


static int w_uac_auth(struct sip_msg* msg, char* str, char* str2)
{
	return (uac_auth(msg)==0)?1:-1;
}


