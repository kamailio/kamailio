/**
 * $Id$
 *
 * MSILO module
 *
 * Copyright (C) 2001-2003 FhG Fokus
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

/**
 * History
 * -------
 *
 * 2003-01-23: switched from t_uac to t_uac_dlg (dcm)
 * 2003-02-28: protocolization of t_uac_dlg completed (jiri)
 * 2003-03-11: updated to the new module interface (andrei)
 *             removed non-constant initializers to some strs (andrei)
 * 2003-03-16: flags parameter added (janakj)
 * 2003-04-05: default_uri #define used (jiri)
 * 2003-04-06: db_init removed from mod_init, will be called from child_init
 *             now (janakj)
 * 2003-04-07: m_dump takes a parameter which sets the way the outgoing URI
 *             is computed (dcm)
 * 2003-08-05 adapted to the new parse_content_type_hdr function (bogdan)
 * 2004-06-07 updated to the new DB api (andrei)
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>

#include "../../sr_module.h"
#include "../../dprint.h"
#include "../../ut.h"
#include "../../timer.h"
#include "../../mem/shm_mem.h"
#include "../../db/db.h"
#include "../../parser/parse_from.h"
#include "../../parser/parse_content.h"
#include "../../parser/contact/parse_contact.h"
#include "../../resolve.h"
#include "../../id.h"

#include "../tm/tm_load.h"

#define CONTACT_PREFIX "Content-Type: text/plain"CRLF"Contact: <"
#define CONTACT_SUFFIX  ">;msilo=yes"CRLF
#define CONTACT_PREFIX_LEN (sizeof(CONTACT_PREFIX)-1)
#define CONTACT_SUFFIX_LEN  (sizeof(CONTACT_SUFFIX)-1)
#define OFFLINE_MESSAGE	"] is offline. The message will be delivered when user goes online."
#define OFFLINE_MESSAGE_LEN	(sizeof(OFFLINE_MESSAGE)-1)

#include "ms_msg_list.h"
#include "msfuncs.h"

#define MAX_DEL_KEYS	1
#define NR_KEYS		9

char *sc_mid      = "mid";       /* 0 */
char *sc_from     = "from_hdr";  /* 1 */
char *sc_to       = "to_hdr";    /* 2 */
char *sc_ruri     = "ruri";      /* 3 */
char *sc_uid      = "uid";       /* 4 */
char *sc_body     = "body";      /* 5 */
char *sc_ctype    = "ctype";     /* 6 */
char *sc_exp_time = "exp_time";  /* 7 */
char *sc_inc_time = "inc_time";  /* 8 */

#define SET_STR_VAL(_str, _res, _r, _c)	\
	if (RES_ROWS(_res)[_r].values[_c].nul == 0) \
	{ \
		switch(RES_ROWS(_res)[_r].values[_c].type) \
		{ \
		case DB_STRING: \
			(_str).s=(char*)RES_ROWS(_res)[_r].values[_c].val.string_val; \
			(_str).len=strlen((_str).s); \
			break; \
		case DB_STR: \
			(_str).len=RES_ROWS(_res)[_r].values[_c].val.str_val.len; \
			(_str).s=(char*)RES_ROWS(_res)[_r].values[_c].val.str_val.s; \
			break; \
		case DB_BLOB: \
			(_str).len=RES_ROWS(_res)[_r].values[_c].val.blob_val.len; \
			(_str).s=(char*)RES_ROWS(_res)[_r].values[_c].val.blob_val.s; \
			break; \
		default: \
			(_str).len=0; \
			(_str).s=NULL; \
		} \
	}

MODULE_VERSION

#define S_TABLE_VERSION 4

/** database connection */
static db_con_t *db_con = NULL;
static db_func_t msilo_dbf;

/** precessed msg list - used for dumping the messages */
msg_list ml = NULL;

/** TM bind */
struct tm_binds tmb;

/** parameters */

