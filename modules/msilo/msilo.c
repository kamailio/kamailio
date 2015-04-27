/*
 * MSILO module
 *
 * Copyright (C) 2001-2003 FhG Fokus
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
#include <sys/ipc.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>

#include "../../sr_module.h"
#include "../../dprint.h"
#include "../../ut.h"
#include "../../timer.h"
#include "../../mem/shm_mem.h"
#include "../../lib/srdb1/db.h"
#include "../../parser/parse_from.h"
#include "../../parser/parse_content.h"
#include "../../parser/contact/parse_contact.h"
#include "../../parser/parse_allow.h"
#include "../../parser/parse_methods.h"
#include "../../resolve.h"
#include "../../usr_avp.h"
#include "../../mod_fix.h"

#include "../../modules/tm/tm_load.h"

#include "ms_msg_list.h"
#include "msfuncs.h"
#include "api.h"

#define MAX_DEL_KEYS	1	
#define NR_KEYS			11

static str sc_mid         = str_init("id");         /*  0 */
static str sc_from        = str_init("src_addr");   /*  1 */
static str sc_to          = str_init("dst_addr");   /*  2 */
static str sc_uri_user    = str_init("username");   /*  3 */
static str sc_uri_host    = str_init("domain");     /*  4 */
static str sc_body        = str_init("body");       /*  5 */
static str sc_ctype       = str_init("ctype");      /*  6 */
static str sc_exp_time    = str_init("exp_time");   /*  7 */
static str sc_inc_time    = str_init("inc_time");   /*  8 */
static str sc_snd_time    = str_init("snd_time");   /*  9 */
static str sc_stored_hdrs = str_init("extra_hdrs"); /* 10 */

#define SET_STR_VAL(_str, _res, _r, _c)	\
	if (RES_ROWS(_res)[_r].values[_c].nul == 0) \
	{ \
		switch(RES_ROWS(_res)[_r].values[_c].type) \
		{ \
		case DB1_STRING: \
			(_str).s=(char*)RES_ROWS(_res)[_r].values[_c].val.string_val; \
			(_str).len=strlen((_str).s); \
			break; \
		case DB1_STR: \
			(_str).len=RES_ROWS(_res)[_r].values[_c].val.str_val.len; \
			(_str).s=(char*)RES_ROWS(_res)[_r].values[_c].val.str_val.s; \
			break; \
		case DB1_BLOB: \
			(_str).len=RES_ROWS(_res)[_r].values[_c].val.blob_val.len; \
			(_str).s=(char*)RES_ROWS(_res)[_r].values[_c].val.blob_val.s; \
			break; \
		default: \
			(_str).len=0; \
			(_str).s=NULL; \
		} \
	}

MODULE_VERSION

#define S_TABLE_VERSION 8

/** database connection */
static db1_con_t *db_con = NULL;
static db_func_t msilo_dbf;

/** precessed msg list - used for dumping the messages */
msg_list ml = NULL;

/** TM bind */
struct tm_binds tmb;

/** parameters */

static str ms_db_url = str_init(DEFAULT_DB_URL);
static str ms_db_table = str_init("silo");
str  ms_reminder = {NULL, 0};
str  ms_outbound_proxy = {NULL, 0};

char*  ms_from = NULL; /*"sip:registrar@example.org";*/
char*  ms_contact = NULL; /*"Contact: <sip:registrar@example.org>\r\n";*/
char*  ms_extra_hdrs = NULL; /*"X-foo: bar\r\nX-bar: foo\r\n";*/
char*  ms_content_type = NULL; /*"Content-Type: text/plain\r\n";*/
char*  ms_offline_message = NULL; /*"<em>I'm offline.</em>"*/
void**  ms_from_sp = NULL;
void**  ms_contact_sp = NULL;
void**  ms_extra_hdrs_sp = NULL;
void**  ms_content_type_sp = NULL;
void**  ms_offline_message_sp = NULL;

int  ms_expire_time = 259200;
int  ms_check_time = 60;
int  ms_send_time = 0;
int  ms_clean_period = 10;
int  ms_use_contact = 1;
int  ms_add_date = 1;
int  ms_add_contact = 0;
int  ms_max_messages = 0;

static str ms_snd_time_avp_param = {NULL, 0};
int_str ms_snd_time_avp_name;
unsigned short ms_snd_time_avp_type;

static str ms_extra_hdrs_avp_param = {NULL, 0};
int_str ms_extra_hdrs_avp_name;
unsigned short ms_extra_hdrs_avp_type;

str msg_type = str_init("MESSAGE");
static int ms_skip_notification_flag = -1;

/** module functions */
static int mod_init(void);
static int child_init(int);

static int m_store(struct sip_msg*, str*);
static int m_dump(struct sip_msg*, str*);

static int m_store_2(struct sip_msg*, char*, char*);
static int m_dump_2(struct sip_msg*, char*, char*);

static void destroy(void);

static int bind_msilo(msilo_api_t* api);

void m_clean_silo(unsigned int ticks, void *);
void m_send_ontimer(unsigned int ticks, void *);

int ms_reset_stime(int mid);

int check_message_support(struct sip_msg* msg);


/** TM callback function */
static void m_tm_callback( struct cell *t, int type, struct tmcb_params *ps);

static cmd_export_t cmds[]={
	{"m_store",  (cmd_function)m_store_2, 0, 0, 0, REQUEST_ROUTE | FAILURE_ROUTE},
	{"m_store",  (cmd_function)m_store_2, 1, fixup_spve_null, 0,
		REQUEST_ROUTE | FAILURE_ROUTE},
	{"m_dump",   (cmd_function)m_dump_2,  0, 0, 0, REQUEST_ROUTE},
	{"m_dump",   (cmd_function)m_dump_2,  1, fixup_spve_null, 0,
		REQUEST_ROUTE},
	{"bind_msilo",(cmd_function)bind_msilo, 1, 0, 0, ANY_ROUTE},
	{0,0,0,0,0,0}
};


