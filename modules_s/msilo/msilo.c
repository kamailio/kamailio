/**
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


#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>

#include "../../sr_module.h"
#include "../../error.h"
#include "../../dprint.h"
#include "../../ut.h"
#include "../im/im_funcs.h"
#include "../../mem/shm_mem.h"
#include "../../mem/mem.h"
#include "../../globals.h"
#include "../../db/db.h"
#include "../tm/t_funcs.h"
#include "../tm/uac.h"
#include "../tm/tm_load.h"

#include "msfuncs.h"

#define NR_KEYS	7	

#define DB_KEY_MID		"mid"
#define DB_KEY_IURI		"iuri"
#define DB_KEY_OURI 	"ouri"
#define DB_KEY_FROM		"from_h"
#define DB_KEY_TO		"to_h"
#define DB_KEY_BODY		"body"
#define DB_KEY_CTYPE	"ctype"
#define DB_KEY_EXP_TIME	"exp_time"
#define DB_KEY_INC_TIME	"inc_time"

#define STR_IDX_NO		8

#define STR_IDX_IURI	0
#define STR_IDX_OURI 	1
#define STR_IDX_FROM	2
#define STR_IDX_TO		3
#define STR_IDX_BODY	4
#define STR_IDX_CTYPE	5
#define STR_IDX_EXP_TIME	6
#define STR_IDX_INC_TIME	7

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
			(_str).len=RES_ROWS(_res)[_r].values[_c].val.str_val.len; \
			(_str).s=(char*)RES_ROWS(_res)[_r].values[_c].val.str_val.s; \
			break; \
		default: \
			(_str).len=0; \
			(_str).s=NULL; \
		} \
	}

/** database connection */
db_con_t *db_con = NULL;

/** TM bind */
struct tm_binds tmb;

/** parameters */

char *db_url="sql://root@127.0.0.1/msilo";
char *db_table="silo";
int  expiration=3600;

/** module functions */
static int mod_init(void);
static int child_init(int);

static int m_store(struct sip_msg*, char*, char*);
static int m_dump(struct sip_msg*, char*, char*);

void destroy(void);

void m_clean_silo(void *);

/** TM callback function */
static void m_tm_callback( struct cell *t, struct sip_msg *msg,
	int code, void *param);

/** module exports */
struct module_exports exports= {
	"msilo",
	(char*[]){
		"m_store",
		"m_dump"
	},
	(cmd_function[]){
		m_store,
		m_dump
	},
	(int[]){
		0,
		0
	},
	(fixup_function[]){
		0,
		0
	},
	2,

	(char*[]) {   /* Module parameter names */
		"db_url",
		"db_table",
		"expiration"
	},
	(modparam_t[]) {   /* Module parameter types */
		STR_PARAM,
		STR_PARAM,
		INT_PARAM
	},
	(void*[]) {   /* Module parameter variable pointers */
		&db_url,
		&db_table,
		&expiration
	},
	3,      /* Number of module paramers */
	
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
	if ( !(load_tm=(load_tm_f)find_export("load_tm", NO_SCRIPT))) {
		LOG(L_ERR, "ERROR: msilo: mod_init: can't import load_tm\n");
		return -1;
	}
	/* let the auto-loading function load all TM stuff */
	if (load_tm( &tmb )==-1)
		return -1;

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
		DBG("MSILO: child %d: Database connection opened successfuly\n", rank);
	}
	return 0;
}

/**
 * store message
 */
