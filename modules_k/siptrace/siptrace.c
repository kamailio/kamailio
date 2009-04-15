/* 
 * $Id$ 
 *
 * siptrace module - helper module to trace sip messages
 *
 * Copyright (C) 2006 Voice Sistem S.R.L.
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
#include "../sl/sl_cb.h"
#include "../../str.h"

MODULE_VERSION

struct tm_binds tmb;

/* module function prototypes */
static int mod_init(void);
static int child_init(int rank);
static void destroy(void);
static int sip_trace(struct sip_msg*, char*, char*);

static int trace_send_duplicate(char *buf, int len);

static void trace_onreq_in(struct cell* t, int type, struct tmcb_params *ps);
static void trace_onreq_out(struct cell* t, int type, struct tmcb_params *ps);
static void trace_onreply_in(struct cell* t, int type, struct tmcb_params *ps);
static void trace_onreply_out(struct cell* t, int type, struct tmcb_params *ps);
static void trace_sl_onreply_out(unsigned int types, struct sip_msg* req, struct sl_cb_param *sl_param);
static void trace_sl_ack_in(unsigned int types, struct sip_msg* req, struct sl_cb_param *sl_param);

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

register_slcb_t register_slcb_f=NULL;	/*!< stateless callback registration */

/*! \brief
 * Exported functions
 */
