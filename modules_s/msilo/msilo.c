/**
 * $Id$
 *
 * MSILO module
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

/**
 * History
 * -------
 *
 * 2003-01-23: switched from t_uac to t_uac_dlg (dcm)
 * 2003-02-28: protocolization of t_uac_dlg completed (jiri)
 * 2003-03-11: updated to the new module interface (andrei)
 *             removed non-constant intializers to some strs (andrei)
 * 2003-03-16: flags parameter added (janakj)
 * 2003-04-05: default_uri #define used (jiri)
 * 2003-04-06: db_init removed from mod_init, will be called from child_init
 *             now (janakj)
 * 2003-04-07: m_dump takes a parameter which sets the way the outgoing URI
 *             is computed (dcm)
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

#include "../tm/tm_load.h"

#include "ms_msg_list.h"
#include "msfuncs.h"

#define MAX_DEL_KEYS	1	
#define NR_KEYS			7

#define DB_KEY_MID		"mid"
#define DB_KEY_FROM		"src_addr"
#define DB_KEY_TO		"dst_addr"
#define DB_KEY_RURI		"r_uri"
#define DB_KEY_BODY		"body"
#define DB_KEY_CTYPE	"ctype"
#define DB_KEY_EXP_TIME	"exp_time"
#define DB_KEY_INC_TIME	"inc_time"

#define IDX_NO				6

#define STR_IDX_FROM		0
#define STR_IDX_TO			1
#define STR_IDX_RURI		2
#define STR_IDX_BODY		3
#define STR_IDX_CTYPE		4
#define STR_IDX_INC_TIME	5

#define STR_IDX_EXP_TIME	6

#define DUMP_IDX_MID		0
#define DUMP_IDX_FROM		1
#define DUMP_IDX_TO			2
#define DUMP_IDX_BODY		3
#define DUMP_IDX_CTYPE		4
#define DUMP_IDX_INC_TIME	5

#define SET_STR_VAL(_str, _res, _r, _c)	\
	if (RES_ROWS(_res)[_r].values[_c].nul != 0) \
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
			(_str).len=RES_ROWS(_res)[_r].values[_c].val.str_val.len; \
			(_str).s=(char*)RES_ROWS(_res)[_r].values[_c].val.str_val.s; \
			break; \
		default: \
			(_str).len=0; \
			(_str).s=NULL; \
		} \
	}

MODULE_VERSION

/** database connection */
db_con_t *db_con = NULL;


/** precessed msg list - used for dumping the messages */
msg_list ml = NULL;

/** TM bind */
struct tm_binds tmb;

/** parameters */

char *db_url=DEFAULT_DB_URL;
char *db_table="silo";
char *registrar=NULL; //"sip:registrar@iptel.org";
int  expire_time=259200;
int  check_time=30;
int  clean_period=5;
int  use_contact=1;

str msg_type = { "MESSAGE", 7};

str reg_addr;

/** module functions */
static int mod_init(void);
static int child_init(int);

static int m_store(struct sip_msg*, char*, char*);
static int m_dump(struct sip_msg*, char*, char*);

void destroy(void);

void m_clean_silo(unsigned int ticks, void *);

/** TM callback function */
static void m_tm_callback( struct cell *t, struct sip_msg *msg,
	int code, void *param);

static cmd_export_t cmds[]={
	{"m_store",  m_store, 1, 0, REQUEST_ROUTE | FAILURE_ROUTE},
	{"m_dump",   m_dump,  0, 0, REQUEST_ROUTE},
	{0,0,0,0,0}
};


static param_export_t params[]={
	{"db_url",       STR_PARAM, &db_url},
	{"db_table",     STR_PARAM, &db_table},
	{"registrar",    STR_PARAM, &registrar},
	{"expire_time",  INT_PARAM, &expire_time},
	{"check_time",   INT_PARAM, &check_time},
	{"clean_period", INT_PARAM, &clean_period},
	{"use_contact",  INT_PARAM, &use_contact},
	{0,0,0}
};


/** module exports */
struct module_exports exports= {
	"msilo",
	cmds,
	params,
	
	mod_init,   /* module initialization function */
	(response_function) 0,
	(destroy_function) destroy,
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

	/* binding to mysql module  */
	if (bind_dbmod())
	{
		DBG("MSILO: ERROR: Database module not found\n");
		return -1;
	}

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
	register_timer( m_clean_silo, 0, check_time);