char *ms_db_url=DEFAULT_DB_URL;
char *ms_db_table="silo";
char *ms_registrar=NULL; //"sip:registrar@iptel.org";
int  ms_expire_time=259200;
int  ms_check_time=30;
int  ms_clean_period=5;
int  ms_use_contact=1;

str msg_type = STR_STATIC_INIT("MESSAGE");

str reg_addr;

/** module functions */
static int mod_init(void);
static int child_init(int);

static int m_store(struct sip_msg*, char*, char*);
static int m_dump(struct sip_msg*, char*, char*);

void destroy(void);

void m_clean_silo(unsigned int ticks, void *);

/** TM callback function */
static void m_tm_callback( struct cell *t, int type, struct tmcb_params *ps);

static cmd_export_t cmds[]={
	{"m_store",  m_store, 2, 0, REQUEST_ROUTE | FAILURE_ROUTE},
	{"m_dump",   m_dump,  1, 0, REQUEST_ROUTE},
	{0,0,0,0,0}
};


static param_export_t params[]={
	{"db_url",       PARAM_STRING, &ms_db_url},
	{"db_table",     PARAM_STRING, &ms_db_table},
	{"registrar",    PARAM_STRING, &ms_registrar},
	{"expire_time",  PARAM_INT,    &ms_expire_time},
	{"check_time",   PARAM_INT,    &ms_check_time},
	{"clean_period", PARAM_INT,    &ms_clean_period},
	{"use_contact",  PARAM_INT,    &ms_use_contact},
	{"sc_mid",       PARAM_STRING, &sc_mid},
	{"sc_from",      PARAM_STRING, &sc_from},
	{"sc_to",        PARAM_STRING, &sc_to},
	{"sc_ruri",      PARAM_STRING, &sc_ruri},
	{"sc_uid",       PARAM_STRING, &sc_uid},
	{"sc_body",      PARAM_STRING, &sc_body},
	{"sc_ctype",     PARAM_STRING, &sc_ctype},
	{"sc_exp_time",  PARAM_STRING, &sc_exp_time},
	{"sc_inc_time",  PARAM_STRING, &sc_inc_time},
	{0,0,0}
};


/** module exports */
struct module_exports exports= {
	"msilo",    /* module id */
	cmds,       /* module's exported functions */
	0,          /* RPC methods */
	params,     /* module's exported parameters */

	mod_init,   /* module initialization function */
	(response_function) 0,       /* response handler */
	(destroy_function) destroy,  /* module destroy function */
	0,
	child_init  /* per-child init function */
};

/**
 * init module function
 */
static int mod_init(void)
{
	str _s;
	int ver = 0;
	load_tm_f  load_tm;

	DBG("MSILO: initializing ...\n");

	/* binding to mysql module  */
	if (bind_dbmod(ms_db_url, &msilo_dbf))
	{
		DBG("MSILO: ERROR: Database module not found\n");
		return -1;
	}

	if (!DB_CAPABILITY(msilo_dbf, DB_CAP_ALL)) {
		LOG(L_ERR, "MSILO: ERROR: Database module does not implement "
		    "all functions needed by the module\n");
		return -1;
	}

	db_con = msilo_dbf.init(ms_db_url);
	if (!db_con)
	{
		LOG(L_ERR,"MSILO:mod_init: Error while connecting database\n");
		return -1;
	}
	_s.s = ms_db_table;
	_s.len = strlen(ms_db_table);
	ver =  table_version(&msilo_dbf, db_con, &_s);
	if(ver!=S_TABLE_VERSION)
	{
		LOG(L_ERR,"MSILO:mod_init: Wrong version v%d for table <%s>,"
				" need v%d\n", ver, ms_db_table, S_TABLE_VERSION);
		return -1;
	}
	if(db_con)
		msilo_dbf.close(db_con);
	db_con = NULL;

	/* import the TM auto-loading function */
	if ( !(load_tm=(load_tm_f)find_export("load_tm", NO_SCRIPT, 0))) {
		LOG(L_ERR, "ERROR: msilo: mod_init: can't import load_tm\n");
		return -1;
	}
	/* let the auto-loading function load all TM stuff */
	if (load_tm( &tmb )==-1)
		return -1;

	ml = msg_list_init();
	if(!ml)
	{
		DBG("ERROR: msilo: mod_init: can't initialize msg list\n");
		return -1;
	}
	register_timer( m_clean_silo, 0, ms_check_time);

	reg_addr.s = ms_registrar;
	reg_addr.len = (ms_registrar)?strlen(ms_registrar):0;

	return 0;
}

