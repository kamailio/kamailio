/*
 * $Id$
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
 *
 * History:
 * -------
 * 2003-03-11: New module interface (janakj)
 */


#include <stdio.h>
#include <string.h>

#include "../../sr_module.h"
#include "../../str.h"
#include "../../msg_translator.h"
#include "../../data_lump_rpl.h"
#include "../../dprint.h"
#include "../../error.h"
#include "../../ut.h"
#include "../../globals.h"
#include "cpl_parser.h"


//char           *resp_buf;
//char           *cpl_server = "127.0.0.1";
//unsigned int   cpl_port = 18011;
//unsigned int   resp_len;
//unsigned int   resp_code;

char *DB_URL       = 0;
char *DB_TABLE     = 0;
int  cache_timeout = 5;




static int cpl_run_script(struct sip_msg* msg, char* str, char* str2);
static int cpl_is_response_accept(struct sip_msg* msg, char* str, char* str2);
static int cpl_is_response_reject(struct sip_msg* msg, char* str, char* str2);
static int cpl_is_response_redirect(struct sip_msg* msg, char* str, char* str2);
static int cpl_update_contact(struct sip_msg* msg, char* str, char* str2);
static int mod_init(void);


/*
 * Exported functions
 */
static cmd_export_t cmds[] = {
	{"cpl_run_script",           cpl_run_script,           0, 0},
	{"cpl_is_response_accept",   cpl_is_response_accept,   0, 0},
	{"cpl_is_response_reject",   cpl_is_response_reject,   0, 0},
	{"cpl_is_response_redirect", cpl_is_response_redirect, 0, 0},
	{"cpl_update_contact",       cpl_update_contact,       0, 0},
	{0, 0, 0, 0}
};


/*
 * Exported parameters
 */
static param_export_t params[] = {
	{"cpl_db",        STR_PARAM, &DB_URL       },
	{"cpl_table",     STR_PARAM, &DB_TABLE     },
	{"cache_timeout", INT_PARAM, &cache_timeout},
	{0, 0, 0}
};


struct module_exports exports = {
	"cpl_module",
	cmds,     /* Exported functions */
	params,   /* Exported parameters */
	mod_init, /* Module initialization function */
	(response_function) 0,
	(destroy_function) 0,
	0,
	0 /* per-child init function */
};


static int mod_init(void)
{
	fprintf(stderr, "cpl - initializing\n");
	return 0;
}


static int cpl_run_script(struct sip_msg* msg, char* str1, char* str2)
{
	return 1;
}



static int cpl_is_response_accept(struct sip_msg* msg, char* str1, char* str2)
{
	return 1;
}


static int cpl_is_response_reject(struct sip_msg* msg, char* str1, char* str2)
{
	return 1;
}


static int cpl_is_response_redirect(struct sip_msg* msg, char* str1,char* str2)
{
	return 1;
}


static int cpl_update_contact(struct sip_msg* msg, char* str1, char* str2)
{
#ifdef cucu
	TRedirectMessage  *redirect;
	struct lump_rpl *lump;
	char *buf, *p;
	int len;
	int i;

	if (resp_code!=REDIRECT_CALL || !resp_buf || !resp_len)
		return -1;

	redirect = parseRedirectResponse( resp_buf , resp_len );
	printRedirectMessage( redirect );

	len = 9 /*"Contact: "*/;
	/* locations*/
	for( i=0 ; i<redirect->numberOfLocations; i++)
		len += 2/*"<>"*/ + redirect->locations[i].urlLength;
	len += redirect->numberOfLocations -1 /*","*/;
	len += CRLF_LEN;

	buf = pkg_malloc( len );
	if(!buf)
	{
		LOG(L_ERR,"ERROR:cpl_update_contact: out of memory! \n");
		return -1;
	}

	p = buf;
	memcpy( p , "Contact: " , 9);
	p += 9;
	for( i=0 ; i<redirect->numberOfLocations; i++)
	{
		if (i) *(p++)=',';
		*(p++) = '<';
		memcpy(p,redirect->locations[i].URL,redirect->locations[i].urlLength);
		p += redirect->locations[i].urlLength;
		*(p++) = '>';
	}
	memcpy(p,CRLF,CRLF_LEN);

	lump = build_lump_rpl( buf , len );
	if(!buf)
	{
		LOG(L_ERR,"ERROR:cpl_update_contact: unable to build lump_rpl! \n");
		pkg_free( buf );
		return -1;
	}
	add_lump_rpl( msg , lump );

	freeRedirectMessage( redirect );
	pkg_free(buf);
#endif
	return 1;
}


