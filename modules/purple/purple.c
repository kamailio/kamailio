/* Kamailio PURPLE MODULE
 * 
 * Copyright (C) 2008 Atos Worldline
 * Contact: Eric PTAK <eric.ptak@atosorigin.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <libxml/parser.h>

#include "../../lib/srdb1/db.h"
#include "../../sr_module.h"
#include "../../data_lump_rpl.h"
#include "../../parser/msg_parser.h"
#include "../../parser/parse_content.h"
#include "../../parser/parse_from.h"
#include "../../modules/tm/tm_load.h"
#include "../../cfg/cfg_struct.h"
#include "../pua/pua_bind.h"
#include "../pua/pidf.h"

#include "purple.h"
#include "purplepipe.h"
#include "miniclient.h"
#include "namespaces.h"
 
MODULE_VERSION

static int init(void);
static int child_init(int rank);
static void destroy(void);
static void runprocs(int rank);
static int func_send_message(struct sip_msg* msg);
static int func_handle_publish(struct sip_msg* msg);
static int func_handle_subscribe(struct sip_msg* msg, char* uri, char *expires);
static int fixup_subscribe(void** param, int param_no);

int pipefds[2] = {-1, -1};

db1_con_t *pa_db = NULL;
db_func_t pa_dbf;
str db_table = str_init("purplemap");
str db_url = str_init(DEFAULT_RODB_URL);
str httpProxy_host = STR_NULL;
int httpProxy_port = 3128;

/* TM functions */
struct tm_binds tmb;

/* functions imported from pua module*/
pua_api_t pua;
send_publish_t pua_send_publish;
send_subscribe_t pua_send_subscribe;
query_dialog_t pua_is_dialog;

/* libxml imported functions */
xmlNodeGetAttrContentByName_t XMLNodeGetAttrContentByName;
xmlDocGetNodeByName_t XMLDocGetNodeByName;
xmlNodeGetNodeByName_t XMLNodeGetNodeByName;
xmlNodeGetNodeContentByName_t XMLNodeGetNodeContentByName;

static proc_export_t procs[] = {
	{"PURPLE Client",  0,  0, runprocs, 1 },
	{0, 0, 0, 0, 0}
};

static cmd_export_t cmds[]={
	{"purple_send_message",		(cmd_function)func_send_message,	0, 0, 0, REQUEST_ROUTE},
	{"purple_handle_publish",	(cmd_function)func_handle_publish,	0, 0, 0, REQUEST_ROUTE},
	{"purple_handle_subscribe",	(cmd_function)func_handle_subscribe,	2, fixup_subscribe, 0, REQUEST_ROUTE},
	{0, 0, 0, 0, 0, 0} 
};

static param_export_t params[]={
	{"db_url",	PARAM_STR, &db_url},
	{"db_table",	PARAM_STR, &db_table},
	{"httpProxy_host", PARAM_STR, &httpProxy_host},
	{"httpProxy_port", INT_PARAM, &httpProxy_port},
	{0, 0, 0}
};

