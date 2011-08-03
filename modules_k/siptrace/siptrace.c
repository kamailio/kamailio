/* 
 * $Id$ 
 *
 * siptrace module - helper module to trace sip messages
 *
 * Copyright (C) 2006 Voice Sistem S.R.L.
 * Copyright (C) 2009 Daniel-Constantin Mierla (asipto.com)
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

/*! \file
 * siptrace module - helper module to trace sip messages
 *
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "../../sr_module.h"
#include "../../dprint.h"
#include "../../ut.h"
#include "../../ip_addr.h"
#include "../../mem/mem.h"
#include "../../mem/shm_mem.h"
#include "../../lib/kmi/mi.h"
#include "../../lib/srdb1/db.h"
#include "../../parser/parse_content.h"
#include "../../parser/parse_from.h"
#include "../../pvar.h"
#include "../../modules/tm/tm_load.h"
#include "../../modules/sl/sl.h"
#include "../../str.h"
#include "../../onsend.h"

#ifdef STATISTICS
#include "../../lib/kcore/statistics.h"
#endif

MODULE_VERSION

struct _siptrace_data {
	struct usr_avp *avp;
	int_str avp_value;
	struct search_state state;
	str body;
	str callid;
	str method;
	str status;
	char *dir;
	str fromtag;
	str fromip;
	str toip;
	char toip_buff[IP_ADDR_MAX_STR_SIZE+12];
	char fromip_buff[IP_ADDR_MAX_STR_SIZE+12];
#ifdef STATISTICS
	stat_var *stat;
#endif
};

struct tm_binds tmb;

/** SL API structure */
sl_api_t slb;

/* module function prototypes */
static int mod_init(void);
static int child_init(int rank);
static void destroy(void);
static int sip_trace(struct sip_msg*, char*, char*);

static int sip_trace_store_db(struct _siptrace_data* sto);
static int trace_send_duplicate(char *buf, int len);

static void trace_onreq_in(struct cell* t, int type, struct tmcb_params *ps);
static void trace_onreq_out(struct cell* t, int type, struct tmcb_params *ps);
static void trace_onreply_in(struct cell* t, int type, struct tmcb_params *ps);
static void trace_onreply_out(struct cell* t, int type, struct tmcb_params *ps);
static void trace_sl_onreply_out(sl_cbp_t *slcb);
static void trace_sl_ack_in(sl_cbp_t *slcb);

static struct mi_root* sip_trace_mi(struct mi_root* cmd, void* param );

static str db_url             = str_init(DEFAULT_RODB_URL);
static str siptrace_table     = str_init("sip_trace");
static str date_column        = str_init("time_stamp");  /* 00 */
static str callid_column      = str_init("callid");      /* 01 */
static str traced_user_column = str_init("traced_user"); /* 02 */
static str msg_column         = str_init("msg");         /* 03 */
static str method_column      = str_init("method");      /* 04 */
static str status_column      = str_init("status");      /* 05 */
static str fromip_column      = str_init("fromip");      /* 06 */
static str toip_column        = str_init("toip");        /* 07 */
static str fromtag_column     = str_init("fromtag");     /* 08 */
static str direction_column   = str_init("direction");   /* 09 */

#define NR_KEYS 10

int trace_flag = -1;
int trace_on   = 0;
int trace_sl_acks = 1;

str    dup_uri_str      = {0, 0};
struct sip_uri *dup_uri = 0;

int *trace_on_flag = NULL;

static unsigned short traced_user_avp_type = 0;
static int_str traced_user_avp;
static str traced_user_avp_str = {NULL, 0};

static unsigned short trace_table_avp_type = 0;
static int_str trace_table_avp;
static str trace_table_avp_str = {NULL, 0};

static str trace_local_ip = {NULL, 0};

db1_con_t *db_con = NULL; 		/*!< database connection */
db_func_t db_funcs;      		/*!< Database functions */

/*! \brief
 * Exported functions
 */
static cmd_export_t cmds[] = {
	{"sip_trace", (cmd_function)sip_trace, 0, 0, 0, ANY_ROUTE},
	{"sip_trace", (cmd_function)sip_trace, 1, 0, 0, ANY_ROUTE},
	{0, 0, 0, 0, 0, 0}
};


/*! \brief
 * Exported parameters
 */