static param_export_t params[]={
	{ "db_url",           PARAM_STR, &ms_db_url             },
	{ "db_table",         PARAM_STR, &ms_db_table           },
	{ "from_address",     PARAM_STRING, &ms_from                 },
	{ "contact_hdr",      PARAM_STRING, &ms_contact              },
	{ "extra_hdrs",       PARAM_STRING, &ms_extra_hdrs           },
	{ "content_type_hdr", PARAM_STRING, &ms_content_type         },
	{ "offline_message",  PARAM_STRING, &ms_offline_message      },
	{ "reminder",         PARAM_STR, &ms_reminder           },
	{ "outbound_proxy",   PARAM_STR, &ms_outbound_proxy     },
	{ "expire_time",      INT_PARAM, &ms_expire_time          },
	{ "check_time",       INT_PARAM, &ms_check_time           },
	{ "send_time",        INT_PARAM, &ms_send_time            },
	{ "clean_period",     INT_PARAM, &ms_clean_period         },
	{ "use_contact",      INT_PARAM, &ms_use_contact          },
	{ "sc_mid",           PARAM_STR, &sc_mid                },
	{ "sc_from",          PARAM_STR, &sc_from               },
	{ "sc_to",            PARAM_STR, &sc_to                 },
	{ "sc_uri_user",      PARAM_STR, &sc_uri_user           },
	{ "sc_uri_host",      PARAM_STR, &sc_uri_host           },
	{ "sc_body",          PARAM_STR, &sc_body               },
	{ "sc_ctype",         PARAM_STR, &sc_ctype              },
	{ "sc_exp_time",      PARAM_STR, &sc_exp_time           },
	{ "sc_inc_time",      PARAM_STR, &sc_inc_time           },
	{ "sc_snd_time",      PARAM_STR, &sc_snd_time           },
	{ "sc_stored_hdrs",   PARAM_STR, &sc_stored_hdrs        },
	{ "snd_time_avp",     PARAM_STR, &ms_snd_time_avp_param },
	{ "extra_hdrs_avp",   PARAM_STR, &ms_extra_hdrs_avp_param },
	{ "add_date",         INT_PARAM, &ms_add_date             },
	{ "max_messages",     INT_PARAM, &ms_max_messages         },
	{ "add_contact",      INT_PARAM, &ms_add_contact          },
	{ "skip_notification_flag", PARAM_INT, &ms_skip_notification_flag },
	{ 0,0,0 }
};

#ifdef STATISTICS
#include "../../lib/kcore/statistics.h"

stat_var* ms_stored_msgs;
stat_var* ms_dumped_msgs;
stat_var* ms_failed_msgs;
stat_var* ms_dumped_rmds;
stat_var* ms_failed_rmds;

stat_export_t msilo_stats[] = {
	{"stored_messages" ,  0,  &ms_stored_msgs  },
	{"dumped_messages" ,  0,  &ms_dumped_msgs  },
	{"failed_messages" ,  0,  &ms_failed_msgs  },
	{"dumped_reminders" , 0,  &ms_dumped_rmds  },
	{"failed_reminders" , 0,  &ms_failed_rmds  },
	{0,0,0}
};

#endif
/** module exports */
struct module_exports exports= {
	"msilo",    /* module id */
	DEFAULT_DLFLAGS, /* dlopen flags */
	cmds,       /* module's exported functions */
	params,     /* module's exported parameters */
#ifdef STATISTICS
	msilo_stats,
#else
	0,          /* exported statistics */
#endif
	0,          /* exported MI functions */
	0,          /* exported pseudo-variables */
	0,          /* extra processes */
	mod_init,   /* module initialization function */
	0,   	    /* response handler */
	(destroy_function) destroy,  /* module destroy function */
	child_init  /* per-child init function */
};

static int bind_msilo(msilo_api_t* api)
{
	if (!api) {
		return -1;
	}
	api->m_store = m_store;
	api->m_dump = m_dump;
	return 0;
}

/**
 * init module function
 */
static int mod_init(void)
{
	pv_spec_t avp_spec;

#ifdef STATISTICS
	/* register statistics */
	if (register_module_stats( exports.name, msilo_stats)!=0 ) {
		LM_ERR("failed to register core statistics\n");
		return -1;
	}
#endif

	if(ms_skip_notification_flag!=-1) {
		if(ms_skip_notification_flag<0 || ms_skip_notification_flag>31) {
			LM_ERR("invalid skip notification flag value: %d\n",
					ms_skip_notification_flag);
			return -1;
		}
		ms_skip_notification_flag = 1 << ms_skip_notification_flag;
	}

	/* binding to mysql module  */
	if (db_bind_mod(&ms_db_url, &msilo_dbf))
	{
		LM_DBG("database module not found\n");
		return -1;
	}

	if (!DB_CAPABILITY(msilo_dbf, DB_CAP_ALL)) {
		LM_ERR("database module does not implement "
		    "all functions needed by the module\n");
		return -1;
	}

	if (ms_snd_time_avp_param.s && ms_snd_time_avp_param.len > 0) {
		if (pv_parse_spec(&ms_snd_time_avp_param, &avp_spec)==0
				|| avp_spec.type!=PVT_AVP) {
			LM_ERR("malformed or non AVP %.*s AVP definition\n",
					ms_snd_time_avp_param.len, ms_snd_time_avp_param.s);
			return -1;
		}

		if(pv_get_avp_name(0, &(avp_spec.pvp), &ms_snd_time_avp_name,
					&ms_snd_time_avp_type)!=0)
		{
			LM_ERR("[%.*s]- invalid AVP definition\n",
					ms_snd_time_avp_param.len, ms_snd_time_avp_param.s);
			return -1;
		}
	}

	if (ms_extra_hdrs_avp_param.s && ms_extra_hdrs_avp_param.len > 0) {
	    if (pv_parse_spec(&ms_extra_hdrs_avp_param, &avp_spec)==0
		|| avp_spec.type!=PVT_AVP) {
		LM_ERR("malformed or non AVP %.*s AVP definition\n",
		       ms_extra_hdrs_avp_param.len, ms_extra_hdrs_avp_param.s);
		return -1;
	    }

	    if (pv_get_avp_name(0, &(avp_spec.pvp), &ms_extra_hdrs_avp_name,
			       &ms_extra_hdrs_avp_type) != 0) {
		LM_ERR("[%.*s]- invalid AVP definition\n",
		       ms_extra_hdrs_avp_param.len, ms_extra_hdrs_avp_param.s);
		return -1;
	    }
	}

	db_con = msilo_dbf.init(&ms_db_url);
	if (!db_con)
	{
		LM_ERR("failed to connect to the database\n");
		return -1;
	}

	if(db_check_table_version(&msilo_dbf, db_con, &ms_db_table, S_TABLE_VERSION) < 0) {
		LM_ERR("error during table version check.\n");
		return -1;
	}
	if(db_con)
		msilo_dbf.close(db_con);
	db_con = NULL;

	/* load the TM API */
	if (load_tm_api(&tmb)!=0) {
		LM_ERR("can't load TM API\n");
		return -1;
	}

	if(ms_from!=NULL)
	{
		ms_from_sp = (void**)pkg_malloc(sizeof(void*));
		if(ms_from_sp==NULL)
		{
			LM_ERR("no more pkg\n");
			return -1;
		}
		*ms_from_sp = (void*)ms_from;
		if(fixup_spve_null(ms_from_sp, 1)!=0)
		{
			LM_ERR("bad contact parameter\n");
			return -1;
		}
	}
	if(ms_contact!=NULL)
	{
		ms_contact_sp = (void**)pkg_malloc(sizeof(void*));
		if(ms_contact_sp==NULL)
		{
			LM_ERR("no more pkg\n");
			return -1;
		}
		*ms_contact_sp = (void*)ms_contact;
		if(fixup_spve_null(ms_contact_sp, 1)!=0)
		{
			LM_ERR("bad contact parameter\n");
			return -1;
		}
	}
	if(ms_extra_hdrs!=NULL)
	{
		ms_extra_hdrs_sp = (void**)pkg_malloc(sizeof(void*));
		if(ms_extra_hdrs_sp==NULL)
		{
			LM_ERR("no more pkg\n");
			return -1;
		}
		*ms_extra_hdrs_sp = (void*)ms_extra_hdrs;
		if(fixup_spve_null(ms_extra_hdrs_sp, 1)!=0)
		{
			LM_ERR("bad extra_hdrs parameter\n");
			return -1;
		}
	}
	if(ms_content_type!=NULL)
	{
		ms_content_type_sp = (void**)pkg_malloc(sizeof(void*));
		if(ms_content_type_sp==NULL)
		{
			LM_ERR("no more pkg\n");
			return -1;
		}
		*ms_content_type_sp = (void*)ms_content_type;
		if(fixup_spve_null(ms_content_type_sp, 1)!=0)
		{
			LM_ERR("bad content_type parameter\n");
			return -1;
		}
	}
	if(ms_offline_message!=NULL)
	{
		ms_offline_message_sp = (void**)pkg_malloc(sizeof(void*));
		if(ms_offline_message_sp==NULL)
		{
			LM_ERR("no more pkg\n");
			return -1;
		}
		*ms_offline_message_sp = (void*)ms_offline_message;
		if(fixup_spve_null(ms_offline_message_sp, 1)!=0)
		{
			LM_ERR("bad offline_message parameter\n");
			return -1;
		}
	}
	if(ms_offline_message!=NULL && ms_content_type==NULL)
	{
		LM_ERR("content_type parameter must be set\n");
		return -1;
	}

	ml = msg_list_init();
	if(ml==NULL)
	{
		LM_ERR("can't initialize msg list\n");
		return -1;
	}
	if(ms_check_time<0)
	{
		LM_ERR("bad check time value\n");
		return -1;
	}
	register_timer(m_clean_silo, 0, ms_check_time);
	if(ms_send_time>0 && ms_reminder.s!=NULL)
		register_timer(m_send_ontimer, 0, ms_send_time);

	return 0;
}

