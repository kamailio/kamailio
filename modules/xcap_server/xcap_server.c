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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
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
#include "../../modules/xcap_client/xcap_callbacks.h"
#include "../../modules/sl/sl.h"
#include "../../lib/kcore/cmpapi.h"
#include "../../ip_addr.h"

#include "xcap_misc.h"

MODULE_VERSION

#define XCAP_TABLE_VERSION   4


static int xcaps_put_db(str* user, str *domain, xcap_uri_t *xuri, str *etag,
		str* doc);
static int xcaps_get_db_doc(str* user, str *domain, xcap_uri_t *xuri,
		str *doc);
static int xcaps_get_db_etag(str* user, str *domain, xcap_uri_t *xuri,
		str *etag);
static int xcaps_del_db(str* user, str *domain, xcap_uri_t *xuri);

static int w_xcaps_put(sip_msg_t* msg, char* puri, char* ppath,
		char* pbody);
static int w_xcaps_get(sip_msg_t* msg, char* puri, char* ppath);
static int w_xcaps_del(sip_msg_t* msg, char* puri, char* ppath);
static int fixup_xcaps_put(void** param, int param_no);
static int check_preconditions(sip_msg_t *msg, str etag_hdr);
static int check_match_header(str body, str *etag);

static int mod_init(void);
static int child_init(int rank);
static void destroy(void);

int xcaps_xpath_ns_param(modparam_t type, void *val);

int xcaps_path_get_auid_type(str *path);
int xcaps_generate_etag_hdr(str *etag);

static str xcaps_db_table = str_init("xcap");
static str xcaps_db_url = str_init(DEFAULT_DB_URL);
static int xcaps_init_time = 0;
static int xcaps_etag_counter = 1;
str xcaps_root = str_init("/xcap-root/");
static int xcaps_directory_scheme = -1;
static str xcaps_directory_hostname = {0, 0};

static str xcaps_buf = {0, 8192};
#define XCAPS_HDR_SIZE	128
static char xcaps_hdr_buf[XCAPS_HDR_SIZE];

static str str_id_col = str_init("id");
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

static pv_export_t mod_pvs[] = {
	{ {"xcapuri", sizeof("xcapuri")-1}, PVT_OTHER, pv_get_xcap_uri,
		pv_set_xcap_uri, pv_parse_xcap_uri_name, 0, 0, 0 },
	{ {0, 0}, 0, 0, 0, 0, 0, 0, 0 }
};