static int m_store(struct sip_msg* msg, char* str1, char* str2)
{
	str body;
	struct to_body to, from, *pto, *pfrom;
	db_key_t db_keys[NR_KEYS];
	db_val_t db_vals[NR_KEYS];
	int nr_keys = 0, val;
	t_content_type ctype;

	DBG("MSILO: m_store: ------------ start ------------\n");
		
	// extract message body - after that whole SIP MESSAGE is parsed
	if ( !im_extract_body(msg, &body) )
	{
		DBG("MSILO: m_store: cannot extract body from sip msg!\n");
		goto error;
	}
	
	db_keys[nr_keys] = DB_KEY_BODY;
	
	db_vals[nr_keys].type = DB_BLOB;
	db_vals[nr_keys].nul = 0;
	db_vals[nr_keys].val.blob_val.s = body.s;
	db_vals[nr_keys].val.blob_val.len = body.len;

	nr_keys++;
	
	// check FROM header
	if(msg->from != NULL && msg->from->body.s != NULL)
	{
		if(msg->from->parsed != NULL)
		{
			pfrom = (struct to_body*)msg->from->parsed;
			DBG("MSILO: m_store: 'From' header ALREADY PARSED: <%.*s>\n",
				pfrom->uri.len, pfrom->uri.s );	
		}
		else
		{
			memset( &from , 0, sizeof(from) );
			parse_to(msg->from->body.s, 
				msg->from->body.s + msg->from->body.len + 1, &from);
			if(from.uri.len > 0) // && from.error == PARSE_OK)
			{
				DBG("MSILO: m_store: 'from' parsed OK <%.*s>.\n",
					from.uri.len, from.uri.s);
				pfrom = &from;
			}
			else
			{
				DBG("MSILO: m_store: 'from' NOT parsed\n");
				goto error;
			}
		}
	}
	else
	{
		DBG("MSILO: m_store: cannot find 'from' header!\n");
		goto error;
	}
	db_keys[nr_keys] = DB_KEY_FROM;
	
	db_vals[nr_keys].type = DB_STR;
	db_vals[nr_keys].nul = 0;
	db_vals[nr_keys].val.str_val.s = pfrom->uri.s;
	db_vals[nr_keys].val.str_val.len = pfrom->uri.len;

	nr_keys++;

	// check TO header
	if(msg->to != NULL && msg->to->body.s != NULL)
	{
		if(msg->to->parsed != NULL)
		{
			pto = (struct to_body*)msg->to->parsed;
			DBG("MSILO: m_store: 'To' header ALREADY PARSED: <%.*s>\n",
				pto->uri.len, pto->uri.s );	
		}
		else
		{
			memset( &to , 0, sizeof(to) );
			parse_to(msg->to->body.s,
					msg->to->body.s + msg->to->body.len + 1, &to);
			if(to.uri.len > 0) // && to.error == PARSE_OK)
			{
				DBG("MSILO: m_store: 'to' parsed OK <%.*s>.\n", 
					to.uri.len, to.uri.s);
				pto = &to;
			}
			else
			{
				DBG("MSILO: m_store: 'to' NOT parsed\n");
				goto error;
			}
		}
	}
	else
	{
		DBG("MSILO: m_store: cannot find 'to' header!\n");
		goto error;
	}
	
	db_keys[nr_keys] = DB_KEY_TO;
	
	db_vals[nr_keys].type = DB_STR;
	db_vals[nr_keys].nul = 0;
	db_vals[nr_keys].val.str_val.s = pto->uri.s;
	db_vals[nr_keys].val.str_val.len = pto->uri.len;

	nr_keys++;
	
	// check 'request uri'
	if ( msg->first_line.u.request.uri.len <= 0 )
	{
		DBG("MSILO: m_store: ERROR getting URI from first line\n");
		goto error;
	}
	db_keys[nr_keys] = DB_KEY_IURI;
	
	db_vals[nr_keys].type = DB_STR;
	db_vals[nr_keys].nul = 0;
	db_vals[nr_keys].val.str_val.s = msg->first_line.u.request.uri.s;
	db_vals[nr_keys].val.str_val.len = msg->first_line.u.request.uri.len;

	nr_keys++;

	// check 'new uri'
	if( msg->new_uri.len > 0 )
	{
		DBG("MSILO: m_store: new URI found\n");
		db_keys[nr_keys] = DB_KEY_OURI;
	
		db_vals[nr_keys].type = DB_STR;
		db_vals[nr_keys].nul = 0;
		db_vals[nr_keys].val.str_val.s = msg->new_uri.s;
		db_vals[nr_keys].val.str_val.len = msg->new_uri.len;

		nr_keys++;
	}
	else
		DBG("MSILO: m_store: new URI not found\n");

	if(parse_headers(msg, HDR_CONTENTTYPE | HDR_EXPIRES, 0) >= 0)
	{
		// add 'content-type'
		if(msg->content_type && msg->content_type->body.len > 0)
		{
			if(parse_content_type(msg->content_type->body.s, 
					msg->content_type->body.len, &ctype, CT_TYPE)
					!= -1)
			{
				DBG("MSILO: m_store: 'content-type' found\n");
				db_keys[nr_keys] = DB_KEY_CTYPE;
				db_vals[nr_keys].type = DB_STR;
				db_vals[nr_keys].nul = 0;
				db_vals[nr_keys].val.str_val.s = ctype.type.s;
				db_vals[nr_keys].val.str_val.len = ctype.type.len;
				nr_keys++;
			}
			
		}
		//check 'expires'
		if(msg->expires && msg->expires->body.len > 0)
		{
			DBG("MSILO: m_store: 'expires' found\n");
			val = atoi(msg->expires->body.s);
			if(val > 0)
				expiration = (expiration<=val)?expiration:val;
		}
	}
	else
	{
		DBG("MSILO: m_store: 'content-type' and 'exprires' threw error"
			"at parsing\n");
	}

	/** current time */
	val = (int)time(NULL);
	
	/** add expiration time */
	db_keys[nr_keys] = DB_KEY_EXP_TIME;
	db_vals[nr_keys].type = DB_INT;
	db_vals[nr_keys].nul = 0;
	db_vals[nr_keys].val.int_val = val+expiration;
	nr_keys++;

	/** add incoming time */
	db_keys[nr_keys] = DB_KEY_INC_TIME;
	db_vals[nr_keys].type = DB_INT;
	db_vals[nr_keys].nul = 0;
	db_vals[nr_keys].val.int_val = val;
	nr_keys++;

	if(db_insert(db_con, db_keys, db_vals, nr_keys) < 0)
	{
		LOG(L_ERR, "MSILO: m_store: error storing message\n");
		goto error;
	}
	DBG("MSILO: m_store: message stored.\n");

	return 1;
error:
	return -1;
}

