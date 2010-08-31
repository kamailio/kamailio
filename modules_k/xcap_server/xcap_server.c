/*
 * $Id$
 *
 * xcap_server module - builtin XCAP server
 *
 * Copyright (C) 2010 Daniel-Constantin Mierla (asipto.com)
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * Kamailio is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * Kamailio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>

#include "../../lib/srdb1/db.h"
#include "../../pt.h"
#include "../../sr_module.h"
#include "../../dprint.h"
#include "../../error.h"
#include "../../ut.h"
#include "../../mem/mem.h"
#include "../../data_lump.h"
#include "../../data_lump_rpl.h"
#include "../../mod_fix.h"
#include "../../parser/parse_uri.h"
#include "../../modules_k/xcap_client/xcap_callbacks.h"
#include "../../modules/sl/sl.h"

#include "xcap_misc.h"

MODULE_VERSION

#define XCAP_TABLE_VERSION   3


static int xcaps_put_db(str* user, str *domain, xcap_uri_t *xuri, str *etag,
		str* doc);
static int xcaps_get_db(str* user, str *domain, xcap_uri_t *xuri,
		str *etag, str *doc);
static int xcaps_del_db(str* user, str *domain, xcap_uri_t *xuri);

static int w_xcaps_put(sip_msg_t* msg, char* puri, char* ppath,
		char* pbody);
static int w_xcaps_get(sip_msg_t* msg, char* puri, char* ppath);
static int w_xcaps_del(sip_msg_t* msg, char* puri, char* ppath);
static int fixup_xcaps_put(void** param, int param_no);

static int mod_init(void);
static int child_init(int rank);
static void destroy(void);

int xcaps_xpath_ns_param(modparam_t type, void *val);

int xcaps_path_get_auid_type(str *path);
int xcaps_generate_etag_hdr(str *etag);

static str xcaps_db_table = str_init("xcap");
static str xcaps_db_url = str_init(DEFAULT_DB_URL);
static str xcaps_root = str_init("/xcap-root/");
static int xcaps_init_time = 0;
static int xcaps_etag_counter = 1;

static str xcaps_buf = {0, 8192};
#define XCAPS_ETAG_SIZE	128
static char xcaps_etag_buf[XCAPS_ETAG_SIZE];

static str str_source_col = str_init("source");
static str str_doc_col = str_init("doc");
static str str_etag_col = str_init("etag");
static str str_username_col = str_init("username");
static str str_domain_col = str_init("domain");
static str str_doc_type_col = str_init("doc_type");
static str str_doc_uri_col = str_init("doc_uri");
static str str_port_col = str_init("port");


/* database connection */
db1_con_t *xcaps_db = NULL;
db_func_t xcaps_dbf;

/** SL API structure */
sl_api_t slb;

static param_export_t params[] = {
	{ "db_url",		STR_PARAM, &xcaps_db_url.s    },
	{ "xcap_table",	STR_PARAM, &xcaps_db_table.s  },
	{ "xcap_root",	STR_PARAM, &xcaps_root.s  },
	{ "buf_size",	INT_PARAM, &xcaps_buf.len  },
	{ "xml_ns",     STR_PARAM|USE_FUNC_PARAM, (void*)xcaps_xpath_ns_param },
	{ 0, 0, 0 }
};

static cmd_export_t cmds[]={
	{"xcaps_put",   (cmd_function)w_xcaps_put,   3,
			fixup_xcaps_put,  0, REQUEST_ROUTE},
	{"xcaps_get",   (cmd_function)w_xcaps_get,   2,
			fixup_xcaps_put,  0, REQUEST_ROUTE},
	{"xcaps_del",   (cmd_function)w_xcaps_del,   2,
			fixup_xcaps_put,  0, REQUEST_ROUTE},
	{0,0,0,0,0,0}
};


