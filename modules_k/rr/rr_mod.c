/*
 * $Id$
 *
 * Route & Record-Route module
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
 * --------
 *  2003-03-11  updated to the new module interface (andrei)
 *  2003-03-16  flags export parameter added (janakj)
 *  2003-03-19  all mallocs/frees replaced w/ pkg_malloc/pkg_free (andrei)
 *  2003-04-01  Added record_route with ip address parameter (janakj)
 *  2003-04-14  enable_full_lr parameter introduced (janakj)
 *  2005-04-10  add_rr_param() and check_route_param() added (bogdan)
 *  2006-02-14  record_route may take as param a string to be used as RR param;
 *              record_route and record_route_preset accept pseudo-variables in
 *              parameters; add_rr_param may be called from BRANCH and FAILURE
 *              routes (bogdan)
 */


#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <regex.h>

#include "../../sr_module.h"
#include "../../ut.h"
#include "../../error.h"
#include "../../items.h"
#include "../../mem/mem.h"
#include "loose.h"
#include "record.h"
#include "rr_cb.h"
#include "api.h"

#ifdef ENABLE_USER_CHECK
#include <string.h>
#include "../../str.h"
str i_user;
char *ignore_user = NULL;
#endif

int append_fromtag = 1;
int enable_double_rr = 1; /* Enable using of 2 RR by default */
int enable_full_lr = 0;   /* Disabled by default */
int add_username = 0;     /* Do not add username by default */

static unsigned int last_rr_msg;

MODULE_VERSION

static int  mod_init(void);
static void mod_destroy(void);
/* fixup functions */
static int regexp_fixup(void** param, int param_no);
static int direction_fixup(void** param, int param_no);
static int it_list_fixup(void** param, int param_no);
/* wrapper functions */
static int w_record_route(struct sip_msg *,char *, char *);
static int w_record_route_preset(struct sip_msg *,char *, char *);
static int w_add_rr_param(struct sip_msg *,char *, char *);
static int w_check_route_param(struct sip_msg *,char *, char *);
static int w_is_direction(struct sip_msg *,char *, char *);

/*
 * Exported functions
 */
static cmd_export_t cmds[] = {
	{"loose_route",          loose_route,           0,     0,
			REQUEST_ROUTE},
	{"record_route",         w_record_route,        0,     0,
			REQUEST_ROUTE|BRANCH_ROUTE|FAILURE_ROUTE},
	{"record_route",         w_record_route,        1,     it_list_fixup,
			REQUEST_ROUTE|BRANCH_ROUTE|FAILURE_ROUTE},
	{"record_route_preset",  w_record_route_preset, 1,     it_list_fixup,
			REQUEST_ROUTE|BRANCH_ROUTE|FAILURE_ROUTE},
	{"add_rr_param",         w_add_rr_param,        1,     it_list_fixup,
			REQUEST_ROUTE|BRANCH_ROUTE|FAILURE_ROUTE},
	{"check_route_param",    w_check_route_param,   1,     regexp_fixup,
			REQUEST_ROUTE},
	{"is_direction",         w_is_direction,        1,     direction_fixup,
			REQUEST_ROUTE},
	{"load_rr",              (cmd_function)load_rr, 0,     0,
			0},
	{0, 0, 0, 0, 0}
};


/*
 * Exported parameters
 */
static param_export_t params[] ={ 
	{"append_fromtag",   INT_PARAM, &append_fromtag  },
	{"enable_double_rr", INT_PARAM, &enable_double_rr},
	{"enable_full_lr",   INT_PARAM, &enable_full_lr  },
#ifdef ENABLE_USER_CHECK
	{"ignore_user",      STR_PARAM, &ignore_user     },
#endif
	{"add_username",     INT_PARAM, &add_username    },
	{0, 0, 0 }
};


struct module_exports exports = {
	"rr",
	DEFAULT_DLFLAGS, /* dlopen flags */
	cmds,        /* Exported functions */
	params,      /* Exported parameters */
	0,           /* exported statistics */
	0,           /* exported MI functions */
	0,           /* exported pseudo-variables */
	mod_init,    /* initialize module */
	0,           /* response function*/
	mod_destroy, /* destroy function */
	0            /* per-child init function */
};