struct module_exports exports= {
        "purple",        /* module's name */
        DEFAULT_DLFLAGS, /* dlopen flags */
        cmds,            /* exported functions */
        params,          /* module parameters */
        0,               /* exported statistics */
        0,               /* exported MI functions */
        0,               /* exported pseudo-variables */
        procs,           /* extra processes */
        init,            /* module initialization function */
        0,               /* response function */
        destroy,         /* destroy function */
        child_init,      /* per-child init function */
};

 
static int init(void) {
	LM_DBG("db_url=%s/%d/%p\n", ZSW(db_url.s), db_url.len,db_url.s);
	load_tm_f  load_tm;
	bind_pua_t bind_pua;
	bind_libxml_t bind_libxml;
	libxml_api_t libxml_api;

	LM_DBG("initializing...\n");

	/* import DB stuff */
	if (db_bind_mod(&db_url, &pa_dbf)) {
		LM_ERR("Database module not found\n");
		return -1;
	}

	pa_db = pa_dbf.init(&db_url);
	if (!pa_db) {
		LM_ERR("connecting database\n");
		return -1;
	}
	
	if (pa_dbf.use_table(pa_db, &db_table) < 0) {
		LM_ERR("in use_table\n");
		return -1;
	}

	if (pa_db && pa_dbf.close)
		pa_dbf.close(pa_db);


	/* import the TM auto-loading function */
	if((load_tm=(load_tm_f)find_export("load_tm", 0, 0))==NULL) {
		LM_ERR("can't import load_tm\n");
		return -1;
	}
	/* let the auto-loading function load all TM stuff */

	if(load_tm(&tmb)==-1) {
		LM_ERR("can't load tm functions\n");
		return -1;
	}

	/* bind libxml wrapper functions */
	if((bind_libxml= (bind_libxml_t)find_export("bind_libxml_api", 1, 0))== NULL) {
		LM_ERR("can't import bind_libxml_api\n");
		return -1;
	}
	if(bind_libxml(&libxml_api)< 0) {
		LM_ERR("can not bind libxml api\n");
		return -1;
	}

	XMLNodeGetAttrContentByName= libxml_api.xmlNodeGetAttrContentByName;
	XMLDocGetNodeByName= libxml_api.xmlDocGetNodeByName;
	XMLNodeGetNodeByName= libxml_api.xmlNodeGetNodeByName;
	XMLNodeGetNodeContentByName= libxml_api.xmlNodeGetNodeContentByName;

	if(XMLNodeGetAttrContentByName== NULL || XMLDocGetNodeByName== NULL ||
		XMLNodeGetNodeByName== NULL || XMLNodeGetNodeContentByName== NULL)
	{
		LM_ERR("libxml wrapper functions could not be bound\n");
		return -1;
	}

	/* bind pua */
	bind_pua= (bind_pua_t)find_export("bind_pua", 1,0);
	if (!bind_pua) {
		LM_ERR("Can't bind pua\n");
		return -1;
	}
	
	if (bind_pua(&pua) < 0) {
		LM_ERR("Can't bind pua\n");
		return -1;
	}
	if(pua.send_publish == NULL) {
		LM_ERR("Could not import send_publish\n");
		return -1;
	}
	pua_send_publish= pua.send_publish;

	if(pua.send_subscribe == NULL) {
		LM_ERR("Could not import send_subscribe\n");
		return -1;
	}
	pua_send_subscribe= pua.send_subscribe;
	
	if(pua.is_dialog == NULL) {
		LM_ERR("Could not import send_subscribe\n");
		return -1;
	}
	pua_is_dialog= pua.is_dialog;

//	if(pua.register_puacb(PURPLE_PUBLISH, publish_reply_handler, NULL)< 0) {
//		LM_ERR("Could not register callback\n");
//		return -1;
//	}	


	if (pipe(pipefds) < 0) {
		LM_ERR("pipe() failed\n");
		return -1;
	}

	/* add space for one extra process */
	register_procs(1);
	/* add child to update local config framework structures */
	cfg_register_child(1);

	return 0;
}

/**
 * initialize child processes
 */
static int child_init(int rank)
{
	int pid;

	if (rank==PROC_MAIN) {
		pid=fork_process(PROC_NOCHLDINIT, "PURPLE Manager", 1);
		if (pid<0)
			return -1; /* error */
		if(pid==0){
			/* child */
			/* initialize the config framework */
			if (cfg_child_init())
				return -1;
			runprocs(1);
		}
	}

	return 0;
}

static void destroy(void) {
	LM_DBG("cleaning up...\n");
	close(pipefds[0]);
}

static void runprocs(int rank) {
	LM_DBG("initializing child process with PID %d\n", (int) getpid());
	close(pipefds[1]);
	
	miniclient_start(pipefds[0]);
}


