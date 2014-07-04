/*
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

/*
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
#include "../../lib/srdb2/db.h"
#include "../../parser/parse_from.h"
#include "../../parser/parse_content.h"
#include "../../parser/contact/parse_contact.h"
#include "../../resolve.h"
#include "../../id.h"

#include "../../modules/tm/tm_load.h"

#define CONTACT_PREFIX "Content-Type: text/plain"CRLF"Contact: <"
#define CONTACT_SUFFIX  ">;msilo=yes"CRLF
#define CONTACT_PREFIX_LEN (sizeof(CONTACT_PREFIX)-1)
#define CONTACT_SUFFIX_LEN  (sizeof(CONTACT_SUFFIX)-1)
#define OFFLINE_MESSAGE	"] is offline. The message will be delivered when user goes online."
#define OFFLINE_MESSAGE_LEN	(sizeof(OFFLINE_MESSAGE)-1)

#include "ms_msg_list.h"
#include "msfuncs.h"

char *sc_mid      = "mid";       /* 0 */
char *sc_from     = "from_hdr";  /* 1 */
char *sc_to       = "to_hdr";    /* 2 */
char *sc_ruri     = "ruri";      /* 3 */
char *sc_uid      = "uid";       /* 4 */
char *sc_body     = "body";      /* 5 */
char *sc_ctype    = "ctype";     /* 6 */
char *sc_exp_time = "exp_time";  /* 7 */
char *sc_inc_time = "inc_time";  /* 8 */

MODULE_VERSION


/** database layer variables */
static db_ctx_t* ctx = NULL;
static db_cmd_t* store = NULL;
static db_cmd_t* load = NULL;
static db_cmd_t* del_mid = NULL;
static db_cmd_t* del_expired = NULL;

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

static void destroy(void);

void m_clean_silo(unsigned int ticks, void *);

/** TM callback function */
static void m_tm_callback( struct cell *t, int type, struct tmcb_params *ps);

