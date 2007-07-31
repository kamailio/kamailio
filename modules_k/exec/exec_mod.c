/*
 * execution module
 *
 * $Id$
 *
 * Copyright (C) 2001-2003 FhG Fokus
 *
 * This file is part of openser, a free SIP server.
 *
 * openser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * openser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * History:
 * -------
 * 2003-03-11: New module interface (janakj)
 * 2003-03-16: flags export parameter added (janakj)
 */


#include <stdio.h>

#include "../../parser/msg_parser.h"
#include "../../str.h"
#include "../../sr_module.h"
#include "../../dprint.h"
#include "../../items.h"
#include "../../parser/parse_uri.h"

#include "exec.h"
#include "kill.h"
#include "exec_hf.h"

MODULE_VERSION

unsigned int time_to_kill=0;

static int mod_init( void );

inline static int w_exec_dset(struct sip_msg* msg, char* cmd, char* foo);
inline static int w_exec_msg(struct sip_msg* msg, char* cmd, char* foo);
inline static int w_exec_avp(struct sip_msg* msg, char* cmd, char* avpl);

static int it_list_fixup(void** param, int param_no);
static int exec_avp_fixup(void** param, int param_no);

inline static void exec_shutdown();

/*
 * Exported functions
 */
static cmd_export_t cmds[] = {
	{"exec_dset", w_exec_dset, 1, it_list_fixup,  REQUEST_ROUTE|FAILURE_ROUTE},
	{"exec_msg",  w_exec_msg,  1, it_list_fixup,  REQUEST_ROUTE|FAILURE_ROUTE},
	{"exec_avp",  w_exec_avp,  1, it_list_fixup,  REQUEST_ROUTE|FAILURE_ROUTE},
	{"exec_avp",  w_exec_avp,  2, exec_avp_fixup, REQUEST_ROUTE|FAILURE_ROUTE},
	{0, 0, 0, 0, 0}
};


/*
 * Exported parameters
 */
static param_export_t params[] = {
	{"time_to_kill", INT_PARAM, &time_to_kill},
	{"setvars",      INT_PARAM, &setvars     },
	{0, 0, 0}
};


