/*
 * $Id: xcap_client.c 2230 2007-06-06 07:13:20Z anca_vamanu $
 *
 * xcap_client module - XCAP client for openser
 *
 * Copyright (C) 2007 Voice Sistem S.R.L.
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
 *  2007-08-20  initial version (anca)
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <curl/curl.h>

#include "../../pt.h"
#include "../../db/db.h"
#include "../../sr_module.h"
#include "../../dprint.h"
#include "../../error.h"
#include "../../ut.h"
#include "../../mem/mem.h"
#include "../../mem/shm_mem.h"
#include "xcap_functions.h"
#include "xcap_client.h"

MODULE_VERSION

static int mod_init(void);
static int child_init(int);
void destroy(void);
struct mi_root* refreshXcapDoc(struct mi_root* cmd, void* param);
int get_auid_flag(str auid);

xcap_callback_t* xcapcb_list= NULL;

static cmd_export_t  cmds[]=
{	
	{"bind_xcap",  (cmd_function)bind_xcap,  1,    0,            0},
	{    0,                     0,           0,    0,            0}
};

static mi_export_t mi_cmds[] = {
	{ "refreshXcapDoc", refreshXcapDoc,      0,  0,  0},
	{ 0,                 0,                  0,  0,  0}
};

/** module exports */
struct module_exports exports= {
	"xcap_client",				/* module name */
	DEFAULT_DLFLAGS,			/* dlopen flags */
	cmds,  						/* exported functions */
	0,  						/* exported parameters */
	0,      					/* exported statistics */
	mi_cmds,   					/* exported MI functions */
	0,							/* exported pseudo-variables */
	0,							/* extra processes */
	mod_init,					/* module initialization function */
	(response_function) 0,      /* response handling function */
	(destroy_function) destroy, /* destroy function */
	child_init                  /* per-child init function */
};

/**
 * init module function
 */
static int mod_init(void)
{
	curl_global_init(CURL_GLOBAL_ALL);
	return 0;
}
static int child_init(int rank)
{ 
	return 0;
}

void destroy(void)
{
	curl_global_cleanup();
}


int parse_doc_url(str doc_url, char** serv_addr, xcap_doc_sel_t* doc_sel)
{
	char* sl, *str_type;	
	
	sl= strchr(doc_url.s, '/');
	*sl= '\0';
	*serv_addr= doc_url.s;
	
	sl++;
	doc_sel->auid.s= sl;
	sl= strchr(sl, '/');
	doc_sel->auid.len= sl- doc_sel->auid.s;
	
	sl++;
	str_type= sl;
	sl= strchr(sl, '/');
	*sl= '\0';

	if(strcasecmp(str_type, "users")== 0)
		doc_sel->type= USERS_TYPE;
	else
	if(strcasecmp(str_type, "group")== 0)
		doc_sel->type= GLOBAL_TYPE;

	sl++;

	return 0;

}
/*
 * mi cmd: refreshXcapDoc
 *			<document url> 
 * */

struct mi_root* refreshXcapDoc(struct mi_root* cmd, void* param)
{
	struct mi_node* node= NULL;
	str doc_url;
	xcap_doc_sel_t doc_sel;
	char* serv_addr;
	char* stream;
	int type;

	node = cmd->node.kids;
	if(node == NULL)
		return 0;

	doc_url = node->value;
	if(doc_url.s == NULL || doc_url.len== 0)
	{
		LOG(L_ERR, "xcap_client:refreshXcapDoc: empty uri\n");
		return init_mi_tree(404, "Empty document URL", 20);
	}
	
	if(node->next!= NULL)
		return 0;

	/* send GET HTTP request to the server */
	stream=	send_http_get(doc_url.s);
	if(stream== NULL)
	{
		LOG(L_ERR, "xcap_client:refreshXcapDoc: ERROR in http get\n");
		return 0;
	}
	
	/* call registered functions with document argument */
	if(parse_doc_url(doc_url, &serv_addr, &doc_sel)< 0)
	{
		LOG(L_ERR, "xcap_client:refreshXcapDoc: ERROR parsing document url\n");
		return 0;
	}

	type= get_auid_flag(doc_sel.auid);
	if(type< 0)
	{
		LOG(L_ERR, "xcap_client:refreshXcapDoc: ERROR incorect auid"
				": %.*s\n",doc_sel.auid.len, doc_sel.auid.s);
		goto error;
	}

	run_xcap_update_cb(type, doc_sel.xid, stream);

	return init_mi_tree(200, "OK", 2);

error:
	if(stream)
		pkg_free(stream);
	return 0;
}

int get_auid_flag(str auid)
{

	switch (auid.len)
	{
		case strlen("pres-rules"):	if(strncmp(auid.s, "pres-rules",
											strlen("pres-rules"))== 0)
										return PRES_RULES;

		case strlen("rls-services"):if(strncmp(auid.s, "rls-services",
											strlen("rls-services"))== 0)
										return RESOURCE_LIST;
	}
	return -1;
}