/**
 * dump message
 */
static int m_dump(struct sip_msg* msg, char* str1, char* str2)
{
	struct to_body to, *pto;
	db_key_t db_keys[1] = { DB_KEY_TO };
	db_val_t db_vals[1];
	db_key_t db_cols[] = {	DB_KEY_MID, DB_KEY_IURI, DB_KEY_OURI, DB_KEY_FROM,
						DB_KEY_BODY, DB_KEY_CTYPE, DB_KEY_INC_TIME 
					};
	db_res_t* db_res = NULL;
	int i, db_no_cols = 7, db_no_keys = 1, *msg_id;
	char hdr_buf[1024], body_buf[1024];

	str str_vals[STR_IDX_NO], *sp, 
			hdr_str  = { hdr_buf, 1024 }, 
			body_str = { body_buf, 1024 },
			msg_type = { "MESSAGE", 7};

	DBG("MSILO: m_dump: ------------ start ------------\n");
	
	// check TO header
	if(msg->to != NULL && msg->to->body.s != NULL)
	{
		if(msg->to->parsed != NULL)
		{
			pto = (struct to_body*)msg->to->parsed;
			DBG("MSILO: m_dump: 'To' header ALREADY PARSED: <%.*s>\n",
				pto->uri.len, pto->uri.s );	
		}
		else
		{
			memset( &to , 0, sizeof(to) );
			parse_to(msg->to->body.s,
				msg->to->body.s + msg->to->body.len + 1, &to);
			if(to.uri.len <= 0) // || to.error != PARSE_OK)
			{
				DBG("MSILO: m_dump: 'to' NOT parsed\n");
				goto error;
			}
			pto = &to;
		}
	}
	else
	{
		DBG("MSILO: m_dump: cannot find 'to' header!\n");
		goto error;
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
				DBG("MSILO: m_dump: user <%.*s> goes offline - expires=%d\n",
						pto->uri.len, pto->uri.s, i);
				goto error;
			}
			else
				DBG("MSILO: m_dump: user <%.*s> online - expires=%d\n",
						pto->uri.len, pto->uri.s, i);
		}
	}
	else
	{
		DBG("MSILO: m_store: 'exprires' threw error at parsing\n");
		goto error;
	}

	db_vals[0].type = DB_STR;
	db_vals[0].nul = 0;
	db_vals[0].val.str_val.s = pto->uri.s;
	db_vals[0].val.str_val.len = pto->uri.len;


	memset(str_vals, 0, STR_IDX_NO*sizeof(str));
	
	if((db_query(db_con, db_keys, db_vals, db_cols, db_no_keys, db_no_cols,
				NULL, &db_res)==0) && (RES_ROW_N(db_res) > 0))
	{
		DBG("MSILO: m_dump: dumping [%d] messages for <%.*s>!!!\n", 
				RES_ROW_N(db_res), pto->uri.len, pto->uri.s);

		for(i = 0; i < RES_ROW_N(db_res); i++) 
		{
		//RES_ROWS(db_res)[i].values[0].val.string_val;

			SET_STR_VAL(str_vals[STR_IDX_IURI], db_res, i, 1);
			SET_STR_VAL(str_vals[STR_IDX_OURI], db_res, i, 2);
			SET_STR_VAL(str_vals[STR_IDX_FROM], db_res, i, 3);
			SET_STR_VAL(str_vals[STR_IDX_BODY], db_res, i, 4);
			SET_STR_VAL(str_vals[STR_IDX_CTYPE], db_res, i, 5);
			
			
			/** determination of req URI */
			/**
			if(str_vals[STR_IDX_OURI].len > 0)
				sp = &str_vals[STR_IDX_OURI];
			else
				if(str_vals[STR_IDX_IURI].len > 0)
					sp = &str_vals[STR_IDX_IURI];
				else
			**/
			sp = &pto->uri;

			hdr_str.len = 1024;		
			if(m_build_headers(&hdr_str, str_vals[STR_IDX_CTYPE]) < 0)
			{
				DBG("MSILO: m_dump: headers bulding failed!!!\n");
				if (db_free_query(db_con, db_res) < 0)
					DBG("MSILO: Error while freeing result of"
						" query\n");
				goto error;
			}
			
			if((msg_id = shm_malloc(sizeof(int))) == 0)
			{
				DBG("MSILO: m_dump: no more share memory!");
				if (db_free_query(db_con, db_res) < 0)
					DBG("MSILO: Error while freeing result of"
						" query\n");
				goto error;
			}
						
			*msg_id = RES_ROWS(db_res)[i].values[0].val.int_val;
			
			DBG("MSILO: m_dump: msg [%d-%d] for: %.*s\n", i+1, *msg_id,
					sp->len, sp->s);
			
			/** sending using IM library */
			/***
			m_send_message( *msg_id, 
				sp, &pto->uri, &str_vals[STR_IDX_FROM], 
				&str_vals[STR_IDX_FROM], &str_vals[STR_IDX_CTYPE],
				&str_vals[STR_IDX_BODY]);
			***/
			
			/** sending using TM function: t_uac */

			body_str.len = 1024;
			if(m_build_body(&body_str, 
					RES_ROWS(db_res)[i].values[6].val.int_val,
					str_vals[STR_IDX_BODY] ) < 0)
			{
				DBG("MSILO: m_dump: sending simple body\n");
				tmb.t_uac(&msg_type, &pto->uri, &hdr_str,
					&str_vals[STR_IDX_BODY], &str_vals[STR_IDX_FROM],
					m_tm_callback, (void*)msg_id, 0
				);
			}
			else
			{
				DBG("MSILO: m_dump: sending composed body\n");
				tmb.t_uac(&msg_type, &pto->uri, &hdr_str,
					&body_str, &str_vals[STR_IDX_FROM],
					m_tm_callback, (void*)msg_id, 0
				);
			}
		}
	}
	else
		DBG("MSILO: m_dump: no stored message for <%.*s>!\n", pto->uri.len,
					pto->uri.s);
	/**
	 * Free the result because we don't need it
	 * anymore
	 */
	if (db_free_query(db_con, db_res) < 0)
		DBG("MSILO: m_dump: Error while freeing result of query\n");

	return 1;