static cmd_export_t cmds[] = {
	{"sip_trace", (cmd_function)sip_trace, 0, 0, 0,
		REQUEST_ROUTE|FAILURE_ROUTE|ONREPLY_ROUTE|BRANCH_ROUTE|LOCAL_ROUTE},
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
#include "../../lib/kcore/statistics.h"

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

	if (flag_idx2mask(&trace_flag)<0)
		return -1;

	/* Find a database module */
	if (db_bind_mod(&db_url, &db_funcs)) {
		LM_ERR("unable to bind database module\n");
		return -1;
	}
	if (!DB_CAPABILITY(db_funcs, DB_CAP_INSERT)) {
		LM_ERR("database modules does not provide all functions needed by module\n");
		return -1;
	}

	trace_on_flag = (int*)shm_malloc(sizeof(int));
	if(trace_on_flag==NULL) {
		LM_ERR("no more shm memory left\n");
		return -1;
	}
	
	*trace_on_flag = trace_on;
	
	/* register callbacks to TM */
	if (load_tm_api(&tmb)!=0) {
		LM_ERR("can't load tm api. Is module tm loaded?\n");
		return -1;
	}

	if(tmb.register_tmcb( 0, 0, TMCB_REQUEST_IN, trace_onreq_in, 0, 0) <=0) {
		LM_ERR("can't register trace_onreq_in\n");
		return -1;
	}

	/* register sl callback */
	register_slcb_f = (register_slcb_t)find_export("register_slcb", 0, 0);
	if(register_slcb_f==NULL) {
		LM_ERR("can't load sl api. Is module sl loaded?\n");
		return -1;
	}
	if(register_slcb_f(SLCB_REPLY_OUT,trace_sl_onreply_out, NULL)!=0) {
		LM_ERR("can't register trace_sl_onreply_out\n");
		return -1;
	}
	if(register_slcb_f(SLCB_ACK_IN,trace_sl_ack_in, NULL)!=0) {
		LM_ERR("can't register trace_sl_ack_in\n");
		return -1;
	}

	if(dup_uri_str.s!=0) {
		dup_uri_str.len = strlen(dup_uri_str.s);
		dup_uri = (struct sip_uri *)pkg_malloc(sizeof(struct sip_uri));
		if(dup_uri==0) {
			LM_ERR("no more pkg memory left\n");
			return -1;
		}
		memset(dup_uri, 0, sizeof(struct sip_uri));
		if(parse_uri(dup_uri_str.s, dup_uri_str.len, dup_uri)<0) {
			LM_ERR("bad dup uri\n");
			return -1;
		}
	}

	if(traced_user_avp_str.s && traced_user_avp_str.len > 0) {
		if (pv_parse_spec(&traced_user_avp_str, &avp_spec)==0 || avp_spec.type!=PVT_AVP) {
			LM_ERR("malformed or non AVP %.*s AVP definition\n", traced_user_avp_str.len, traced_user_avp_str.s);
			return -1;
		}

		if(pv_get_avp_name(0, &avp_spec.pvp, &traced_user_avp, &traced_user_avp_type)!=0) {
			LM_ERR("[%.*s] - invalid AVP definition\n", traced_user_avp_str.len, traced_user_avp_str.s);
			return -1;
		}
	} else {
		traced_user_avp.n = 0;
		traced_user_avp_type = 0;
	}
	if(trace_table_avp_str.s && trace_table_avp_str.len > 0) {
		if (pv_parse_spec(&trace_table_avp_str, &avp_spec)==0 || avp_spec.type!=PVT_AVP) {
			LM_ERR("malformed or non AVP %.*s AVP definition\n", trace_table_avp_str.len, trace_table_avp_str.s);
			return -1;
		}

		if(pv_get_avp_name(0, &avp_spec.pvp, &trace_table_avp, &trace_table_avp_type)!=0) {
			LM_ERR("[%.*s] - invalid AVP definition\n", trace_table_avp_str.len, trace_table_avp_str.s);
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
	db_con = db_funcs.init(&db_url);
	if (!db_con) {
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
		avp=search_first_avp(trace_table_avp_type, trace_table_avp, &avp_value, 0);

	if(avp==NULL || !is_avp_str_val(avp) || avp_value.s.len<=0)
		return &siptrace_table;

	return &avp_value.s;
}

static int sip_trace(struct sip_msg *msg, char *s1, char *s2)
{
	db_key_t db_keys[NR_KEYS];
	db_val_t db_vals[NR_KEYS];
	static char toip_buff[IP_ADDR_MAX_STR_SIZE+6];
	static char fromip_buff[IP_ADDR_MAX_STR_SIZE+6];
	int_str avp_value;
	struct usr_avp *avp;
	
	if(msg==NULL) {
		LM_DBG("no uas request, local transaction\n");
		return -1;
	}

	avp = NULL;
	if(traced_user_avp.n!=0)
		avp=search_first_avp(traced_user_avp_type, traced_user_avp, &avp_value, 0);

	if((avp==NULL) && (trace_on_flag==NULL || *trace_on_flag==0)) {
		LM_DBG("trace off...\n");
		return -1;
	}
	
	if(parse_from_header(msg)==-1 || msg->from==NULL || get_from(msg)==NULL) {
		LM_ERR("cannot parse FROM header\n");
		goto error;
	}
	
	if(parse_headers(msg, HDR_CALLID_F, 0)!=0 || msg->callid==NULL || msg->callid->body.s==NULL) {
		LM_ERR("cannot parse call-id\n");
		goto error;
	}

	db_keys[0] = &msg_column;
	db_vals[0].type = DB1_BLOB;
	db_vals[0].nul = 0;
	db_vals[0].val.blob_val.s = msg->buf;
	db_vals[0].val.blob_val.len = msg->len;
	
	db_keys[1] = &callid_column;
	db_vals[1].type = DB1_STR;
	db_vals[1].nul = 0;
	db_vals[1].val.str_val.s = msg->callid->body.s;
	db_vals[1].val.str_val.len = msg->callid->body.len;
	
	db_keys[2] = &method_column;
	db_vals[2].type = DB1_STR;
	db_vals[2].nul = 0;
	if(msg->first_line.type==SIP_REQUEST) {
		db_vals[2].val.str_val.s = msg->first_line.u.request.method.s;
		db_vals[2].val.str_val.len = msg->first_line.u.request.method.len;
	} else {
		db_vals[2].val.str_val.s = "";
		db_vals[2].val.str_val.len = 0;
	}
		
	db_keys[3] = &status_column;
	db_vals[3].type = DB1_STR;
	db_vals[3].nul = 0;
	if(msg->first_line.type==SIP_REPLY) {
		db_vals[3].val.str_val.s = msg->first_line.u.reply.status.s;
		db_vals[3].val.str_val.len = msg->first_line.u.reply.status.len;
	} else {
		db_vals[3].val.str_val.s = "";
		db_vals[3].val.str_val.len = 0;
	}
		
	db_keys[4] = &fromip_column;
	db_vals[4].type = DB1_STRING;
	db_vals[4].nul = 0;
	siptrace_copy_proto(msg->rcv.proto, fromip_buff);
	strcat(fromip_buff, ip_addr2a(&msg->rcv.src_ip));
	strcat(fromip_buff,":");
	strcat(fromip_buff, int2str(msg->rcv.src_port, NULL));
	db_vals[4].val.string_val = fromip_buff;
	
	db_keys[5] = &toip_column;
	db_vals[5].type = DB1_STRING;
	db_vals[5].nul = 0;
	// db_vals[5].val.string_val = ip_addr2a(&msg->rcv.dst_ip);;
	siptrace_copy_proto(msg->rcv.proto, toip_buff);
	strcat(toip_buff, ip_addr2a(&msg->rcv.dst_ip));
	strcat(toip_buff,":");
	strcat(toip_buff, int2str(msg->rcv.dst_port, NULL));
	db_vals[5].val.string_val = toip_buff;
	
	db_keys[6] = &date_column;
	db_vals[6].type = DB1_DATETIME;
	db_vals[6].nul = 0;
	db_vals[6].val.time_val = time(NULL);
	
	db_keys[7] = &direction_column;
	db_vals[7].type = DB1_STRING;
	db_vals[7].nul = 0;
	db_vals[7].val.string_val = "in";
	
	db_keys[8] = &fromtag_column;
	db_vals[8].type = DB1_STR;
	db_vals[8].nul = 0;
	db_vals[8].val.str_val.s = get_from(msg)->tag_value.s;
	db_vals[8].val.str_val.len = get_from(msg)->tag_value.len;
	
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
		if(msg->first_line.type==SIP_REPLY) {
			update_stat(siptrace_rpl, 1);
		} else {
			update_stat(siptrace_req, 1);
		}
#endif
	}
	
	if(avp==NULL)
		goto done;
	
	trace_send_duplicate(db_vals[0].val.blob_val.s, db_vals[0].val.blob_val.len);
	
	db_vals[9].val.str_val.s = avp_value.s.s;
	db_vals[9].val.str_val.len = avp_value.s.len;

	LM_DBG("storing info...\n");
	if(db_funcs.insert(db_con, db_keys, db_vals, NR_KEYS) < 0) {
		LM_ERR("error storing trace\n");
		goto error;
	}

	avp = search_next_avp( avp, &avp_value);
	while(avp!=NULL) {
		db_vals[9].val.str_val.s = avp_value.s.s;
		db_vals[9].val.str_val.len = avp_value.s.len;

		LM_DBG("storing info...\n");
		if(db_funcs.insert(db_con, db_keys, db_vals, NR_KEYS) < 0) {
			LM_ERR("error storing trace\n");
			goto error;
		}
		avp = search_next_avp( avp, &avp_value);
	}

done:
	return 1;
error:
	return -1;
}

#define trace_is_off(_msg) \
	(trace_on_flag==NULL || *trace_on_flag==0 || \
		((_msg)->flags&trace_flag)==0)

static void trace_onreq_in(struct cell* t, int type, struct tmcb_params *ps)
{
	struct sip_msg* msg;
	int_str         avp_value;
	struct usr_avp* avp;

	if(t==NULL || ps==NULL) {
		LM_DBG("no uas request, local transaction\n");
		return;
	}
	
	msg = ps->req;
	if(msg==NULL) {
		LM_DBG("no uas request, local transaction\n");
		return;
	}
	
	avp = NULL;
	if(traced_user_avp.n!=0)
		avp=search_first_avp(traced_user_avp_type, traced_user_avp, &avp_value, 0);

	if((avp==NULL) && trace_is_off(msg)) {
		LM_DBG("trace off...\n");
		return;
	}
	
	if(parse_from_header(msg)==-1 || msg->from==NULL || get_from(msg)==NULL) {
		LM_ERR("cannot parse FROM header\n");
		return;
	}

	if(parse_headers(msg, HDR_CALLID_F, 0)!=0) {
		LM_ERR("cannot parse call-id\n");
		return;
	}

	if(tmb.register_tmcb( 0, t, TMCB_REQUEST_BUILT, trace_onreq_out, 0, 0) <=0) {
		LM_ERR("can't register trace_onreq_out\n");
		return;
	}

	if(tmb.register_tmcb( 0, t, TMCB_RESPONSE_IN, trace_onreply_in, 0, 0) <=0) {
		LM_ERR("can't register trace_onreply_in\n");
		return;
	}

	if(tmb.register_tmcb( 0, t, TMCB_RESPONSE_OUT, trace_onreply_out, 0, 0) <=0) {
		LM_ERR("can't register trace_onreply_out\n");
		return;
	}
}

static void trace_onreq_out(struct cell* t, int type, struct tmcb_params *ps)
{
	db_key_t db_keys[NR_KEYS];
	db_val_t db_vals[NR_KEYS];
	static char fromip_buff[IP_ADDR_MAX_STR_SIZE+12];
	static char toip_buff[IP_ADDR_MAX_STR_SIZE+12];
	struct sip_msg* msg;
	int_str        avp_value;
	struct usr_avp *avp;
	struct ip_addr to_ip;
	int len;
	str *sbuf;
	struct dest_info *dst;
	
	if(t==NULL || ps==NULL) {
		LM_DBG("no uas request, local transaction\n");
		return;
	}
	msg=ps->req;
	if(msg==NULL) {
		LM_DBG("no uas msg, local transaction\n");
		return;
	}
	
	avp = NULL;
	if(traced_user_avp.n!=0)
		avp=search_first_avp(traced_user_avp_type, traced_user_avp, &avp_value, 0);

	if((avp==NULL) && trace_is_off(msg) ) {
		LM_DBG("trace off...\n");
		return;
	}

	if(parse_from_header(msg)==-1 || msg->from==NULL || get_from(msg)==NULL) {
		LM_ERR("cannot parse FROM header\n");
		goto error;
	}

	if(parse_headers(msg, HDR_CALLID_F, 0)!=0) {
		LM_ERR("cannot parse call-id\n");
		return;
	}

	db_keys[0] = &msg_column;
	db_vals[0].type = DB1_BLOB;
	db_vals[0].nul = 0;
	sbuf = (str*)ps->extra1;
	if(sbuf!=NULL && sbuf->len>0) {
		db_vals[0].val.blob_val.s   = sbuf->s;
		db_vals[0].val.blob_val.len = sbuf->len;
	} else {
		db_vals[0].val.blob_val.s   = "No request buffer";
		db_vals[0].val.blob_val.len = sizeof("No request buffer")-1;
	}
	
	/* check Call-ID header */
	if(msg->callid==NULL || msg->callid->body.s==NULL) {
		LM_ERR("cannot find Call-ID header!\n");
		goto error;
	}

	db_keys[1] = &callid_column;
	db_vals[1].type = DB1_STR;
	db_vals[1].nul = 0;
	db_vals[1].val.str_val.s = msg->callid->body.s;
	db_vals[1].val.str_val.len = msg->callid->body.len;
	
	db_keys[2] = &method_column;
	db_vals[2].type = DB1_STR;
	db_vals[2].nul = 0;
	sbuf = (str*)ps->extra1;
	if(sbuf!=NULL && sbuf->len > 7 && !strncasecmp(sbuf->s, "CANCEL ", 7)) {
		db_vals[2].val.str_val.s = "CANCEL";
		db_vals[2].val.str_val.len = 6;
	} else {
		db_vals[2].val.str_val.s = t->method.s;
		db_vals[2].val.str_val.len = t->method.len;
	}
		
	db_keys[3] = &status_column;
	db_vals[3].type = DB1_STR;
	db_vals[3].nul = 0;
	db_vals[3].val.str_val.s = "";
	db_vals[3].val.str_val.len = 0;
		
	memset(&to_ip, 0, sizeof(struct ip_addr));
	dst = (struct dest_info*)ps->extra2;

	db_keys[4] = &fromip_column;
	db_vals[4].type = DB1_STRING;
	db_vals[4].nul = 0;
	if (trace_local_ip.s && trace_local_ip.len > 0) {
		db_vals[4].val.string_val = trace_local_ip.s;
	} else {
		if(dst==0 || dst->send_sock==0 || dst->send_sock->sock_str.s==0) {
			siptrace_copy_proto(msg->rcv.proto, fromip_buff);
			strcat(fromip_buff, ip_addr2a(&msg->rcv.dst_ip));
			strcat(fromip_buff,":");
			strcat(fromip_buff, int2str(msg->rcv.dst_port, NULL));
			db_vals[4].val.string_val = fromip_buff;
		} else {
			db_vals[4].type = DB1_STR;
			db_vals[4].val.str_val = dst->send_sock->sock_str;
		}
	}
	
	db_keys[5] = &toip_column;
	db_vals[5].type = DB1_STRING;
	db_vals[5].nul = 0;
	if(dst==0) {
		db_vals[5].val.string_val = "any:255.255.255.255";
	} else {
		su2ip_addr(&to_ip, &dst->to);
		siptrace_copy_proto(dst->proto, toip_buff);
		strcat(toip_buff, ip_addr2a(&to_ip));
		strcat(toip_buff, ":");
		strcat(toip_buff,
				int2str((unsigned long)su_getport(&dst->to), &len));
		LM_DBG("dest [%s]\n", toip_buff);
		db_vals[5].val.string_val = toip_buff;
	}
	
	db_keys[6] = &date_column;
	db_vals[6].type = DB1_DATETIME;
	db_vals[6].nul = 0;
	db_vals[6].val.time_val = time(NULL);
	
	db_keys[7] = &direction_column;
	db_vals[7].type = DB1_STRING;
	db_vals[7].nul = 0;
	db_vals[7].val.string_val = "out";
	
	db_keys[8] = &fromtag_column;
	db_vals[8].type = DB1_STR;
	db_vals[8].nul = 0;
	db_vals[8].val.str_val.s = get_from(msg)->tag_value.s;
	db_vals[8].val.str_val.len = get_from(msg)->tag_value.len;
	
	db_funcs.use_table(db_con, siptrace_get_table());

	db_keys[9] = &traced_user_column;
	db_vals[9].type = DB1_STR;
	db_vals[9].nul = 0;

	if( !trace_is_off(msg) ) {
		db_vals[9].val.str_val.s   = "";
		db_vals[9].val.str_val.len = 0;
	
		LM_DBG("storing info...\n");
		if(db_funcs.insert(db_con, db_keys, db_vals, NR_KEYS) < 0) {
			LM_ERR("error storing trace\n");
			goto error;
		}
#ifdef STATISTICS
		update_stat(siptrace_req, 1);
#endif
	}
	
	if(avp==NULL)
		goto done;
	
	trace_send_duplicate(db_vals[0].val.blob_val.s, db_vals[0].val.blob_val.len);
	
	db_vals[9].val.str_val.s = avp_value.s.s;
	db_vals[9].val.str_val.len = avp_value.s.len;

	LM_DBG("storing info...\n");
	if(db_funcs.insert(db_con, db_keys, db_vals, NR_KEYS) < 0) {
		LM_ERR("error storing trace\n");
		goto error;
	}

	avp = search_next_avp(avp, &avp_value);
	while(avp!=NULL) {
		db_vals[9].val.str_val.s = avp_value.s.s;
		db_vals[9].val.str_val.len = avp_value.s.len;

		LM_DBG("storing info...\n");
		if(db_funcs.insert(db_con, db_keys, db_vals, NR_KEYS) < 0) {
			LM_ERR("error storing trace\n");
			goto error;
		}
		avp = search_next_avp( avp, &avp_value);
	}
	
done:	
	return;
error:
	return;
}

static void trace_onreply_in(struct cell* t, int type, struct tmcb_params *ps)
{
	db_key_t db_keys[NR_KEYS];
	db_val_t db_vals[NR_KEYS];

	static char fromip_buff[IP_ADDR_MAX_STR_SIZE+12];
	static char toip_buff[IP_ADDR_MAX_STR_SIZE+12];
	struct sip_msg* msg;
	struct sip_msg* req;
	int_str        avp_value;
	struct usr_avp *avp;
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
	
	avp = NULL;
	if(traced_user_avp.n!=0)
		avp=search_first_avp(traced_user_avp_type, traced_user_avp, &avp_value, 0);

	if((avp==NULL) &&  trace_is_off(req)) {
		LM_DBG("trace off...\n");
		return;
	}

	if(parse_from_header(msg)==-1 || msg->from==NULL || get_from(msg)==NULL) {
		LM_ERR("cannot parse FROM header\n");
		goto error;
	}

	if(parse_headers(msg, HDR_CALLID_F, 0)!=0) {
		LM_ERR("cannot parse call-id\n");
		return;
	}

	db_keys[0] = &msg_column;
	db_vals[0].type = DB1_BLOB;
	db_vals[0].nul = 0;
	if(msg->len>0) {
		db_vals[0].val.blob_val.s   = msg->buf;
		db_vals[0].val.blob_val.len = msg->len;
	} else {
		db_vals[0].val.blob_val.s   = "No reply buffer";
		db_vals[0].val.blob_val.len = sizeof("No reply buffer")-1;
	}

	/* check Call-ID header */
	if(msg->callid==NULL || msg->callid->body.s==NULL) {
		LM_ERR("cannot find Call-ID header!\n");
		goto error;
	}

	db_keys[1] = &callid_column;
	db_vals[1].type = DB1_STR;
	db_vals[1].nul = 0;
	db_vals[1].val.str_val.s = msg->callid->body.s;
	db_vals[1].val.str_val.len = msg->callid->body.len;
	
	db_keys[2] = &method_column;
	db_vals[2].type = DB1_STR;
	db_vals[2].nul = 0;
	db_vals[2].val.str_val.s = t->method.s;
	db_vals[2].val.str_val.len = t->method.len;
		
	db_keys[3] = &status_column;
	db_vals[3].type = DB1_STRING;
	db_vals[3].nul = 0;
	strcpy(statusbuf, int2str(ps->code, NULL));
	db_vals[3].val.string_val = statusbuf;
		
	db_keys[4] = &fromip_column;
	db_vals[4].type = DB1_STRING;
	db_vals[4].nul = 0;
	siptrace_copy_proto(msg->rcv.proto, fromip_buff);
	strcat(fromip_buff, ip_addr2a(&msg->rcv.src_ip));
	strcat(fromip_buff,":");
	strcat(fromip_buff, int2str(msg->rcv.src_port, NULL));
	db_vals[4].val.string_val = fromip_buff;
	
	db_keys[5] = &toip_column;
	db_vals[5].type = DB1_STRING;
	db_vals[5].nul = 0;
	// db_vals[5].val.string_val = ip_addr2a(&msg->rcv.dst_ip);;
	if(trace_local_ip.s && trace_local_ip.len > 0) {
		db_vals[5].val.string_val = trace_local_ip.s;
	} else {
		siptrace_copy_proto(msg->rcv.proto, toip_buff);
		strcat(toip_buff, ip_addr2a(&msg->rcv.dst_ip));
		strcat(toip_buff,":");
		strcat(toip_buff, int2str(msg->rcv.dst_port, NULL));
		db_vals[5].val.string_val = toip_buff;
	}
	
	db_keys[6] = &date_column;
	db_vals[6].type = DB1_DATETIME;
	db_vals[6].nul = 0;
	db_vals[6].val.time_val = time(NULL);
	
	db_keys[7] = &direction_column;
	db_vals[7].type = DB1_STRING;
	db_vals[7].nul = 0;
	db_vals[7].val.string_val = "in";
	
	db_keys[8] = &fromtag_column;
	db_vals[8].type = DB1_STR;
	db_vals[8].nul = 0;
	db_vals[8].val.str_val.s = get_from(msg)->tag_value.s;
	db_vals[8].val.str_val.len = get_from(msg)->tag_value.len;
	
	db_funcs.use_table(db_con, siptrace_get_table());

	db_keys[9] = &traced_user_column;
	db_vals[9].type = DB1_STR;
	db_vals[9].nul = 0;

	if( !trace_is_off(req) ) {
		db_vals[9].val.str_val.s   = "";
		db_vals[9].val.str_val.len = 0;
	
		LM_DBG("storing info...\n");
		if(db_funcs.insert(db_con, db_keys, db_vals, NR_KEYS) < 0) {
			LM_ERR("error storing trace\n");
			goto error;
		}
#ifdef STATISTICS
		update_stat(siptrace_rpl, 1);
#endif
	}
	
	if(avp==NULL)
		goto done;
	
	trace_send_duplicate(db_vals[0].val.blob_val.s, db_vals[0].val.blob_val.len);
	
	db_vals[9].val.str_val.s = avp_value.s.s;
	db_vals[9].val.str_val.len = avp_value.s.len;

	LM_DBG("storing info...\n");
	if(db_funcs.insert(db_con, db_keys, db_vals, NR_KEYS) < 0) {
		LM_ERR("error storing trace\n");
		goto error;
	}

	avp = search_next_avp( avp, &avp_value);
	while(avp!=NULL) {
		db_vals[9].val.str_val.s = avp_value.s.s;
		db_vals[9].val.str_val.len = avp_value.s.len;

		LM_DBG("storing info ...\n");
		if(db_funcs.insert(db_con, db_keys, db_vals, NR_KEYS) < 0) {
			LM_ERR("error storing trace\n");
			goto error;
		}
		avp = search_next_avp( avp, &avp_value);
	}
	
done:
	return;
error:
	return;
}

static void trace_onreply_out(struct cell* t, int type, struct tmcb_params *ps)
{
	db_key_t db_keys[NR_KEYS];
	db_val_t db_vals[NR_KEYS];
	int faked = 0;
	static char fromip_buff[IP_ADDR_MAX_STR_SIZE+12];
	static char toip_buff[IP_ADDR_MAX_STR_SIZE+12];
	struct sip_msg* msg;
	struct sip_msg* req;
	int_str        avp_value;
	struct usr_avp *avp;
	struct ip_addr to_ip;
	int len;
	char statusbuf[8];
	str *sbuf;
	struct dest_info *dst;

	if (t==NULL || t->uas.request==0 || ps==NULL) {
		LM_DBG("no uas request, local transaction\n");
		return;
	}
	
	avp = NULL;
	if(traced_user_avp.n!=0)
		avp=search_first_avp(traced_user_avp_type, traced_user_avp, &avp_value, 0);

	if((avp==NULL) &&  trace_is_off(t->uas.request)) {
		LM_DBG("trace off...\n");
		return;
	}
	
	req = ps->req;
	msg = ps->rpl;
	if(msg==NULL || msg==FAKED_REPLY) {
		msg = t->uas.request;
		faked = 1;
	}

	if(parse_from_header(msg)==-1 || msg->from==NULL || get_from(msg)==NULL) {
		LM_ERR("cannot parse FROM header\n");
		goto error;
	}

	if(parse_headers(msg, HDR_CALLID_F, 0)!=0) {
		LM_ERR("cannot parse call-id\n");
		return;
	}

	db_keys[0] = &msg_column;
	db_vals[0].type = DB1_BLOB;
	db_vals[0].nul = 0;
	sbuf = (str*)ps->extra1;
	if(faked==0) {
		if(sbuf!=0 && sbuf->len>0) {
			db_vals[0].val.blob_val.s   = sbuf->s;
			db_vals[0].val.blob_val.len = sbuf->len;
		} else if(t->uas.response.buffer.s!=NULL) {
			db_vals[0].val.blob_val.s   = t->uas.response.buffer.s;
			db_vals[0].val.blob_val.len = t->uas.response.buffer.len;
		} else if(msg->len>0) {
			db_vals[0].val.blob_val.s   = msg->buf;
			db_vals[0].val.blob_val.len = msg->len;
		} else {
			db_vals[0].val.blob_val.s   = "No reply buffer";
			db_vals[0].val.blob_val.len = sizeof("No reply buffer")-1;
		}
	} else {
		if(sbuf!=0 && sbuf->len>0) {
			db_vals[0].val.blob_val.s   = sbuf->s;
			db_vals[0].val.blob_val.len = sbuf->len;
		} else if(t->uas.response.buffer.s==NULL) {
			db_vals[0].val.blob_val.s = "No reply buffer";
			db_vals[0].val.blob_val.len = sizeof("No reply buffer")-1;
		} else {
			db_vals[0].val.blob_val.s = t->uas.response.buffer.s;
			db_vals[0].val.blob_val.len = t->uas.response.buffer.len;
		}
	}
	
	/* check Call-ID header */
	if(msg->callid==NULL || msg->callid->body.s==NULL) {
		LM_ERR("cannot find Call-ID header!\n");
		goto error;
	}

	db_keys[1] = &callid_column;
	db_vals[1].type = DB1_STR;
	db_vals[1].nul = 0;
	db_vals[1].val.str_val.s = msg->callid->body.s;
	db_vals[1].val.str_val.len = msg->callid->body.len;
	
	db_keys[2] = &method_column;
	db_vals[2].type = DB1_STR;
	db_vals[2].nul = 0;
	db_vals[2].val.str_val.s = t->method.s;
	db_vals[2].val.str_val.len = t->method.len;
		
	db_keys[4] = &fromip_column;
	db_vals[4].type = DB1_STRING;
	db_vals[4].nul = 0;
	
	if(trace_local_ip.s && trace_local_ip.len > 0) {
		db_vals[4].val.string_val = trace_local_ip.s;
	} else {
		siptrace_copy_proto(msg->rcv.proto, fromip_buff);

		strcat(fromip_buff, ip_addr2a(&req->rcv.dst_ip));
		strcat(fromip_buff,":");
		strcat(fromip_buff, int2str(req->rcv.dst_port, NULL));
		db_vals[4].val.string_val = fromip_buff;
	}
	
	db_keys[3] = &status_column;
	db_vals[3].type = DB1_STRING;
	db_vals[3].nul = 0;
	strcpy(statusbuf, int2str(ps->code, NULL));
	db_vals[3].val.string_val = statusbuf;
		
	db_keys[5] = &toip_column;
	db_vals[5].type = DB1_STRING;
	db_vals[5].nul = 0;
	memset(&to_ip, 0, sizeof(struct ip_addr));
	dst = (struct dest_info*)ps->extra2;
	if(dst==0) {
		db_vals[5].val.string_val = "any:255.255.255.255";
	} else {
		su2ip_addr(&to_ip, &dst->to);
		siptrace_copy_proto(dst->proto, toip_buff);
		strcat(toip_buff, ip_addr2a(&to_ip));
		strcat(toip_buff, ":");
		strcat(toip_buff, int2str((unsigned long)su_getport(&dst->to), &len));
		LM_DBG("dest [%s]\n", toip_buff);
		db_vals[5].val.string_val = toip_buff;
	}
	
	db_keys[6] = &date_column;
	db_vals[6].type = DB1_DATETIME;
	db_vals[6].nul = 0;
	db_vals[6].val.time_val = time(NULL);
	
	db_keys[7] = &direction_column;
	db_vals[7].type = DB1_STRING;
	db_vals[7].nul = 0;
	db_vals[7].val.string_val = "out";
	
	db_keys[8] = &fromtag_column;
	db_vals[8].type = DB1_STR;
	db_vals[8].nul = 0;
	db_vals[8].val.str_val.s = get_from(msg)->tag_value.s;
	db_vals[8].val.str_val.len = get_from(msg)->tag_value.len;
	
	db_funcs.use_table(db_con, siptrace_get_table());

	db_keys[9] = &traced_user_column;
	db_vals[9].type = DB1_STR;
	db_vals[9].nul = 0;

	if( !trace_is_off(req) ) {
		db_vals[9].val.str_val.s   = "";
		db_vals[9].val.str_val.len = 0;
	
		LM_DBG("storing info...\n");
		if(db_funcs.insert(db_con, db_keys, db_vals, NR_KEYS) < 0) {
			LM_ERR("error storing trace\n");
			goto error;
		}
#ifdef STATISTICS
		update_stat(siptrace_rpl, 1);
#endif
	}
	
	if(avp==NULL)
		goto done;
	
	trace_send_duplicate(db_vals[0].val.blob_val.s, db_vals[0].val.blob_val.len);
	
	db_vals[9].val.str_val.s = avp_value.s.s;
	db_vals[9].val.str_val.len = avp_value.s.len;

	LM_DBG("storing info...\n");
	if(db_funcs.insert(db_con, db_keys, db_vals, NR_KEYS) < 0) {
		LM_ERR("error storing trace\n");
		goto error;
	}

	avp = search_next_avp( avp, &avp_value);
	while(avp!=NULL)
	{
		db_vals[9].val.str_val.s = avp_value.s.s;
		db_vals[9].val.str_val.len = avp_value.s.len;

		LM_DBG("### - storing info (%d) ...\n", faked);
		if(db_funcs.insert(db_con, db_keys, db_vals, NR_KEYS) < 0) {
			LM_ERR("error storing trace\n");
			goto error;
		}
		avp = search_next_avp( avp, &avp_value);
	}
	
done:
	return;
error:
	return;
}

static void trace_sl_ack_in( unsigned int types, struct sip_msg* req, struct sl_cb_param *sl_param)
{
	LM_DBG("storing ack...\n");
	sip_trace(req, 0, 0);
}

static void trace_sl_onreply_out( unsigned int types, struct sip_msg* req, struct sl_cb_param *sl_param)
{
	db_key_t db_keys[NR_KEYS];
	db_val_t db_vals[NR_KEYS];
	static char fromip_buff[IP_ADDR_MAX_STR_SIZE+12];
	static char toip_buff[IP_ADDR_MAX_STR_SIZE+12];
	int faked = 0;
	struct sip_msg* msg;
	int_str        avp_value;
	struct usr_avp *avp;
	struct ip_addr to_ip;
	int len;
	char statusbuf[5];

	if(req==NULL || sl_param==NULL)
	{
		LM_ERR("bad parameters\n");
		goto error;
	}
	
	avp = NULL;
	if(traced_user_avp.n!=0)
		avp=search_first_avp(traced_user_avp_type, traced_user_avp, &avp_value, 0);

	if((avp==NULL) && trace_is_off(req)) {
		LM_DBG("trace off...\n");
		return;
	}
	
	msg = req;
	faked = 1;
	
	if(parse_from_header(msg)==-1 || msg->from==NULL || get_from(msg)==NULL) {
		LM_ERR("cannot parse FROM header\n");
		goto error;
	}

	if(parse_headers(msg, HDR_CALLID_F, 0)!=0) {
		LM_ERR("cannot parse call-id\n");
		return;
	}

	db_keys[0] = &msg_column;
	db_vals[0].type = DB1_BLOB;
	db_vals[0].nul = 0;
	db_vals[0].val.blob_val.s   = (sl_param->buffer)?sl_param->buffer->s:"";
	db_vals[0].val.blob_val.len = (sl_param->buffer)?sl_param->buffer->len:0;
	
	/* check Call-ID header */
	if(msg->callid==NULL || msg->callid->body.s==NULL) {
		LM_ERR("cannot find Call-ID header!\n");
		goto error;
	}

	db_keys[1] = &callid_column;
	db_vals[1].type = DB1_STR;
	db_vals[1].nul = 0;
	db_vals[1].val.str_val.s = msg->callid->body.s;
	db_vals[1].val.str_val.len = msg->callid->body.len;
	
	db_keys[2] = &method_column;
	db_vals[2].type = DB1_STR;
	db_vals[2].nul = 0;
	db_vals[2].val.str_val.s = msg->first_line.u.request.method.s;
	db_vals[2].val.str_val.len = msg->first_line.u.request.method.len;
		
	db_keys[4] = &fromip_column;
	db_vals[4].type = DB1_STRING;
	db_vals[4].nul = 0;
	if(trace_local_ip.s && trace_local_ip.len > 0) {
		db_vals[4].val.string_val = trace_local_ip.s;
	} else {
		siptrace_copy_proto(msg->rcv.proto, fromip_buff);

		strcat(fromip_buff, ip_addr2a(&req->rcv.dst_ip));
		strcat(fromip_buff,":");
		strcat(fromip_buff, int2str(req->rcv.dst_port, NULL));
		db_vals[4].val.string_val = fromip_buff;
	}

	db_keys[3] = &status_column;
	db_vals[3].type = DB1_STRING;
	db_vals[3].nul = 0;
	strcpy(statusbuf, int2str(sl_param->code, NULL));
	db_vals[3].val.string_val = statusbuf;
		
	db_keys[5] = &toip_column;
	db_vals[5].type = DB1_STRING;
	db_vals[5].nul = 0;
	memset(&to_ip, 0, sizeof(struct ip_addr));
	if(sl_param->dst==0)
	{
		db_vals[5].val.string_val = "any:255.255.255.255";
	} else {
		su2ip_addr(&to_ip, sl_param->dst);
		siptrace_copy_proto(req->rcv.proto, toip_buff);

		strcat(toip_buff, ip_addr2a(&to_ip));
		strcat(toip_buff, ":");
		strcat(toip_buff,
				int2str((unsigned long)su_getport(sl_param->dst), &len));
		LM_DBG("dest [%s]\n", toip_buff);
		db_vals[5].val.string_val = toip_buff;
	}
	
	db_keys[6] = &date_column;
	db_vals[6].type = DB1_DATETIME;
	db_vals[6].nul = 0;
	db_vals[6].val.time_val = time(NULL);
	
	db_keys[7] = &direction_column;
	db_vals[7].type = DB1_STRING;
	db_vals[7].nul = 0;
	db_vals[7].val.string_val = "out";
	
	db_keys[8] = &fromtag_column;
	db_vals[8].type = DB1_STR;
	db_vals[8].nul = 0;
	db_vals[8].val.str_val.s = get_from(msg)->tag_value.s;
	db_vals[8].val.str_val.len = get_from(msg)->tag_value.len;
	
	db_funcs.use_table(db_con, siptrace_get_table());

	db_keys[9] = &traced_user_column;
	db_vals[9].type = DB1_STR;
	db_vals[9].nul = 0;

	if( !trace_is_off(msg) ) {
		db_vals[9].val.str_val.s   = "";
		db_vals[9].val.str_val.len = 0;
	
		LM_DBG("storing info...\n");
		if(db_funcs.insert(db_con, db_keys, db_vals, NR_KEYS) < 0) {
			LM_ERR("error storing trace\n");
			goto error;
		}
#ifdef STATISTICS
		update_stat(siptrace_rpl, 1);
#endif
	}
	
	if(avp==NULL)
		goto done;
	
	trace_send_duplicate(db_vals[0].val.blob_val.s, db_vals[0].val.blob_val.len);
	
	db_vals[9].val.str_val.s = avp_value.s.s;
	db_vals[9].val.str_val.len = avp_value.s.len;

	LM_DBG("storing info...\n");
	if(db_funcs.insert(db_con, db_keys, db_vals, NR_KEYS) < 0) {
		LM_ERR("error storing trace\n");
		goto error;
	}

	avp = search_next_avp( avp, &avp_value);
	while(avp!=NULL) {
		db_vals[9].val.str_val.s = avp_value.s.s;
		db_vals[9].val.str_val.len = avp_value.s.len;

		LM_DBG("### - storing info (%d) ...\n", faked);
		if(db_funcs.insert(db_con, db_keys, db_vals, NR_KEYS) < 0) {
			LM_ERR("error storing trace\n");
			goto error;
		}
		avp = search_next_avp( avp, &avp_value);
	}
	
done:
	return;
error:
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

	if ( node->value.len==2 && (node->value.s[0]=='o'|| node->value.s[0]=='O') &&
			(node->value.s[1]=='n'|| node->value.s[1]=='N')) {
		*trace_on_flag = 1;
		return init_mi_tree( 200, MI_SSTR(MI_OK));
	} else if ( node->value.len==3 && (node->value.s[0]=='o'|| node->value.s[0]=='O') 
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
	union sockaddr_union* to;
	struct socket_info* send_sock;
	struct proxy_l * p;
	int proto;
	int ret;
	
	if(buf==NULL || len <= 0)
		return -1;
	
	if(dup_uri_str.s==0 || dup_uri==NULL)
		return 0;
	
	to=(union sockaddr_union*)pkg_malloc(sizeof(union sockaddr_union));
	if (to==0){ LM_ERR("out of pkg memory\n");
		return -1;
	}
	
	/* create a temporary proxy*/
	proto = PROTO_UDP;
	p=mk_proxy(&dup_uri->host, (dup_uri->port_no)?dup_uri->port_no:SIP_PORT, proto, 0);
	if (p==0) {
		LM_ERR("bad host name in uri\n");
		pkg_free(to);
		return -1;
	}
	
	hostent2su(to, &p->host, p->addr_idx, (p->port)?p->port:SIP_PORT);
	
	ret = -1;
	
	do {
		send_sock=get_send_socket(0, to, proto);
		if (send_sock==0){
			LM_ERR("can't forward to af %d, proto %d no corresponding listening socket\n", 
					to->s.sa_family,proto);
			continue;
		}

		if (msg_send(send_sock, proto, to, 0, buf, len)<0) {
			LM_ERR("cannot send duplicate message\n");
			continue;
		}
		ret = 0;
		break;
	} while( get_next_su( p, to, 0)==0 );
	
	free_proxy(p); /* frees only p content, not p itself */
	pkg_free(p);
	pkg_free(to);

	return ret;
}