#ifdef STATIC_EXEC
struct module_exports exec_exports = {
#else
struct module_exports exports= {
#endif
	"exec",
	DEFAULT_DLFLAGS,/* dlopen flags */
	cmds,           /* Exported functions */
	params,         /* Exported parameters */
	0,              /* exported statistics */
	0,              /* exported MI functions */
	0,              /* exported pseudo-variables */
	0,              /* extra processes */
	mod_init,       /* initialization module */
	0,              /* response function */
	exec_shutdown,  /* destroy function */
	0               /* per-child init function */
};

void exec_shutdown()
{
	if (time_to_kill) destroy_kill();
}


static int mod_init( void )
{
	LOG(L_INFO, "EXEC - initializing\n");
	if (time_to_kill) initialize_kill();
	return 0;
}

inline static int w_exec_dset(struct sip_msg* msg, char* cmd, char* foo)
{
	str *uri;
	environment_t *backup;
	int ret;
	str command;
	xl_elem_t *model;
	
	if(msg==0 || cmd==0)
		return -1;
	
	backup=0;
	if (setvars) {
		backup=set_env(msg);
		if (!backup) {
			LOG(L_ERR, "ERROR: w_exec_msg: no env created\n");
			return -1;
		}
	}

	if (msg->new_uri.s && msg->new_uri.len)
		uri=&msg->new_uri;
	else
		uri=&msg->first_line.u.request.uri;
	
	model = (xl_elem_t*)cmd;
	if(xl_printf_s(msg, model, &command)<0)
	{
		LOG(L_ERR,
			"exec:w_exec_dset: error - cannot print the format\n");
		return -1;
	}
	
	DBG("exec:w_exec_dset: executing [%s]\n", command.s);

	ret=exec_str(msg, command.s, uri->s, uri->len);
	if (setvars) {
		unset_env(backup);
	}
	return ret;
}


inline static int w_exec_msg(struct sip_msg* msg, char* cmd, char* foo)
{
	environment_t *backup;
	int ret;
#define EXEC_MSG_PRINTBUF_SIZE   1024
	static char exec_msg_printbuf[EXEC_MSG_PRINTBUF_SIZE];
	int printbuf_len;
	xl_elem_t *model;
	char *cp;
	
	if(msg==0 || cmd==0)
		return -1;

	backup=0;
	if (setvars) {
		backup=set_env(msg);
		if (!backup) {
			LOG(L_ERR, "ERROR:w_exec_msg: no env created\n");
			return -1;
		}
	}
	
	model = (xl_elem_t*)cmd;
	if(model->next==0 && model->spec.itf==0) {
		cp = model->text.s;
	} else {
		printbuf_len = EXEC_MSG_PRINTBUF_SIZE-1;
		if(xl_printf(msg, model, exec_msg_printbuf, &printbuf_len)<0)
		{
			LOG(L_ERR,
				"exec:w_exec_msg: error - cannot print the format\n");
			return -1;
		}
		cp = exec_msg_printbuf;
	}
	
	DBG("exec:w_exec_msg: executing [%s]\n", cp);
	
	ret=exec_msg(msg, cp);
	if (setvars) {
		unset_env(backup);
	}
	return ret;
}

inline static int w_exec_avp(struct sip_msg* msg, char* cmd, char* avpl)
{
	environment_t *backup;
	int ret;
	str command;
	xl_elem_t *model;
	
	if(msg==0 || cmd==0)
		return -1;
	
	backup=0;
	if (setvars) {
		backup=set_env(msg);
		if (!backup) {
			LOG(L_ERR, "ERROR: w_exec_avp: no env created\n");
			return -1;
		}
	}

	model = (xl_elem_t*)cmd;
	if(xl_printf_s(msg, model, &command)<0)
	{
		LOG(L_ERR,
			"exec:w_exec_avp: error - cannot print the format\n");
		return -1;
	}
	
	DBG("exec:w_exec_avp: executing [%s]\n", command.s);

	ret=exec_avp(msg, command.s, (itemname_list_p)avpl);
	if (setvars) {
		unset_env(backup);
	}
	return ret;
}

/*
 * Convert char* parameter to xl_elem parameter
 */
static int it_list_fixup(void** param, int param_no)
{
	xl_elem_t *model;
	if(param_no==1)
	{
		if(*param)
		{
			if(xl_parse_format((char*)(*param), &model, XL_DISABLE_COLORS)<0)
			{
				LOG(L_ERR, "ERROR:exec:it_list_fixup: wrong format[%s]\n",
					(char*)(*param));
				return E_UNSPEC;
			}
			*param = (void*)model;
		}
	} else {
		LOG(L_ERR, "ERROR:exec:it_list_fixup: wrong format[%s]\n",
					(char*)(*param));
		return E_UNSPEC;
	}
	return 0;
}

static int exec_avp_fixup(void** param, int param_no)
{
	xl_elem_t *model = NULL;
	itemname_list_t *anlist = NULL;
	char *s;

	s = (char*)(*param);
	if (param_no==1)
	{
		if(s==NULL)
		{
			LOG(L_ERR, "ERROR:exec:exec_avp_fixup: null format in P%d\n",
					param_no);
			return E_UNSPEC;
		}
		if(xl_parse_format(s, &model, XL_DISABLE_COLORS|XL_THROW_ERROR)<0)
		{
			LOG(L_ERR,
				"ERROR:exec:exec_avp_fixup: wrong format[%s]\n",
				s);
			return E_UNSPEC;
		}
			
		*param = (void*)model;
		return 0;
	} else if(param_no==2) {
		if(s==NULL)
		{
			LOG(L_ERR, "ERROR:exec:exec_avp_fixup: null format in P%d\n",
					param_no);
			return E_UNSPEC;
		}
		anlist = parse_itemname_list(s, XL_AVP);
		if(anlist==NULL)
		{
			LOG(L_ERR,
				"ERROR:exec_avp_fixup: bad format in P%d [%s]\n",
				param_no, s);
			return E_UNSPEC;
		}
		*param = (void*)anlist;
		return 0;
	}

	return 0;
}