/** module exports */
struct module_exports exports= {
	"xcap_server",				/* module name */
	DEFAULT_DLFLAGS,			/* dlopen flags */
	cmds,  						/* exported functions */
	params,						/* exported parameters */
	0,      					/* exported statistics */
	0,							/* exported MI functions */
	0,							/* exported pseudo-variables */
	0,							/* extra processes */
	mod_init,					/* module initialization function */
	0,							/* response handling function */
	destroy,                    /* destroy function */
	child_init					/* per-child init function */
};

/**
 * init module function
 */
static int mod_init(void)
{

	xcaps_db_url.len   = (xcaps_db_url.s) ? strlen(xcaps_db_url.s) : 0;
	xcaps_db_table.len = (xcaps_db_table.s) ? strlen(xcaps_db_table.s) : 0;
	xcaps_root.len     = (xcaps_root.s) ? strlen(xcaps_root.s) : 0;
	
	if(xcaps_buf.len<=0)
	{
		LM_ERR("invalid buffer size\n");
		return -1;
	}

	xcaps_buf.s = (char*)pkg_malloc(xcaps_buf.len+1);
	if(xcaps_buf.s==NULL)
	{
		LM_ERR("no pkg\n");
		return -1;
	}

	/* binding to mysql module  */
	if (db_bind_mod(&xcaps_db_url, &xcaps_dbf))
	{
		LM_ERR("Database module not found\n");
		return -1;
	}
	
	if (!DB_CAPABILITY(xcaps_dbf, DB_CAP_ALL)) {
		LM_ERR("Database module does not implement all functions"
				" needed by the module\n");
		return -1;
	}

	xcaps_db = xcaps_dbf.init(&xcaps_db_url);
	if (xcaps_db==NULL)
	{
		LM_ERR("connecting to database\n");
		return -1;
	}

	if(db_check_table_version(&xcaps_dbf, xcaps_db, &xcaps_db_table,
				XCAP_TABLE_VERSION) < 0) {
		LM_ERR("error during table version check.\n");
		return -1;
	}
	xcaps_dbf.close(xcaps_db);
	xcaps_db = NULL;

	/* bind the SL API */
	if (sl_load_api(&slb)!=0) {
		LM_ERR("cannot bind to SL API\n");
		return -1;
	}

	xcaps_init_time = (int)time(NULL);
	return 0;
}

/**
 *
 */
static int child_init(int rank)
{
	if (rank==PROC_INIT || rank==PROC_MAIN || rank==PROC_TCP_MAIN)
		return 0; /* do nothing for the main process */

	if((xcaps_db = xcaps_dbf.init(&xcaps_db_url))==NULL)
	{
		LM_ERR("cannot connect to db\n");
		return -1;
	}
	return 0;
}

/**
 *
 */
static void destroy(void)
{
	if(xcaps_db != NULL)
		xcaps_dbf.close(xcaps_db);
}


/**
 *
 */
static int xcaps_send_reply(sip_msg_t *msg, int code, str *reason,
		str *hdrs, str *ctype, str *body)
{
	str tbuf;

	if(hdrs->len>0)
	{
		if (add_lump_rpl(msg, hdrs->s, hdrs->len, LUMP_RPL_HDR) == 0)
		{
			LM_ERR("failed to insert extra-headers lump\n");
			return -1;
		}
	}

	if(ctype->len>0)
	{
		/* add content-type */
		tbuf.len=sizeof("Content-Type: ") - 1 + ctype->len + CRLF_LEN;
		tbuf.s=pkg_malloc(sizeof(char)*(tbuf.len));

		if (tbuf.len==0)
		{
			LM_ERR("out of pkg memory\n");
			return -1;
		}
		memcpy(tbuf.s, "Content-Type: ", sizeof("Content-Type: ") - 1);
		memcpy(tbuf.s+sizeof("Content-Type: ") - 1, ctype->s, ctype->len);
		memcpy(tbuf.s+sizeof("Content-Type: ") - 1 + ctype->len,
				CRLF, CRLF_LEN);
		if (add_lump_rpl(msg, tbuf.s, tbuf.len, LUMP_RPL_HDR) == 0)
		{
			LM_ERR("failed to insert content-type lump\n");
			pkg_free(tbuf.s);
			return -1;
		}
		pkg_free(tbuf.s);
	}
	if(body->len>0)
	{
		if (add_lump_rpl(msg, body->s, body->len, LUMP_RPL_BODY) < 0)
		{
			LM_ERR("Error while adding reply lump\n");
			return -1;
		}
	}
	if (slb.freply(msg, code, reason) < 0)
	{
		LM_ERR("Error while sending reply\n");
		return -1;
	}
	return 0;
}

