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
 */


#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "../../sr_module.h"
#include "../../dprint.h"
#include "../../error.h"
#include "../../mem/mem.h"
#include "../tm/tm_load.h"
#include "../tm/t_hooks.h"

#include "from.h"
#include "auth.h"


MODULE_VERSION

/* global param variables */
static char *from_param_chr = "vsf";
str from_param;
int from_restore_mode = FROM_NO_RESTORE;
struct tm_binds uac_tmb;

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
									REQUEST_ROUTE|FAILURE_ROUTE },
	{"uac_replace_from",  w_replace_from1,  1, fixup_replace_from1,
									REQUEST_ROUTE|FAILURE_ROUTE },
	{"uac_restore_from",  w_restore_from,   0,                  0,
									REQUEST_ROUTE|FAILURE_ROUTE|ONREPLY_ROUTE },
	{"uac_auth",          w_uac_auth,       0,                  0,
									FAILURE_ROUTE},
	{0,0,0,0,0}
};



/* Exported parameters */
static param_export_t params[] = {
	{"from_store_param",  STR_PARAM,                &from_param_chr      },
	{"from_restore_mode", INT_PARAM,                &from_restore_mode   },
	{"credential",        STR_PARAM|USE_FUNC_PARAM, &add_credential      },
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

	from_param.s = from_param_chr;
	from_param.len = strlen(from_param_chr);
	if (from_param.len==0)
	{
		LOG(L_ERR,"ERROR:uac:mod_init: from_tag cannot be empty\n");
		goto error;
	}

	if (from_restore_mode!=FROM_NO_RESTORE &&
			from_restore_mode!=FROM_AUTO_RESTORE &&
			from_restore_mode!=FROM_MANUAL_RESTORE )
	{
		LOG(L_ERR,"ERROR:uac:mod_init: invalid (%d) restore_from mode\n",
			from_restore_mode);
	}

	/* load the TM API */
	if (load_tm_api(&uac_tmb)!=0) {
		LOG(L_ERR, "ERROR:uac:mod_init(: can't load TM API\n");
		goto error;
	}

	if (from_restore_mode==FROM_AUTO_RESTORE)
	{
		/* get all transactions */
		if (uac_tmb.register_tmcb( 0, 0, TMCB_REQUEST_IN, tr_checker, 0)!=1)
		{
			LOG(L_ERR,"ERROR:uac:mod_init: failed to install TM callback\n");
			goto error;
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
	str *s;

	/* convert to str */
	s = (str*)pkg_malloc( sizeof(str) );
	if (s==0)
	{
		LOG(L_CRIT,"ERROR:uac:fixup_replace_from1: no more pkg mem\n");
		return E_OUT_OF_MEM;
	}
	s->s = (char*)*param;
	s->len = strlen(s->s);
	if (s->len==0)
	{
		LOG(L_CRIT,"ERROR:uac:fixup_replace_from1: empty parameter "
			"not accepted\n");
		return E_UNSPEC;
	}

	*param=(void*)s;
	return 0;
}


static int fixup_replace_from2(void** param, int param_no)
{
	char *p;
	str *s;

	/* convert to str */
	s = (str*)pkg_malloc( sizeof(str) );
	if (s==0)
	{
		LOG(L_CRIT,"ERROR:uac:fixup_replace_from2: no more pkg mem\n");
		return E_OUT_OF_MEM;
	}
	s->s = (char*)*param;
	s->len = strlen(s->s);
	if (s->len==0)
	{
		pkg_free(s->s);
		s->s = 0;
	}

	if (param_no==1)
	{
		if (s->len)
		{
			/* put " to display name */
			p = (char*)pkg_malloc( s->len+2 );
			if (p==0)
			{
				LOG(L_CRIT,"ERROR:uac:fixup_replace_from2: no more pkg mem\n");
				return E_OUT_OF_MEM;
			}
			p[0] = '\"';
			memcpy( p+1, s->s, s->len);
			p[s->len+1] = '\"';
			pkg_free(s->s);
			s->s = p;
			s->len += 2;
		}
	} else if (param_no==2)
	{
		/* do not allow both params empty */
		if (s->s==0 && ((str*)(*(param-1)))->s==0 )
		{
			LOG(L_CRIT,"ERROR:uac:fixup_replace_from2: both parameter "
				"are empty\n");
			return E_UNSPEC;
		}
	}

	*param=(void*)s;
	return 0;
}



/************************** wrapper functions ******************************/

static int w_restore_from(struct sip_msg *msg,  char* foo, char* bar)
{
	restore_from( msg , (msg->first_line.type==SIP_REQUEST)?1:0 );
	return 1;
}


static int w_replace_from1(struct sip_msg* msg, char* uri, char* str2)
{
	return (replace_from( msg, 0, (str*)uri)==0)?1:-1;
}


static int w_replace_from2(struct sip_msg* msg, char* dsp, char* uri)
{
	return (replace_from( msg, (str*)dsp, (str*)uri)==0)?1:-1;
}


static int w_uac_auth(struct sip_msg* msg, char* str, char* str2)
{
	return (uac_auth(msg)==0)?1:-1;
}


