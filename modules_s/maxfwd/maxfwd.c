/*
 * $Id$
 *
 * MAXFWD module
 *
 *
 * Copyright (C) 2001-2003 Fhg Fokus
 *
 * This file is part of ser, a free SIP server.
 *
 * ser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * For a license to use the ser software under conditions
 * other than those described here, or to purchase support for this
 * software, please contact iptel.org by e-mail at the following addresses:
 *    info@iptel.org
 *
 * ser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
/*
 * History:
 * --------
 *  2003-03-11  updated to the new module interface (andrei)
 *  2003-03-16  flags export parameter added (janakj)
 *  2003-03-19  all mallocs/frees replaced w/ pkg_malloc/pkg_free (andrei)
 *  2004-08-15  max value of max-fwd header is configurable via max_limit
 *              module param (bogdan)
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

static int max_limit = 16;

static int fixup_maxfwd_header(void** param, int param_no);
static int w_process_maxfwd_header(struct sip_msg* msg,char* str,char* str2);
static int mod_init(void);

static cmd_export_t cmds[]={
	{"mf_process_maxfwd_header", w_process_maxfwd_header, 1, 
		fixup_maxfwd_header, REQUEST_ROUTE},
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
	if ( max_limit>255 ) {
		LOG(L_ERR,"ERROR:maxfwd:init: max limit (%d) to high (<255)\n",
			max_limit);
		return -1;
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
			if (code>255){
				LOG(L_ERR, "ERROR:maxfwd:fixup_maxfwd_header: "
					"number to big <%ld> (max=255)\n",code);
				return E_UNSPEC;
			}
			if ( max_limit && code>max_limit) {
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
		case -1:
			add_maxfwd_header( msg, (unsigned int)(unsigned long)str1 );
			break;
		case -2:
			break;
		case 0:
			return -1;
		default:
			if (max_limit && val>max_limit){
				LOG(L_NOTICE, "NOTICE:maxfwd:process_maxfwd_header: "
					"value %d decreased to %d\n", val, max_limit);
				val = max_limit+1;
			}
			if ( decrement_maxfwd(msg, val, &mf_value)!=1 )
				LOG( L_ERR,"ERROR:maxfwd:process_maxfwd_header: "
					"decrement failed!\n");
	}
	return 1;
}