/**
 * Initialize children
 */
static int child_init(int rank)
{
	if (rank==PROC_INIT || rank==PROC_MAIN || rank==PROC_TCP_MAIN)
		return 0; /* do nothing for the main process */

	LM_DBG("rank #%d / pid <%d>\n", rank, getpid());
	if (msilo_dbf.init==0)
	{
		LM_CRIT("database not bound\n");
		return -1;
	}
	db_con = msilo_dbf.init(&ms_db_url);
	if (!db_con)
	{
		LM_ERR("child %d: failed to connect database\n", rank);
		return -1;
	}
	else
	{
		if (msilo_dbf.use_table(db_con, &ms_db_table) < 0) {
			LM_ERR("child %d: failed in use_table\n", rank);
			return -1;
		}
		
		LM_DBG("#%d database connection opened successfully\n", rank);
	}
	return 0;
}

/**
 * get_non_mandatory_headers
 * Extracts additional headers into the given buffer for storing alongside the message
 * returns the length of the created data
 *
 * It is assumed that all headers have been parsed at this point
 */
static int get_non_mandatory_headers(struct sip_msg *msg, char *buf, int buf_len)
{
	struct hdr_field *hdrs;
	int len = 0;
	int_str avp_value;
	struct usr_avp *avp;

	if (ms_extra_hdrs_avp_name.n != 0) {
	    avp = NULL;
	    avp = search_first_avp(ms_extra_hdrs_avp_type,
				   ms_extra_hdrs_avp_name, &avp_value, 0);
	    if ((avp != NULL) && is_avp_str_val(avp)) {
		if (buf_len <= avp_value.s.len) {
		    LM_ERR("insufficient space to store headers in silo\n");
		    return -1;
		}
		memcpy(buf, avp_value.s.s, avp_value.s.len);
		return avp_value.s.len;
	    }
	}

	for (hdrs = msg->headers; hdrs != NULL; hdrs = hdrs->next)
	{
		switch (hdrs->type) {
			case HDR_OTHER_T:
			case HDR_PPI_T:
			case HDR_PAI_T:
			case HDR_PRIVACY_T:
				if (buf_len <= hdrs->len)
				{
					LM_ERR("Insufficient space to store headers in silo\n");
					return -1;
				}
				memcpy(buf, hdrs->name.s, hdrs->len);
				len += hdrs->len;
				buf += hdrs->len;
				buf_len -= hdrs->len;
				break;
			default:
				break;
		}
	}
	return len;
}

/**
 * store message
 * mode = "0" -- look for outgoing URI starting with new_uri
 * 		= "1" -- look for outgoing URI starting with r-uri
 * 		= "2" -- look for outgoing URI only at to header
 */