/**
 * Initialize children
 */
static int child_init(int rank)
{

	if (rank==PROC_MAIN || rank==PROC_TCP_MAIN)
		return 0; /* do nothing for the main or tcp_main processes */
	DBG("MSILO: init_child #%d / pid <%d>\n", rank, getpid());
	if (msilo_dbf.init==0)
	{
		LOG(L_CRIT, "BUG: msilo: child_init: database not bound\n");
		return -1;
	}
	db_con = msilo_dbf.init(ms_db_url);
	if (!db_con)
	{
		LOG(L_ERR,"MSILO: child %d: Error while connecting database\n", rank);
		return -1;
	}
	else
	{
		if (msilo_dbf.use_table(db_con, ms_db_table) < 0) {
			LOG(L_ERR, "MSILO: child %d: Error in use_table\n", rank);
			return -1;
		}

		DBG("MSILO: child %d: Database connection opened successfully\n", rank);
	}
	return 0;
}


/**
 * store message
 * mode = "0" -- look for outgoing URI starting with new_uri
 * 		= "1" -- look for outgoing URI starting with r-uri
 * 		= "2" -- look for outgoing URI only at to header
 * next_hop = parameter specifying next hop for outgoing messages (like outbound proxy)
 */
static int m_store(struct sip_msg* msg, char* str1, char* str2)
{
	str body, str_hdr, ctaddr, uri, uid;
	struct to_body* to, *from;
	db_key_t db_keys[NR_KEYS-1];
	db_val_t db_vals[NR_KEYS-1];

	int nr_keys = 0, val, lexpire;
	t_content_type ctype;
	static char buf[512];
	static char buf1[1024];
	int mime, mode;
	str next_hop = STR_NULL;

	DBG("MSILO: m_store: ------------ start ------------\n");

	if (!str1) {
		LOG(L_ERR, "MSILO:m_store: Invalid parameter value\n");
		goto error;
	}
	mode = str1[0] - '0';
	if (str2) {
		next_hop.s = str2;
		next_hop.len = strlen(str2);
	}

	if (get_to_uid(&uid, msg) < 0) {
		LOG(L_ERR, "MSILO:m_store: Unable to find out identity of user\n");
	        goto error;
	}

	/* get message body - after that whole SIP MESSAGE is parsed */
	body.s = get_body( msg );
	if (body.s==0) {
		LOG(L_ERR,"MSILO:m_store: ERROR cannot extract body from msg\n");
		goto error;
	}

	/* content-length (if present) must be already parsed */
	if (!msg->content_length) {
		LOG(L_ERR,"MSILO:m_store: ERROR no Content-Length header found!\n");
		goto error;
	}
	body.len = get_content_length(msg);

	/* check if the body of message contains something */
	if(body.len <= 0) {
		DBG("MSILO:m_store: body of the message is empty!\n");
		goto error;
	}

	to = get_to(msg);
	if (!to) {
		LOG(L_ERR, "MSILO:m_store: Cannot get To header\n");
		goto error;
	}

	if (parse_from_header(msg) < 0) {
		LOG(L_ERR, "MSILO:m_store: Error while Parsing From header\n");
		goto error;
	}
	from = get_from(msg);
	if (!from) {
		LOG(L_ERR, "MSILO:m_store: Cannot find From header\n");
		goto error;
	}

	db_keys[nr_keys] = sc_to;
	db_vals[nr_keys].type = DB_STR;
	db_vals[nr_keys].nul = 0;
	db_vals[nr_keys].val.str_val = to->uri;
	nr_keys++;

	db_keys[nr_keys] = sc_from;
	db_vals[nr_keys].type = DB_STR;
	db_vals[nr_keys].nul = 0;
	db_vals[nr_keys].val.str_val = from->uri;
	nr_keys++;

	db_keys[nr_keys] = sc_ruri;
	db_vals[nr_keys].type = DB_STR;
	db_vals[nr_keys].nul = 0;

	switch(mode) {
	case 0:
		uri = *GET_RURI(msg);
		     /* new_ruri, orig_ruri*/
		break;
	case 1:
		     /* orig_ruri */
		uri = msg->first_line.u.request.uri;
		break;
	case 2:
	        uri = to->uri;
		break;
	default:
		LOG(L_ERR, "MSILO:m_store: Unrecognized parameter value: %s\n", str1);
		goto error;
	}
	db_vals[nr_keys].val.str_val = uri;
	nr_keys++;

	db_keys[nr_keys] = sc_uid;
	db_vals[nr_keys].type = DB_STR;
	db_vals[nr_keys].nul = 0;
	db_vals[nr_keys].val.str_val = uid;
	nr_keys++;

	/* add the message's body in SQL query */

	db_keys[nr_keys] = sc_body;
	db_vals[nr_keys].type = DB_BLOB;
	db_vals[nr_keys].nul = 0;
	db_vals[nr_keys].val.blob_val = body;
	nr_keys++;

	lexpire = ms_expire_time;
	/* add 'content-type' -- parse the content-type header */
	if ((mime=parse_content_type_hdr(msg))<1 )
	{
		LOG(L_ERR,"MSILO:m_store: ERROR cannot parse Content-Type header\n");
		goto error;
	}

	db_keys[nr_keys]      = sc_ctype;
	db_vals[nr_keys].type = DB_STR;
	db_vals[nr_keys].nul  = 0;
	db_vals[nr_keys].val.str_val.s   = "text/plain";
	db_vals[nr_keys].val.str_val.len = 10;

	/** check the content-type value */
	if( mime!=(TYPE_TEXT<<16)+SUBTYPE_PLAIN
		&& mime!=(TYPE_MESSAGE<<16)+SUBTYPE_CPIM )
	{
		if(m_extract_content_type(msg->content_type->body.s,
				msg->content_type->body.len, &ctype, CT_TYPE) != -1)
		{
			DBG("MSILO:m_store: 'content-type' found\n");
			db_vals[nr_keys].val.str_val   = ctype.type;
		}
	}
	nr_keys++;

	/* check 'expires' -- no more parsing - already done by get_body() */
	if(msg->expires && msg->expires->body.len > 0)
	{
		DBG("MSILO:m_store: 'expires' found\n");
		val = atoi(msg->expires->body.s);
		if(val > 0)
			lexpire = (ms_expire_time<=val)?ms_expire_time:val;
	}

	/* current time */
	val = (int)time(NULL);

	/* add expiration time */
	db_keys[nr_keys] = sc_exp_time;
	db_vals[nr_keys].type = DB_DATETIME;
	db_vals[nr_keys].nul = 0;
	db_vals[nr_keys].val.time_val = val+lexpire;
	nr_keys++;

	/* add incoming time */
	db_keys[nr_keys] = sc_inc_time;
	db_vals[nr_keys].type = DB_DATETIME;
	db_vals[nr_keys].nul = 0;
	db_vals[nr_keys].val.time_val = val;
	nr_keys++;

	if (msilo_dbf.use_table(db_con, ms_db_table) < 0)
	{
		LOG(L_ERR, "MSILO:m_store: Error in use_table\n");
		goto error;
	}

	if(msilo_dbf.insert(db_con, db_keys, db_vals, nr_keys) < 0)
	{
		LOG(L_ERR, "MSILO:m_store: error storing message\n");
		goto error;
	}
	DBG("MSILO:m_store: message stored. uid:<%.*s> F:<%.*s>\n",
		uid.len, uid.s, from->uri.len, ZSW(from->uri.s));

	if(reg_addr.len > 0
		&& reg_addr.len+CONTACT_PREFIX_LEN+CONTACT_SUFFIX_LEN+1<1024)
	{
		DBG("MSILO:m_store: sending info message.\n");
		strcpy(buf1, CONTACT_PREFIX);
		strncat(buf1,reg_addr.s,reg_addr.len);
		strncat(buf1, CONTACT_SUFFIX, CONTACT_SUFFIX_LEN);
		str_hdr.len = CONTACT_PREFIX_LEN+reg_addr.len+CONTACT_SUFFIX_LEN;
		str_hdr.s = buf1;

		strncpy(buf, "User [", 6);
		body.len = 6;
		if(uri.len+OFFLINE_MESSAGE_LEN+7/*6+1*/ < 512)
		{
			strncpy(buf+body.len, uri.s, uri.len);
			body.len += uri.len;
		}
		strncpy(buf+body.len, OFFLINE_MESSAGE, OFFLINE_MESSAGE_LEN);
		body.len += OFFLINE_MESSAGE_LEN;

		body.s = buf;

		/* look for Contact header -- must be parsed by now*/
		ctaddr.s = NULL;
		if(ms_use_contact && msg->contact!=NULL && msg->contact->body.s!=NULL
				&& msg->contact->body.len > 0)
		{
			DBG("MSILO:m_store: contact header found\n");
			if((msg->contact->parsed!=NULL
				&& ((contact_body_t*)(msg->contact->parsed))->contacts!=NULL)
				|| (parse_contact(msg->contact)==0
				&& msg->contact->parsed!=NULL
				&& ((contact_body_t*)(msg->contact->parsed))->contacts!=NULL))
			{
				DBG("MSILO:m_store: using contact header for info msg\n");
				ctaddr.s =
				((contact_body_t*)(msg->contact->parsed))->contacts->uri.s;
				ctaddr.len =
				((contact_body_t*)(msg->contact->parsed))->contacts->uri.len;

				if(!ctaddr.s || ctaddr.len < 6 || strncmp(ctaddr.s, "sip:", 4)
					|| ctaddr.s[4]==' ')
					ctaddr.s = NULL;
				else
					DBG("MSILO:m_store: feedback contact [%.*s]\n",
							ctaddr.len,ctaddr.s);
			}
		}

		tmb.t_request(&msg_type,  /* Type of the message */
				(ctaddr.s)?&ctaddr:&from->uri,    /* Request-URI */
				&from->uri,       /* To */
				&reg_addr,        /* From */
				&str_hdr,         /* Optional headers including CRLF */
				&body,            /* Message body */
				&next_hop,        /* next hop */
				NULL,             /* Callback function */
				NULL              /* Callback parameter */
			);
	}

	return 1;
error:
	return -1;
}