/**
 *
 */
int xcaps_xpath_hack(str *buf, int type)
{
	char *match;
	char *repl;
	char c;
	char *p;
	char *start;

	if(buf==NULL || buf->len <=10)
		return 0;

	if(type==0)
	{
		match = " xmlns=";
		repl  = " x____=";
	} else {
		match = " x____=";
		repl  = " xmlns=";
	}

	start = buf->s;
	c = buf->s[buf->len-1];
	buf->s[buf->len-1] = '\0';
	while((p = strstr(start, match))!=NULL)
	{
		memcpy(p, repl, 7);
		start = p + 7;
	}
	buf->s[buf->len-1] = c;
	return 0;
}

/**
 *
 */
static int xcaps_put_db(str* user, str *domain, xcap_uri_t *xuri, str *etag,
		str* doc)
{
	db_key_t qcols[9];
	db_val_t qvals[9];
	int ncols = 0;


	/* insert in xcap table*/
	qcols[ncols] = &str_username_col;
	qvals[ncols].type = DB1_STR;
	qvals[ncols].nul = 0;
	qvals[ncols].val.str_val = *user;
	ncols++;
	
	qcols[ncols] = &str_domain_col;
	qvals[ncols].type = DB1_STR;
	qvals[ncols].nul = 0;
	qvals[ncols].val.str_val = *domain;
	ncols++;
	
	qcols[ncols] = &str_doc_type_col;
	qvals[ncols].type = DB1_INT;
	qvals[ncols].nul = 0;
	qvals[ncols].val.int_val= xuri->type;
	ncols++;

	qcols[ncols] = &str_doc_col;
	qvals[ncols].type = DB1_STR;
	qvals[ncols].nul = 0;
	qvals[ncols].val.str_val= *doc;
	ncols++;

	qcols[ncols] = &str_etag_col;
	qvals[ncols].type = DB1_STR;
	qvals[ncols].nul = 0;
	qvals[ncols].val.str_val= *etag;
	ncols++;

	qcols[ncols] = &str_source_col;
	qvals[ncols].type = DB1_INT;
	qvals[ncols].nul = 0;
	qvals[ncols].val.int_val = 0;
	ncols++;

	qcols[ncols] = &str_doc_uri_col;
	qvals[ncols].type = DB1_STR;
	qvals[ncols].nul = 0;
	qvals[ncols].val.str_val= xuri->adoc;
	ncols++;

	qcols[ncols] = &str_port_col;
	qvals[ncols].type = DB1_INT;
	qvals[ncols].nul = 0;
	qvals[ncols].val.int_val= 0;
	ncols++;

	if (xcaps_dbf.use_table(xcaps_db, &xcaps_db_table) < 0) 
	{
		LM_ERR("in use_table-[table]= %.*s\n", xcaps_db_table.len,
				xcaps_db_table.s);
		goto error;
	}
	
	if(xcaps_dbf.insert(xcaps_db, qcols, qvals, ncols)< 0)
	{
		LM_ERR("in sql insert\n");
		goto error;
	}

	return 0;
error:
	return -1;
}