static int m_store(struct sip_msg* msg, str *owner_s)
{
	str body, str_hdr, ctaddr;
	struct to_body *pto, *pfrom;
	struct sip_uri puri;
	str duri;
#define EXTRA_HDRS_BUF_LEN	1024
	static char extra_hdrs_buf[EXTRA_HDRS_BUF_LEN];
	str extra_hdrs;
	db_key_t db_keys[NR_KEYS-1];
	db_val_t db_vals[NR_KEYS-1];
	db_key_t db_cols[1]; 
	db1_res_t* res = NULL;
	uac_req_t uac_r;
	
	int nr_keys = 0, val, lexpire;
	content_type_t ctype;
#define MS_BUF1_SIZE	1024
	static char ms_buf1[MS_BUF1_SIZE];
	int mime;
	str notify_from;
	str notify_body;
	str notify_ctype;
	str notify_contact;

	int_str        avp_value;
	struct usr_avp *avp;

	LM_DBG("------------ start ------------\n");

	/* get message body - after that whole SIP MESSAGE is parsed */
	body.s = get_body( msg );
	if (body.s==0) 
	{
		LM_ERR("cannot extract body from msg\n");
		goto error;
	}
	
	/* content-length (if present) must be already parsed */
	if (!msg->content_length) 
	{
		LM_ERR("no Content-Length header found!\n");
		goto error;
	}
	body.len = get_content_length( msg );

	/* check if the body of message contains something */
	if(body.len <= 0)
	{
		LM_ERR("body of the message is empty!\n");
		goto error;
	}
	
	/* get TO URI */
	if(parse_to_header(msg)<0)
	{
	    LM_ERR("failed getting 'to' header!\n");
	    goto error;
	}
	
	pto = get_to(msg);
	
	/* get the owner */
	memset(&puri, 0, sizeof(struct sip_uri));
	if(owner_s != NULL)
	{
		if(parse_uri(owner_s->s, owner_s->len, &puri)!=0)
		{
			LM_ERR("bad owner SIP address!\n");
			goto error;
		} else {
			LM_DBG("using user id [%.*s]\n", owner_s->len, owner_s->s);
		}
	} else { /* get it from R-URI */
		if(msg->new_uri.len <= 0)
		{
			if(msg->first_line.u.request.uri.len <= 0)
			{
				LM_ERR("bad dst URI!\n");
				goto error;
			}
			duri = msg->first_line.u.request.uri;
		} else {
			duri = msg->new_uri;
		}
		LM_DBG("NEW R-URI found - check if is AoR!\n");
		if(parse_uri(duri.s, duri.len, &puri)!=0)
		{
			LM_ERR("bad dst R-URI!!\n");
			goto error;
		}
	}
	if(puri.user.len<=0)
	{
		LM_ERR("no username for owner\n");
		goto error;
	}
	
	db_keys[nr_keys] = &sc_uri_user;
	
	db_vals[nr_keys].type = DB1_STR;
	db_vals[nr_keys].nul = 0;
	db_vals[nr_keys].val.str_val.s = puri.user.s;
	db_vals[nr_keys].val.str_val.len = puri.user.len;

	nr_keys++;

	db_keys[nr_keys] = &sc_uri_host;
	
	db_vals[nr_keys].type = DB1_STR;
	db_vals[nr_keys].nul = 0;
	db_vals[nr_keys].val.str_val.s = puri.host.s;
	db_vals[nr_keys].val.str_val.len = puri.host.len;

	nr_keys++;

	if (msilo_dbf.use_table(db_con, &ms_db_table) < 0)
	{
		LM_ERR("failed to use_table\n");
		goto error;
	}

	if (ms_max_messages > 0) {
	    db_cols[0] = &sc_inc_time;
	    if (msilo_dbf.query(db_con, db_keys, 0, db_vals, db_cols,
				2, 1, 0, &res) < 0 ) {
			LM_ERR("failed to query the database\n");
			return -1;
	    }
	    if (RES_ROW_N(res) >= ms_max_messages) {
			LM_ERR("too many messages for AoR '%.*s@%.*s'\n",
			    puri.user.len, puri.user.s, puri.host.len, puri.host.s);
 	        msilo_dbf.free_result(db_con, res);
		return -1;
	    }
	    msilo_dbf.free_result(db_con, res);
	}

	/* Set To key */
	db_keys[nr_keys] = &sc_to;
	
	db_vals[nr_keys].type = DB1_STR;
	db_vals[nr_keys].nul = 0;
	db_vals[nr_keys].val.str_val.s = pto->uri.s;
	db_vals[nr_keys].val.str_val.len = pto->uri.len;

	nr_keys++;

	/* check FROM URI */
	if ( parse_from_header( msg )<0 ) 
	{
		LM_ERR("cannot parse From header\n");
		goto error;
	}
	pfrom = get_from(msg);
	LM_DBG("'From' header: <%.*s>\n", pfrom->uri.len, pfrom->uri.s);	
	
	db_keys[nr_keys] = &sc_from;
	
	db_vals[nr_keys].type = DB1_STR;
	db_vals[nr_keys].nul = 0;
	db_vals[nr_keys].val.str_val.s = pfrom->uri.s;
	db_vals[nr_keys].val.str_val.len = pfrom->uri.len;

	nr_keys++;

	/* add the message's body in SQL query */

	db_keys[nr_keys] = &sc_body;

	db_vals[nr_keys].type = DB1_BLOB;
	db_vals[nr_keys].nul = 0;
	db_vals[nr_keys].val.blob_val.s = body.s;
	db_vals[nr_keys].val.blob_val.len = body.len;

	nr_keys++;

	lexpire = ms_expire_time;
	/* add 'content-type' -- parse the content-type header */
	if ((mime=parse_content_type_hdr(msg))<1 ) 
	{
		LM_ERR("cannot parse Content-Type header\n");
		goto error;
	}

	db_keys[nr_keys]      = &sc_ctype;
	db_vals[nr_keys].type = DB1_STR;
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
			LM_DBG("'content-type' found\n");
			db_vals[nr_keys].val.str_val.s   = ctype.type.s;
			db_vals[nr_keys].val.str_val.len = ctype.type.len;
		}
	}
	nr_keys++;

	/* check 'expires' -- no more parsing - already done by get_body() */
	if(msg->expires && msg->expires->body.len > 0)
	{
		LM_DBG("'expires' found\n");
		val = atoi(msg->expires->body.s);
		if(val > 0)
			lexpire = (ms_expire_time<=val)?ms_expire_time:val;
	}

	/* current time */
	val = (int)time(NULL);

	/* add expiration time */
	db_keys[nr_keys] = &sc_exp_time;
	db_vals[nr_keys].type = DB1_INT;
	db_vals[nr_keys].nul = 0;
	db_vals[nr_keys].val.int_val = val+lexpire;
	nr_keys++;

	/* add incoming time */
	db_keys[nr_keys] = &sc_inc_time;
	db_vals[nr_keys].type = DB1_INT;
	db_vals[nr_keys].nul = 0;
	db_vals[nr_keys].val.int_val = val;
	nr_keys++;

	/* add sending time */
	db_keys[nr_keys] = &sc_snd_time;
	db_vals[nr_keys].type = DB1_INT;
	db_vals[nr_keys].nul = 0;
	db_vals[nr_keys].val.int_val = 0;
	if(ms_snd_time_avp_name.n!=0)
	{
		avp = NULL;
		avp=search_first_avp(ms_snd_time_avp_type, ms_snd_time_avp_name,
				&avp_value, 0);
		if(avp!=NULL && is_avp_str_val(avp))
		{
			if(ms_extract_time(&avp_value.s, &db_vals[nr_keys].val.int_val)!=0)
				db_vals[nr_keys].val.int_val = 0;
		}
	}
	nr_keys++;

	/* add the extra headers in SQL query */
	extra_hdrs.s = extra_hdrs_buf;
	extra_hdrs.len = get_non_mandatory_headers(msg, extra_hdrs_buf, EXTRA_HDRS_BUF_LEN);
	if (extra_hdrs.len < 0)
	{
	  goto error;
	}

	db_keys[nr_keys] = &sc_stored_hdrs;

	db_vals[nr_keys].type = DB1_BLOB;
	db_vals[nr_keys].nul = 0;
	db_vals[nr_keys].val.blob_val.s = extra_hdrs.s;
	db_vals[nr_keys].val.blob_val.len = extra_hdrs.len;

	nr_keys++;
	
	if(msilo_dbf.insert(db_con, db_keys, db_vals, nr_keys) < 0)
	{
		LM_ERR("failed to store message\n");
		goto error;
	}
	LM_DBG("message stored. T:<%.*s> F:<%.*s>\n",
		pto->uri.len, pto->uri.s, pfrom->uri.len, pfrom->uri.s);
	
