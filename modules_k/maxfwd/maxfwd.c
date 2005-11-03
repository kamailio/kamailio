/*
 * $Id$
 *
 * MAXFWD module
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
 *  2004-08-15  max value of max-fwd header is configurable via max_limit
 *              module param (bogdan)
 *  2005-09-15  max_limit param cannot be disabled anymore (according to RFC)
 *              (bogdan)
 *  2005-11-03  is_maxfwd_lt() function added; MF value saved in 
 *              msg->maxforwards->parsed (bogdan)
 */


#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "../../sr_module.h"
#include "../../dprint.h"
#include "../../error.h"
#include "../../ut.h"
#include "../../mem/mem.h"
#include "mf_funcs.h"

MODULE_VERSION

#define MAXFWD_UPPER_LIMIT 256

static int max_limit = MAXFWD_UPPER_LIMIT;

static int fixup_maxfwd_header(void** param, int param_no);
static int w_process_maxfwd_header(struct sip_msg* msg,char* str,char* str2);
static int is_maxfwd_lt(struct sip_msg *msg, char *slimit, char *foo);
static int mod_init(void);

static cmd_export_t cmds[]={
	{"mf_process_maxfwd_header", w_process_maxfwd_header, 1,
		fixup_maxfwd_header, REQUEST_ROUTE},
	{"is_maxfwd_lt", is_maxfwd_lt, 1,
		fixup_maxfwd_header, REQUEST_ROUTE|FAILURE_ROUTE|BRANCH_ROUTE},
	{0,0,0,0,0}
};

static param_export_t params[]={
	{"max_limit",    INT_PARAM,  &max_limit},
	{0,0,0}
};



#ifdef STATIC_MAXFWD
struct module_exports maxfwd_exports = {
#else
struct module_exports exports= {
#endif
	"maxfwd",
	cmds,
	params,
	mod_init,
	(response_function) 0,
	(destroy_function) 0,
	0,
	0  /* per-child init function */
};



static int mod_init(void)
{
	LOG(L_NOTICE, "Maxfwd module- initializing\n");
	if ( max_limit<1 || max_limit>MAXFWD_UPPER_LIMIT ) {
		LOG(L_ERR,"ERROR:maxfwd:init: invalid max limit (%d) [1,%d]\n",
			max_limit,MAXFWD_UPPER_LIMIT);
		return E_CFG;
	}
	return 0;
}



static int fixup_maxfwd_header(void** param, int param_no)
{
	unsigned long code;
	int err;

	if (param_no==1){
		code=str2s(*param, strlen(*param), &err);
		if (err==0){
			if (code<1 || code>MAXFWD_UPPER_LIMIT){
				LOG(L_ERR, "ERROR:maxfwd:fixup_maxfwd_header: "
					"invalid MAXFWD number <%ld> [1,%d\\n",
					code,MAXFWD_UPPER_LIMIT);
				return E_UNSPEC;
			}
			if (code>max_limit) {
				LOG(L_ERR, "ERROR:maxfwd:fixup_maxfwd_header: "
					"default value <%ld> bigger than max limit(%d)\n",
					code, max_limit);
				return E_UNSPEC;
			}
			pkg_free(*param);
			*param=(void*)code;
			return 0;
		}else{
			LOG(L_ERR, "ERROR:maxfwd:fixup_maxfwd_header: bad  number <%s>\n",
					(char*)(*param));
			return E_UNSPEC;
		}
	}
	return 0;
}



static int w_process_maxfwd_header(struct sip_msg* msg, char* str1,char* str2)
{
	int val;
	str mf_value;

	val=is_maxfwd_present(msg, &mf_value);
	switch (val) {
		/* header not found */
		case -1:
			if (add_maxfwd_header( msg, (unsigned int)(unsigned long)str1)!=0)
				goto error;
			return 2;
		/* error */
		case -2:
			break;
		/* found */
		case 0:
			return -1;
		default:
			if (val>max_limit){
				DBG("DBG:maxfwd:process_maxfwd_header: "
					"value %d decreased to %d\n", val, max_limit);
				val = max_limit+1;
			}
			if ( decrement_maxfwd(msg, val, &mf_value)!=0 ) {
				LOG( L_ERR,"ERROR:maxfwd:process_maxfwd_header: "
					"decrement failed!\n");
				goto error;
			}
	}

	return 1;
error:
	return -2;
}



static int is_maxfwd_lt(struct sip_msg *msg, char *slimit, char *foo)
{
	str mf_value;
	int limit;
	int val;

	limit = (int)(long)slimit;
	val = is_maxfwd_present( msg, &mf_value);
	DBG("DEBUG:maxfwd:is_maxfwd_lt: value = %d \n",val);

	if ( val<0 ) {
		/* error or not found */
		return val-1;
	} else if ( val>=limit ) {
		/* greater or equal than/to limit */
		return -1;
	}

	return 1;
}

