/*
 * $Id$
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

struct module_exports exports = {
	"cpl_module",
	(char*[]){		"cpl_run_script",
				"cpl_is_response_accept",
				"cpl_is_response_reject",
				"cpl_is_response_redirect",
				"cpl_update_contact"
			},
	(cmd_function[]){
				cpl_run_script,
				cpl_is_response_accept,
				cpl_is_response_reject,
				cpl_is_response_redirect,
				cpl_update_contact
				},
	(int[]){
				0,
				0,
				0,
				0,
				0
			},
	(fixup_function[]){
				0,
				0,
				0,
				0,
				0
		},
	5,

	(char*[]) {   /* Module parameter names */
		"cpl_db",
		"cpl_table"
		"cache_timeout"
	},
	(modparam_t[]) {   /* Module parameter types */
		STR_PARAM,
		STR_PARAM,
		INT_PARAM
	},
	(void*[]) {   /* Module parameter variable pointers */
		&DB_URL,
		&DB_TABLE,
		&cache_timeout
	},
	2,      /* Number of module paramers */


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