#ifdef STATISTICS
	update_stat(ms_stored_msgs, 1);
#endif

	if(ms_from==NULL || ms_offline_message == NULL
			|| (ms_skip_notification_flag!=-1
				&& (msg->flags & ms_skip_notification_flag)))
		goto done;

	LM_DBG("sending info message.\n");
	if(fixup_get_svalue(msg, (gparam_p)*ms_from_sp, &notify_from)!=0
			|| notify_from.len<=0)
	{
		LM_WARN("cannot get notification From address\n");
		goto done;
	}
	if(fixup_get_svalue(msg, (gparam_p)*ms_offline_message_sp, &notify_body)!=0
			|| notify_body.len<=0)
	{
		LM_WARN("cannot get notification body\n");
		goto done;
	}
	if(fixup_get_svalue(msg, (gparam_p)*ms_content_type_sp, &notify_ctype)!=0
			|| notify_ctype.len<=0)
	{
		LM_WARN("cannot get notification content type\n");
		goto done;
	}

	if(ms_contact!=NULL && fixup_get_svalue(msg, (gparam_p)*ms_contact_sp,
				&notify_contact)==0 && notify_contact.len>0)
	{
		if(notify_contact.len+notify_ctype.len>=MS_BUF1_SIZE)
		{
			LM_WARN("insufficient buffer to build notification headers\n");
			goto done;
		}
		memcpy(ms_buf1, notify_contact.s, notify_contact.len);
		memcpy(ms_buf1+notify_contact.len, notify_ctype.s, notify_ctype.len);
		str_hdr.s = ms_buf1;
		str_hdr.len = notify_contact.len + notify_ctype.len;
	} else {
		str_hdr = notify_ctype;
	}

	/* look for Contact header -- must be parsed by now*/
	ctaddr.s = NULL;
	if(ms_use_contact && msg->contact!=NULL && msg->contact->body.s!=NULL
			&& msg->contact->body.len > 0)
	{
		LM_DBG("contact header found\n");
		if((msg->contact->parsed!=NULL 
			&& ((contact_body_t*)(msg->contact->parsed))->contacts!=NULL)
			|| (parse_contact(msg->contact)==0
			&& msg->contact->parsed!=NULL
			&& ((contact_body_t*)(msg->contact->parsed))->contacts!=NULL))
		{
			LM_DBG("using contact header for info msg\n");
			ctaddr.s = 
			((contact_body_t*)(msg->contact->parsed))->contacts->uri.s;
			ctaddr.len =
			((contact_body_t*)(msg->contact->parsed))->contacts->uri.len;
		
			if(!ctaddr.s || ctaddr.len < 6 || strncmp(ctaddr.s, "sip:", 4)
				|| ctaddr.s[4]==' ')
				ctaddr.s = NULL;
			else
				LM_DBG("feedback contact [%.*s]\n",	ctaddr.len,ctaddr.s);
		}
	}
		
	memset(&uac_r,'\0', sizeof(uac_r));
	uac_r.method = &msg_type;
	uac_r.headers = &str_hdr;
	uac_r.body = &notify_body;
	tmb.t_request(&uac_r,  /* UAC Req */
				  (ctaddr.s)?&ctaddr:&pfrom->uri,    /* Request-URI */
				  &pfrom->uri,      /* To */
				  &notify_from,     /* From */
				  (ms_outbound_proxy.s)?&ms_outbound_proxy:0 /* outbound uri */
		);

done:
	return 1;
error:
	return -1;
}

/**
 * store message
 */
static int m_store_2(struct sip_msg* msg, char* owner, char* s2)
{
	str owner_s;
	if (owner != NULL)
	{
		if(fixup_get_svalue(msg, (gparam_p)owner, &owner_s)!=0)
		{
			LM_ERR("invalid owner uri parameter");
			return -1;
		}
		return m_store(msg, &owner_s);
	}
	return m_store(msg, NULL);
}

/**
 * dump message
 */