static param_export_t params[] = {
	{ "db_url",             PARAM_STR, &xcaps_db_url    },
	{ "xcap_table",         PARAM_STR, &xcaps_db_table  },
	{ "xcap_root",          PARAM_STR, &xcaps_root  },
	{ "buf_size",           INT_PARAM, &xcaps_buf.len  },
	{ "xml_ns",             PARAM_STRING|USE_FUNC_PARAM, (void*)xcaps_xpath_ns_param },
	{ "directory_scheme",   INT_PARAM, &xcaps_directory_scheme },
	{ "directory_hostname", PARAM_STR, &xcaps_directory_hostname },
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
	mod_pvs,					/* exported pseudo-variables */
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

	if (xcaps_directory_scheme < -1 || xcaps_directory_scheme > 1)
	{
		LM_ERR("invalid xcaps_directory_scheme\n");
		return -1;
	}
	
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

	if(hdrs && hdrs->len>0)
	{
		if (add_lump_rpl(msg, hdrs->s, hdrs->len, LUMP_RPL_HDR) == 0)
		{
			LM_ERR("failed to insert extra-headers lump\n");
			return -1;
		}
	}

	if(ctype && ctype->len>0)
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
	if(body && body->len>0)
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
	db_key_t qcols[9], rcols[2], ucols[5];
	db_val_t qvals[9], uvals[5];
	db1_res_t *res = NULL;
	int ncols = 0, num_ucols = 0, nrows = 0;

	if(xcaps_check_doc_validity(doc)<0)
	{
		LM_ERR("invalid xml doc to insert in database\n");
		goto error;
	}

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

	qcols[ncols] = &str_doc_uri_col;
	qvals[ncols].type = DB1_STR;
	qvals[ncols].nul = 0;
	qvals[ncols].val.str_val= xuri->adoc;
	ncols++;

	rcols[0] = &str_id_col;

	if (xcaps_dbf.use_table(xcaps_db, &xcaps_db_table) < 0) 
	{
		LM_ERR("in use_table-[table]= %.*s\n", xcaps_db_table.len,
				xcaps_db_table.s);
		goto error;
	}

	if (xcaps_dbf.query(xcaps_db, qcols, 0, qvals, rcols, ncols, 1, 0, &res) < 0)
	{
		LM_ERR("in sql query\n");
		goto error;
	}

	nrows = RES_ROW_N(res);
	xcaps_dbf.free_result(xcaps_db, res);

	if (nrows == 0)
	{
		qcols[ncols] = &str_doc_col;
		qvals[ncols].type = DB1_BLOB;
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

		qcols[ncols] = &str_port_col;
		qvals[ncols].type = DB1_INT;
		qvals[ncols].nul = 0;
		qvals[ncols].val.int_val = 0;
		ncols++;

		if(xcaps_dbf.insert(xcaps_db, qcols, qvals, ncols)< 0)
		{
			LM_ERR("in sql insert\n");
			goto error;
		}
	}
	else if (nrows == 1)
	{
		ucols[num_ucols] = &str_doc_col;
		uvals[num_ucols].type = DB1_BLOB;
		uvals[num_ucols].nul = 0;
		uvals[num_ucols].val.str_val= *doc;
		num_ucols++;

		ucols[num_ucols] = &str_etag_col;
		uvals[num_ucols].type = DB1_STR;
		uvals[num_ucols].nul = 0;
		uvals[num_ucols].val.str_val= *etag;
		num_ucols++;

		ucols[num_ucols] = &str_source_col;
		uvals[num_ucols].type = DB1_INT;
		uvals[num_ucols].nul = 0;
		uvals[num_ucols].val.int_val = 0;
		num_ucols++;

		ucols[num_ucols] = &str_port_col;
		uvals[num_ucols].type = DB1_INT;
		uvals[num_ucols].nul = 0;
		uvals[num_ucols].val.int_val = 0;
		num_ucols++;

		if (xcaps_dbf.update(xcaps_db, qcols, 0, qvals, ucols, uvals, ncols, num_ucols) < 0)
		{
			LM_ERR("in sql update\n");
			goto error;
		}
	}
	else
	{
		LM_ERR("found %d copies of the same document in XCAP Server\n", nrows);
		goto error;
	}

	return 0;
error:
	return -1;
}