static param_export_t params[] = {
	{"db_url",             STR_PARAM, &db_url.s             },
	{"table",              STR_PARAM, &siptrace_table.s     },
	{"date_column",        STR_PARAM, &date_column.s        },
	{"callid_column",      STR_PARAM, &callid_column.s      },
	{"traced_user_column", STR_PARAM, &traced_user_column.s },
	{"msg_column",         STR_PARAM, &msg_column.s         },
	{"method_column",      STR_PARAM, &method_column.s      },
	{"status_column",      STR_PARAM, &status_column.s      },
	{"fromip_column",      STR_PARAM, &fromip_column.s      },
	{"toip_column",        STR_PARAM, &toip_column.s        },
	{"fromtag_column",     STR_PARAM, &fromtag_column.s     },
	{"direction_column",   STR_PARAM, &direction_column.s   },
	{"trace_flag",         INT_PARAM, &trace_flag           },
	{"trace_on",           INT_PARAM, &trace_on             },
	{"traced_user_avp",    STR_PARAM, &traced_user_avp_str.s},
	{"trace_table_avp",    STR_PARAM, &trace_table_avp_str.s},
	{"duplicate_uri",      STR_PARAM, &dup_uri_str.s        },
	{"trace_local_ip",     STR_PARAM, &trace_local_ip.s     },
	{"trace_sl_acks",      INT_PARAM, &trace_sl_acks        },
	{0, 0, 0}
};

/*! \brief
 * MI commands
 */
static mi_export_t mi_cmds[] = {
	{ "sip_trace", sip_trace_mi,   0,  0,  0 },
	{ 0, 0, 0, 0, 0}
};


#ifdef STATISTICS
stat_var* siptrace_req;
stat_var* siptrace_rpl;

stat_export_t siptrace_stats[] = {
	{"traced_requests" ,  0,  &siptrace_req  },
	{"traced_replies"  ,  0,  &siptrace_rpl  },
	{0,0,0}
};
#endif

/*! \brief module exports */
struct module_exports exports = {
	"siptrace", 
	DEFAULT_DLFLAGS, /*!< dlopen flags */
	cmds,       /*!< Exported functions */
	params,     /*!< Exported parameters */
#ifdef STATISTICS
	siptrace_stats,  /*!< exported statistics */
#else
	0,          /*!< exported statistics */
#endif
	mi_cmds,    /*!< exported MI functions */
	0,          /*!< exported pseudo-variables */
	0,          /*!< extra processes */
	mod_init,   /*!< module initialization function */
	0,          /*!< response function */
	destroy,    /*!< destroy function */
	child_init  /*!< child initialization function */
};