static int m_dump(struct sip_msg* msg, str* owner_s)
{
	struct to_body *pto = NULL;
	db_key_t db_keys[3];
	db_key_t ob_key;
	db_op_t  db_ops[3];
	db_val_t db_vals[3];
	db_key_t db_cols[7];
	db1_res_t* db_res = NULL;
	int i, db_no_cols = 7, db_no_keys = 3, mid, n;
	static char hdr_buf[1024];
	static char body_buf[1024];
	struct sip_uri puri;
	uac_req_t uac_r;
	str str_vals[5], hdr_str, body_str, extra_hdrs_str, tmp_extra_hdrs;
	time_t rtime;
	
	/* init */
	ob_key = &sc_mid;

	db_keys[0]=&sc_uri_user;
	db_keys[1]=&sc_uri_host;
	db_keys[2]=&sc_snd_time;
	db_ops[0]=OP_EQ;
	db_ops[1]=OP_EQ;
	db_ops[2]=OP_EQ;

	db_cols[0]=&sc_mid;
	db_cols[1]=&sc_from;
	db_cols[2]=&sc_to;
	db_cols[3]=&sc_body;
	db_cols[4]=&sc_ctype;
	db_cols[5]=&sc_inc_time;
	db_cols[6]=&sc_stored_hdrs;

	
	LM_DBG("------------ start ------------\n");
	hdr_str.s=hdr_buf;
	hdr_str.len=1024;
	body_str.s=body_buf;
	body_str.len=1024;
	
	/* check for TO header */
	if(parse_to_header(msg)<0)
	{
		LM_ERR("failed parsing  To header\n");
		goto error;
	}

	pto = get_to(msg);

	/**
	 * check if has expires=0 (REGISTER)
	 */
	if(msg->first_line.u.request.method_value==METHOD_REGISTER)
	{
		if (check_message_support(msg)!=0) {
		    LM_DBG("MESSAGE method not supported\n");
			return -1;
		}
	}
	 
	/* get the owner */
	memset(&puri, 0, sizeof(struct sip_uri));
	if(owner_s)
	{
		if(parse_uri(owner_s->s, owner_s->len, &puri)!=0)
		{
			LM_ERR("bad owner SIP address!\n");
			goto error;
		} else {
			LM_DBG("using user id [%.*s]\n", owner_s->len, owner_s->s);
		}
	} else { /* get it from  To URI */
		if(parse_uri(pto->uri.s, pto->uri.len, &puri)!=0)
		{
			LM_ERR("bad owner To URI!\n");
			goto error;
		}
	}
	if(puri.user.len<=0 || puri.user.s==NULL
			|| puri.host.len<=0 || puri.host.s==NULL)
	{
		LM_ERR("bad owner URI!\n");
		goto error;
	}

	db_vals[0].type = DB1_STR;
	db_vals[0].nul = 0;
	db_vals[0].val.str_val.s = puri.user.s;
	db_vals[0].val.str_val.len = puri.user.len;

	db_vals[1].type = DB1_STR;
	db_vals[1].nul = 0;
	db_vals[1].val.str_val.s = puri.host.s;
	db_vals[1].val.str_val.len = puri.host.len;

	db_vals[2].type = DB1_INT;
	db_vals[2].nul = 0;
	db_vals[2].val.int_val = 0;

	if (db_con == NULL) {
	    LM_ERR("database connection has not been established\n");
	    goto error;
	}

	if (msilo_dbf.use_table(db_con, &ms_db_table) < 0)
	{
		LM_ERR("failed to use_table\n");
		return -1;
	}

	if (msilo_dbf.query(db_con,db_keys,db_ops,db_vals,db_cols,db_no_keys,
			    db_no_cols, ob_key, &db_res) < 0) {
	    LM_ERR("failed to query database\n");
	    goto error;
	}

	if (RES_ROW_N(db_res) <= 0) {
	    LM_DBG("no stored message for <%.*s>!\n", pto->uri.len, pto->uri.s);
	    goto done;
	}
		
	LM_DBG("dumping [%d] messages for <%.*s>!!!\n", 
	       RES_ROW_N(db_res), pto->uri.len, pto->uri.s);

	for(i = 0; i < RES_ROW_N(db_res); i++) 
	{
		mid =  RES_ROWS(db_res)[i].values[0].val.int_val;
		if(msg_list_check_msg(ml, mid))
		{
			LM_DBG("message[%d] mid=%d already sent.\n", i, mid);
			continue;
		}
		
		memset(str_vals, 0, 5*sizeof(str));
		SET_STR_VAL(str_vals[0], db_res, i, 1); /* from */
		SET_STR_VAL(str_vals[1], db_res, i, 2); /* to */
		SET_STR_VAL(str_vals[2], db_res, i, 3); /* body */
		SET_STR_VAL(str_vals[3], db_res, i, 4); /* ctype */
		SET_STR_VAL(str_vals[4], db_res, i, 6); /* stored hdrs */
		rtime = 
			(time_t)RES_ROWS(db_res)[i].values[5/*inc time*/].val.int_val;
		
		if (ms_extra_hdrs != NULL) {
		    if (fixup_get_svalue(msg, (gparam_p)*ms_extra_hdrs_sp,
					 &extra_hdrs_str) != 0) {
			if (msilo_dbf.free_result(db_con, db_res) < 0)
				LM_ERR("failed to free the query result\n");
			LM_ERR("unable to get extra_hdrs value\n");
			goto error;
		    }
		} else {
		    extra_hdrs_str.len = 0;
		}

		tmp_extra_hdrs.len = extra_hdrs_str.len+str_vals[4].len;
		if(tmp_extra_hdrs.len>0)
		{
			if ((tmp_extra_hdrs.s = pkg_malloc(tmp_extra_hdrs.len)) == NULL)
			{
				LM_ERR("Out of pkg memory");
				if (msilo_dbf.free_result(db_con, db_res) < 0)
					LM_ERR("failed to free the query result\n");
				msg_list_set_flag(ml, mid, MS_MSG_ERRO);
				goto error;
			}
			if(extra_hdrs_str.len>0)
				memcpy(tmp_extra_hdrs.s, extra_hdrs_str.s, extra_hdrs_str.len);
			memcpy(tmp_extra_hdrs.s+extra_hdrs_str.len, str_vals[4].s, str_vals[4].len);
		} else {
			tmp_extra_hdrs.len = 0;
			tmp_extra_hdrs.s = "";
		}
		hdr_str.len = 1024;
		if(m_build_headers(&hdr_str, str_vals[3] /*ctype*/,
				   str_vals[0]/*from*/, rtime /*Date*/,
				   tmp_extra_hdrs /*extra_hdrs*/) < 0)
		{
			LM_ERR("headers building failed [%d]\n", mid);
			if(tmp_extra_hdrs.len>0)
				pkg_free(tmp_extra_hdrs.s);
			if (msilo_dbf.free_result(db_con, db_res) < 0)
				LM_ERR("failed to free the query result\n");
			msg_list_set_flag(ml, mid, MS_MSG_ERRO);
			goto error;
		}
		if(tmp_extra_hdrs.len>0)
			pkg_free(tmp_extra_hdrs.s);
			
		LM_DBG("msg [%d-%d] for: %.*s\n", i+1, mid,	pto->uri.len, pto->uri.s);
			
		/** sending using TM function: t_uac */
		body_str.len = 1024;
		/* send composed body only if content type is text/plain */
		if ((str_vals[3].len == 10) &&
		    (strncmp(str_vals[3].s, "text/plain", 10) == 0)) {
		    n = m_build_body(&body_str, rtime, str_vals[2/*body*/], 0);
		} else {
		    n = -1;
		}
		if(n<0)
			LM_DBG("sending simple body\n");
		else
			LM_DBG("sending composed body\n");
		
		memset(&uac_r,'\0', sizeof(uac_r));
		uac_r.method = &msg_type;
		uac_r.headers = &hdr_str;
		uac_r.body = (n<0)?&str_vals[2]:&body_str;
		uac_r.cb_flags = TMCB_LOCAL_COMPLETED;
		uac_r.cb  = m_tm_callback;
		uac_r.cbp = (void*)(long)mid;

		tmb.t_request(&uac_r,  /* UAC Req */
					  &str_vals[1],  /* Request-URI */
					  &str_vals[1],  /* To */
					  &str_vals[0],  /* From */
					  (ms_outbound_proxy.s)?&ms_outbound_proxy:0 /* ob uri */
			);
	}

done:
	/**
	 * Free the result because we don't need it
	 * anymore
	 */
	if ((db_res !=NULL) && msilo_dbf.free_result(db_con, db_res) < 0)
		LM_ERR("failed to free result of query\n");

	return 1;
error:
	return -1;
}

