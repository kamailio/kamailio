#include <stdio.h>
#include <string.h>

#include "../../sr_module.h"
#include "../../data_lump_rpl.h"
#include "../../dprint.h"
#include "../../error.h"
#include "../../ut.h"
#include "jcpli.h"

#define   CPL_SERVER  "gorn.fokus.gmd.de"
#define   CPL_PORT       18011


char               *resp_buf;
unsigned int   resp_len;
unsigned int   resp_code;



static int cpl_run_script(struct sip_msg* msg, char* str, char* str2);
static int cpl_is_response_accept(struct sip_msg* msg, char* str, char* str2);
static int cpl_is_response_reject(struct sip_msg* msg, char* str, char* str2);
static int cpl_is_response_redirect(struct sip_msg* msg, char* str, char* str2);
static int cpl_update_contact(struct sip_msg* msg, char* str, char* str2);


static struct module_exports cpl_exports= {
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
	(response_function) 0,
	(destroy_function) 0,
	0
};


struct module_exports* mod_register()
{
	fprintf(stderr, "cpl - registering\n");
	return &cpl_exports;
}


static int cpl_run_script(struct sip_msg* msg, char* str, char* str2)
{

	if (resp_buf)
	{
		pkg_free(resp_buf);
		resp_buf = 0;
	}

	resp_code =executeCPLForSIPMessage( msg->orig, msg->len, CPL_SERVER,
	  CPL_PORT, &resp_buf, &resp_len);
	if (!resp_code)
	{
		LOG( L_ERR ,  "ERROR : cpl_run_script : cpl running failed!\n");
		goto error;
	}
	DBG("DEBUG : cpl_run_script : response received -> %d\n",resp_code);

	return 1;

error:
	return -1;
}



static int cpl_is_response_accept(struct sip_msg* msg, char* str, char* str2)
{
	return (resp_code==ACCEPT_CALL?1:-1);
}


static int cpl_is_response_reject(struct sip_msg* msg, char* str, char* str2)
{
	if (resp_code==REJECT_CALL && resp_buf && resp_len)
		return 1;
	return -1;
}


static int cpl_is_response_redirect(struct sip_msg* msg, char* str, char* str2)
{
	if (resp_code==REDIRECT_CALL && resp_buf && resp_len)
		return 1;
	return -1;
}

static int cpl_update_contact(struct sip_msg* msg, char* str, char* str2)
{
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
	return 1;
}