/*! \brief Initialize siptrace module */
static int mod_init(void)
{
	pv_spec_t avp_spec;
	sl_cbelem_t slcb;

#ifdef STATISTICS
	/* register statistics */
	if (register_module_stats(exports.name, siptrace_stats)!=0)
	{
		LM_ERR("failed to register core statistics\n");
		return -1;
	}
#endif

	if(register_mi_mod(exports.name, mi_cmds)!=0)
	{
		LM_ERR("failed to register MI commands\n");
		return -1;
	}

	db_url.len = strlen(db_url.s);
	siptrace_table.len = strlen(siptrace_table.s);
	date_column.len = strlen(date_column.s);
	callid_column.len = strlen(callid_column.s);
	traced_user_column.len = strlen(traced_user_column.s);
	msg_column.len = strlen(msg_column.s);
	method_column.len = strlen(method_column.s);
	status_column.len = strlen(status_column.s);
	fromip_column.len = strlen(fromip_column.s);
	toip_column.len = strlen(toip_column.s);
	fromtag_column.len = strlen(fromtag_column.s);
	direction_column.len = strlen(direction_column.s);
	if (traced_user_avp_str.s)
		traced_user_avp_str.len = strlen(traced_user_avp_str.s);
	if (trace_table_avp_str.s)
		trace_table_avp_str.len = strlen(trace_table_avp_str.s);
	if (dup_uri_str.s)
		dup_uri_str.len = strlen(dup_uri_str.s);
	if (trace_local_ip.s)
		trace_local_ip.len = strlen(trace_local_ip.s);

	if (trace_flag<0 || trace_flag>(int)MAX_FLAG)
	{
		LM_ERR("invalid trace flag %d\n", trace_flag);
		return -1;
	}
	trace_flag = 1<<trace_flag;

	/* Find a database module */
	if (db_bind_mod(&db_url, &db_funcs))
	{
		LM_ERR("unable to bind database module\n");
		return -1;
	}
	if (!DB_CAPABILITY(db_funcs, DB_CAP_INSERT))
	{
		LM_ERR("database modules does not provide all functions needed"
				" by module\n");
		return -1;
	}

	trace_on_flag = (int*)shm_malloc(sizeof(int));
	if(trace_on_flag==NULL) {
		LM_ERR("no more shm memory left\n");
		return -1;
	}
	
	*trace_on_flag = trace_on;
	
	/* register callbacks to TM */
	if (load_tm_api(&tmb)!=0)
	{
		LM_ERR("can't load tm api. Is module tm loaded?\n");
		return -1;
	}

	if(tmb.register_tmcb( 0, 0, TMCB_REQUEST_IN, trace_onreq_in, 0, 0) <=0)
	{
		LM_ERR("can't register trace_onreq_in\n");
		return -1;
	}

	/* bind the SL API */
	if (sl_load_api(&slb)!=0) {
		LM_ERR("cannot bind to SL API\n");
		return -1;
	}

	/* register sl callbacks */
	memset(&slcb, 0, sizeof(sl_cbelem_t));

	slcb.type = SLCB_REPLY_READY;
	slcb.cbf  = trace_sl_onreply_out;
    if (slb.register_cb(&slcb) != 0) {
        LM_ERR("can't register for SLCB_REPLY_READY\n");
        return -1;
    }

	if(trace_sl_acks)
	{
		slcb.type = SLCB_ACK_FILTERED;
		slcb.cbf  = trace_sl_ack_in;
		if (slb.register_cb(&slcb) != 0) {
			LM_ERR("can't register for SLCB_ACK_FILTERED\n");
			return -1;
		}
	}

	if(dup_uri_str.s!=0)
	{
		dup_uri_str.len = strlen(dup_uri_str.s);
		dup_uri = (struct sip_uri *)pkg_malloc(sizeof(struct sip_uri));
		if(dup_uri==0)
		{
			LM_ERR("no more pkg memory left\n");
			return -1;
		}
		memset(dup_uri, 0, sizeof(struct sip_uri));
		if(parse_uri(dup_uri_str.s, dup_uri_str.len, dup_uri)<0)
		{
			LM_ERR("bad dup uri\n");
			return -1;
		}
	}

	if(traced_user_avp_str.s && traced_user_avp_str.len > 0)
	{
		if (pv_parse_spec(&traced_user_avp_str, &avp_spec)==0
				|| avp_spec.type!=PVT_AVP)
		{
			LM_ERR("malformed or non AVP %.*s AVP definition\n",
					traced_user_avp_str.len, traced_user_avp_str.s);
			return -1;
		}

		if(pv_get_avp_name(0, &avp_spec.pvp, &traced_user_avp,
					&traced_user_avp_type)!=0)
		{
			LM_ERR("[%.*s] - invalid AVP definition\n",
					traced_user_avp_str.len, traced_user_avp_str.s);
			return -1;
		}
	} else {
		traced_user_avp.n = 0;
		traced_user_avp_type = 0;
	}
	if(trace_table_avp_str.s && trace_table_avp_str.len > 0)
	{
		if (pv_parse_spec(&trace_table_avp_str, &avp_spec)==0
				|| avp_spec.type!=PVT_AVP)
		{
			LM_ERR("malformed or non AVP %.*s AVP definition\n",
					trace_table_avp_str.len, trace_table_avp_str.s);
			return -1;
		}

		if(pv_get_avp_name(0, &avp_spec.pvp, &trace_table_avp,
					&trace_table_avp_type)!=0)
		{
			LM_ERR("[%.*s] - invalid AVP definition\n",
					trace_table_avp_str.len, trace_table_avp_str.s);
			return -1;
		}
	} else {
		trace_table_avp.n = 0;
		trace_table_avp_type = 0;
	}

	return 0;
}


static int child_init(int rank)
{
	if (rank==PROC_INIT || rank==PROC_MAIN || rank==PROC_TCP_MAIN)
		return 0; /* do nothing for the main process */

	db_con = db_funcs.init(&db_url);
	if (!db_con)
	{
		LM_ERR("unable to connect to database. Please check configuration.\n");
		return -1;
	}

	return 0;
}


static void destroy(void)
{
	if (db_con!=NULL)
		db_funcs.close(db_con);
	if (trace_on_flag)
		shm_free(trace_on_flag);
}

static inline int siptrace_copy_proto(int proto, char *buf)
{
	if(buf==0)
		return -1;
	if(proto==PROTO_TCP) {
		strcpy(buf, "tcp:");
	} else if(proto==PROTO_TLS) {
		strcpy(buf, "tls:");
	} else if(proto==PROTO_SCTP) {
		strcpy(buf, "sctp:");
	} else {
		strcpy(buf, "udp:");
	}
	return 0;
}

static inline str* siptrace_get_table(void)
{
	static int_str         avp_value;
	struct usr_avp *avp;

	if(trace_table_avp.n==0)
		return &siptrace_table;

	avp = NULL;
	if(trace_table_avp.n!=0)
		avp=search_first_avp(trace_table_avp_type, trace_table_avp, &avp_value,
				0);

	if(avp==NULL || !is_avp_str_val(avp) || avp_value.s.len<=0)
		return &siptrace_table;

	return &avp_value.s;
}