static cmd_export_t cmds[]={
	{"m_store",  m_store, 2, 0, REQUEST_ROUTE | FAILURE_ROUTE},
	{"m_store",  m_store, 1, 0, REQUEST_ROUTE | FAILURE_ROUTE},
	{"m_dump",   m_dump,  1, 0, REQUEST_ROUTE},
	{"m_dump",   m_dump,  0, 0, REQUEST_ROUTE},
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
	load_tm_f  load_tm;

	DBG("MSILO: initializing ...\n");

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


void msilo_db_close(void)
{
	if (store) db_cmd_free(store);
	store = NULL;

	if (load) db_cmd_free(load);
	load = NULL;

	if (del_mid) db_cmd_free(del_mid);
	del_mid = NULL;

	if (del_expired) db_cmd_free(del_expired);
	del_expired = NULL;

	if (ctx) {
		db_disconnect(ctx);
		db_ctx_free(ctx);
		ctx = NULL;
	}
}


int msilo_db_init(char* db_url)
{
	db_fld_t del_mid_param[] = {
		{.name = sc_mid, .type = DB_INT},
		{.name = 0}
	};

	db_fld_t del_expired_param[] = {
		{.name = sc_exp_time, .type = DB_DATETIME, .op = DB_LEQ},
		{.name = 0}
	};

	db_fld_t store_param[] = {
		{.name = sc_to,       .type = DB_STR     },
		{.name = sc_from,     .type = DB_STR     },
		{.name = sc_ruri,     .type = DB_STR     },
		{.name = sc_uid,      .type = DB_STR     },
		{.name = sc_body,     .type = DB_BLOB    },
		{.name = sc_ctype,    .type = DB_STR     },
		{.name = sc_exp_time, .type = DB_DATETIME},
		{.name = sc_inc_time, .type = DB_DATETIME},
		{.name = 0}
	};

	db_fld_t load_match[] = {
		{.name = sc_uid, .type = DB_STR},
		{.name = 0}
	};

	db_fld_t load_cols[] = {
		{.name = sc_mid,      .type = DB_INT},
		{.name = sc_from,     .type = DB_STR},
		{.name = sc_to,       .type = DB_STR},
		{.name = sc_body,     .type = DB_BLOB},
		{.name = sc_ctype,    .type = DB_STR},
		{.name = sc_inc_time, .type = DB_DATETIME},
		{.name = sc_ruri,     .type = DB_STR},
		{.name = 0}
	};

	ctx = db_ctx("msilo");
	if (!ctx) goto error;
	if (db_add_db(ctx, db_url) < 0) goto error;
	if (db_connect(ctx) < 0) goto error;

	store = db_cmd(DB_PUT, ctx, ms_db_table, NULL, NULL, store_param);
	if (!store) goto error;

	load = db_cmd(DB_GET, ctx, ms_db_table, load_cols, load_match, NULL);
	if (!store) goto error;

	del_mid = db_cmd(DB_DEL, ctx, ms_db_table, NULL, del_mid_param, NULL);
	if (!del_mid) goto error;

	del_expired = db_cmd(DB_DEL, ctx, ms_db_table, NULL, del_expired_param, NULL);
	if (!store) goto error;

    return 0;

error:
	msilo_db_close();
	ERR("msilo: Error while initializing database layer\n");
	return -1;
}






/**
 * Initialize children
 */
static int child_init(int rank)
{

	if (rank==PROC_INIT || rank==PROC_MAIN || rank==PROC_TCP_MAIN)
		return 0; /* do nothing for the main or tcp_main processes */

	DBG("MSILO: init_child #%d / pid <%d>\n", rank, getpid());
	if (msilo_db_init(ms_db_url) < 0) return -1;
	return 0;
}


/**
 * destroy function
 */
static void destroy(void)
{
	DBG("MSILO: destroy module ...\n");
	msg_list_free(ml);

	msilo_db_close();
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

	int val, lexpire;
	t_content_type ctype;
	static char buf[512];
	static char buf1[1024];
	int mime, mode;
	str next_hop = STR_NULL;
	uac_req_t	uac_r;

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

	store->vals[0].v.lstr = to->uri;
	store->vals[1].v.lstr = from->uri;

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

	store->vals[2].v.lstr = uri;
	store->vals[3].v.lstr = uid;
	/* add the message's body in SQL query */
	store->vals[4].v.blob = body;

	lexpire = ms_expire_time;
	/* add 'content-type' -- parse the content-type header */
	if ((mime=parse_content_type_hdr(msg))<1 )
	{
		LOG(L_ERR,"MSILO:m_store: ERROR cannot parse Content-Type header\n");
		goto error;
	}

	store->vals[5].v.lstr.s = "text/plain";
	store->vals[5].v.lstr.len = 10;

	/** check the content-type value */
	if( mime!=(TYPE_TEXT<<16)+SUBTYPE_PLAIN
		&& mime!=(TYPE_MESSAGE<<16)+SUBTYPE_CPIM )
	{
		if(m_extract_content_type(msg->content_type->body.s,
				msg->content_type->body.len, &ctype, CT_TYPE) != -1)
		{
			DBG("MSILO:m_store: 'content-type' found\n");
			store->vals[5].v.lstr = ctype.type;
		}
	}

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
	store->vals[6].v.time = val + lexpire;
	store->vals[7].v.time = val;

	if (db_exec(NULL, store) < 0) {
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

		set_uac_req(&uac_r,
				&msg_type,        /* Type of the message */
				&str_hdr,         /* Optional headers including CRLF */
				&body,            /* Message body */
				0,                /* dialog */
				0,                /* callback flags */
				0,                /* callback function */
				0                 /* callback parameter */
					);
				
		tmb.t_request(&uac_r,
				(ctaddr.s)?&ctaddr:&from->uri,    /* Request-URI */
				&from->uri,       /* To */
				&reg_addr,        /* From */
				next_hop.len ? &next_hop: NULL        /* next hop */
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
	db_res_t* res = NULL;
	db_rec_t* rec;
	int i, mid, n;
	char hdr_buf[1024], body_buf[1024];

	str str_vals[5], hdr_str , body_str, uid;
	time_t rtime;
	str next_hop = STR_NULL;
	uac_req_t	uac_r;
	
	i=0; /* fix warning in DBG() */
	if (str1) {
		next_hop.s = str1;
		next_hop.len = strlen(str1);
	}

	DBG("MSILO:m_dump: ------------ start ------------\n");

	if (get_to_uid(&uid, msg) < 0) {
		LOG(L_ERR, "MSILO:m_dump: Unable to retrieve identity of user\n");
		goto error;
	}

	hdr_str.s = hdr_buf;
	hdr_str.len = 1024;
	body_str.s = body_buf;
	body_str.len = 1024;

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

	load->match[0].v.lstr = uid;

	if (db_exec(&res, load) < 0) {
		ERR("msilo: Error while loading messages from database\n");
		goto error;
	}
	if (!res || !(rec = db_first(res))) {
		DBG("MSILO:m_dump: no stored message for <%.*s>!\n", STR_FMT(&uid));
		goto done;
	}

	for(; rec; rec = db_next(res)) {
		if (rec->fld[0].flags & DB_NULL) {
			ERR("msilo: Database returned message with NULL msgid, skipping\n");
			continue;
		}
		mid = rec->fld[0].v.int4;
		if(msg_list_check_msg(ml, mid))
		{
			DBG("MSILO:m_dump: message[%d] mid=%d already sent.\n",
				i, mid);
			continue;
		}

		memset(str_vals, 0, 4*sizeof(str));
		if (!(rec->fld[1].flags & DB_NULL)) str_vals[0] = rec->fld[1].v.lstr;
		if (!(rec->fld[2].flags & DB_NULL)) str_vals[1] = rec->fld[2].v.lstr;
		if (!(rec->fld[3].flags & DB_NULL)) str_vals[2] = rec->fld[3].v.lstr;
		if (!(rec->fld[4].flags & DB_NULL)) str_vals[3] = rec->fld[4].v.lstr;
		if (!(rec->fld[6].flags & DB_NULL)) str_vals[4] = rec->fld[6].v.lstr;

		hdr_str.len = 1024;
		if(m_build_headers(&hdr_str, str_vals[3] /*ctype*/,
				str_vals[0]/*from*/) < 0)
		{
			DBG("MSILO:m_dump: headers building failed!!!\n");
			msg_list_set_flag(ml, mid, MS_MSG_ERRO);
			goto error;
		}

		DBG("MSILO:m_dump: msg [%d-%d] for: %.*s\n", i+1, mid,
				uid.len, ZSW(uid.s));

		/** sending using TM function: t_uac */
		body_str.len = 1024;
		if (rec->fld[5].flags & DB_NULL) {
			rtime = 0;
		} else {
			rtime = rec->fld[5].v.time;
		}
		n = m_build_body(&body_str, rtime, str_vals[2/*body*/]);
		if(n<0)
			DBG("MSILO:m_dump: sending simple body\n");
		else
			DBG("MSILO:m_dump: sending composed body\n");
		
		set_uac_req(&uac_r,
					&msg_type,  /* Type of the message */
					&hdr_str,         /* Optional headers including CRLF */
					(n<0)?&str_vals[2]:&body_str, /* Message body */
					0,                      /* dialog */
					TMCB_LOCAL_COMPLETED,   /* callback flags */
					m_tm_callback,          /* Callback function */
					(void*)(long)mid        /* Callback parameter */
					);
		tmb.t_request(&uac_r,
					  &str_vals[4],     /* Request-URI */
					  &str_vals[1],     /* To */
					  &str_vals[0],     /* From */
					  next_hop.len ? &next_hop: NULL /* next hop */
					  );
	}
	
 done:
	/**
	 * Free the result because we don't need it
	 * anymore
	 */
	if (res) db_res_free(res);
	return 1;
 error:
	if (res) db_res_free(res);
	return -1;
}

/**
 * - cleaning up the messages that got reply
 * - delete expired messages from database
 */
void m_clean_silo(unsigned int ticks, void *param)
{
	msg_list_el mle = NULL, p;

	DBG("MSILO:clean_silo: cleaning stored messages - %d\n", ticks);

	msg_list_check(ml);
	mle = p = msg_list_reset(ml);
	while(p) {
		if(p->flag & MS_MSG_DONE) {
			del_mid->match[0].v.int4 = p->msgid;
			DBG("MSILO:clean_silo: cleaning sent message [%d]\n", p->msgid);
			if (db_exec(NULL, del_mid) < 0) {
				DBG("MSILO:clean_silo: error while cleaning message %d.\n", p->msgid);
			}
		}
		p = p->next;
	}

	msg_list_el_free_all(mle);

	/* cleaning expired messages */
	if(ticks % (ms_check_time * ms_clean_period) < ms_check_time) {
		DBG("MSILO:clean_silo: cleaning expired messages\n");
		del_expired->match[0].v.time = (int)time(NULL);
		if (db_exec(NULL, del_expired) < 0) {
			DBG("MSILO:clean_silo: ERROR cleaning expired messages\n");
		}
	}
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