/**
 * dump message
 */
static int m_dump(struct sip_msg* msg, char* str1, char* str2)
{
	db_key_t db_keys[1];
	db_val_t db_vals[1];
	db_key_t db_cols[7];
	db_res_t* db_res = NULL;
	int i, db_no_cols = 7, db_no_keys = 1, mid, n;
	char hdr_buf[1024], body_buf[1024];

	str str_vals[5], hdr_str , body_str, uid;
	time_t rtime;
	str next_hop = STR_NULL;
	
	if (str1) {
		next_hop.s = str1;
		next_hop.len = strlen(str1);
	}

	/* init */
	db_keys[0]=sc_uid;
	db_cols[0]=sc_mid;

	db_cols[1]=sc_from;
	db_cols[2]=sc_to;
	db_cols[3]=sc_body;
	db_cols[4]=sc_ctype;
	db_cols[5]=sc_inc_time;
	db_cols[6]=sc_ruri;


	DBG("MSILO:m_dump: ------------ start ------------\n");

	if (get_to_uid(&uid, msg) < 0) {
		LOG(L_ERR, "MSILO:m_dump: Unable to retrieve identity of user\n");
		goto error;
	}

	hdr_str.s=hdr_buf;
	hdr_str.len=1024;
	body_str.s=body_buf;
	body_str.len=1024;

	/**
	 * check if has expires=0 (REGISTER)
	 */
	if(parse_headers(msg, HDR_EXPIRES_F, 0) >= 0)
	{
		/* check 'expires' > 0 */
		if(msg->expires && msg->expires->body.len > 0)
		{
			i = atoi(msg->expires->body.s);
			if(i <= 0)
			{ /* user goes offline */
				DBG("MSILO:m_dump: user <%.*s> goes offline - expires=%d\n",
						uid.len, uid.s, i);
				goto error;
			}
			else
				DBG("MSILO:m_dump: user <%.*s> online - expires=%d\n",
						uid.len, uid.s, i);
		}
	}
	else
	{
		DBG("MSILO:m_dump: 'expires' threw error at parsing\n");
		goto error;
	}

	db_vals[0].type = DB_STR;
	db_vals[0].nul = 0;
	db_vals[0].val.str_val = uid;

	if (msilo_dbf.use_table(db_con, ms_db_table) < 0)
	{
		LOG(L_ERR, "MSILO:m_dump: Error in use_table\n");
		goto error;
	}

	if((msilo_dbf.query(db_con,db_keys,NULL,db_vals,db_cols,db_no_keys,
				db_no_cols, NULL,&db_res)!=0) || (RES_ROW_N(db_res) <= 0))
	{
		DBG("MSILO:m_dump: no stored message for <%.*s>!\n", uid.len,
					ZSW(uid.s));
		goto done;
	}

	DBG("MSILO:m_dump: dumping [%d] messages for <%.*s>!!!\n",
			RES_ROW_N(db_res), uid.len, ZSW(uid.s));

	for(i = 0; i < RES_ROW_N(db_res); i++)
	{
		mid =  RES_ROWS(db_res)[i].values[0].val.int_val;
		if(msg_list_check_msg(ml, mid))
		{
			DBG("MSILO:m_dump: message[%d] mid=%d already sent.\n",
				i, mid);
			continue;
		}

		memset(str_vals, 0, 4*sizeof(str));
		SET_STR_VAL(str_vals[0], db_res, i, 1); /* from */
		SET_STR_VAL(str_vals[1], db_res, i, 2); /* to */
		SET_STR_VAL(str_vals[2], db_res, i, 3); /* body */
		SET_STR_VAL(str_vals[3], db_res, i, 4); /* ctype */
		SET_STR_VAL(str_vals[4], db_res, i, 6); /* Request-URI */

		hdr_str.len = 1024;
		if(m_build_headers(&hdr_str, str_vals[3] /*ctype*/,
				str_vals[0]/*from*/) < 0)
		{
			DBG("MSILO:m_dump: headers building failed!!!\n");
			if (msilo_dbf.free_result(db_con, db_res) < 0)
				DBG("MSILO:m_dump: Error while freeing result of"
					" query\n");
			msg_list_set_flag(ml, mid, MS_MSG_ERRO);
			goto error;
		}

		DBG("MSILO:m_dump: msg [%d-%d] for: %.*s\n", i+1, mid,
				uid.len, ZSW(uid.s));

		/** sending using TM function: t_uac */
		body_str.len = 1024;
		rtime =
			(time_t)RES_ROWS(db_res)[i].values[5/*inc time*/].val.time_val;
		n = m_build_body(&body_str, rtime, str_vals[2/*body*/]);
		if(n<0)
			DBG("MSILO:m_dump: sending simple body\n");
		else
			DBG("MSILO:m_dump: sending composed body\n");

			tmb.t_request(&msg_type,  /* Type of the message */
					&str_vals[4],     /* Request-URI */
					&str_vals[1],     /* To */
					&str_vals[0],     /* From */
					&hdr_str,         /* Optional headers including CRLF */
					(n<0)?&str_vals[2]:&body_str, /* Message body */
					&next_hop, /* next hop */
					m_tm_callback,    /* Callback function */
					(void*)(long)mid        /* Callback parameter */
				);
	}

done:
	/**
	 * Free the result because we don't need it
	 * anymore
	 */
	if (db_res!=NULL && msilo_dbf.free_result(db_con, db_res) < 0)
		DBG("MSILO:m_dump: Error while freeing result of query\n");

	return 1;
error:
	return -1;
}