static int sip_trace_prepare(sip_msg_t *msg)
{
	if(parse_from_header(msg)==-1 || msg->from==NULL || get_from(msg)==NULL) {
		LM_ERR("cannot parse FROM header\n");
		goto error;
	}
	
	if(parse_headers(msg, HDR_CALLID_F, 0)!=0 || msg->callid==NULL
			|| msg->callid->body.s==NULL) {
		LM_ERR("cannot parse call-id\n");
		goto error;
	}

	return 0;
error:
	return -1;
}

static int sip_trace_store(struct _siptrace_data *sto)
{
	if(sto==NULL)
	{
		LM_DBG("invalid parameter\n");
		return -1;
	}
	
	trace_send_duplicate(sto->body.s, sto->body.len);
	return sip_trace_store_db(sto);
}

static int sip_trace_store_db(struct _siptrace_data *sto)
{
	db_key_t db_keys[NR_KEYS];
	db_val_t db_vals[NR_KEYS];

	db_keys[0] = &msg_column;
	db_vals[0].type = DB1_BLOB;
	db_vals[0].nul = 0;
	db_vals[0].val.blob_val = sto->body;
	
	db_keys[1] = &callid_column;
	db_vals[1].type = DB1_STR;
	db_vals[1].nul = 0;
	db_vals[1].val.str_val = sto->callid;
	
	db_keys[2] = &method_column;
	db_vals[2].type = DB1_STR;
	db_vals[2].nul = 0;
	db_vals[2].val.str_val = sto->method;

	db_keys[3] = &status_column;
	db_vals[3].type = DB1_STR;
	db_vals[3].nul = 0;
	db_vals[3].val.str_val = sto->status;
		
	db_keys[4] = &fromip_column;
	db_vals[4].type = DB1_STR;
	db_vals[4].nul = 0;
	db_vals[4].val.str_val = sto->fromip;
	
	db_keys[5] = &toip_column;
	db_vals[5].type = DB1_STR;
	db_vals[5].nul = 0;
	db_vals[5].val.str_val = sto->toip;
	
	db_keys[6] = &date_column;
	db_vals[6].type = DB1_DATETIME;
	db_vals[6].nul = 0;
	db_vals[6].val.time_val = time(NULL);
	
	db_keys[7] = &direction_column;
	db_vals[7].type = DB1_STRING;
	db_vals[7].nul = 0;
	db_vals[7].val.string_val = sto->dir;
	
	db_keys[8] = &fromtag_column;
	db_vals[8].type = DB1_STR;
	db_vals[8].nul = 0;
	db_vals[8].val.str_val = sto->fromtag;
	
	db_funcs.use_table(db_con, siptrace_get_table());
	
	db_keys[9] = &traced_user_column;
	db_vals[9].type = DB1_STR;
	db_vals[9].nul = 0;

	if(trace_on_flag!=NULL && *trace_on_flag!=0) {
		db_vals[9].val.str_val.s   = "";
		db_vals[9].val.str_val.len = 0;
	
		LM_DBG("storing info...\n");
		if(db_funcs.insert(db_con, db_keys, db_vals, NR_KEYS) < 0) {
			LM_ERR("error storing trace\n");
			goto error;
		}
#ifdef STATISTICS
		update_stat(sto->stat, 1);
#endif
	}
	
	if(sto->avp==NULL)
		goto done;
	
	db_vals[9].val.str_val = sto->avp_value.s;

	LM_DBG("storing info...\n");
	if(db_funcs.insert(db_con, db_keys, db_vals, NR_KEYS) < 0) {
		LM_ERR("error storing trace\n");
		goto error;
	}

	sto->avp = search_next_avp(&sto->state, &sto->avp_value);
	while(sto->avp!=NULL) {
		db_vals[9].val.str_val = sto->avp_value.s;

		LM_DBG("storing info...\n");
		if(db_funcs.insert(db_con, db_keys, db_vals, NR_KEYS) < 0) {
			LM_ERR("error storing trace\n");
			goto error;
		}
		sto->avp = search_next_avp(&sto->state, &sto->avp_value);
	}

done:
	return 1;
error:
	return -1;
}