static str xcaps_str_ok		= {"OK", 2};
static str xcaps_str_srverr	= {"Server error", 12};
static str xcaps_str_notfound	= {"Not found", 9};
static str xcaps_str_precon	= {"Precondition Failed", 19};
static str xcaps_str_notmod	= {"Not Modified", 12};
static str xcaps_str_notallowed = {"Method Not Allowed", 18};
static str xcaps_str_notimplemented = {"Not Implemented", 15};
static str xcaps_str_appxml	= {"application/xml", 15};
static str xcaps_str_apprlxml	= {"application/resource-lists+xml", 30};
static str xcaps_str_apprsxml	= {"application/rls-services+xml", 28};
#if 0
static str xcaps_str_nocontent	= {"No content", 10};
static str xcaps_str_appxcxml	= {"application/xcap-caps+xml", 25};
static str xcaps_str_appsexml	= {"application/vnd.oma.search+xml", 30};
#endif
static str xcaps_str_appapxml	= {"application/auth-policy+xml", 27};
static str xcaps_str_appupxml	= {"application/vnd.oma.user-profile+xml", 36}; 
static str xcaps_str_apppcxml	= {"application/vnd.oma.pres-content+xml", 36};
static str xcaps_str_apppdxml	= {"application/pidf+xml", 20};
static str xcaps_str_appdrxml	= {"application/vnd.oma.xcap-directory+xml", 38};


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
	str etag = {0, 0};
	str etag_hdr = {0, 0};
	str tbuf;
	str nbuf = {0, 0};
	str allow = {0, 0};
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

	switch(xuri.type)
	{
	case DIRECTORY:
	case XCAP_CAPS:
		allow.s = xcaps_hdr_buf;
		allow.len = snprintf(allow.s, XCAPS_HDR_SIZE, "Allow: GET\r\n");
		xcaps_send_reply(msg, 405, &xcaps_str_notallowed, &allow, NULL, NULL);
		break;
	case SEARCH:
		allow.s = xcaps_hdr_buf;
		allow.len = snprintf(allow.s, XCAPS_HDR_SIZE, "Allow: POST\r\n");
		xcaps_send_reply(msg, 405, &xcaps_str_notallowed, &allow, NULL, NULL);
		break;
	default:
		xm = (pv_elem_t*)pbody;
		body.len = xcaps_buf.len - 1;
		if(pv_printf(msg, xm, xcaps_buf.s, &body.len)<0)
		{
			LM_ERR("unable to get body\n");
			goto error;
		}
		if(body.len <= 0)
		{
			LM_ERR("invalid body parameter\n");
			goto error;
		}
		body.s = (char*)pkg_malloc(body.len+1);
		if(body.s==NULL)
		{
			LM_ERR("no more pkg\n");
			goto error;
		}
		memcpy(body.s, xcaps_buf.s, body.len);
		body.s[body.len] = '\0';

		xcaps_get_db_etag(&turi.user, &turi.host, &xuri, &etag);
		if(check_preconditions(msg, etag)!=1)
		{
			xcaps_send_reply(msg, 412, &xcaps_str_precon, NULL, NULL, NULL);

			pkg_free(body.s);
			return -2;
		}

		if(xuri.nss!=NULL && xuri.node.len>0)
		{
			/* partial document upload
			 *   - fetch, update, delete and store
			 */
			if(xcaps_get_db_doc(&turi.user, &turi.host, &xuri, &tbuf) != 0)
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
		}

		if(xcaps_generate_etag_hdr(&etag_hdr)<0)
		{
			LM_ERR("could not generate etag\n");
			goto error;
		}
		etag.s = etag_hdr.s + 7; /* 'ETag: "' */
		etag.len = etag_hdr.len - 10; /* 'ETag: "  "\r\n' */
		if(xcaps_put_db(&turi.user, &turi.host,
					&xuri, &etag, &body)<0)
		{
			LM_ERR("could not store document\n");
			goto error;
		}
		xcaps_send_reply(msg, 200, &xcaps_str_ok, &etag_hdr,
					NULL, NULL);

		if(body.s!=NULL)
			pkg_free(body.s);

		break;
	}

	return 1;

error:
	xcaps_send_reply(msg, 500, &xcaps_str_srverr, NULL, NULL, NULL);
	if(body.s!=NULL)
		pkg_free(body.s);
	return -1;
}

/**
 *
 */