/**
 * - cleaning up the messages that got reply
 * - delete expired messages from database
 */
void m_clean_silo(unsigned int ticks, void *param)
{
	msg_list_el mle = NULL, p;
	db_key_t db_keys[MAX_DEL_KEYS];
	db_val_t db_vals[MAX_DEL_KEYS];
	db_op_t  db_ops[1] = { OP_LEQ };
	int n;

	DBG("MSILO:clean_silo: cleaning stored messages - %d\n", ticks);

	msg_list_check(ml);
	mle = p = msg_list_reset(ml);
	n = 0;
	while(p)
	{
		if(p->flag & MS_MSG_DONE)
		{
			db_keys[n] = sc_mid;
			db_vals[n].type = DB_INT;
			db_vals[n].nul = 0;
			db_vals[n].val.int_val = p->msgid;
			DBG("MSILO:clean_silo: cleaning sent message [%d]\n", p->msgid);
			n++;
			if(n==MAX_DEL_KEYS)
			{
				if (msilo_dbf.delete(db_con, db_keys, NULL, db_vals, n) < 0)
					DBG("MSILO:clean_silo: error cleaning %d messages.\n",n);
				n = 0;
			}
		}
		p = p->next;
	}
	if(n>0)
	{
		if (msilo_dbf.delete(db_con, db_keys, NULL, db_vals, n) < 0)
			DBG("MSILO:clean_silo: error cleaning %d messages\n", n);
		n = 0;
	}

	msg_list_el_free_all(mle);

	/* cleaning expired messages */
	if(ticks%(ms_check_time*ms_clean_period)<ms_check_time)
	{
		DBG("MSILO:clean_silo: cleaning expired messages\n");
		db_keys[0] = sc_exp_time;
		db_vals[0].type = DB_DATETIME;
		db_vals[0].nul = 0;
		db_vals[0].val.time_val = (int)time(NULL);
		if (msilo_dbf.delete(db_con, db_keys, db_ops, db_vals, 1) < 0)
			DBG("MSILO:clean_silo: ERROR cleaning expired messages\n");
	}
}


/**
 * destroy function
 */
void destroy(void)
{
	DBG("MSILO: destroy module ...\n");
	msg_list_free(ml);

	if(db_con && msilo_dbf.close)
		msilo_dbf.close(db_con);
}

/**
 * TM callback function - delete message from database if was sent OK
 */
void m_tm_callback( struct cell *t, int type, struct tmcb_params *ps)
{
	int mid = -1;
	
	DBG("MSILO:m_tm_callback: completed with status %d\n", ps->code);
	if(!ps->param)
	{
		DBG("MSILO m_tm_callback: message id not received\n");
		goto done;
	}
	mid = (int)(long)(*ps->param);
	if(!db_con)
	{
		DBG("MSILO:m_tm_callback: db_con is NULL\n");
		goto done;
	}
	if(ps->code < 200 || ps->code >= 300)
	{
		
		DBG("MSILO:m_tm_callback: message <%d> was not sent successfully\n",
				mid);
		msg_list_set_flag(ml, mid, MS_MSG_ERRO);
		goto done;
	}

	msg_list_set_flag(ml, mid, MS_MSG_DONE);

done:
	return;
}