static int sip_trace(struct sip_msg *msg, char *dir, char *s2)
{
	struct _siptrace_data sto;

	if(msg==NULL) {
		LM_DBG("nothing to trace\n");
		return -1;
	}
	memset(&sto, 0, sizeof(struct _siptrace_data));

	if(traced_user_avp.n!=0)
		sto.avp=search_first_avp(traced_user_avp_type, traced_user_avp,
				&sto.avp_value, &sto.state);

	if((sto.avp==NULL) && (trace_on_flag==NULL || *trace_on_flag==0)) {
		LM_DBG("trace off...\n");
		return -1;
	}
	if(sip_trace_prepare(msg)<0)
		return -1;

	sto.body.s = msg->buf;
	sto.body.len = msg->len;
	sto.callid = msg->callid->body;

	if(msg->first_line.type==SIP_REQUEST) {
		sto.method = msg->first_line.u.request.method;
	} else {
		sto.method.s = "";
		sto.method.len = 0;
	}

	if(msg->first_line.type==SIP_REPLY) {
		sto.status = msg->first_line.u.reply.status;
	} else {
		sto.status.s = "";
		sto.status.len = 0;
	}

	siptrace_copy_proto(msg->rcv.proto, sto.fromip_buff);
	strcat(sto.fromip_buff, ip_addr2a(&msg->rcv.src_ip));
	strcat(sto.fromip_buff,":");
	strcat(sto.fromip_buff, int2str(msg->rcv.src_port, NULL));
	sto.fromip.s = sto.fromip_buff;
	sto.fromip.len = strlen(sto.fromip_buff);

	siptrace_copy_proto(msg->rcv.proto, sto.toip_buff);
	strcat(sto.toip_buff, ip_addr2a(&msg->rcv.dst_ip));
	strcat(sto.toip_buff,":");
	strcat(sto.toip_buff, int2str(msg->rcv.dst_port, NULL));
	sto.toip.s = sto.toip_buff;
	sto.toip.len = strlen(sto.toip_buff);

	sto.dir = (dir)?dir:"in";

	sto.fromtag = get_from(msg)->tag_value;

#ifdef STATISTICS
	if(msg->first_line.type==SIP_REPLY) {
		sto.stat = siptrace_rpl;
	} else {
		sto.stat = siptrace_req;
	}
#endif
	return sip_trace_store(&sto);
}

#define trace_is_off(_msg) \
	(trace_on_flag==NULL || *trace_on_flag==0 || \
		((_msg)->flags&trace_flag)==0)

static void trace_onreq_in(struct cell* t, int type, struct tmcb_params *ps)
{
	struct sip_msg* msg;
	int_str         avp_value;
	struct usr_avp* avp;

	if(t==NULL || ps==NULL)
	{
		LM_DBG("no uas request, local transaction\n");
		return;
	}
	
	msg = ps->req;
	if(msg==NULL)
	{
		LM_DBG("no uas request, local transaction\n");
		return;
	}
	
	avp = NULL;
	if(traced_user_avp.n!=0)
		avp=search_first_avp(traced_user_avp_type, traced_user_avp, &avp_value,
				0);

	if((avp==NULL) && trace_is_off(msg))
	{
		LM_DBG("trace off...\n");
		return;
	}
	
	if(parse_from_header(msg)==-1 || msg->from==NULL || get_from(msg)==NULL)
	{
		LM_ERR("cannot parse FROM header\n");
		return;
	}

	if(parse_headers(msg, HDR_CALLID_F, 0)!=0)
	{
		LM_ERR("cannot parse call-id\n");
		return;
	}

	if(tmb.register_tmcb(0, t, TMCB_REQUEST_SENT, trace_onreq_out, 0, 0) <=0)
	{
		LM_ERR("can't register trace_onreq_out\n");
		return;
	}
	if(tmb.register_tmcb(0, t, TMCB_RESPONSE_IN, trace_onreply_in, 0, 0) <=0)
	{
		LM_ERR("can't register trace_onreply_in\n");
		return;
	}

	if(tmb.register_tmcb(0, t, TMCB_RESPONSE_SENT, trace_onreply_out, 0, 0)<=0)
	{
		LM_ERR("can't register trace_onreply_out\n");
		return;
	}
}