static int xcaps_get_db_doc(str* user, str *domain, xcap_uri_t *xuri, str *doc)
{
	db_key_t qcols[3];
	db_val_t qvals[3];
	int ncols = 0;
	db_key_t rcols[3];
	int nrcols = 0;
	db1_res_t* db_res = NULL;
	str s;

	/* returned cols from table xcap*/
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

	/* doc */
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

	if(xcaps_check_doc_validity(doc)<0)
	{
		LM_ERR("invalid xml doc retrieved from database\n");
		goto error;
	}

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
 * get the etag from database record for (user@domain, xuri)
 * - return: -1 error; 0 - found; 1 - not found
 *
 */
static int xcaps_get_db_etag(str* user, str *domain, xcap_uri_t *xuri, str *etag)
{
	db_key_t qcols[3];
	db_val_t qvals[3];
	int ncols = 0;
	db_key_t rcols[3];
	int nrcols = 0;
	db1_res_t* db_res = NULL;
	str s;

	/* returned cols from xcap table*/
	rcols[nrcols] = &str_etag_col;
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
	etag->len = snprintf(xcaps_hdr_buf, XCAPS_HDR_SIZE,
			"ETag: \"%.*s\"\r\n", s.len, s.s);
	if(etag->len < 0)
	{
		LM_ERR("error printing etag hdr\n ");
		goto error;
	}
	if(etag->len >= XCAPS_HDR_SIZE)
	{
		LM_ERR("etag buffer overflow\n");
		goto error;
	}

	etag->s = xcaps_hdr_buf;
	etag->s[etag->len] = '\0';

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

static int xcaps_get_directory(struct sip_msg *msg, str *user, str *domain, str *directory)
{
	db_key_t qcols[2];
	db_val_t qvals[2], *values;
	db_key_t rcols[3];
	db_row_t *rows;
	db1_res_t* db_res = NULL;
	int n_qcols = 0, n_rcols = 0;
	int i, cur_type = 0, cur_pos = 0;
	int doc_type_col, doc_uri_col, etag_col;
	str auid_string = {0, 0};
	struct hdr_field *hdr = msg->headers;
	str server_name = {0, 0};

	qcols[n_qcols] = &str_username_col;
	qvals[n_qcols].type = DB1_STR;
	qvals[n_qcols].nul = 0;
	qvals[n_qcols].val.str_val = *user;
	n_qcols++;
	
	qcols[n_qcols] = &str_domain_col;
	qvals[n_qcols].type = DB1_STR;
	qvals[n_qcols].nul = 0;
	qvals[n_qcols].val.str_val = *domain;
	n_qcols++;

	rcols[doc_type_col = n_rcols++] = &str_doc_type_col;
	rcols[doc_uri_col = n_rcols++] = &str_doc_uri_col;
	rcols[etag_col = n_rcols++] = &str_etag_col;

	if (xcaps_dbf.use_table(xcaps_db, &xcaps_db_table) < 0) 
	{
		LM_ERR("in use_table-[table]= %.*s\n", xcaps_db_table.len,
				xcaps_db_table.s);
		goto error;
	}

	if (xcaps_dbf.query(xcaps_db, qcols, 0, qvals, rcols, n_qcols,
				n_rcols, &str_doc_type_col, &db_res) < 0)
	{
		LM_ERR("in sql query\n");
		goto error;

	}

	if (db_res == NULL)
		goto error;

	directory->s = xcaps_buf.s;
	directory->len = 0;

	directory->len += snprintf(directory->s + directory->len,
					xcaps_buf.len - directory->len,
			"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\r\n"
			"<xcap-directory xmlns=\"urn:oma:xml:xdm:xcap-directory\">\r\n");

	rows = RES_ROWS(db_res);
	for (i = 0; i < RES_ROW_N(db_res); i++)
	{
		values = ROW_VALUES(&rows[i]);

		if (cur_type != VAL_INT(&values[doc_type_col]))
		{
			if (cur_type != 0)
			{
				directory->len += snprintf(directory->s + directory->len,
								xcaps_buf.len - directory->len,
			"</folder>\r\n");
			}
			cur_type = VAL_INT(&values[doc_type_col]);

			memset(&auid_string, 0, sizeof(str));
			while(xcaps_auid_list[cur_pos].auid.s != NULL)
			{
				if (xcaps_auid_list[cur_pos].type == cur_type)
				{
					auid_string.s = xcaps_auid_list[cur_pos].auid.s;
					auid_string.len = xcaps_auid_list[cur_pos].auid.len;
					break;
				}
				cur_pos++;
			}

			if (auid_string.s == NULL)
			{
				goto error;
			}

			directory->len += snprintf(directory->s + directory->len,
							xcaps_buf.len - directory->len,
			"<folder auid=\"%.*s\">\r\n",
							auid_string.len, auid_string.s);
		}

		switch(xcaps_directory_scheme)
		{
		case -1:
			directory->len += snprintf(directory->s + directory->len,
							xcaps_buf.len - directory->len,
			"<entry uri=\"%s://", msg->rcv.proto == PROTO_TLS ? "https" : "http");
			break;
		case 0:
			directory->len += snprintf(directory->s + directory->len,
							xcaps_buf.len - directory->len,
			"<entry uri=\"http://");
			break;
		case 1:
			directory->len += snprintf(directory->s + directory->len,
							xcaps_buf.len - directory->len,
			"<entry uri=\"https://");
			break;
		}

		if (xcaps_directory_hostname.len > 0)
		{
			directory->len += snprintf(directory->s + directory->len,
						xcaps_buf.len - directory->len,
			"%.*s", xcaps_directory_hostname.len, xcaps_directory_hostname.s);
		}
		else
		{
			if (parse_headers(msg, HDR_EOH_F, 0) < 0)
			{
				LM_ERR("error parsing headers\n");
				goto error;
			}

			while (hdr != NULL)
			{
				if (cmp_hdrname_strzn(&hdr->name, "Host", 4) == 0)
				{
					server_name = hdr->body;
					break;
				}
				hdr = hdr->next;
			}

			if (server_name.len > 0)
			{
				directory->len += snprintf(directory->s + directory->len,
							xcaps_buf.len - directory->len,
			"%.*s", server_name.len, server_name.s);
			}
			else
			{
				server_name.s = pkg_malloc(IP6_MAX_STR_SIZE + 6);
				server_name.len = ip_addr2sbuf(&msg->rcv.dst_ip, server_name.s, IP6_MAX_STR_SIZE);
				directory->len += snprintf(directory->s + directory->len,
							xcaps_buf.len - directory->len,
			"%.*s:%d", server_name.len, server_name.s, msg->rcv.dst_port);
				pkg_free(server_name.s);
			}
		}

		directory->len += snprintf(directory->s + directory->len,
						xcaps_buf.len - directory->len,
		"%s\" etag=\"%s\"/>\r\n",
					VAL_STRING(&values[doc_uri_col]),
					VAL_STRING(&values[etag_col]));
	}

	if (cur_type != 0)
	{
		directory->len += snprintf(directory->s + directory->len, xcaps_buf.len - directory->len,
		"</folder>\r\n");
	}
	directory->len += snprintf(directory->s + directory->len, xcaps_buf.len - directory->len,
		"</xcap-directory>");

	if (db_res != NULL)
		xcaps_dbf.free_result(xcaps_db, db_res);

	return 0;

error:
	if (db_res != NULL)
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
	str etag = {0, 0};
	str body = {0, 0};
	str new_body = {0, 0};
	int ret = 0;
	xcap_uri_t xuri;
	str *ctype;
	str allow;

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

	switch(xuri.type)
	{
	case DIRECTORY:
		if (strncmp(xuri.file.s, "directory.xml", xuri.file.len) == 0)
		{
			if (xcaps_get_directory(msg, &turi.user, &turi.host, &body) < 0)
				goto error;

			xcaps_send_reply(msg, 200, &xcaps_str_ok, NULL, &xcaps_str_appdrxml, &body);
		}
		else
		{
			xcaps_send_reply(msg, 404, &xcaps_str_notfound, NULL, NULL, NULL);
		}
		break;
	case XCAP_CAPS:
		xcaps_send_reply(msg, 501, &xcaps_str_notimplemented, NULL, NULL, NULL);
		break;
	case SEARCH:
		allow.s = xcaps_hdr_buf;
		allow.len = snprintf(allow.s, XCAPS_HDR_SIZE, "Allow: POST\r\n");
		xcaps_send_reply(msg, 405, &xcaps_str_notallowed, &allow, NULL, NULL);
		break;
	default:
		if((ret=xcaps_get_db_etag(&turi.user, &turi.host, &xuri, &etag))<0)
		{ 
			LM_ERR("could not fetch etag for xcap document\n");
			goto error;
		}
		if (ret==1)
		{
			/* doc not found */
			xcaps_send_reply(msg, 404, &xcaps_str_notfound, NULL,
					NULL, NULL);
			return 1;
		}
	
		if((ret=check_preconditions(msg, etag))==-1)
		{
			xcaps_send_reply(msg, 412, &xcaps_str_precon, NULL,
					NULL, NULL);
			return -2;
		} else if (ret==-2) {
			xcaps_send_reply(msg, 304, &xcaps_str_notmod, NULL,
					NULL, NULL);
			return -2;
		}

		if((ret=xcaps_get_db_doc(&turi.user, &turi.host, &xuri, &body))<0)
		{
			LM_ERR("could not fetch xcap document\n");
			goto error;
		}
		if(ret!=0)
		{
			/* doc not found */
			xcaps_send_reply(msg, 404, &xcaps_str_notfound, NULL,
					NULL, NULL);
			break;
		}

		if(xuri.nss!=NULL && xuri.node.len>0)
		{
			if((new_body.s = pkg_malloc(body.len))==NULL)
			{
				LM_ERR("allocating package memory\n");
				goto error;
			}
			new_body.len = body.len;
			
			if(xcaps_xpath_hack(&body, 0)<0)
			{
				LM_ERR("could not hack xcap document\n");
				goto error;
			}
			if(xcaps_xpath_get(&body, &xuri.node, &new_body)<0)
			{
				LM_ERR("could not retrieve element from xcap document\n");
				goto error;
			}
			if(new_body.len<=0)
			{
				/* element not found */
				xcaps_send_reply(msg, 404, &xcaps_str_notfound, NULL,
					NULL, NULL);
				pkg_free(new_body.s);
				new_body.s = NULL;
				break;
			}
			if(xcaps_xpath_hack(&new_body, 1)<0)
			{
				LM_ERR("could not hack xcap document\n");
				goto error;
			}
			memcpy(body.s, new_body.s, new_body.len);
			body.len = new_body.len;
			pkg_free(new_body.s);
			new_body.s = NULL;
		}

		/* doc or element found */
		ctype = &xcaps_str_appxml;
		if(xuri.type==RESOURCE_LIST)
			ctype = &xcaps_str_apprlxml;
		else if(xuri.type==PRES_RULES)
			ctype = &xcaps_str_appapxml;
		else if(xuri.type==RLS_SERVICE)
			ctype = &xcaps_str_apprsxml;
		else if(xuri.type==USER_PROFILE)
			ctype = &xcaps_str_appupxml;
		else if(xuri.type==PRES_CONTENT)
			ctype = &xcaps_str_apppcxml;
		else if(xuri.type==PIDF_MANIPULATION)
			ctype = &xcaps_str_apppdxml;
		xcaps_send_reply(msg, 200, &xcaps_str_ok, &etag,
				ctype, &body);

		break;
	}

	return 1;

error:
	if (new_body.s) pkg_free(new_body.s);
	xcaps_send_reply(msg, 500, &xcaps_str_srverr, NULL,
				NULL, NULL);
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
	str body = {0, 0};
	str etag_hdr = {0, 0};
	str etag = {0, 0};
	str tbuf;
	str allow = {0, 0};

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

	switch(xuri.type)
	{
	case DIRECTORY:
	case XCAP_CAPS:
		allow.s = xcaps_hdr_buf;
		allow.len = snprintf(allow.s, XCAPS_HDR_SIZE, "Allow: GET\r\n");
		xcaps_send_reply(msg, 405, &xcaps_str_notallowed, &allow, NULL, NULL);
		break;
	case SEARCH:
		allow.s = xcaps_hdr_buf;
		allow.len = snprintf(allow.s, XCAPS_HDR_SIZE, "Allow: POST\r\n");
		xcaps_send_reply(msg, 405, &xcaps_str_notallowed, &allow, NULL, NULL);
		break;
	default:
		if(xcaps_get_db_etag(&turi.user, &turi.host, &xuri, &etag)!=0)
		{ 
			LM_ERR("could not fetch etag for xcap document\n");
			goto error;
		}

		if(check_preconditions(msg, etag)!=1)
		{
			xcaps_send_reply(msg, 412, &xcaps_str_precon, NULL,
					NULL, NULL);
			return -2;
		}

		if(xuri.nss==NULL)
		{
			/* delete document */
			if(xcaps_del_db(&turi.user, &turi.host, &xuri)<0)
			{
				LM_ERR("could not delete document\n");
				goto error;
			}
			xcaps_send_reply(msg, 200, &xcaps_str_ok, NULL,
					NULL, NULL);
		} else {
			/* delete element */
			if(xcaps_get_db_doc(&turi.user, &turi.host, &xuri, &tbuf) != 0)
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
			if(xcaps_generate_etag_hdr(&etag_hdr)<0)
			{
				LM_ERR("could not generate etag\n");
				goto error;
			}
			etag.s = etag_hdr.s + 7; /* 'ETag: "' */
			etag.len = etag_hdr.len - 10; /* 'ETag: "  "\r\n' */
			if(xcaps_put_db(&turi.user, &turi.host,
					&xuri, &etag, &body)<0)
			{
				LM_ERR("could not store document\n");
				goto error;
			}
			xcaps_send_reply(msg, 200, &xcaps_str_ok, &etag_hdr,
					NULL, NULL);
			if(body.s!=NULL)
				pkg_free(body.s);
		}

		break;
	}

	return 1;

error:
	xcaps_send_reply(msg, 500, &xcaps_str_srverr, NULL,
				NULL, NULL);
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

	if(s.len>12 && strstr(s.s, "/pres-rules/")!=NULL)
	{
		LM_DBG("matched pres-rules\n");
		ret = PRES_RULES;
		goto done;
	}

	if(s.len>35 && strstr(s.s, "/org.openmobilealliance.pres-rules/")!=NULL)
	{
		LM_DBG("matched oma pres-rules\n");
		ret = PRES_RULES;
		goto done;
	}

	if(s.len>14 && strstr(s.s, "/rls-services/")!=NULL)
	{
		LM_DBG("matched rls-services\n");
		ret = RLS_SERVICE;
		goto done;
	}

	if(s.len>19 && strstr(s.s, "pidf-manipulation")!=NULL)
	{
		LM_DBG("matched pidf-manipulation\n");
		ret = PIDF_MANIPULATION;
		goto done;
	}

	if(s.len>16 && strstr(s.s, "/resource-lists/")!=NULL)
	{
		LM_DBG("matched resource-lists\n");
		ret = RESOURCE_LIST;
		goto done;
	}

        if(s.len>11 && strstr(s.s, "/xcap-caps/")!=NULL)
	{
                LM_DBG("matched xcap-caps\n");
                ret = XCAP_CAPS;
		goto done;
	}

        if(s.len> 37 && strstr(s.s, "/org.openmobilealliance.user-profile/")!=NULL)
	{
                LM_DBG("matched oma user-profile\n");
                ret = USER_PROFILE;
		goto done;
	}

        if(s.len> 37 && strstr(s.s, "/org.openmobilealliance.pres-content/")!=NULL)
	{
                LM_DBG("matched oma pres-content\n");
                ret = PRES_CONTENT;
		goto done;
	}

	if(s.len>31 && strstr(s.s, "/org.openmobilealliance.search?")!=NULL)
	{
                LM_DBG("matched oma search\n");
                ret = SEARCH;
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
	etag->len = snprintf(xcaps_hdr_buf, XCAPS_HDR_SIZE,
			"ETag: \"sr-%d-%d-%d\"\r\n", xcaps_init_time, my_pid(),
			xcaps_etag_counter++);
	if(etag->len <0)
	{
		LM_ERR("error printing etag\n ");
		return -1;
	}
	if(etag->len >= XCAPS_HDR_SIZE)
	{
		LM_ERR("etag buffer overflow\n");
		return -1;
	}

	etag->s = xcaps_hdr_buf;
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

static int check_preconditions(sip_msg_t *msg, str etag_hdr)
{
	struct hdr_field* hdr = msg->headers;
	int ifmatch_found=0;
	int matched_matched=0;
	int matched_nonematched=0;

	if (parse_headers(msg, HDR_EOH_F, 0) < 0)
	{
		LM_ERR("error parsing headers\n");
		return 1;
	}

	if (etag_hdr.len > 0)
	{
		str etag;

		/* Keep the surrounding "s in the ETag */
		etag.s = etag_hdr.s + 6; /* 'ETag: ' */
		etag.len = etag_hdr.len - 8; /* 'ETag: "  "\r\n' */

		while (hdr!=NULL)
		{
			if(cmp_hdrname_strzn(&hdr->name, "If-Match", 8)==0)
			{
				ifmatch_found = 1;
				if (check_match_header(hdr->body, &etag)>0)
					matched_matched = 1;
			}
			else if (cmp_hdrname_strzn(&hdr->name, "If-None-Match", 13)==0)
			{
				if (check_match_header(hdr->body, &etag)>0)
					matched_nonematched = 1;
			}
			hdr = hdr->next;
		}
	} else {
		while (hdr!=NULL)
		{
			if(cmp_hdrname_strzn(&hdr->name, "If-Match", 8)==0)
				ifmatch_found = 1;

			hdr = hdr->next;
		}
	}

	if (ifmatch_found == 1 && matched_matched == 0)
		return -1;
	else if (matched_nonematched == 1)
		return -2;
	else
		return 1;
}

static int check_match_header(str body, str *etag)
{
	if (etag == NULL)
		return -1;

	if (etag->s == NULL || etag->len == 0)
		return -1;

	do
	{
		char *start_pos, *end_pos, *old_body_pos;
		int cur_etag_len;

		if ((start_pos = strchr(body.s, '"')) == NULL)
			return -1;
		if ((end_pos = strchr(start_pos + 1, '"')) == NULL)
			return -1;
		cur_etag_len = end_pos - start_pos + 1;
	
		if (strncmp(start_pos, etag->s, cur_etag_len)==0)
			return 1;
		else if (strncmp(start_pos, "\"*\"", cur_etag_len)==0)
			return 1;

		old_body_pos = body.s;
		if ((body.s = strchr(end_pos, ',')) == NULL)
			return -1;
		body.len -= body.s - old_body_pos;
	} while (body.len > 0);

	return -1;
}