error:
	return -1;
}

/**
 * delete expired messages from database - waiting foe new DB module
 */
void m_clean_silo(void *param)
{
	db_key_t db_keys[] = { DB_KEY_EXP_TIME };
	db_val_t db_vals[] = { { DB_INT, 0, { .int_val = (int)time(NULL) } } };
	
	LOG(L_ERR, "MSILO: clean_silo: cleaning expired messages\n");
	
	if (db_delete(db_con, db_keys, db_vals, 1) < 0) 
		LOG(L_ERR, "MSILO: clean_silo: error cleaning exp. messages\n");
}


/**
 * destroy function
 */
void destroy(void)
{
	DBG("MSILO: destroy module ...\n");
	if(db_con)
		db_close(db_con);
}

/** 
 * TM callback function - delete message from database if was sent OK
 */
void m_tm_callback( struct cell *t, struct sip_msg *msg,
	int code, void *param)
{
	db_key_t db_keys[] = { DB_KEY_MID };
	db_val_t db_vals[1];
	
	DBG("MSILO: m_tm_callback: completed with status %d\n", code);
	if(!t->cbp)
	{
		DBG("MSILO: m_tm_callback: message id not received\n");
		goto done;
	}
	if(!db_con)
	{
		DBG("MSILO: m_tm_callback: db_con is NULL\n");
		goto done;
	}
	if(code < 200 || code >= 300)
	{
		DBG("MSILO: m_tm_callback: message <%d> was not sent successfully\n",
				*((int*)t->cbp));
		goto done;
	}
	
	db_vals[0].type = DB_INT;
	db_vals[0].nul = 0;
	db_vals[0].val.int_val = *((int*)t->cbp);
	
	if (db_delete(db_con, db_keys, db_vals, 1) < 0) 
		LOG(L_ERR,"MSILO: m_tm_callback: error deleting sent message"
				" <%d>\n", db_vals[0].val.int_val);
	else
		DBG("MSILO: m_tm_callback: message <%d> deleted from database\n",
				db_vals[0].val.int_val);
	
done:
	return;
}