static str xcaps_str_empty      = {"", 0};
static str xcaps_str_ok         = {"OK", 2};
static str xcaps_str_srverr     = {"Server error", 12};
static str xcaps_str_nocontent  = {"No content", 10};
static str xcaps_str_appxml     = {"application/xml", 15};

/**
 *
 */
static int w_xcaps_put(sip_msg_t* msg, char* puri, char* ppath,
		char* pbody)
{
	struct sip_uri turi;
	str uri;
	str path;
	str body = {0, 0};
	str etag;
	str etag_hdr;
	str tbuf;
	str nbuf = {0, 0};
	pv_elem_t *xm;
	xcap_uri_t xuri;

	if(puri==0 || ppath==0 || pbody==0)
	{
		LM_ERR("invalid parameters\n");
		goto error;
	}

	if(fixup_get_svalue(msg, (gparam_p)puri, &uri)!=0)
	{
		LM_ERR("unable to get uri\n");
		goto error;
	}
	if(uri.s==NULL || uri.len == 0)
	{
		LM_ERR("invalid uri parameter\n");
		goto error;
	}

	if(fixup_get_svalue(msg, (gparam_p)ppath, &path)!=0)
	{
		LM_ERR("unable to get path\n");
		goto error;
	}
	if(path.s==NULL || path.len == 0)
	{
		LM_ERR("invalid path parameter\n");
		goto error;
	}

	xm = (pv_elem_t*)pbody;
	body.len = xcaps_buf.len - 1;
	body.s   = xcaps_buf.s;
	if(pv_printf(msg, xm, body.s, &body.len)<0)
	{
		LM_ERR("unable to get body\n");
		goto error;
	}
	if(body.s==NULL || body.len <= 0)
	{
		LM_ERR("invalid body parameter\n");
		goto error;
	}
	nbuf.s = (char*)pkg_malloc(body.len+1);
	if(nbuf.s==NULL)
	{
		LM_ERR("no more pkg\n");
		body.s = NULL;
		goto error;
	}

	memcpy(nbuf.s, body.s, body.len);
	body.s = nbuf.s;
	body.s[body.len] = '\0';
	nbuf.s = NULL;

	if(parse_uri(uri.s, uri.len, &turi)!=0)
	{
		LM_ERR("parsing uri parameter\n");
		goto error;
	}
	/* TODO: do xml parsing for validation */

	if(xcap_parse_uri(&path, &xcaps_root, &xuri)<0)
	{
		LM_ERR("cannot parse xcap uri [%.*s]\n",
				path.len, path.s);
		goto error;
	}
	if(xuri.nss==NULL || xuri.node.len<=0)
	{
		/* full document upload
		 *   - fetch and then delete is too expensive if record in db
		 *   - just try to delete
		 */
		if(xcaps_del_db(&turi.user, &turi.host, &xuri)<0)
		{
			LM_ERR("could not delete document\n");
			goto error;
		}
	} else {
		/* partial document upload
		 *   - fetch, update, delete and store
		 */
		if(xcaps_get_db(&turi.user, &turi.host, &xuri, &etag, &tbuf)<0)
		{
			LM_ERR("could not fetch xcap document\n");
			goto error;
		}
		if(xcaps_xpath_hack(&tbuf, 0)<0)
		{
			LM_ERR("could not hack xcap document\n");
			goto error;
		}
		if(xcaps_xpath_set(&tbuf, &xuri.node, &body, &nbuf)<0)
		{
			LM_ERR("could not update xcap document\n");
			goto error;
		}
		if(nbuf.len<=0)
		{
			LM_ERR("no new content\n");
			goto error;
		}
		pkg_free(body.s);
		body = nbuf;
		if(xcaps_xpath_hack(&body, 1)<0)
		{
			LM_ERR("could not hack xcap document\n");
			goto error;
		}
		if(xcaps_del_db(&turi.user, &turi.host, &xuri)<0)
		{
			LM_ERR("could not delete document\n");
			goto error;
		}
	}

	if(xcaps_generate_etag_hdr(&etag_hdr)<0)
	{
		LM_ERR("could not generate etag\n");
		goto error;
	}
	etag.s = etag_hdr.s + 10; /* 'SIP-ETag: ' */
	etag.len = etag_hdr.len - 12; /* 'SIP-ETag: '  '\r\n' */
	if(xcaps_put_db(&turi.user, &turi.host,
				&xuri, &etag, &body)<0)
	{
		LM_ERR("could not store document\n");
		goto error;
	}
	xcaps_send_reply(msg, 200, &xcaps_str_ok, &etag_hdr,
				&xcaps_str_empty, &xcaps_str_empty);
	if(body.s!=NULL)
		pkg_free(body.s);
	return 1;

error:
	xcaps_send_reply(msg, 500, &xcaps_str_srverr, &xcaps_str_empty,
				&xcaps_str_empty, &xcaps_str_empty);
	if(body.s!=NULL)
		pkg_free(body.s);
	return -1;
}