	reg_addr.s = registrar;
	reg_addr.len = (registrar)?strlen(registrar):0;

	return 0;
}

/**
 * Initialize childs
 */
static int child_init(int rank)
{
	DBG("MSILO: init_child #%d / pid <%d>\n", rank, getpid());
	db_con = db_init(db_url);
	if (!db_con)
	{
		LOG(L_ERR,"MSILO: child %d: Error while connecting database\n", rank);
		return -1;
	}
	else
	{
		db_use_table(db_con, db_table);
		DBG("MSILO: child %d: Database connection opened successfully\n", rank);
	}
	return 0;
}

/**
 * store message
 * mode = "0" -- look for outgoing URI starting with new_uri
 * 		= "1" -- look for outgoing URI starting with r-uri
 * 		= "2" -- look for outgoing URI only at to header
 */
static int m_store(struct sip_msg* msg, char* mode, char* str2)
{
	str body, str_hdr, sruri, ctaddr;
	struct to_body to, *pto, *pfrom;
	db_key_t db_keys[NR_KEYS];
	db_val_t db_vals[NR_KEYS];
	int nr_keys = 0, val, lexpire;
	t_content_type ctype;
	char buf[512], buf1[1024], *p;

	DBG("MSILO: m_store: ------------ start ------------\n");

	// extract message body - after that whole SIP MESSAGE is parsed
	/* get the message's body */
	body.s = get_body( msg );
	if (body.s==0) 
	{
		LOG(L_ERR,"MSILO:m_store: ERROR cannot extract body from msg\n");
		goto error;
	}
	
	/* content-length (if present) must be already parsed */
	if (!msg->content_length) 
	{
		LOG(L_ERR,"MSILO:m_store: ERROR no Content-Length header found!\n");
		goto error;
	}
	body.len = get_content_length( msg );

	// check if the body of message contains something
	if(body.len <= 0)
	{
		DBG("MSILO:m_store: body of the message is empty!\n");
		goto error;
	}
	
	// check TO header
	if(!msg->to || !msg->to->body.s)
	{
		DBG("MSILO:m_store: cannot find 'to' header!\n");
		goto error;
	}

	if(msg->to->parsed != NULL)
	{
		pto = (struct to_body*)msg->to->parsed;
		DBG("MSILO:m_store: 'To' header ALREADY PARSED: <%.*s>\n",
			pto->uri.len, pto->uri.s );	
	}
	else
	{
		DBG("MSILO:m_store: 'To' header NOT PARSED ->parsing ...\n");
		memset( &to , 0, sizeof(to) );
		parse_to(msg->to->body.s, msg->to->body.s+msg->to->body.len+1, &to);
		if(to.uri.len > 0) // && to.error == PARSE_OK)
		{
			DBG("MSILO:m_store: 'To' parsed OK <%.*s>.\n", 
				to.uri.len, to.uri.s);
			pto = &to;
		}
		else
		{
			DBG("MSILO:m_store: ERROR 'To' cannot be parsed\n");
			goto error;
		}
	}
	
	if(pto->uri.len == reg_addr.len && 
			!strncasecmp(pto->uri.s, reg_addr.s, reg_addr.len))
	{
		DBG("MSILO:m_store: message to MSILO REGISTRAR!\n");
		goto error;
	}

	db_keys[nr_keys] = DB_KEY_TO;
	
	db_vals[nr_keys].type = DB_STR;
	db_vals[nr_keys].nul = 0;
	db_vals[nr_keys].val.str_val.s = pto->uri.s;
	db_vals[nr_keys].val.str_val.len = pto->uri.len;

	nr_keys++;

	// check FROM header
	if(!msg->from || !msg->from->body.s)
	{
		DBG("MSILO:m_store: ERROR cannot find 'from' header!\n");
		goto error;
	}

	if(msg->from->parsed != NULL)
		DBG("MSILO:m_store: 'From' header ALREADY PARSED\n");	
	else
	{
		DBG("MSILO:m_store: 'From' header NOT PARSED\n");
		/* parsing from header */
		if ( parse_from_header( msg )==-1 ) 
		{
			DBG("MSILO:m_store: ERROR cannot parse FROM header\n");
			goto error;
		}
	}
	pfrom = (struct to_body*)msg->from->parsed;
	DBG("MSILO:m_store: 'From' header: <%.*s>\n",pfrom->uri.len,pfrom->uri.s);	
	
	if(reg_addr.s && pfrom->uri.len == reg_addr.len && 
			!strncasecmp(pfrom->uri.s, reg_addr.s, reg_addr.len))
	{
		DBG("MSILO:m_store: message from MSILO REGISTRAR!\n");
		goto error;
	}

	db_keys[nr_keys] = DB_KEY_FROM;
	
	db_vals[nr_keys].type = DB_STR;
	db_vals[nr_keys].nul = 0;
	db_vals[nr_keys].val.str_val.s = pfrom->uri.s;
	db_vals[nr_keys].val.str_val.len = pfrom->uri.len;

	nr_keys++;

	// chech for RURI
	sruri.len = 0;
	if(mode && mode[0]=='0' && msg->new_uri.len > 0)
	{
		DBG("MSILO:m_store: NEW R-URI found - check if is AoR!\n");
		p = msg->new_uri.s;
		while((p < msg->new_uri.s+msg->new_uri.len) && *p!='@')
			p++;
		if(p < msg->new_uri.s+msg->new_uri.len && p > msg->new_uri.s)
		{
			DBG("MSILO:m_store: NEW R-URI used\n");
			sruri.s = msg->new_uri.s;
			// check for parameters
			while((p < msg->new_uri.s+msg->new_uri.len) && *p!=';')
				p++;
			sruri.len = p - msg->new_uri.s;
		}
	}
	
	if(mode && mode[0]<='1' && sruri.len == 0 
			&& msg->first_line.u.request.uri.len > 0 )
	{
		DBG("MSILO:m_store: R-URI found - check if is AoR!\n");
		p = msg->first_line.u.request.uri.s;
		while((p < msg->first_line.u.request.uri.s+
				msg->first_line.u.request.uri.len) && *p!='@')
			p++;
		if(p<msg->first_line.u.request.uri.s+msg->first_line.u.request.uri.len
			&& p > msg->first_line.u.request.uri.s)
		{
			DBG("MSILO:m_store: R-URI used\n");
			sruri.s = msg->first_line.u.request.uri.s;
			// check for parameters
			while((p < msg->first_line.u.request.uri.s
					+ msg->first_line.u.request.uri.len) && *p!=';')
				p++;
			sruri.len = p - msg->first_line.u.request.uri.s;
		}
	}
	
	if (sruri.len == 0)
	{
		DBG("MSILO:m_store: TO used as R-URI\n");
		sruri.s = pto->uri.s;
		sruri.len = pto->uri.len;
	}
	
	db_keys[nr_keys] = DB_KEY_RURI;
	
	db_vals[nr_keys].type = DB_STR;
	db_vals[nr_keys].nul = 0;
	db_vals[nr_keys].val.str_val.s = sruri.s;
	db_vals[nr_keys].val.str_val.len = sruri.len;

	nr_keys++;

	/* add the message's body in SQL query */
	
	db_keys[nr_keys] = DB_KEY_BODY;
	
	db_vals[nr_keys].type = DB_BLOB;
	db_vals[nr_keys].nul = 0;
	db_vals[nr_keys].val.blob_val.s = body.s;
	db_vals[nr_keys].val.blob_val.len = body.len;

	nr_keys++;
	
	lexpire = expire_time;
	// add 'content-type'
	/* parse the content-type header */
	if (parse_content_type_hdr(msg)==-1 ) 
	{
		LOG(L_ERR,"MSILO:m_store: ERROR cannot parse Content-Type header\n");
		goto error;
	}

	/** check the content-type value */
	if(msg->content_type && msg->content_type->body.len > 0
		&& get_content_type(msg)!=CONTENT_TYPE_TEXT_PLAIN
		&& get_content_type(msg)!=CONTENT_TYPE_MESSAGE_CPIM )
	{
		if(m_extract_content_type(msg->content_type->body.s, 
				msg->content_type->body.len, &ctype, CT_TYPE) != -1)
		{
			DBG("MSILO:m_store: 'content-type' found\n");
			db_keys[nr_keys] = DB_KEY_CTYPE;
			db_vals[nr_keys].type = DB_STR;
			db_vals[nr_keys].nul = 0;
			db_vals[nr_keys].val.str_val.s = ctype.type.s;
			db_vals[nr_keys].val.str_val.len = ctype.type.len;
			nr_keys++;
		}
	}

	// check 'expires'
	// no more parseing - already done by get_body()
	// if(parse_headers(msg, HDR_EXPIRES,0)!=-1)
	if(msg->expires && msg->expires->body.len > 0)
	{
		DBG("MSILO:m_store: 'expires' found\n");
		val = atoi(msg->expires->body.s);
		if(val > 0)
			lexpire = (expire_time<=val)?expire_time:val;
	}

	/** current time */
	val = (int)time(NULL);
	
	/** add expiration time */
	db_keys[nr_keys] = DB_KEY_EXP_TIME;
	db_vals[nr_keys].type = DB_INT;
	db_vals[nr_keys].nul = 0;
	db_vals[nr_keys].val.int_val = val+lexpire;
	nr_keys++;

	/** add incoming time */
	db_keys[nr_keys] = DB_KEY_INC_TIME;
	db_vals[nr_keys].type = DB_INT;
	db_vals[nr_keys].nul = 0;
	db_vals[nr_keys].val.int_val = val;
	nr_keys++;

	if(db_insert(db_con, db_keys, db_vals, nr_keys) < 0)
	{
		LOG(L_ERR, "MSILO:m_store: error storing message\n");
		goto error;
	}
	DBG("MSILO:m_store: message stored. T:<%.*s> F:<%.*s>\n",
		pto->uri.len, pto->uri.s, pfrom->uri.len, pfrom->uri.s);
	if(reg_addr.len > 0 && reg_addr.len+33+2*CRLF_LEN < 1024)
	{
		DBG("MSILO:m_store: sending info message.\n");
		strcpy(buf1,"Content-Type: text/plain"CRLF"Contact: ");
		str_hdr.len = 24 + CRLF_LEN + 9;
		strncat(buf1,reg_addr.s,reg_addr.len);
		str_hdr.len += reg_addr.len;
		strcat(buf1, CRLF);
		str_hdr.len += CRLF_LEN;
		str_hdr.s = buf1;
	
		strncpy(buf, "User [", 6);
		body.len = 6;
		if(pto->uri.len+75 < 512)
		{
			strncpy(buf+body.len, pto->uri.s, pto->uri.len);
			body.len += pto->uri.len;
		}
		strncpy(buf+body.len, "] is offline.", 13);
		body.len += 12;
		strncpy(buf+body.len, " The message will be delivered", 30);
		body.len += 30;
		strncpy(buf+body.len, " when user goes online.", 23);
		body.len += 23;

		body.s = buf;

		// look for Contact header
		ctaddr.s = NULL;
		if(use_contact && parse_headers(msg,HDR_CONTACT,0)!=-1 && msg->contact
				&& msg->contact->body.s && msg->contact->body.len>0)
		{
			ctaddr.s = msg->contact->body.s;
			ctaddr.len = msg->contact->body.len;
			p = ctaddr.s;
			while(p<ctaddr.s+ctaddr.len && *p!='<')
				p++;
			if(*p == '<')
			{
				p++;
				ctaddr.s = p;
				while(p<ctaddr.s+ctaddr.len && *p!='>')
						p++;
				if(p<ctaddr.s+ctaddr.len)
					ctaddr.len = p-ctaddr.s;
			}
			if(!ctaddr.s || ctaddr.len < 6 || strncmp(ctaddr.s, "sip:", 4)
				|| ctaddr.s[4]==' ')
				ctaddr.s = NULL;
			else
				DBG("MSILO:m_store: feedback contact [%.*s]\n",
							ctaddr.len,ctaddr.s);
		}
		
		// tmb.t_uac(&msg_type,&pfrom->uri,&str_hdr,&body,&reg_addr,0,0,0);
		tmb.t_request(&msg_type,  /* Type of the message */
				(ctaddr.s)?&ctaddr:&pfrom->uri,    /* Request-URI */
				&pfrom->uri,      /* To */
				&reg_addr,        /* From */
				&str_hdr,         /* Optional headers including CRLF */
				&body,            /* Message body */
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
	struct to_body to, *pto = NULL;
	db_key_t db_keys[1] = { DB_KEY_RURI };
	db_val_t db_vals[1];
	db_key_t db_cols[] = {	DB_KEY_MID, DB_KEY_FROM, DB_KEY_TO, DB_KEY_BODY,
							DB_KEY_CTYPE, DB_KEY_INC_TIME 
					};
	db_res_t* db_res = NULL;
	int i, db_no_cols = IDX_NO, db_no_keys = 1, *msg_id, mid, n;
	char hdr_buf[1024], body_buf[1024];

	str str_vals[IDX_NO], hdr_str , body_str;
	time_t rtime;

	DBG("MSILO:m_dump: ------------ start ------------\n");
	hdr_str.s=hdr_buf;
	hdr_str.len=1024;
	body_str.s=body_buf;
	body_str.len=1024;
	
	// check for TO header 
	if(parse_headers(msg, HDR_TO, 0)==-1 || !msg->to || !msg->to->body.s)
	{
		LOG(L_ERR,"MSILO:m_dump: ERROR cannot find TO HEADER!\n");
		goto error;
	}

	// check TO header
	if(msg->to->parsed != NULL)
	{
		pto = (struct to_body*)msg->to->parsed;
		DBG("MSILO:m_dump: 'To' header ALREADY PARSED: <%.*s>\n",
			pto->uri.len, pto->uri.s );	
	}
	else
	{
		memset( &to , 0, sizeof(to) );
		parse_to(msg->to->body.s,
			msg->to->body.s + msg->to->body.len + 1, &to);
		if(to.uri.len <= 0) // || to.error != PARSE_OK)
		{
			DBG("MSILO:m_dump: 'To' header NOT parsed\n");
			goto error;
		}
		pto = &to;
	}

	/**
	 * check if has expires=0 (REGISTER)
	 */
	if(parse_headers(msg, HDR_EXPIRES, 0) >= 0)
	{
		//check 'expires' > 0
		if(msg->expires && msg->expires->body.len > 0)
		{
			i = atoi(msg->expires->body.s);
			if(i <= 0)
			{ // user goes offline
				DBG("MSILO:m_dump: user <%.*s> goes offline - expires=%d\n",
						pto->uri.len, pto->uri.s, i);
				goto error;
			}
			else
				DBG("MSILO:m_dump: user <%.*s> online - expires=%d\n",
						pto->uri.len, pto->uri.s, i);
		}
	}
	else
	{
		DBG("MSILO:m_dump: 'exprires' threw error at parsing\n");
		goto error;
	}

	db_vals[0].type = DB_STR;
	db_vals[0].nul = 0;
	db_vals[0].val.str_val.s = pto->uri.s;
	db_vals[0].val.str_val.len = pto->uri.len;


	memset(str_vals, 0, IDX_NO*sizeof(str));
	
	if((db_query(db_con,db_keys,NULL,db_vals,db_cols,db_no_keys,db_no_cols,
			NULL,&db_res)==0) && (RES_ROW_N(db_res) > 0))
	{
		DBG("MSILO:m_dump: dumping [%d] messages for <%.*s>!!!\n", 
				RES_ROW_N(db_res), pto->uri.len, pto->uri.s);

		for(i = 0; i < RES_ROW_N(db_res); i++) 
		{
			mid =  RES_ROWS(db_res)[i].values[DUMP_IDX_MID].val.int_val;
			if(msg_list_check_msg(ml, mid))
			{
				DBG("MSILO:m_dump: message[%d] mid=%d already sent.\n", 
					i, mid);
				continue;
			}
			
			SET_STR_VAL(str_vals[STR_IDX_FROM], db_res, i, DUMP_IDX_FROM);
			SET_STR_VAL(str_vals[STR_IDX_TO], db_res, i, DUMP_IDX_TO);
			SET_STR_VAL(str_vals[STR_IDX_BODY], db_res, i, DUMP_IDX_BODY);
			SET_STR_VAL(str_vals[STR_IDX_CTYPE], db_res, i, DUMP_IDX_CTYPE);
			
			hdr_str.len = 1024;		
			if(m_build_headers(&hdr_str, str_vals[STR_IDX_CTYPE],
					str_vals[STR_IDX_FROM]) < 0)
			{
				DBG("MSILO:m_dump: headers bulding failed!!!\n");
				if (db_free_query(db_con, db_res) < 0)
					DBG("MSILO:m_dump: Error while freeing result of"
						" query\n");
				msg_list_set_flag(ml, mid, MS_MSG_ERRO);
				goto error;
			}
			
			if((msg_id = shm_malloc(sizeof(int))) == 0)
			{
				DBG("MSILO:m_dump: no more share memory!");
				if (db_free_query(db_con, db_res) < 0)
					DBG("MSILO:m_dump: Error while freeing result of"
						" query\n");
				msg_list_set_flag(ml, mid, MS_MSG_ERRO);
				goto error;
			}
						
			*msg_id = mid;
			
			DBG("MSILO:m_dump: msg [%d-%d] for: %.*s\n", i+1, *msg_id,
					pto->uri.len, pto->uri.s);
			
			/** sending using TM function: t_uac */

			body_str.len = 1024;
			rtime = 
				(time_t)RES_ROWS(db_res)[i].values[DUMP_IDX_INC_TIME].val.int_val;
			n = m_build_body(&body_str, rtime, str_vals[STR_IDX_BODY]);
			if(n<0)
				DBG("MSILO:m_dump: sending simple body\n");
			else
				DBG("MSILO:m_dump: sending composed body\n");
			
			//tmb.t_uac(&msg_type, &pto->uri, &hdr_str,
			//	&body_str, &str_vals[STR_IDX_FROM],
			//	m_tm_callback, (void*)msg_id, 0
			//);
			
			tmb.t_request(&msg_type,  /* Type of the message */
					&pto->uri,               /* Request-URI */
					&str_vals[STR_IDX_TO],   /* To */
					&str_vals[STR_IDX_FROM], /* From */
					&hdr_str,         /* Optional headers including CRLF */
					(n<0)?&str_vals[STR_IDX_BODY]:&body_str, /* Message body */
					m_tm_callback,    /* Callback function */
					(void*)msg_id     /* Callback parameter */
				);
		}
	}
	else
		DBG("MSILO:m_dump: no stored message for <%.*s>!\n", pto->uri.len,
					pto->uri.s);
	/**
	 * Free the result because we don't need it
	 * anymore
	 */
	if (db_free_query(db_con, db_res) < 0)
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
			db_keys[n] = DB_KEY_MID;
			db_vals[n].type = DB_INT;
			db_vals[n].nul = 0;
			db_vals[n].val.int_val = p->msgid;
			DBG("MSILO:clean_silo: cleaning sent message [%d]\n", p->msgid);
			n++;
			if(n==MAX_DEL_KEYS)
			{
				if (db_delete(db_con, db_keys, NULL, db_vals, n) < 0) 
					DBG("MSILO:clean_silo: error cleaning %d messages.\n",n);
				n = 0;
			}
		}
		p = p->next;
	}
	if(n>0)
	{
		if (db_delete(db_con, db_keys, NULL, db_vals, n) < 0) 
			DBG("MSILO:clean_silo: error cleaning %d messages\n", n);
		n = 0;
	}

	msg_list_el_free_all(mle);
	
	// cleaning expired messages
	if(ticks%(check_time*clean_period)<check_time)
	{
		DBG("MSILO:clean_silo: cleaning expired messages\n");
		db_keys[0] = DB_KEY_EXP_TIME;
		db_vals[0].type = DB_INT;
		db_vals[0].nul = 0;
		db_vals[0].val.int_val = (int)time(NULL);
		if (db_delete(db_con, db_keys, db_ops, db_vals, 1) < 0) 
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

	if(db_con)
		db_close(db_con);
}

/** 
 * TM callback function - delete message from database if was sent OK
 */
void m_tm_callback( struct cell *t, struct sip_msg *msg,
	int code, void *param)
{
	DBG("MSILO:m_tm_callback: completed with status %d\n", code);
	if(!t->cbp)
	{
		DBG("MSILO m_tm_callback: message id not received\n");
		goto done;
	}
	if(!db_con)
	{
		DBG("MSILO:m_tm_callback: db_con is NULL\n");
		goto done;
	}
	if(code < 200 || code >= 300)
	{
		DBG("MSILO:m_tm_callback: message <%d> was not sent successfully\n",
				*((int*)t->cbp));
		msg_list_set_flag(ml, *((int*)t->cbp), MS_MSG_ERRO);
		goto done;
	}

	msg_list_set_flag(ml, *((int*)t->cbp), MS_MSG_DONE);
	
done:
	return;
}