static int func_send_message(struct sip_msg* msg) {
	str body, from_uri, dst, tagid;
	int mime;

	LM_DBG("handling MESSAGE\n");
	
	/* extract body */
	if (!(body.s = get_body(msg))) {
		LM_ERR("failed to extract body\n");
		return -1;
	}
	if (!msg->content_length) {
		LM_ERR("no content-length found\n");
		return -1;
	}
	body.len = get_content_length(msg);
	if ((mime = parse_content_type_hdr(msg)) < 1) {
		LM_ERR("failed parse content-type\n");
		return -1;
	}
	//if (mime != (TYPE_TEXT << 16) + SUBTYPE_PLAIN && mime != (TYPE_MESSAGE << 16) + SUBTYPE_CPIM) {
	if ((mime >> 16) != TYPE_TEXT) {
		LM_ERR("invalid content-type 0x%x\n", mime);
		return -1;
	}

	/* extract sender */
	if (parse_headers(msg, HDR_TO_F | HDR_FROM_F, 0) == -1 || !msg->to || !msg->from) {
		LM_ERR("no To/From headers\n");
		return -1;
	}
	if (parse_from_header(msg) < 0 || !msg->from->parsed) {
		LM_ERR("failed to parse From header\n");
		return -1;
	}
	from_uri = ((struct to_body *) msg->from->parsed)->uri;
	tagid = ((struct to_body *) msg->from->parsed)->tag_value;
	LM_DBG("message from <%.*s>\n", from_uri.len, from_uri.s);

	/* extract recipient */
	dst.len = 0;
	if (msg->new_uri.len > 0) {
		LM_DBG("using new URI as destination\n");
		dst = msg->new_uri;
	} else if (msg->first_line.u.request.uri.s 
			&& msg->first_line.u.request.uri.len > 0) {
		LM_DBG("using R-URI as destination\n");
		dst = msg->first_line.u.request.uri;
	} else if (msg->to->parsed) {
		LM_DBG("using TO-URI as destination\n");
		dst = ((struct to_body *) msg->to->parsed)->uri;
	} else {
		LM_ERR("failed to find a valid destination\n");
		return -1;
	}
	
	if (purple_send_message_cmd(&from_uri, &dst, &body, &tagid) < 0) {
		LM_ERR("error sending message cmd via pipe\n");
		return -1;
	}
	else
		LM_DBG("message parsed and sent to pipe successfully\n");
	return 1;
}

static int func_handle_publish(struct sip_msg* msg) {
	str body, from_uri, tagid, note;
	enum purple_publish_basic basic;
	PurpleStatusPrimitive primitive;
	xmlDocPtr sip_doc= NULL;
	xmlNodePtr sip_root= NULL;
	xmlNodePtr node = NULL;
	char* basicstr= NULL, *notestr, notebuff[512];
	int mime;
	note.s = notebuff;
	note.len = 0;
	notebuff[0] = 0;

	LM_DBG("handling PUBLISH\n");
	
	/* extract body */
	if (!(body.s = get_body(msg))) {
		LM_ERR("failed to extract body\n");
		return -1;
	}
	if (!msg->content_length) {
		LM_ERR("no content-length found\n");
		return -1;
	}
	body.len = get_content_length(msg);
	mime = parse_content_type_hdr(msg);

	/* extract sender */
	if (parse_headers(msg, HDR_TO_F | HDR_FROM_F, 0) == -1 || !msg->to || !msg->from) {
		LM_ERR("no To/From headers\n");
		return -1;
	}
	if (parse_from_header(msg) < 0 || !msg->from->parsed) {
		LM_ERR("failed to parse From header\n");
		return -1;
	}
	from_uri = ((struct to_body *) msg->from->parsed)->uri;
	tagid = ((struct to_body *) msg->from->parsed)->tag_value;
	LM_DBG("publish from <%.*s>\n", from_uri.len, from_uri.s);

	if (mime == 0) {
		LM_DBG("no content-type\n");
		basic = PURPLE_BASIC_CLOSED;
		primitive = PURPLE_STATUS_OFFLINE;	
	        if (purple_send_publish_cmd(basic, primitive, &from_uri, &tagid, &note) < 0) {
			LM_ERR("error sending publish cmd via pipe\n");
	                return -1;
		}
		LM_DBG("message parsed and sent to pipe successfully\n");
	        return 1;			
	}

	/*extractiong the information from the sip message body*/
	sip_doc= xmlParseMemory(body.s, body.len);
	if(sip_doc== NULL) {
		LM_ERR("while parsing xml memory\n");
		return -1;
	}
	sip_root= XMLDocGetNodeByName(sip_doc, "presence", NULL);
	if(sip_root== NULL) {
		LM_ERR("while extracting 'presence' node\n");
		goto error;
	}
	
	node = XMLNodeGetNodeByName(sip_root, "basic", NULL);
	if(node== NULL) {
		LM_ERR("while extracting status basic node\n");
		goto error;
	}
	basicstr= (char*)xmlNodeGetContent(node);
	if(basicstr== NULL)  {
		LM_ERR("while extracting status basic node content\n");
		goto error;
	}
	if(xmlStrcasecmp( (unsigned char*)basicstr,(unsigned char*) "closed")==0 ) {
		basic = PURPLE_BASIC_CLOSED;
		primitive = PURPLE_STATUS_OFFLINE;
	}/* else the status is open so no type attr should be added */
	else {
		basic = PURPLE_BASIC_OPEN;
		primitive = PURPLE_STATUS_AVAILABLE;
	}
	
	node = NULL;
	node = XMLNodeGetNodeByName(sip_root, "note", "dm");
	if (node) {
		notestr = (char*) xmlNodeGetContent(node);
		if (notestr) {
			LM_DBG("found <dm:note>%s</dm:node>\n", notestr);
			if (xmlStrcasecmp((unsigned char*)notestr, (unsigned char*) "Busy")==0)
				primitive = PURPLE_STATUS_UNAVAILABLE;
			else if (xmlStrcasecmp((unsigned char*)notestr, (unsigned char*) "On the Phone")==0) {
				primitive = PURPLE_STATUS_UNAVAILABLE;
				note.len = sprintf(note.s, "On the Phone");
			}
			else if (xmlStrcasecmp((unsigned char*)notestr, (unsigned char*) "Away")==0)
				primitive = PURPLE_STATUS_AWAY;
			else if (xmlStrcasecmp((unsigned char*)notestr, (unsigned char*) "Idle")==0) {
				primitive = PURPLE_STATUS_AVAILABLE;
				note.len = sprintf(note.s, "Idle");
			}
			else
				note.len = sprintf(note.s, "%s", notestr);
		}
	}

	node = NULL;
	node = XMLNodeGetNodeByName(sip_root, "user-input", "rpid");
	if (node) {
		notestr = (char*) xmlNodeGetContent(node);
		if (notestr) {
			LM_DBG("found <rpid:user-input>%s</rpid:user-input>\n", notestr);
			note.len = sprintf(note.s, "%s", notestr);
		}
	}	

	if (purple_send_publish_cmd(basic, primitive, &from_uri, &tagid, &note) < 0) {
		LM_ERR("error sending publish cmd via pipe\n");
		return -1;
	}
	LM_DBG("message parsed and sent to pipe successfully\n");
	return 1;
	
error:
	if(sip_doc)
		xmlFreeDoc(sip_doc);
	xmlCleanupParser();
	xmlMemoryDump();

	return -1;	
	
}