/**
 * dump message
 */
static int m_dump_2(struct sip_msg* msg, char* owner, char* s2)
{
	str owner_s;
	if (owner != NULL)
	{
		if(fixup_get_svalue(msg, (gparam_p)owner, &owner_s)!=0)
		{
			LM_ERR("invalid owner uri parameter");
			return -1;
		}
		return m_dump(msg, &owner_s);
	}
	return m_dump(msg, NULL);
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
	
	LM_DBG("cleaning stored messages - %d\n", ticks);
	
	msg_list_check(ml);
	mle = p = msg_list_reset(ml);
	n = 0;
	if (msilo_dbf.use_table(db_con, &ms_db_table) < 0)
	{
		LM_ERR("failed to use_table\n");
		return;
	}
	while(p)
	{
		if(p->flag & MS_MSG_DONE)
		{
#ifdef STATISTICS
			if(p->flag & MS_MSG_TSND)
				update_stat(ms_dumped_msgs, 1);
			else
				update_stat(ms_dumped_rmds, 1);
#endif

			db_keys[n] = &sc_mid;
			db_vals[n].type = DB1_INT;
			db_vals[n].nul = 0;
			db_vals[n].val.int_val = p->msgid;
			LM_DBG("cleaning sent message [%d]\n", p->msgid);
			n++;
			if(n==MAX_DEL_KEYS)
			{
				if (msilo_dbf.delete(db_con, db_keys, NULL, db_vals, n) < 0) 
					LM_ERR("failed to clean %d messages.\n",n);
				n = 0;
			}
		}
		if((p->flag & MS_MSG_ERRO) && (p->flag & MS_MSG_TSND))
		{ /* set snd time to 0 */
			ms_reset_stime(p->msgid);
#ifdef STATISTICS
			update_stat(ms_failed_rmds, 1);
#endif

		}
#ifdef STATISTICS
		if((p->flag & MS_MSG_ERRO) && !(p->flag & MS_MSG_TSND))
			update_stat(ms_failed_msgs, 1);
#endif
		p = p->next;
	}
	if(n>0)
	{
		if (msilo_dbf.delete(db_con, db_keys, NULL, db_vals, n) < 0) 
			LM_ERR("failed to clean %d messages\n", n);
		n = 0;
	}

	msg_list_el_free_all(mle);
	
	/* cleaning expired messages */
	if(ticks%(ms_check_time*ms_clean_period)<ms_check_time)
	{
		LM_DBG("cleaning expired messages\n");
		db_keys[0] = &sc_exp_time;
		db_vals[0].type = DB1_INT;
		db_vals[0].nul = 0;
		db_vals[0].val.int_val = (int)time(NULL);
		if (msilo_dbf.delete(db_con, db_keys, db_ops, db_vals, 1) < 0) 
			LM_DBG("ERROR cleaning expired messages\n");
	}
}


/**
 * destroy function
 */
static void destroy(void)
{
	msg_list_free(ml);

	if(db_con && msilo_dbf.close)
		msilo_dbf.close(db_con);
}

/** 
 * TM callback function - delete message from database if was sent OK
 */
void m_tm_callback( struct cell *t, int type, struct tmcb_params *ps)
{
	if(ps->param==NULL || *ps->param==0)
	{
		LM_DBG("message id not received\n");
		goto done;
	}
	
	LM_DBG("completed with status %d [mid: %ld/%d]\n",
		ps->code, (long)ps->param, *((int*)ps->param));
	if(!db_con)
	{
		LM_ERR("db_con is NULL\n");
		goto done;
	}
	if(ps->code >= 300)
	{
		LM_DBG("message <%d> was not sent successfully\n", *((int*)ps->param));
		msg_list_set_flag(ml, *((int*)ps->param), MS_MSG_ERRO);
		goto done;
	}

	LM_DBG("message <%d> was sent successfully\n", *((int*)ps->param));
	msg_list_set_flag(ml, *((int*)ps->param), MS_MSG_DONE);

done:
	return;
}