static int mod_init(void)
{
	DBG("rr - initializing\n");
#ifdef ENABLE_USER_CHECK
	if(ignore_user)
	{
		i_user.s = ignore_user;
		i_user.len = strlen(ignore_user);
	}
	else
	{
		i_user.s = 0;
		i_user.len = 0;
	}
#endif
	return 0;
}


static void mod_destroy()
{
	destroy_rrcb_lists();
}


static int regexp_fixup(void** param, int param_no)
{
	regex_t* re;

	if (param_no==1) {
		if ((re=pkg_malloc(sizeof(regex_t)))==0) {
			LOG(L_ERR,"ERROR:rr:regexp_fixup: no more pkg memory\n");
			return E_OUT_OF_MEM;
		}
		if (regcomp(re, *param, REG_EXTENDED|REG_ICASE|REG_NEWLINE) ) {
			pkg_free(re);
			LOG(L_ERR, "ERROR:rr:regexp_fixup: bad regexp %s\n",(char*)*param);
			return E_BAD_RE;
		}
		/* free string */
		pkg_free(*param);
		/* replace it with the compiled re */
		*param=re;
	}
	return 0;
}


static int it_list_fixup(void** param, int param_no)
{
	xl_elem_t *model;
	if(*param)
	{
		if(xl_parse_format((char*)(*param), &model, XL_DISABLE_COLORS)<0)
		{
			LOG(L_ERR, "ERROR:textops:item_list_fixup: wrong format[%s]\n",
				(char*)(*param));
			return E_UNSPEC;
		}
		*param = (void*)model;
	}
	return 0;
}


static int direction_fixup(void** param, int param_no)
{
	char *s;
	int n;

	if (!append_fromtag) {
		LOG(L_ERR,"ERROR:rr:direction_fixup: usage of \"is_direction\" function "
			"requires parameter \"append_fromtag\" enabled!!");
		return E_CFG;
	}
	if (param_no==1) {
		n = 0;
		s = (char*) *param;
		if ( strcasecmp(s,"downstream")==0 ) {
			n = RR_FLOW_DOWNSTREAM;
		} else if ( strcasecmp(s,"upstream")==0 ) {
			n = RR_FLOW_UPSTREAM;
		} else {
			LOG(L_ERR,"ERROR:rr:direction_fixup: unknown direction '%s'\n",s);
			return E_CFG;
		}
		/* free string */
		pkg_free(*param);
		/* replace it with the flag */
		*param = (void*)(unsigned long)n;
	}
	return 0;
}


static int w_record_route(struct sip_msg *msg, char *key, char *bar)
{
	str s;

	if (msg->id == last_rr_msg) {
		LOG(L_ERR, "ERROR:rr:record_route: Double attempt to record-route\n");
		return -1;
	}

	if (key && xl_printf_s(msg, (xl_elem_t*)key, &s)<0) {
		LOG(L_ERR,"ERROR:rr:w_record_route1: failed to print "
			"the format\n");
		return -1;
	}
	if ( record_route( msg, key?&s:0 )<0 )
		return -1;

	last_rr_msg = msg->id;
	return 1;
}


static int w_record_route_preset(struct sip_msg *msg, char *key, char *bar)
{
	str s;

	if (msg->id == last_rr_msg) {
		LOG(L_ERR, "ERROR:rr:record_route_preset: Double attempt to "
			"record-route\n");
		return -1;
	}

	if (xl_printf_s(msg, (xl_elem_t*)key, &s)<0) {
		LOG(L_ERR,"ERROR:rr:w_record_route_preset: failed to print "
			"the format\n");
		return -1;
	}
	if ( record_route_preset( msg, &s)<0 )
		return -1;

	last_rr_msg = msg->id;
	return 1;
}


static int w_add_rr_param(struct sip_msg *msg, char *key, char *foo)
{
	str s;

	if (xl_printf_s(msg, (xl_elem_t*)key, &s)<0) {
		LOG(L_ERR,"ERROR:rr:w_add_rr_param: failed to print the format\n");
		return -1;
	}
	return ((add_rr_param( msg, &s)==0)?1:-1);
}



static int w_check_route_param(struct sip_msg *msg,char *re, char *foo)
{
	return ((check_route_param(msg,(regex_t*)re)==0)?1:-1);
}



static int w_is_direction(struct sip_msg *msg,char *dir, char *foo)
{
	return ((is_direction(msg,(int)(long)dir)==0)?1:-1);
}