/**
 *
 */
static int xcaps_get_db(str* user, str *domain, xcap_uri_t *xuri,
		str *etag, str *doc)
{
	db_key_t qcols[4];
	db_val_t qvals[4];
	int ncols = 0;
	db_key_t rcols[4];
	int nrcols = 0;
	db1_res_t* db_res = NULL;
	str s;

	/* returned cols from xcap table*/
	rcols[nrcols] = &str_etag_col;
	nrcols++;
	rcols[nrcols] = &str_doc_col;
	nrcols++;

	/* query cols in xcap table*/
	qcols[ncols] = &str_username_col;
	qvals[ncols].type = DB1_STR;
	qvals[ncols].nul = 0;
	qvals[ncols].val.str_val = *user;
	ncols++;

	qcols[ncols] = &str_domain_col;
	qvals[ncols].type = DB1_STR;
	qvals[ncols].nul = 0;
	qvals[ncols].val.str_val = *domain;
	ncols++;

	qcols[ncols] = &str_doc_uri_col;
	qvals[ncols].type = DB1_STR;
	qvals[ncols].nul = 0;
	qvals[ncols].val.str_val= xuri->adoc;
	ncols++;

	if (xcaps_dbf.use_table(xcaps_db, &xcaps_db_table) < 0) 
	{
		LM_ERR("in use_table-[table]= %.*s\n", xcaps_db_table.len,
				xcaps_db_table.s);
		goto error;
	}

	if(xcaps_dbf.query(xcaps_db, qcols, NULL, qvals, rcols,
				ncols, nrcols, NULL, &db_res)< 0)
	{
		LM_ERR("in sql query\n");
		goto error;
	}
	if (RES_ROW_N(db_res) <= 0)
	{
		LM_DBG("no document\n");
		goto notfound;
	}
	/* etag */
	switch(RES_ROWS(db_res)[0].values[0].type)
	{
		case DB1_STRING:
			s.s=(char*)RES_ROWS(db_res)[0].values[0].val.string_val;
			s.len=strlen(s.s);
		break;
		case DB1_STR:
			s.len=RES_ROWS(db_res)[0].values[0].val.str_val.len;
			s.s=(char*)RES_ROWS(db_res)[0].values[0].val.str_val.s;
		break;
		case DB1_BLOB:
			s.len=RES_ROWS(db_res)[0].values[0].val.blob_val.len;
			s.s=(char*)RES_ROWS(db_res)[0].values[0].val.blob_val.s;
		break;
		default:
			s.len=0;
			s.s=NULL;
	}
	if(s.len==0)
	{
		LM_ERR("no etag in db record\n");
		goto error;
	}
	etag->len = snprintf(xcaps_etag_buf, XCAPS_ETAG_SIZE,
			"SIP-ETag: %.*s\r\n", s.len, s.s);
	if(etag->len < 0)
	{
		LM_ERR("error printing etag hdr\n ");
		goto error;
	}
	if(etag->len >= XCAPS_ETAG_SIZE)
	{
		LM_ERR("etag buffer overflow\n");
		goto error;
	}

	etag->s = xcaps_etag_buf;
	etag->s[etag->len] = '\0';

	/* doc */
	switch(RES_ROWS(db_res)[0].values[1].type)
	{
		case DB1_STRING:
			s.s=(char*)RES_ROWS(db_res)[0].values[1].val.string_val;
			s.len=strlen(s.s);
		break;
		case DB1_STR:
			s.len=RES_ROWS(db_res)[0].values[1].val.str_val.len;
			s.s=(char*)RES_ROWS(db_res)[0].values[1].val.str_val.s;
		break;
		case DB1_BLOB:
			s.len=RES_ROWS(db_res)[0].values[1].val.blob_val.len;
			s.s=(char*)RES_ROWS(db_res)[0].values[1].val.blob_val.s;
		break;
		default:
			s.len=0;
			s.s=NULL;
	}
	if(s.len==0)
	{
		LM_ERR("no xcap doc in db record\n");
		goto error;
	}
	if(s.len>xcaps_buf.len-1)
	{
		LM_ERR("xcap doc buffer overflow\n");
		goto error;
	}
	doc->len = s.len;
	doc->s = xcaps_buf.s;
	memcpy(doc->s, s.s, s.len);
	doc->s[doc->len] = '\0';

	xcaps_dbf.free_result(xcaps_db, db_res);
	return 0;

notfound:
	xcaps_dbf.free_result(xcaps_db, db_res);
	return 1;

error:
	if(db_res!=NULL)
		xcaps_dbf.free_result(xcaps_db, db_res);
	return -1;
}