void m_send_ontimer(unsigned int ticks, void *param)
{
	db_key_t db_keys[2];
	db_op_t  db_ops[2];
	db_val_t db_vals[2];
	db_key_t db_cols[6];
	db1_res_t* db_res = NULL;
	int i, db_no_cols = 6, db_no_keys = 2, mid, n;
	static char hdr_buf[1024];
	static char uri_buf[1024];
	static char body_buf[1024];
	str puri;
	time_t ttime;
	uac_req_t uac_r;
	str str_vals[4], hdr_str, body_str;
	str extra_hdrs_str = {0};
	time_t stime;

	if(ms_reminder.s==NULL)
	{
		LM_WARN("reminder address null\n");
		return;
	}
	
	/* init */
	db_keys[0]=&sc_snd_time;
	db_keys[1]=&sc_snd_time;
	db_ops[0]=OP_NEQ;
	db_ops[1]=OP_LEQ;

	db_cols[0]=&sc_mid;
	db_cols[1]=&sc_uri_user;
	db_cols[2]=&sc_uri_host;
	db_cols[3]=&sc_body;
	db_cols[4]=&sc_ctype;
	db_cols[5]=&sc_snd_time;

	
	LM_DBG("------------ start ------------\n");
	hdr_str.s=hdr_buf;
	hdr_str.len=1024;
	body_str.s=body_buf;
	body_str.len=1024;
	
	db_vals[0].type = DB1_INT;
	db_vals[0].nul = 0;
	db_vals[0].val.int_val = 0;
	
	db_vals[1].type = DB1_INT;
	db_vals[1].nul = 0;
	ttime = time(NULL);
	db_vals[1].val.int_val = (int)ttime;
	
	if (msilo_dbf.use_table(db_con, &ms_db_table) < 0)
	{
		LM_ERR("failed to use_table\n");
		return;
	}

	if (msilo_dbf.query(db_con,db_keys,db_ops,db_vals,db_cols,db_no_keys,
			    db_no_cols, NULL,&db_res) < 0) {
	    LM_ERR("failed to query database\n");
	    goto done;
	}

	if (RES_ROW_N(db_res) <= 0)
	{
		LM_DBG("no message for <%.*s>!\n", 24, ctime((const time_t*)&ttime));
		goto done;
	}
		
	LM_DBG("dumping [%d] messages for <%.*s>!!!\n", RES_ROW_N(db_res), 24,
			ctime((const time_t*)&ttime));

	for(i = 0; i < RES_ROW_N(db_res); i++) 
	{
		mid =  RES_ROWS(db_res)[i].values[0].val.int_val;
		if(msg_list_check_msg(ml, mid))
		{
			LM_DBG("message[%d] mid=%d already sent.\n", i, mid);
			continue;
		}
		
		memset(str_vals, 0, 4*sizeof(str));
		SET_STR_VAL(str_vals[0], db_res, i, 1); /* user */
		SET_STR_VAL(str_vals[1], db_res, i, 2); /* host */
		SET_STR_VAL(str_vals[2], db_res, i, 3); /* body */
		SET_STR_VAL(str_vals[3], db_res, i, 4); /* ctype */

		extra_hdrs_str.len = 0;
		hdr_str.len = 1024;
		if(m_build_headers(&hdr_str, str_vals[3] /*ctype*/,
				   ms_reminder/*from*/,0/*Date*/,
				   extra_hdrs_str/*extra*/)
		   < 0)
		{
			LM_ERR("headers building failed [%d]\n", mid);
			if (msilo_dbf.free_result(db_con, db_res) < 0)
				LM_DBG("failed to free result of query\n");
			msg_list_set_flag(ml, mid, MS_MSG_ERRO);
			return;
		}

		puri.s = uri_buf;
		puri.len = 4 + str_vals[0].len + 1 + str_vals[1].len;
		memcpy(puri.s, "sip:", 4);
		memcpy(puri.s+4, str_vals[0].s, str_vals[0].len);
		puri.s[4+str_vals[0].len] = '@';
		memcpy(puri.s+4+str_vals[0].len+1, str_vals[1].s, str_vals[1].len);
		
		LM_DBG("msg [%d-%d] for: %.*s\n", i+1, mid,	puri.len, puri.s);
			
		/** sending using TM function: t_uac */
		body_str.len = 1024;
		stime = 
			(time_t)RES_ROWS(db_res)[i].values[5/*snd time*/].val.int_val;
		n = m_build_body(&body_str, 0, str_vals[2/*body*/], stime);
		if(n<0)
			LM_DBG("sending simple body\n");
		else
			LM_DBG("sending composed body\n");
		
		msg_list_set_flag(ml, mid, MS_MSG_TSND);


		memset(&uac_r, '\0', sizeof(uac_r));
		uac_r.method  = &msg_type;
		uac_r.headers = &hdr_str;
		uac_r.body = (n<0)?&str_vals[2]:&body_str;
		uac_r.cb_flags = TMCB_LOCAL_COMPLETED;
		uac_r.cb  = m_tm_callback;
		uac_r.cbp = (void*)(long)mid;
		tmb.t_request(&uac_r,  /* UAC Req */
					  &puri,         /* Request-URI */
					  &puri,         /* To */
					  &ms_reminder,  /* From */
					  (ms_outbound_proxy.s)?&ms_outbound_proxy:0 /* ob uri */
			);
	}

done:
	/**
	 * Free the result because we don't need it anymore
	 */
	if ((db_res != NULL) && msilo_dbf.free_result(db_con, db_res) < 0)
		LM_DBG("failed to free result of query\n");

	return;
}

int ms_reset_stime(int mid)
{
	db_key_t db_keys[1];
	db_op_t  db_ops[1];
	db_val_t db_vals[1];
	db_key_t db_cols[1];
	db_val_t db_cvals[1];
	
	db_keys[0]=&sc_mid;
	db_ops[0]=OP_EQ;

	db_vals[0].type = DB1_INT;
	db_vals[0].nul = 0;
	db_vals[0].val.int_val = mid;
	

	db_cols[0]=&sc_snd_time;
	db_cvals[0].type = DB1_INT;
	db_cvals[0].nul = 0;
	db_cvals[0].val.int_val = 0;
	
	LM_DBG("updating send time for [%d]!\n", mid);
	
	if (msilo_dbf.use_table(db_con, &ms_db_table) < 0)
	{
		LM_ERR("failed to use_table\n");
		return -1;
	}

	if(msilo_dbf.update(db_con,db_keys,db_ops,db_vals,db_cols,db_cvals,1,1)!=0)
	{
		LM_ERR("failed to make update for [%d]!\n",	mid);
		return -1;
	}
	return 0;
}

/*
 * Check if REGISTER request has contacts that support MESSAGE method or
 * if MESSAGE method is listed in Allow header and contact does not have 
 * methods parameter.
 */
int check_message_support(struct sip_msg* msg)
{
	contact_t* c;
	unsigned int allow_message = 0;
	unsigned int allow_hdr = 0;
	str *methods_body;
	unsigned int methods;
	int expires;
	int posexp = 0;

	/* Parse all headers in order to see all Allow headers */
	if (parse_headers(msg, HDR_EOH_F, 0) == -1)
	{
		LM_ERR("failed to parse headers\n");
		return -1;
	}

	if (parse_allow(msg) == 0)
	{
		allow_hdr = 1;
		allow_message = get_allow_methods(msg) & METHOD_MESSAGE;
	}
	LM_DBG("Allow message: %u\n", allow_message);

	if (!msg->contact)
	{
		LM_DBG("no Contact found\n");
		return -1;
	}
	if (parse_contact(msg->contact) < 0)
	{
		LM_ERR("failed to parse Contact HF\n");
		return -1;
	}
	if (((contact_body_t*)msg->contact->parsed)->star)
	{
		LM_DBG("* Contact found\n");
		return -1;
	}

	if (contact_iterator(&c, msg, 0) < 0)
		return -1;

	/* 
	 * Check contacts for MESSAGE method in methods parameter list
	 * If contact does not have methods parameter, use Allow header methods,
	 * if any.  Stop if MESSAGE method is found.
	 */
	while(c)
	{
		/* calculate expires */
		expires=1; /* 0 is explicitely set in hdr or param */
		if(c->expires==NULL || c->expires->body.len<=0)
		{
			if(msg->expires!=NULL && msg->expires->body.len>0)
				expires = atoi(msg->expires->body.s);
		} else {
			str2int(&c->expires->body, (unsigned int*)(&expires));
		}
		/* skip contacts with zero expires */
		if (expires > 0)
		{
			posexp = 1;
			if (c->methods)
			{
				methods_body = &(c->methods->body);
				if (parse_methods(methods_body, &methods) < 0)
				{
					LM_ERR("failed to parse contact methods\n");
					return -1;
				}
				if (methods & METHOD_MESSAGE)
				{
					LM_DBG("MESSAGE contact found\n");
					return 0;
				}
			} else {
				if (allow_message)
				{
					LM_DBG("MESSAGE found in Allow Header\n");
					return 0;
				}
			}
		}
		if (contact_iterator(&c, msg, c) < 0)
		{
			LM_DBG("MESSAGE contact not found\n");
			return -1;
		}
	}

	/* no positivie expires header */
	if(posexp==0)
		return -1;

	/* no Allow header and no methods in Contact => dump MESSAGEs */
	if(allow_hdr==0)
		return 0;
	return -1;
}