static void trace_onreq_out(struct cell* t, int type, struct tmcb_params *ps)
{
	struct _siptrace_data sto;
	sip_msg_t *msg;
	struct ip_addr to_ip;
	int len;
	struct dest_info *dst;
	
	if(t==NULL || ps==NULL) {
		LM_DBG("very weird\n");
		return;
	}

	if(ps->flags&TMCB_RETR_F) {
		LM_DBG("retransmission\n");
		return;
	}
	msg=ps->req;
	if(msg==NULL) {
		LM_DBG("no uas msg, local transaction\n");
		return;
	}
	memset(&sto, 0, sizeof(struct _siptrace_data));

	if(traced_user_avp.n!=0)
		sto.avp=search_first_avp(traced_user_avp_type, traced_user_avp,
				&sto.avp_value, &sto.state);

	if((sto.avp==NULL) && trace_is_off(msg) ) {
		LM_DBG("trace off...\n");
		return;
	}

	if(sip_trace_prepare(msg)<0)
		return;

	if(ps->send_buf.len>0) {
		sto.body = ps->send_buf;
	} else {
		sto.body.s   = "No request buffer";
		sto.body.len = sizeof("No request buffer")-1;
	}
	
	sto.callid = msg->callid->body;

	if(ps->send_buf.len>10) {
		sto.method.s = ps->send_buf.s;
		sto.method.len = 0;
		while(sto.method.len<ps->send_buf.len) {
			if(ps->send_buf.s[sto.method.len]==' ')
				break;
			sto.method.len++;
		}
		if(sto.method.len==ps->send_buf.len)
			sto.method.len = 10;
	} else {
		sto.method = t->method;
	}

	sto.status.s = "";
	sto.status.len = 0;
		
	memset(&to_ip, 0, sizeof(struct ip_addr));
	dst = ps->dst;

	if (trace_local_ip.s && trace_local_ip.len > 0) {
		sto.fromip = trace_local_ip;
	} else {
		if(dst==0 || dst->send_sock==0 || dst->send_sock->sock_str.s==0) {
			siptrace_copy_proto(msg->rcv.proto, sto.fromip_buff);
			strcat(sto.fromip_buff, ip_addr2a(&msg->rcv.dst_ip));
			strcat(sto.fromip_buff,":");
			strcat(sto.fromip_buff, int2str(msg->rcv.dst_port, NULL));
			sto.fromip.s = sto.fromip_buff;
			sto.fromip.len = strlen(sto.fromip_buff);
		} else {
			sto.fromip = dst->send_sock->sock_str;
		}
	}
	
	if(dst==0) {
		sto.toip.s = "any:255.255.255.255";
		sto.toip.len = 19;
	} else {
		su2ip_addr(&to_ip, &dst->to);
		siptrace_copy_proto(dst->proto, sto.toip_buff);
		strcat(sto.toip_buff, ip_addr2a(&to_ip));
		strcat(sto.toip_buff, ":");
		strcat(sto.toip_buff,
				int2str((unsigned long)su_getport(&dst->to), &len));
		sto.toip.s = sto.toip_buff;
		sto.toip.len = strlen(sto.toip_buff);
	}
	
	sto.dir = "out";
	
	sto.fromtag = get_from(msg)->tag_value;
	
#ifdef STATISTICS
	sto.stat = siptrace_req;
#endif
	
	sip_trace_store(&sto);
	return;
}

static void trace_onreply_in(struct cell* t, int type, struct tmcb_params *ps)
{
	struct _siptrace_data sto;
	sip_msg_t *msg;
	sip_msg_t *req;
	char statusbuf[8];

	if(t==NULL || t->uas.request==0 || ps==NULL) {
		LM_DBG("no uas request, local transaction\n");
		return;
	}

	req = ps->req;
	msg = ps->rpl;
	if(msg==NULL || req==NULL) {
		LM_DBG("no reply\n");
		return;
	}
	memset(&sto, 0, sizeof(struct _siptrace_data));
	
	if(traced_user_avp.n!=0)
		sto.avp=search_first_avp(traced_user_avp_type, traced_user_avp,
				&sto.avp_value, &sto.state);

	if((sto.avp==NULL) &&  trace_is_off(req)) {
		LM_DBG("trace off...\n");
		return;
	}

	if(sip_trace_prepare(msg)<0)
		return;

	sto.body.s = msg->buf;
	sto.body.len = msg->len;

	sto.callid = msg->callid->body;
	
	sto.method = t->method;

	strcpy(statusbuf, int2str(ps->code, &sto.status.len));
	sto.status.s = statusbuf;

	siptrace_copy_proto(msg->rcv.proto, sto.fromip_buff);
	strcat(sto.fromip_buff, ip_addr2a(&msg->rcv.src_ip));
	strcat(sto.fromip_buff,":");
	strcat(sto.fromip_buff, int2str(msg->rcv.src_port, NULL));
	sto.fromip.s = sto.fromip_buff;
	sto.fromip.len = strlen(sto.fromip_buff);
	
	if(trace_local_ip.s && trace_local_ip.len > 0) {
		sto.toip = trace_local_ip;
	} else {
		siptrace_copy_proto(msg->rcv.proto, sto.toip_buff);
		strcat(sto.toip_buff, ip_addr2a(&msg->rcv.dst_ip));
		strcat(sto.toip_buff,":");
		strcat(sto.toip_buff, int2str(msg->rcv.dst_port, NULL));
		sto.toip.s = sto.toip_buff;
		sto.toip.len = strlen(sto.toip_buff);
	}
	
	sto.dir = "in";
	
	sto.fromtag = get_from(msg)->tag_value;
#ifdef STATISTICS
	sto.stat = siptrace_rpl;
#endif
	
	sip_trace_store(&sto);
	return;
}