/**
 *
 */
static int w_xcaps_get(sip_msg_t* msg, char* puri, char* ppath)
{
	struct sip_uri turi;
	str uri;
	str path;
	str etag;
	str body;
	int ret = 0;
	xcap_uri_t xuri;

	if(puri==0 || ppath==0)
	{
		LM_ERR("invalid parameters\n");
		return -1;
	}

	if(fixup_get_svalue(msg, (gparam_p)puri, &uri)!=0)
	{
		LM_ERR("unable to get uri\n");
		return -1;
	}
	if(uri.s==NULL || uri.len == 0)
	{
		LM_ERR("invalid uri parameter\n");
		return -1;
	}

	if(fixup_get_svalue(msg, (gparam_p)ppath, &path)!=0)
	{
		LM_ERR("unable to get path\n");
		return -1;
	}
	if(path.s==NULL || path.len == 0)
	{
		LM_ERR("invalid path parameter\n");
		return -1;
	}

	if(parse_uri(uri.s, uri.len, &turi)!=0)
	{
		LM_ERR("parsing uri parameter\n");
		goto error;
	}

	if(xcap_parse_uri(&path, &xcaps_root, &xuri)<0)
	{
		LM_ERR("cannot parse xcap uri [%.*s]\n",
				path.len, path.s);
		goto error;
	}

	if((ret=xcaps_get_db(&turi.user, &turi.host, &xuri, &etag, &body))<0)
	{
		LM_ERR("could not fetch xcap document\n");
		goto error;
	}
	if(ret==0)
	{
		/* doc found */
		xcaps_send_reply(msg, 200, &xcaps_str_ok, &etag,
				&xcaps_str_appxml, &body);
	} else {
		/* doc not found */
		xcaps_send_reply(msg, 204, &xcaps_str_nocontent, &xcaps_str_empty,
				&xcaps_str_empty, &xcaps_str_empty);
	}
	return 1;

error:
	xcaps_send_reply(msg, 500, &xcaps_str_srverr, &xcaps_str_empty,
				&xcaps_str_empty, &xcaps_str_empty);
	return -1;
}


/**
 *
 */