static int func_handle_subscribe(struct sip_msg* msg, char* uri, char* expires) {
	str from, to;
	int iexpires = -1;
	char ruri_buff[512], expires_buff[512];
	int ruri_len = 511, expires_len = 511;
	
	if (pv_printf(msg, (pv_elem_t*)uri, ruri_buff, &ruri_len) < 0) {
		LM_ERR("cannot print ruri into the format\n");
		return -1;
	}
	
	if (pv_printf(msg, (pv_elem_t*)expires, expires_buff, &expires_len) < 0) {
		LM_ERR("cannot print expires into the format\n");
		return -1;
	}

	iexpires = atoi(expires_buff);
	
	/* extract sender */
	LM_DBG("handling SUBSCRIBE %s (%s)\n", ruri_buff, expires_buff);
	if (parse_headers(msg, HDR_TO_F | HDR_FROM_F, 0) == -1 || !msg->to || !msg->from) {
		LM_ERR("no To/From headers\n");
		return -1;
	}
	if (parse_from_header(msg) < 0 || !msg->from->parsed) {
		LM_ERR("failed to parse From header\n");
		return -1;
	}
	from.len = 0;
	from = ((struct to_body *) msg->from->parsed)->uri;

	/* extract recipient */
	to.len = 0;
	/*
	if (msg->new_uri.len > 0) {
		LM_DBG("using new URI as destination\n");
		to = msg->new_uri;
	} else if (msg->first_line.u.request.uri.s 
			&& msg->first_line.u.request.uri.len > 0) {
		LM_DBG("using R-URI as destination\n");
		to = msg->first_line.u.request.uri;
	} else */
	if (msg->to->parsed) {
		LM_DBG("using TO-URI as destination\n");
		to = ((struct to_body *) msg->to->parsed)->uri;
	} else {
		LM_ERR("failed to find a valid destination\n");
		return -1;
	}
	
	if (purple_send_subscribe_cmd(&from, &to, iexpires) < 0) {
		LM_ERR("error sending subscribe cmd via pipe\n");
		return -1;
	}
	LM_DBG("subscribe parsed and sent to pipe successfully\n");
	return 1;
}

static int fixup_subscribe(void** param, int param_no) {
	pv_elem_t *model;
	str s;
	if(*param) {
		s.s = (char*)(*param); s.len = strlen(s.s);
		if(pv_parse_format(&s, &model)<0) {
			LM_ERR("wrong format[%s]\n",(char*)(*param));
			return E_UNSPEC;
		}
		*param = (void*)model;
		return 1;
	}
	LM_ERR("null format\n");
	return E_UNSPEC;
}