static void trace_onreply_out(struct cell* t, int type, struct tmcb_params *ps)
{
	struct _siptrace_data sto;
	int faked = 0;
	struct sip_msg* msg;
	struct sip_msg* req;
	struct ip_addr to_ip;
	int len;
	char statusbuf[8];
	struct dest_info *dst;

	if (t==NULL || t->uas.request==0 || ps==NULL) {
		LM_DBG("no uas request, local transaction\n");
		return;
	}
	
	if(ps->flags&TMCB_RETR_F) {
		LM_DBG("retransmission\n");
		return;
	}
	memset(&sto, 0, sizeof(struct _siptrace_data));
	if(traced_user_avp.n!=0)
		sto.avp=search_first_avp(traced_user_avp_type, traced_user_avp,
				&sto.avp_value, &sto.state);

	if((sto.avp==NULL) &&  trace_is_off(t->uas.request)) {
		LM_DBG("trace off...\n");
		return;
	}
	
	req = ps->req;
	msg = ps->rpl;
	if(msg==NULL || msg==FAKED_REPLY) {
		msg = t->uas.request;
		faked = 1;
	}

	if(sip_trace_prepare(msg)<0)
		return;

	if(faked==0) {
		if(ps->send_buf.len>0) {
			sto.body = ps->send_buf;
		} else if(t->uas.response.buffer!=NULL) {
			sto.body.s = t->uas.response.buffer;
			sto.body.len = t->uas.response.buffer_len;
		} else if(msg->len>0) {
			sto.body.s = msg->buf;
			sto.body.len = msg->len;
		} else {
			sto.body.s = "No reply buffer";
			sto.body.len = sizeof("No reply buffer")-1;
		}
	} else {
		if(ps->send_buf.len>0) {
			sto.body = ps->send_buf;
		} else if(t->uas.response.buffer!=NULL) {
			sto.body.s = t->uas.response.buffer;
			sto.body.len = t->uas.response.buffer_len;
		} else {
			sto.body.s = "No reply buffer";
			sto.body.len = sizeof("No reply buffer")-1;
		}
	}
	
	sto.callid = msg->callid->body;
	sto.method = t->method;

	if(trace_local_ip.s && trace_local_ip.len > 0) {
		sto.fromip = trace_local_ip;
	} else {
		siptrace_copy_proto(msg->rcv.proto, sto.fromip_buff);
		strcat(sto.fromip_buff, ip_addr2a(&req->rcv.dst_ip));
		strcat(sto.fromip_buff,":");
		strcat(sto.fromip_buff, int2str(req->rcv.dst_port, NULL));
		sto.fromip.s = sto.fromip_buff;
		sto.fromip.len = strlen(sto.fromip_buff);
	}
	
	strcpy(statusbuf, int2str(ps->code, &sto.status.len));
	sto.status.s = statusbuf;
		
	memset(&to_ip, 0, sizeof(struct ip_addr));
	dst = ps->dst;
	if(dst==0) {
		sto.toip.s = "any:255.255.255.255";
		sto.toip.len = 19;
	} else {
		su2ip_addr(&to_ip, &dst->to);
		siptrace_copy_proto(dst->proto, sto.toip_buff);
		strcat(sto.toip_buff, ip_addr2a(&to_ip));
		strcat(sto.toip_buff, ":");
		strcat(sto.toip_buff,
				int2str((unsigned long)su_getport(&dst->to), &len));
		sto.toip.s = sto.toip_buff;
		sto.toip.len = strlen(sto.toip_buff);
	}
	
	sto.dir = "out";
	sto.fromtag = get_from(msg)->tag_value;
	
#ifdef STATISTICS
	sto.stat = siptrace_rpl;
#endif

	sip_trace_store(&sto);
	return;
}

static void trace_sl_ack_in(sl_cbp_t *slcbp)
{
	sip_msg_t *req;
	LM_DBG("storing ack...\n");
	req = slcbp->req;
	sip_trace(req, 0, 0);
}