static int xcaps_del_db(str* user, str *domain, xcap_uri_t *xuri)
{
	db_key_t qcols[4];
	db_val_t qvals[4];
	int ncols = 0;

	/* delete in xcap table*/
	qcols[ncols] = &str_username_col;
	qvals[ncols].type = DB1_STR;
	qvals[ncols].nul = 0;
	qvals[ncols].val.str_val = *user;
	ncols++;
	
	qcols[ncols] = &str_domain_col;
	qvals[ncols].type = DB1_STR;
	qvals[ncols].nul = 0;
	qvals[ncols].val.str_val = *domain;
	ncols++;
	
	qcols[ncols] = &str_doc_uri_col;
	qvals[ncols].type = DB1_STR;
	qvals[ncols].nul = 0;
	qvals[ncols].val.str_val= xuri->adoc;
	ncols++;

	if (xcaps_dbf.use_table(xcaps_db, &xcaps_db_table) < 0) 
	{
		LM_ERR("in use_table-[table]= %.*s\n", xcaps_db_table.len,
				xcaps_db_table.s);
		goto error;
	}
	
	if(xcaps_dbf.delete(xcaps_db, qcols, NULL, qvals, ncols)< 0)
	{
		LM_ERR("in sql delete\n");
		goto error;
	}

	return 0;
error:
	return -1;
}


/**
 *
 */
static int w_xcaps_del(sip_msg_t* msg, char* puri, char* ppath)
{
	struct sip_uri turi;
	str uri;
	str path;
	xcap_uri_t xuri;
	str body;
	str etag_hdr;
	str etag;
	str tbuf;

	if(puri==0 || ppath==0)
	{
		LM_ERR("invalid parameters\n");
		return -1;
	}

	if(fixup_get_svalue(msg, (gparam_p)puri, &uri)!=0)
	{
		LM_ERR("unable to get uri\n");
		return -1;
	}
	if(uri.s==NULL || uri.len == 0)
	{
		LM_ERR("invalid uri parameter\n");
		return -1;
	}

	if(fixup_get_svalue(msg, (gparam_p)ppath, &path)!=0)
	{
		LM_ERR("unable to get path\n");
		return -1;
	}
	if(path.s==NULL || path.len == 0)
	{
		LM_ERR("invalid path parameter\n");
		return -1;
	}

	if(parse_uri(uri.s, uri.len, &turi)!=0)
	{
		LM_ERR("parsing uri parameter\n");
		goto error;
	}

	if(xcap_parse_uri(&path, &xcaps_root, &xuri)<0)
	{
		LM_ERR("cannot parse xcap uri [%.*s]\n",
				path.len, path.s);
		goto error;
	}


	if(xuri.nss==NULL)
	{
		/* delete document */
		if(xcaps_del_db(&turi.user, &turi.host, &xuri)<0)
		{
			LM_ERR("could not delete document\n");
			goto error;
		}
		xcaps_send_reply(msg, 200, &xcaps_str_ok, &xcaps_str_empty,
				&xcaps_str_empty, &xcaps_str_empty);
	} else {
		/* delete element */
		if(xcaps_get_db(&turi.user, &turi.host, &xuri, &etag, &tbuf)<0)
		{
			LM_ERR("could not fetch xcap document\n");
			goto error;
		}
		if(xcaps_xpath_hack(&tbuf, 0)<0)
		{
			LM_ERR("could not hack xcap document\n");
			goto error;
		}
		if(xcaps_xpath_set(&tbuf, &xuri.node, NULL, &body)<0)
		{
			LM_ERR("could not update xcap document\n");
			goto error;
		}
		if(body.len<=0)
		{
			LM_ERR("no new content\n");
			goto error;
		}
		if(xcaps_xpath_hack(&body, 1)<0)
		{
			LM_ERR("could not hack xcap document\n");
			goto error;
		}
		if(xcaps_del_db(&turi.user, &turi.host, &xuri)<0)
		{
			LM_ERR("could not delete document\n");
			goto error;
		}
		if(xcaps_generate_etag_hdr(&etag_hdr)<0)
		{
			LM_ERR("could not generate etag\n");
			goto error;
		}
		etag.s = etag_hdr.s + 10; /* 'SIP-ETag: ' */
		etag.len = etag_hdr.len - 12; /* 'SIP-ETag: '  '\r\n' */
		if(xcaps_put_db(&turi.user, &turi.host,
				&xuri, &etag, &body)<0)
		{
			LM_ERR("could not store document\n");
			goto error;
		}
		xcaps_send_reply(msg, 200, &xcaps_str_ok, &etag_hdr,
				&xcaps_str_empty, &xcaps_str_empty);
		if(body.s!=NULL)
			pkg_free(body.s);
		return 1;
	}
	return 1;

error:
	xcaps_send_reply(msg, 500, &xcaps_str_srverr, &xcaps_str_empty,
				&xcaps_str_empty, &xcaps_str_empty);
	if(body.s!=NULL)
		pkg_free(body.s);
	return -1;
}

/**
 *
 */
int xcaps_path_get_auid_type(str *path)
{
	str s;
	char c;
	int ret;

	ret = -1;
	if(path==NULL)
		return -1;
	if(path->len<xcaps_root.len)
		return -1;

	if(strncmp(path->s, xcaps_root.s, xcaps_root.len)!=0)
	{
		LM_ERR("missing xcap-root in [%.*s]\n", path->len, path->s);
		return -1;
	}

	s.s = path->s + xcaps_root.len - 1;
	s.len = path->len - xcaps_root.len + 1;

	c = s.s[s.len];
	s.s[s.len] = '\0';

	if(s.len>12
			&& strstr(s.s, "/pres-rules/")!=NULL)
	{
		LM_DBG("matched pres-rules\n");
		ret = PRES_RULES;
		goto done;
	}

	if(s.len>14
			&& strstr(s.s, "/rls-services/")!=NULL)
	{
		LM_DBG("matched rls-services\n");
		ret = RLS_SERVICE;
		goto done;
	}

	if(s.len>19
			&& strstr(s.s, "pidf-manipulation")!=NULL)
	{
		LM_DBG("matched pidf-manipulation\n");
		ret = PIDF_MANIPULATION;
		goto done;
	}

	if(s.len>16
			&& strstr(s.s, "/resource-lists/")!=NULL)
	{
		LM_DBG("matched resource-lists\n");
		ret = RESOURCE_LIST;
		goto done;
	}

done:
	s.s[s.len] = c;
	return ret;
}

/**
 *
 */
int xcaps_generate_etag_hdr(str *etag)
{
	etag->len = snprintf(xcaps_etag_buf, XCAPS_ETAG_SIZE,
			"SIP-ETag: sr-%d-%d-%d\r\n", xcaps_init_time, my_pid(),
			xcaps_etag_counter++);
	if(etag->len <0)
	{
		LM_ERR("error printing etag\n ");
		return -1;
	}
	if(etag->len >= XCAPS_ETAG_SIZE)
	{
		LM_ERR("etag buffer overflow\n");
		return -1;
	}

	etag->s = xcaps_etag_buf;
	etag->s[etag->len] = '\0';
	return 0;
}

/** 
 * 
 */
static int fixup_xcaps_put(void** param, int param_no)
{
	str s;
	pv_elem_t *xm;
	if (param_no == 1) {
	    return fixup_spve_null(param, 1);
	} else if (param_no == 2) {
	    return fixup_spve_null(param, 1);
	} else if (param_no == 3) {
		s.s = (char*)(*param); s.len = strlen(s.s);
		if(pv_parse_format(&s, &xm)<0)
		{
			LM_ERR("wrong format[%s]\n", (char*)(*param));
			return E_UNSPEC;
		}
		*param = (void*)xm;
	    return 0;
	}
	return 0;
}