static void trace_sl_onreply_out(sl_cbp_t *slcbp)
{
	sip_msg_t *req;
	struct _siptrace_data sto;
	int faked = 0;
	struct sip_msg* msg;
	struct ip_addr to_ip;
	int len;
	char statusbuf[5];

	if(slcbp==NULL || slcbp->req==NULL)
	{
		LM_ERR("bad parameters\n");
		return;
	}
	req = slcbp->req;
	
	memset(&sto, 0, sizeof(struct _siptrace_data));
	if(traced_user_avp.n!=0)
		sto.avp=search_first_avp(traced_user_avp_type, traced_user_avp,
				&sto.avp_value, &sto.state);

	if((sto.avp==NULL) && trace_is_off(req)) {
		LM_DBG("trace off...\n");
		return;
	}
	
	msg = req;
	faked = 1;

	if(sip_trace_prepare(msg)<0)
		return;
	
	sto.body.s = (slcbp->reply)?slcbp->reply->s:"";
	sto.body.len = (slcbp->reply)?slcbp->reply->len:0;
	
	sto.callid = msg->callid->body;
	sto.method = msg->first_line.u.request.method;
		
	if(trace_local_ip.len > 0) {
		sto.fromip = trace_local_ip;
	} else {
		siptrace_copy_proto(msg->rcv.proto, sto.fromip_buff);
		strcat(sto.fromip_buff, ip_addr2a(&req->rcv.dst_ip));
		strcat(sto.fromip_buff,":");
		strcat(sto.fromip_buff, int2str(req->rcv.dst_port, NULL));
		sto.fromip.s = sto.fromip_buff;
		sto.fromip.len = strlen(sto.fromip_buff);
	}

	strcpy(statusbuf, int2str(slcbp->code, &sto.status.len));
	sto.status.s = statusbuf;
		
	memset(&to_ip, 0, sizeof(struct ip_addr));
	if(slcbp->dst==0)
	{
		sto.toip.s = "any:255.255.255.255";
		sto.toip.len = 19;
	} else {
		su2ip_addr(&to_ip, &slcbp->dst->to);
		siptrace_copy_proto(req->rcv.proto, sto.toip_buff);
		strcat(sto.toip_buff, ip_addr2a(&to_ip));
		strcat(sto.toip_buff, ":");
		strcat(sto.toip_buff,
				int2str((unsigned long)su_getport(&slcbp->dst->to), &len));
		sto.toip.s = sto.toip_buff;
		sto.toip.len = strlen(sto.toip_buff);
	}
	
	sto.dir = "out";
	sto.fromtag = get_from(msg)->tag_value;
	
#ifdef STATISTICS
	sto.stat = siptrace_rpl;
#endif
	
	sip_trace_store(&sto);
	return;
}


/*! \brief
 * MI Sip_trace command
 *
 * MI command format:
 * name: sip_trace
 * attribute: name=none, value=[on|off]
 */
static struct mi_root* sip_trace_mi(struct mi_root* cmd_tree, void* param )
{
	struct mi_node* node;
	
	struct mi_node *rpl; 
	struct mi_root *rpl_tree ; 

	node = cmd_tree->node.kids;
	if(node == NULL) {
		rpl_tree = init_mi_tree( 200, MI_SSTR(MI_OK));
		if (rpl_tree == 0)
			return 0;
		rpl = &rpl_tree->node;

		if (*trace_on_flag == 0 ) {
			node = add_mi_node_child(rpl,0,0,0,MI_SSTR("off"));
		} else if (*trace_on_flag == 1) {
			node = add_mi_node_child(rpl,0,0,0,MI_SSTR("on"));
		}
		return rpl_tree ;
	}
	if(trace_on_flag==NULL)
		return init_mi_tree( 500, MI_SSTR(MI_INTERNAL_ERR));

	if ( node->value.len==2 && (node->value.s[0]=='o'
				|| node->value.s[0]=='O') &&
			(node->value.s[1]=='n'|| node->value.s[1]=='N')) {
		*trace_on_flag = 1;
		return init_mi_tree( 200, MI_SSTR(MI_OK));
	} else if ( node->value.len==3 && (node->value.s[0]=='o'
				|| node->value.s[0]=='O')
			&& (node->value.s[1]=='f'|| node->value.s[1]=='F')
			&& (node->value.s[2]=='f'|| node->value.s[2]=='F')) {
		*trace_on_flag = 0;
		return init_mi_tree( 200, MI_SSTR(MI_OK));
	} else {
		return init_mi_tree( 400, MI_SSTR(MI_BAD_PARM));
	}
}

static int trace_send_duplicate(char *buf, int len)
{
	struct dest_info dst;
	struct proxy_l * p;
	
	if(buf==NULL || len <= 0)
		return -1;
	
	if(dup_uri_str.s==0 || dup_uri==NULL)
		return 0;
	
	init_dest_info(&dst);
	/* create a temporary proxy*/
	dst.proto = PROTO_UDP;
	p=mk_proxy(&dup_uri->host, (dup_uri->port_no)?dup_uri->port_no:SIP_PORT,
			dst.proto);
	if (p==0)
	{
		LM_ERR("bad host name in uri\n");
		return -1;
	}
	
	hostent2su(&dst.to, &p->host, p->addr_idx, (p->port)?p->port:SIP_PORT);
	
	dst.send_sock=get_send_socket(0, &dst.to, dst.proto);
	if (dst.send_sock==0)
	{
		LM_ERR("can't forward to af %d, proto %d no corresponding"
				" listening socket\n", dst.to.s.sa_family, dst.proto);
		goto error;
	}

	if (msg_send(&dst, buf, len)<0)
	{
		LM_ERR("cannot send duplicate message\n");
		goto error;
	}
	
	free_proxy(p); /* frees only p content, not p itself */
	pkg_free(p);
	return 0;
error:
	free_proxy(p); /* frees only p content, not p itself */
	pkg_free(p);
	return -1;
}
