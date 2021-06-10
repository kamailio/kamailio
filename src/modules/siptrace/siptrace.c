/*
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
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
#include "../../core/sr_module.h"
#include "../../core/mod_fix.h"
#include "../../core/dprint.h"
#include "../../core/ut.h"
#include "../../core/ip_addr.h"
#include "../../core/mem/mem.h"
#include "../../core/mem/shm_mem.h"
#include "../../core/rpc.h"
#include "../../core/rpc_lookup.h"
#include "../../lib/srdb1/db.h"
#include "../../core/parser/parse_content.h"
#include "../../core/parser/parse_from.h"
#include "../../core/parser/parse_cseq.h"
#include "../../core/pvar.h"
#include "../../modules/tm/tm_load.h"
#include "../../modules/sl/sl.h"
#include "../../modules/dialog/dlg_load.h"
#include "../../core/str.h"
#include "../../core/onsend.h"
#include "../../core/events.h"
#include "../../core/kemi.h"

#include "siptrace_data.h"
#include "siptrace_hep.h"
#include "siptrace_send.h"

MODULE_VERSION

#define SIPTRACE_ANYADDR "any:255.255.255.255:5060"
#define SIPTRACE_ANYADDR_LEN (sizeof(SIPTRACE_ANYADDR) - 1)

#define trace_is_off(_msg)                        \
	 ((((_msg)->msg_flags & FL_SIPTRACE) == 0) \
		|| ((trace_flag != 0) && (_msg->flags & trace_flag) == 0))

#define is_null_pv(_str) \
	(!str_strcmp(&_str, pv_get_null_str()))

struct tm_binds tmb;
struct dlg_binds dlgb;

/** SL API structure */
sl_api_t slb;

/* module function prototypes */
static int mod_init(void);
static int siptrace_init_rpc(void);
static int child_init(int rank);
static void destroy(void);
static int sip_trace(sip_msg_t *msg, dest_info_t *, str *, char *);
static int w_sip_trace0(struct sip_msg *, char *p1, char *p2);
static int w_sip_trace1(struct sip_msg *, char *dest, char *p2);
static int w_sip_trace2(struct sip_msg *, char *dest, char *correlation_id);
static int w_sip_trace3(struct sip_msg *, char *dest, char *correlation_id, char *trace_type);
static int fixup_siptrace(void **param, int param_no);
static int fixup_free_siptrace(void **param, int param_no);
static int w_sip_trace_mode(sip_msg_t *msg, char *pmode, char *p2);

static int parse_siptrace_uri(str* duri, dest_info_t* dst);
static enum siptrace_type_t parse_siptrace_flag(str* sflags);

static int w_hlog1(struct sip_msg *, char *message, char *);
static int w_hlog2(struct sip_msg *, char *correlationid, char *message);

static int sip_trace_store_db(siptrace_data_t *sto);

static void trace_onreq_out(struct cell *t, int type, struct tmcb_params *ps);
static void trace_onreply_in(struct cell *t, int type, struct tmcb_params *ps);
static void trace_onreply_out(struct cell *t, int type, struct tmcb_params *ps);
static void trace_tm_neg_ack_in(struct cell *t, int type, struct tmcb_params *ps);
static void trace_sl_onreply_out(sl_cbp_t *slcb);
static void trace_sl_ack_in(sl_cbp_t *slcb);



static void trace_transaction(sip_msg_t* msg, siptrace_info_t* info, int dlg_tran);
static void trace_dialog(struct dlg_cell* dlg, int type, struct dlg_cb_params *params);
static void trace_dialog_transaction(struct dlg_cell* dlg, int type, struct dlg_cb_params *params);
static void free_trace_info(void* trace_info);
static int  add_info_xavp(siptrace_info_t* info);
static inline int parse_raw_uri(siptrace_info_t* info);

int siptrace_net_data_recv(sr_event_param_t *evp);
int siptrace_net_data_send(sr_event_param_t *evp);
static int _siptrace_mode = 0;


static str db_url = str_init(DEFAULT_DB_URL);
static str siptrace_table = str_init("sip_trace");
static str date_column = str_init("time_stamp");		 /* 00 */
static str callid_column = str_init("callid");			 /* 01 */
static str traced_user_column = str_init("traced_user"); /* 02 */
static str msg_column = str_init("msg");				 /* 03 */
static str method_column = str_init("method");			 /* 04 */
static str status_column = str_init("status");			 /* 05 */
static str fromip_column = str_init("fromip");			 /* 06 */
static str toip_column = str_init("toip");				 /* 07 */
static str fromtag_column = str_init("fromtag");		 /* 08 */
static str direction_column = str_init("direction");	 /* 09 */
static str time_us_column = str_init("time_us");		 /* 10 */
static str totag_column = str_init("totag");			 /* 11 */

#define XAVP_TRACE_INFO_NAME "trace_info"
static str xavp_trace_info_name_s = str_init(XAVP_TRACE_INFO_NAME);

#define NR_KEYS 12
#define SIP_TRACE_TABLE_VERSION 4

int trace_flag_param = -1;
int trace_flag = 0;

int trace_on = 0;
int trace_sl_acks = 1;

int trace_to_database = 1;
int trace_delayed = 0;

int hep_version = 1;
int hep_capture_id = 1;
int hep_vendor_id = 0;
str auth_key_str = {0, 0};

int xheaders_write = 0;
int xheaders_read = 0;

str force_send_sock_str = {0, 0};
struct sip_uri *force_send_sock_uri = 0;

str dup_uri_str = {0, 0};
struct sip_uri *dup_uri = 0;

int *trace_on_flag = NULL;
int *trace_to_database_flag = NULL;

int *xheaders_write_flag = NULL;
int *xheaders_read_flag = NULL;

static unsigned short traced_user_avp_type = 0;
static int_str traced_user_avp;
static str traced_user_avp_str = {NULL, 0};

static unsigned short trace_table_avp_type = 0;
static int_str trace_table_avp;
static str trace_table_avp_str = {NULL, 0};

static str trace_local_ip = {NULL, 0};

int hep_mode_on = 0;

db1_con_t *db_con = NULL; /*!< database connection */
db_func_t db_funcs;		  /*!< Database functions */

/*! \brief
 * Exported functions
 */
static cmd_export_t cmds[] = {
	{"sip_trace", (cmd_function)w_sip_trace0, 0, 0, 0,
		ANY_ROUTE},
	{"sip_trace", (cmd_function)w_sip_trace1, 1, fixup_siptrace, 0,
		ANY_ROUTE},
	{"sip_trace", (cmd_function)w_sip_trace2, 2, fixup_siptrace, fixup_free_siptrace,
		ANY_ROUTE},
	{"sip_trace", (cmd_function)w_sip_trace3, 3, fixup_siptrace, fixup_free_siptrace,
		ANY_ROUTE},
	{"hlog", (cmd_function)w_hlog1, 1, fixup_spve_null, 0,
		ANY_ROUTE},
	{"hlog", (cmd_function)w_hlog2, 2, fixup_spve_spve, 0,
		ANY_ROUTE},
	{"sip_trace_mode", (cmd_function)w_sip_trace_mode, 1, fixup_spve_null,
		fixup_free_spve_null, ANY_ROUTE},
	{0, 0, 0, 0, 0, 0}
};


/*! \brief
 * Exported parameters
 */
static param_export_t params[] = {
	{"auth_key", PARAM_STR, &auth_key_str},
	{"db_url", PARAM_STR, &db_url}, {"table", PARAM_STR, &siptrace_table},
	{"date_column", PARAM_STR, &date_column},
	{"callid_column", PARAM_STR, &callid_column},
	{"traced_user_column", PARAM_STR, &traced_user_column},
	{"msg_column", PARAM_STR, &msg_column},
	{"method_column", PARAM_STR, &method_column},
	{"status_column", PARAM_STR, &status_column},
	{"fromip_column", PARAM_STR, &fromip_column},
	{"toip_column", PARAM_STR, &toip_column},
	{"fromtag_column", PARAM_STR, &fromtag_column},
	{"totag_column", PARAM_STR, &totag_column},
	{"direction_column", PARAM_STR, &direction_column},
	{"trace_flag", INT_PARAM, &trace_flag_param},
	{"trace_on", INT_PARAM, &trace_on},
	{"traced_user_avp", PARAM_STR, &traced_user_avp_str},
	{"trace_table_avp", PARAM_STR, &trace_table_avp_str},
	{"duplicate_uri", PARAM_STR, &dup_uri_str},
	{"trace_to_database", INT_PARAM, &trace_to_database},
	{"trace_local_ip", PARAM_STR, &trace_local_ip},
	{"trace_sl_acks", INT_PARAM, &trace_sl_acks},
	{"xheaders_write", INT_PARAM, &xheaders_write},
	{"xheaders_read", INT_PARAM, &xheaders_read},
	{"hep_mode_on", INT_PARAM, &hep_mode_on},
	{"force_send_sock", PARAM_STR, &force_send_sock_str},
	{"hep_version", INT_PARAM, &hep_version},
	{"hep_capture_id", INT_PARAM, &hep_capture_id},
	{"trace_delayed", INT_PARAM, &trace_delayed},
	{"trace_mode", PARAM_INT, &_siptrace_mode}, {0, 0, 0}
};

#ifdef STATISTICS
stat_var *siptrace_req;
stat_var *siptrace_rpl;

stat_export_t siptrace_stats[] = {
	{"traced_requests", 0, &siptrace_req},
	{"traced_replies", 0, &siptrace_rpl}, {0, 0, 0}
};
#endif

/*! \brief module exports */
struct module_exports exports = {
	"siptrace",     /*!< module name */
	DEFAULT_DLFLAGS, /*!< dlopen flags */
	cmds,			/*!< exported functions */
	params,			/*!< exported parameters */
	0,				/*!< exported rpc functions */
	0,				/*!< exported pseudo-variables */
	0,				/*!< response function */
	mod_init,		/*!< module initialization function */
	child_init,		/*!< child initialization function */
	destroy			/*!< destroy function */
};


/*! \brief Initialize siptrace module */
static int mod_init(void)
{
	pv_spec_t avp_spec;
	sl_cbelem_t slcb;

#ifdef STATISTICS
	/* register statistics */
	if(register_module_stats(exports.name, siptrace_stats) != 0) {
		LM_ERR("failed to register core statistics\n");
		return -1;
	}
#endif

	if(siptrace_init_rpc() != 0) {
		LM_ERR("failed to register RPC commands\n");
		return -1;
	}

	if(trace_flag_param!=-1) {
		if(trace_flag_param < -1 || trace_flag_param > (int)MAX_FLAG) {
			LM_ERR("invalid trace flag %d\n", trace_flag_param);
			return -1;
		}
		trace_flag = 1 << trace_flag_param;
	}

	trace_to_database_flag = (int *)shm_malloc(sizeof(int));
	if(trace_to_database_flag == NULL) {
		LM_ERR("no more shm memory left\n");
		return -1;
	}

	*trace_to_database_flag = trace_to_database;

	if(hep_version != 1 && hep_version != 2 && hep_version != 3) {
		LM_ERR("unsupported version of HEP");
		return -1;
	}

	/* Find a database module if needed */
	if(trace_to_database_flag != NULL && *trace_to_database_flag != 0) {
		if(db_bind_mod(&db_url, &db_funcs)) {
			LM_ERR("unable to bind database module\n");
			return -1;
		}
		if(trace_to_database_flag && !DB_CAPABILITY(db_funcs, DB_CAP_INSERT)) {
			LM_ERR("database modules does not provide all functions needed"
				   " by module\n");
			return -1;
		}
	}

	trace_on_flag = (int *)shm_malloc(sizeof(int));
	if(trace_on_flag == NULL) {
		LM_ERR("no more shm memory left\n");
		return -1;
	}

	*trace_on_flag = trace_on;

	xheaders_write_flag = (int *)shm_malloc(sizeof(int));
	xheaders_read_flag = (int *)shm_malloc(sizeof(int));
	if(!(xheaders_write_flag && xheaders_read_flag)) {
		LM_ERR("no more shm memory left\n");
		return -1;
	}
	*xheaders_write_flag = xheaders_write;
	*xheaders_read_flag = xheaders_read;

	/* register callbacks to TM */
	if(load_tm_api(&tmb) != 0) {
		LM_WARN("can't load tm api. Will not install tm callbacks.\n");
	}

	if (load_dlg_api(&dlgb) < 0) {
		LM_INFO("can't load dlg api. Will not install dialog callbacks.\n");
	} else {
		if (dlgb.register_dlgcb(NULL, DLGCB_CREATED, trace_dialog, NULL, NULL) != 0) {
			LM_ERR("failed to register dialog callbacks! Tracing dialogs won't be available\n");
		}
	}

	/* bind the SL API */
	if(sl_load_api(&slb) != 0) {
		LM_WARN("cannot bind to SL API. Will not install sl callbacks.\n");
	} else {
		/* register sl callbacks */
		memset(&slcb, 0, sizeof(sl_cbelem_t));

		slcb.type = SLCB_REPLY_READY;
		slcb.cbf = trace_sl_onreply_out;
		if(slb.register_cb(&slcb) != 0) {
			LM_ERR("can't register for SLCB_REPLY_READY\n");
			return -1;
		}

		if(trace_sl_acks) {
			slcb.type = SLCB_ACK_FILTERED;
			slcb.cbf = trace_sl_ack_in;
			if(slb.register_cb(&slcb) != 0) {
				LM_ERR("can't register for SLCB_ACK_FILTERED\n");
				return -1;
			}
		}
	}

	if(dup_uri_str.s != 0) {
		dup_uri = (struct sip_uri *)pkg_malloc(sizeof(struct sip_uri));
		if(dup_uri == 0) {
			LM_ERR("no more pkg memory left\n");
			return -1;
		}
		memset(dup_uri, 0, sizeof(struct sip_uri));
		if(parse_uri(dup_uri_str.s, dup_uri_str.len, dup_uri) < 0) {
			LM_ERR("bad duplicate_uri\n");
			return -1;
		}
	}

	if(force_send_sock_str.s != 0) {
		force_send_sock_str.len = strlen(force_send_sock_str.s);
		force_send_sock_uri =
				(struct sip_uri *)pkg_malloc(sizeof(struct sip_uri));
		if(force_send_sock_uri == 0) {
			LM_ERR("no more pkg memory left\n");
			return -1;
		}
		memset(force_send_sock_uri, 0, sizeof(struct sip_uri));
		if(parse_uri(force_send_sock_str.s, force_send_sock_str.len,
				   force_send_sock_uri)
				< 0) {
			LM_ERR("bad force_send_sock\n");
			return -1;
		}
	}

	if(traced_user_avp_str.s && traced_user_avp_str.len > 0) {
		if(pv_parse_spec(&traced_user_avp_str, &avp_spec) == 0
				|| avp_spec.type != PVT_AVP) {
			LM_ERR("malformed or non AVP %.*s AVP definition\n",
					traced_user_avp_str.len, traced_user_avp_str.s);
			return -1;
		}

		if(pv_get_avp_name(
				   0, &avp_spec.pvp, &traced_user_avp, &traced_user_avp_type)
				!= 0) {
			LM_ERR("[%.*s] - invalid AVP definition\n", traced_user_avp_str.len,
					traced_user_avp_str.s);
			return -1;
		}
	} else {
		traced_user_avp.n = 0;
		traced_user_avp_type = 0;
	}
	if(trace_table_avp_str.s && trace_table_avp_str.len > 0) {
		if(pv_parse_spec(&trace_table_avp_str, &avp_spec) == 0
				|| avp_spec.type != PVT_AVP) {
			LM_ERR("malformed or non AVP %.*s AVP definition\n",
					trace_table_avp_str.len, trace_table_avp_str.s);
			return -1;
		}

		if(pv_get_avp_name(
				   0, &avp_spec.pvp, &trace_table_avp, &trace_table_avp_type)
				!= 0) {
			LM_ERR("[%.*s] - invalid AVP definition\n", trace_table_avp_str.len,
					trace_table_avp_str.s);
			return -1;
		}
	} else {
		trace_table_avp.n = 0;
		trace_table_avp_type = 0;
	}

	if(_siptrace_mode == 1) {
		sr_event_register_cb(SREV_NET_DATA_RECV, siptrace_net_data_recv);
		sr_event_register_cb(SREV_NET_DATA_SEND, siptrace_net_data_send);
	}
	return 0;
}


static int child_init(int rank)
{
	if(rank == PROC_INIT || rank == PROC_MAIN || rank == PROC_TCP_MAIN)
		return 0; /* do nothing for the main process */

	if(trace_to_database_flag != NULL && *trace_to_database_flag != 0) {
		db_con = db_funcs.init(&db_url);
		if(!db_con) {
			LM_ERR("unable to connect to database. Please check "
				   "configuration.\n");
			return -1;
		}
		if(DB_CAPABILITY(db_funcs, DB_CAP_QUERY)) {
			if(db_check_table_version(
					&db_funcs, db_con, &siptrace_table, SIP_TRACE_TABLE_VERSION)
						< 0) {
				DB_TABLE_VERSION_ERROR(siptrace_table);
				db_funcs.close(db_con);
				db_con = 0;
				return -1;
			}
		}
	}

	return 0;
}


static void destroy(void)
{
	if(trace_to_database_flag != NULL) {
		if(db_con != NULL) {
			db_funcs.close(db_con);
		}
		shm_free(trace_to_database_flag);
	}

	if(trace_on_flag) {
		shm_free(trace_on_flag);
	}
}

static inline str *siptrace_get_table(void)
{
	static int_str avp_value;
	struct usr_avp *avp;

	if(trace_table_avp.n == 0)
		return &siptrace_table;

	avp = NULL;
	if(trace_table_avp.n != 0)
		avp = search_first_avp(
				trace_table_avp_type, trace_table_avp, &avp_value, 0);

	if(avp == NULL || !is_avp_str_val(avp) || avp_value.s.len <= 0)
		return &siptrace_table;

	return &avp_value.s;
}

static int sip_trace_store(siptrace_data_t *sto, dest_info_t *dst,
		str *correlation_id_str)
{
	int ret = 1;

	if(sto == NULL) {
		LM_DBG("invalid parameter\n");
		return -1;
	}

	gettimeofday(&sto->tv, NULL);

	if(sip_trace_xheaders_read(sto) != 0)
		return -1;

	ret = sip_trace_store_db(sto);

	if(sip_trace_xheaders_write(sto) != 0)
		return -1;

	if(hep_mode_on) {
		trace_send_hep_duplicate(
				&sto->body, &sto->fromip, &sto->toip, dst, correlation_id_str);
	} else {
		/* sip_trace_mode() will not set a destination, uses duplicate_uri */
		if(dst || trace_to_database == 0) {
			trace_send_duplicate(sto->body.s, sto->body.len, dst);
		}
	}

	if(sip_trace_xheaders_free(sto) != 0)
		return -1;

	return ret;
}

static int sip_trace_store_db(siptrace_data_t *sto)
{
	if(trace_to_database_flag == NULL || *trace_to_database_flag == 0)
		goto done;

	if(db_con == NULL) {
		LM_DBG("database connection not initialized\n");
		return -1;
	}

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
	db_vals[6].val.time_val = sto->tv.tv_sec;

	db_keys[7] = &direction_column;
	db_vals[7].type = DB1_STRING;
	db_vals[7].nul = 0;
	db_vals[7].val.string_val = sto->dir;

	db_keys[8] = &fromtag_column;
	db_vals[8].type = DB1_STR;
	db_vals[8].nul = 0;
	db_vals[8].val.str_val = sto->fromtag;

	db_keys[9] = &traced_user_column;
	db_vals[9].type = DB1_STR;
	db_vals[9].nul = 0;

	db_keys[10] = &time_us_column;
	db_vals[10].type = DB1_INT;
	db_vals[10].nul = 0;
	db_vals[10].val.int_val = sto->tv.tv_usec;

	db_keys[11] = &totag_column;
	db_vals[11].type = DB1_STR;
	db_vals[11].nul = 0;
	db_vals[11].val.str_val = sto->totag;

	db_funcs.use_table(db_con, siptrace_get_table());

	if(trace_on_flag != NULL && *trace_on_flag != 0) {
		db_vals[9].val.str_val.s = "";
		db_vals[9].val.str_val.len = 0;

		LM_DBG("storing info...\n");
		if(trace_delayed != 0 && db_funcs.insert_delayed != NULL) {
			if(db_funcs.insert_delayed(db_con, db_keys, db_vals, NR_KEYS) < 0) {
				LM_ERR("error storing trace\n");
				goto error;
			}
		} else {
			if(db_funcs.insert(db_con, db_keys, db_vals, NR_KEYS) < 0) {
				LM_ERR("error storing trace\n");
				goto error;
			}
		}
#ifdef STATISTICS
		update_stat(sto->stat, 1);
#endif
	}

	if(sto->avp == NULL)
		goto done;

	db_vals[9].val.str_val = sto->avp_value.s;

	LM_DBG("storing info...\n");
	if(trace_delayed != 0 && db_funcs.insert_delayed != NULL) {
		if(db_funcs.insert_delayed(db_con, db_keys, db_vals, NR_KEYS) < 0) {
			LM_ERR("error storing trace\n");
			goto error;
		}
	} else {
		if(db_funcs.insert(db_con, db_keys, db_vals, NR_KEYS) < 0) {
			LM_ERR("error storing trace\n");
			goto error;
		}
	}

	sto->avp = search_next_avp(&sto->state, &sto->avp_value);
	while(sto->avp != NULL) {
		db_vals[9].val.str_val = sto->avp_value.s;

		LM_DBG("storing info...\n");
		if(trace_delayed != 0 && db_funcs.insert_delayed != NULL) {
			if(db_funcs.insert_delayed(db_con, db_keys, db_vals, NR_KEYS) < 0) {
				LM_ERR("error storing trace\n");
				goto error;
			}
		} else {
			if(db_funcs.insert(db_con, db_keys, db_vals, NR_KEYS) < 0) {
				LM_ERR("error storing trace\n");
				goto error;
			}
		}
		sto->avp = search_next_avp(&sto->state, &sto->avp_value);
	}

done:
	return 1;
error:
	return -1;
}

enum siptrace_type_t parse_siptrace_flag(str* sflags)
{
	int idx;
	enum siptrace_type_t trace_type = SIPTRACE_NONE;

	if (sflags == NULL || sflags->s == NULL || sflags->len == 0) {
		return SIPTRACE_NONE;
	}

	for (idx = 0; idx < sflags->len; idx++) {
		switch(sflags->s[idx]|0x20) { /*|0x20 - to lowercase */
			case SIPTRACE_MESSAGE:
			case SIPTRACE_TRANSACTION:
			case SIPTRACE_DIALOG:
				if (trace_type != SIPTRACE_NONE) {
					LM_ERR("only one tracing flag can be used <%.*s>!\n",
							sflags->len, sflags->s);
					return SIPTRACE_NONE;
				}

				trace_type = (sflags->s[idx]|0x20);
				break;
			case ' ':
			case '\t':
				break;
			default:
				LM_ERR("Invalid character <%c> in <%.*s> at position <%d>!\n", sflags->s[idx],
						sflags->len, sflags->s, idx);
				return SIPTRACE_NONE;
		}
	}

	return trace_type;
}

static int fixup_siptrace(void **param, int param_no)
{
	str sflags;
	enum siptrace_type_t trace_type;

	if(param_no < 1 || param_no > 3) {
		LM_DBG("params:%s\n", (char *)*param);
		return 0;
	}

	if (param_no == 1 || param_no == 2) {
		/* correlation id */
		return fixup_spve_all(param, param_no);
	} else if (param_no == 3) {
		/* tracing type; string only */
		sflags.s = (char *)*param;
		sflags.len = strlen(sflags.s);

		trace_type = parse_siptrace_flag(&sflags);
		if (trace_type == SIPTRACE_NONE) {
			LM_ERR("Failed to parse trace type!\n");
			return -1;
		}

		*param = pkg_malloc(sizeof(trace_type));
		if (*param == NULL) {
			LM_ERR("no more pkg memory!\n");
			return -1;
		}
		memcpy(*param, &trace_type, sizeof(trace_type));
	}

	return 0;
}

static int fixup_free_siptrace(void **param, int param_no)
{
	if (param_no == 1 || param_no == 2) {
		/* correlation id */
		return fixup_free_spve_all(param, param_no);
	} if (param_no == 3) {
		/* tracing type; string only */
		if (*param) {
			pkg_free(*param);
		}
	}

	return 0;
}


/**
 *
 * parse_siptrace_uri (to replace siptrace_fixup and ki_sip_trace_dst_cid beginning)
 * parse_siptrace_type
 *
 */

static int parse_siptrace_uri(str* duri, dest_info_t* dst)
{
	sip_uri_t uri;
	proxy_l_t *p = NULL;

	if (dst == NULL) {
		LM_ERR("bad destination!\n");
		return -1;
	}

	if (duri == NULL || duri->len <= 0) {
		if(dup_uri) {
			uri = *dup_uri;
		} else {
			LM_ERR("Missing duplicate URI\n");
			return -1;
		}
	} else {
		memset(&uri, 0, sizeof(struct sip_uri));
		if(parse_uri(duri->s, duri->len, &uri) < 0) {
			LM_ERR("bad dup uri\n");
			return -1;
		}
	}

	init_dest_info(dst);

	/* create a temporary proxy*/
	dst->proto = PROTO_UDP;
	p = mk_proxy(&uri.host, (uri.port_no) ? uri.port_no : SIP_PORT, dst->proto);
	if(p == 0) {
		LM_ERR("bad host name in uri\n");
		return -1;
	}

	hostent2su(&dst->to, &p->host, p->addr_idx, (p->port) ? p->port : SIP_PORT);

	/* free temporary proxy*/
	if(p) {
		free_proxy(p); /* frees only p content, not p itself */
		pkg_free(p);
	}

	return 0;
}

/**
 *
 */
static int sip_trace_helper(sip_msg_t *msg, dest_info_t *dst, str *duri,
		str *corid, char *dir, enum siptrace_type_t trace_type)
{
	siptrace_info_t* info = NULL;
	char *p = NULL;

	if (trace_type == SIPTRACE_TRANSACTION || trace_type == SIPTRACE_DIALOG) {
		int alloc_size = sizeof(siptrace_info_t);

		/*
		 * for each type check that conditions are created
		 * transaction: it's a request starting a transaction; tm module loaded
		 * dialog: it's an INVITE; dialog module is loaded
		 *
		 * */
		if (tmb.t_gett == NULL) {
			LM_WARN("TM module not loaded! Tracing only current message!\n");
			goto trace_current;
		}

		if (trace_type == SIPTRACE_DIALOG && dlgb.get_dlg == NULL) {
			LM_WARN("DIALOG module not loaded! Tracing only current message!\n");
			goto trace_current;
		}

		if (msg->first_line.type != SIP_REQUEST ||
				(trace_type == SIPTRACE_DIALOG && msg->first_line.u.request.method_value != METHOD_INVITE)) {
			LM_WARN("When tracing a %s sip_trace() has to be initiated on the %s\n",
					trace_type == SIPTRACE_TRANSACTION ? "transaction" : "dialog",
					trace_type == SIPTRACE_TRANSACTION ? "request message" : "initial invite");
			return -1;
		}

		if (corid) {
			alloc_size += corid->len;
		}

		if (duri) {
			alloc_size += duri->len;
		}

		info = shm_malloc(alloc_size);
		if (info == NULL) {
			LM_ERR("No more shm!\n");
			return -1;
		}
		memset(info, 0, alloc_size);

		p = (char *)(info + 1);
		/* could use the dest_info we've already parsed but there's no way to pass
		 * it to DLGCB_CREATED callback so the only thing to do is keep
		 * it as uri, serialize in a dlg_var and parse again in DLGCB_CREATED */
		if(corid) {
			info->correlation_id.s = p;
			info->correlation_id.len = corid->len;
			memcpy(info->correlation_id.s, corid->s, corid->len);
		}
		if (duri) {
			info->uriState = STRACE_RAW_URI;
			info->u.dup_uri.s = p + ((info->correlation_id.s)?info->correlation_id.len:0);
			memcpy(info->u.dup_uri.s, duri->s, duri->len);
			info->u.dup_uri.len = duri->len;
		} else {
			info->uriState = STRACE_UNUSED_URI;
		}

		if (trace_type == SIPTRACE_TRANSACTION) {
			trace_transaction(msg, info, 0);
		} else if (trace_type == SIPTRACE_DIALOG) {
			if (unlikely(dlgb.set_dlg_var == NULL)) {
				/* FIXME should we abort tracing here? */
				LM_WARN("Dialog api not loaded! will trace only current transaction!\n");
			} else {
				/* serialize what's in info */
				/* save correlation id in siptrace_info avp
				 * we want to have traced user avp value at the moment of sip_trace function call*/
				if (add_info_xavp(info) < 0) {
					LM_ERR("failed to serialize siptrace info! Won't trace dialog!\n");
					return -1;
				} else {
					msg->msg_flags |= FL_SIPTRACE;
				}
			}

			/**
			 * WARNING: don't move trace_transaction before  add_info_xavp()
			 * add_info_xavp() expects the URI in RAW format, unparsed
			 * trace_transaction() parses the URI if it finds it in raw format;
			 * a BUG will be thrown if this happens
			 */
			trace_transaction(msg, info, 1);
		}
	}

	if(trace_type != SIPTRACE_MESSAGE && trace_is_off(msg)) {
		LM_DBG("trace off...\n");
		return 1;
	}

trace_current:
	return sip_trace(msg, dst, corid, dir);
}

/**
 * Send sip trace with destination and correlation id and specify what messages to be traced
 */
static int ki_sip_trace_dst_cid_flag(sip_msg_t *msg, str *duri, str *cid, str* sflag)
{
	dest_info_t dst;
	enum siptrace_type_t trace_type;

	if(duri) {
		if (parse_siptrace_uri(duri, &dst) < 0) {
			LM_ERR("failed to parse siptrace uri!\n");
			return -1;
		}
	}

	if (sflag) {
		trace_type = parse_siptrace_flag(sflag);
		if (trace_type == SIPTRACE_NONE) {
			LM_ERR("Invalid flags <%.*s>\n", sflag->len, sflag->s);
		}
	} else {
		trace_type = SIPTRACE_MESSAGE;
	}

	return sip_trace_helper(msg, (duri)?&dst:NULL, duri, cid, NULL, trace_type);
}

/**
 * Send sip trace with destination and correlation id
 */
static int ki_sip_trace_dst_cid(sip_msg_t *msg, str *duri, str *cid)
{
	return ki_sip_trace_dst_cid_flag(msg, duri, cid, NULL);
}

/**
 * Send sip trace with destination
 */
static int ki_sip_trace_dst(sip_msg_t *msg, str *duri)
{
	return ki_sip_trace_dst_cid_flag(msg, duri, NULL, NULL);
}

/**
 *
 */
static int ki_sip_trace(sip_msg_t *msg)
{
	return ki_sip_trace_dst_cid_flag(msg, NULL, NULL, NULL);
}

/**
 *
 */
static int w_sip_trace0(sip_msg_t *msg, char *dest, char *correlation_id)
{
	return w_sip_trace3(msg, NULL, NULL, NULL);
}

/**
 *
 */
static int w_sip_trace1(sip_msg_t *msg, char *dest, char *p2)
{
	return w_sip_trace3(msg, dest, NULL, NULL);
}

/**
 *
 */
static int w_sip_trace2(sip_msg_t *msg, char *dest, char *correlation_id)
{
	return w_sip_trace3(msg, dest, correlation_id, NULL);
}


static int w_sip_trace3(sip_msg_t *msg, char *dest, char *correlation_id, char *trace_type_p)
{
	str dup_uri_param_str = {0, 0};
	str correlation_id_str = {0, 0};
	dest_info_t dest_info;
	enum siptrace_type_t trace_type;

	/* to support tracing to database without destination parameter - old mode */
	if (dest || trace_to_database == 0) {
		if (dest) {
			if(fixup_get_svalue(msg, (gparam_t *)dest, &dup_uri_param_str) != 0) {
				LM_ERR("unable to parse the dest URI string\n");
				return -1;
			}
		}

		if (dup_uri_param_str.s == 0 || (is_null_pv(dup_uri_param_str))) {
			if (dup_uri_str.s == 0 || dup_uri_str.len == 0) {
				LM_ERR("no duplicate_uri modparam nor duplicate uri sip_trace() argument provided!\n");
				return -1;
			}

			dup_uri_param_str = dup_uri_str;
		}

		/* if arg dest uri is null  dup_uri_param_str will have length 0 and global dup_uri will be used */
		if (parse_siptrace_uri(&dup_uri_param_str, &dest_info) < 0) {
			LM_ERR("failed to parse uri!\n");
			return -1;
		}
	} else {
		memset(&dest_info, 0, sizeof(dest_info_t));
	}

	if (correlation_id) {
		if(fixup_get_svalue(msg, (gparam_t *)correlation_id, &correlation_id_str)
				!= 0) {
			LM_ERR("unable to parse the correlation id\n");
			return -1;
		}
	}

	if (trace_type_p != NULL) {
		trace_type = *(enum siptrace_type_t *)(trace_type_p);
	} else {
		/* fallback to default - transaction tracking when flag is set,
		 * otherwise only the current message*/
		if(msg->flags & trace_flag) {
			trace_type = SIPTRACE_TRANSACTION;
		} else {
			trace_type = SIPTRACE_MESSAGE;
		}
	}

	return sip_trace_helper(msg, (dest)?&dest_info:NULL,
			(dest)?&dup_uri_param_str:NULL,
			(correlation_id)?&correlation_id_str:NULL, NULL, trace_type);
}

static int sip_trace(sip_msg_t *msg, dest_info_t *dst,
		str *correlation_id_str, char *dir)
{
	siptrace_data_t sto;
	onsend_info_t *snd_inf = NULL;

	if(msg == NULL) {
		LM_DBG("nothing to trace\n");
		return -1;
	}

	if(dst) {
		if(dst->send_sock == 0) {
			dst->send_sock = get_send_socket(0, &dst->to, dst->proto);
			if(dst->send_sock == 0) {
				LM_ERR("can't forward to af %d, proto %d no corresponding"
					   " listening socket\n",
						dst->to.s.sa_family, dst->proto);
				return -1;
			}
		}
	}

	memset(&sto, 0, sizeof(siptrace_data_t));

	if(traced_user_avp.n != 0)
		sto.avp = search_first_avp(traced_user_avp_type, traced_user_avp,
				&sto.avp_value, &sto.state);

	if((sto.avp == NULL) && (trace_on_flag == NULL || *trace_on_flag == 0)) {
		LM_DBG("trace off...\n");
		return -1;
	}

	if(sip_trace_prepare(msg) < 0)
		return -1;

	sto.callid = msg->callid->body;

	if(msg->first_line.type == SIP_REQUEST) {
		sto.method = msg->first_line.u.request.method;
	} else {
		if(parse_headers(msg, HDR_CSEQ_F, 0) != 0 || msg->cseq == NULL
				|| msg->cseq->parsed == NULL) {
			LM_ERR("cannot parse cseq header\n");
			return -1;
		}
		sto.method = get_cseq(msg)->method;
	}

	if(msg->first_line.type == SIP_REPLY) {
		sto.status = msg->first_line.u.reply.status;
	} else {
		sto.status.s = "";
		sto.status.len = 0;
	}

	snd_inf = get_onsend_info();
	if(snd_inf == NULL) {
		sto.body.s = msg->buf;
		sto.body.len = msg->len;

		sto.dir = (dir) ? dir : "in";

		if(trace_local_ip.s && trace_local_ip.len > 0
				&& strncmp(sto.dir, "out", 3) == 0) {
			sto.fromip = trace_local_ip;
		} else {
			sto.fromip.len = snprintf(sto.fromip_buff, SIPTRACE_ADDR_MAX, "%s:%s:%d",
					siptrace_proto_name(msg->rcv.proto),
					ip_addr2a(&msg->rcv.src_ip), (int)msg->rcv.src_port);
			if(sto.fromip.len<0 || sto.fromip.len>=SIPTRACE_ADDR_MAX) {
				LM_ERR("failed to format toip buffer (%d)\n", sto.fromip.len);
				sto.fromip.s = SIPTRACE_ANYADDR;
				sto.fromip.len = SIPTRACE_ANYADDR_LEN;
			} else {
				sto.fromip.s = sto.fromip_buff;
			}
		}

		if(trace_local_ip.s && trace_local_ip.len > 0
				&& strncmp(sto.dir, "in", 2) == 0) {
			sto.toip = trace_local_ip;
		} else {
			sto.toip.len = snprintf(sto.toip_buff, SIPTRACE_ADDR_MAX, "%s:%s:%d",
					siptrace_proto_name(msg->rcv.proto), ip_addr2a(&msg->rcv.dst_ip),
					(int)msg->rcv.dst_port);
			if(sto.toip.len<0 || sto.toip.len>=SIPTRACE_ADDR_MAX) {
				LM_ERR("failed to format toip buffer (%d)\n", sto.toip.len);
				sto.toip.s = SIPTRACE_ANYADDR;
				sto.toip.len = SIPTRACE_ANYADDR_LEN;
			} else {
				sto.toip.s = sto.toip_buff;
			}
		}
	} else {
		sto.body.s = snd_inf->buf;
		sto.body.len = snd_inf->len;

		if(trace_local_ip.s && trace_local_ip.len > 0) {
			sto.fromip = trace_local_ip;
		} else {
			if(snd_inf->send_sock->sock_str.len>=SIPTRACE_ADDR_MAX-1) {
				LM_WARN("local socket address is too large\n");
				sto.fromip.s = SIPTRACE_ANYADDR;
				sto.fromip.len = SIPTRACE_ANYADDR_LEN;
			} else {
				strncpy(sto.fromip_buff, snd_inf->send_sock->sock_str.s,
						snd_inf->send_sock->sock_str.len);
				sto.fromip.s = sto.fromip_buff;
				sto.fromip.len = snd_inf->send_sock->sock_str.len;
			}
		}

		sto.toip.len = snprintf(sto.toip_buff, SIPTRACE_ADDR_MAX, "%s:%s:%d",
				siptrace_proto_name(snd_inf->send_sock->proto),
				suip2a(snd_inf->to, sizeof(*snd_inf->to)),
				(int)su_getport(snd_inf->to));
		if(sto.toip.len<0 || sto.toip.len>=SIPTRACE_ADDR_MAX) {
			LM_ERR("failed to format toip buffer (%d)\n", sto.toip.len);
			sto.toip.s = SIPTRACE_ANYADDR;
			sto.toip.len = SIPTRACE_ANYADDR_LEN;
		} else {
			sto.toip.s = sto.toip_buff;
		}

		sto.dir = "out";
	}

	sto.fromtag = get_from(msg)->tag_value;
	sto.totag = get_to(msg)->tag_value;

#ifdef STATISTICS
	if(msg->first_line.type == SIP_REPLY) {
		sto.stat = siptrace_rpl;
	} else {
		sto.stat = siptrace_req;
	}
#endif
	return sip_trace_store(&sto, dst, correlation_id_str);
}


/**
 *
 */
static int ki_sip_trace_mode(sip_msg_t *msg, str *smode)
{
	enum siptrace_type_t trace_type;

	if(smode==NULL || smode->s==NULL || smode->len<=0) {
		LM_INFO("no tracing mode - trace message\n");
		trace_type = SIPTRACE_MESSAGE;
	} else {
		switch(smode->s[0]) {
			case 'M':
			case 'm':
				trace_type = SIPTRACE_MESSAGE;
			break;
			case 'T':
			case 't':
				trace_type = SIPTRACE_TRANSACTION;
			break;
			case 'D':
			case 'd':
				trace_type = SIPTRACE_DIALOG;
			break;
			default:
				trace_type = SIPTRACE_MESSAGE;
				LM_INFO("unexpected tracing mode [%.*s] - trace message\n",
						smode->len, smode->s);
		}
	}

	return sip_trace_helper(msg, NULL, NULL, NULL, NULL, trace_type);
}

/**
 *
 */
static int w_sip_trace_mode(sip_msg_t *msg, char *pmode, char *p2)
{
	str smode = STR_NULL;
	if(fixup_get_svalue(msg, (gparam_t*)pmode, &smode)<0) {
		LM_ERR("failed to get tracing mode parameter\n");
		return -1;
	}
	return ki_sip_trace_mode(msg, &smode);
}

static void trace_onreq_out(struct cell *t, int type, struct tmcb_params *ps)
{
	siptrace_data_t sto;
	siptrace_info_t* info;
	sip_msg_t *msg;
	ip_addr_t to_ip;
	dest_info_t *dst;

	if(t == NULL || ps == NULL) {
		LM_ERR("unexpected parameter values\n");
		return;
	}

	if(ps->flags & TMCB_RETR_F) {
		LM_DBG("retransmission - ignoring\n");
		return;
	}
	info = (siptrace_info_t *)(*ps->param);

	msg = ps->req;
	if(msg == NULL) {
		/* check if it is outgoing cancel, t is INVITE
		 * and send_buf starts with CANCEL */
		if(is_invite(t) && ps->send_buf.len > 7
				&& strncmp(ps->send_buf.s, "CANCEL ", 7) == 0) {
			msg = t->uas.request;
			if(msg == NULL) {
				LM_DBG("no uas msg for INVITE transaction\n");
				return;
			} else {
				LM_DBG("recording CANCEL based on INVITE transaction\n");
			}
		} else {
			LM_DBG("no uas msg, local transaction\n");
			return;
		}
	}

	/* for incoming cancel this is the only play(i've found) where I have the CANCEL transaction
	 * and can register a callback for the reply */
	memset(&sto, 0, sizeof(siptrace_data_t));

	if (unlikely(type == TMCB_E2ECANCEL_IN)) {
		msg->msg_flags |= FL_SIPTRACE;

		if(tmb.register_tmcb(msg, 0, TMCB_RESPONSE_READY, trace_onreply_out, info, 0)
					<= 0) {
			LM_ERR("can't register trace_onreply_out\n");
			return;
		}
	} else {
		if(traced_user_avp.n != 0)
			sto.avp = search_first_avp(traced_user_avp_type, traced_user_avp,
					&sto.avp_value, &sto.state);
	}

	if((sto.avp == NULL) && trace_is_off(msg)) {
		LM_DBG("trace off...\n");
		return;
	}

	if(sip_trace_prepare(msg) < 0)
		return;

	if (unlikely(type == TMCB_E2ECANCEL_IN)) {
		sto.body.s = msg->buf;
		sto.body.len = msg->len;
	} else {
		if(ps->send_buf.len > 0) {
			sto.body = ps->send_buf;
		} else {
			sto.body.s = "No request buffer";
			sto.body.len = sizeof("No request buffer") - 1;
		}
	}

	sto.callid = msg->callid->body;

	if(ps->send_buf.len > 10) {
		sto.method.s = ps->send_buf.s;
		sto.method.len = 0;
		while(sto.method.len < ps->send_buf.len) {
			if(ps->send_buf.s[sto.method.len] == ' ')
				break;
			sto.method.len++;
		}
		if(sto.method.len == ps->send_buf.len)
			sto.method.len = 10;
	} else {
		sto.method = t->method;
	}

	sto.status.s = "";
	sto.status.len = 0;

	memset(&to_ip, 0, sizeof(struct ip_addr));
	/* destination info from the original message
	 * used to fetch information to set the from and to for this message
	 * different from the dest_info in siptrace_info which is the socket
	 * used to send the message */
	dst = ps->dst;

	if(trace_local_ip.s && trace_local_ip.len > 0) {
		sto.fromip = trace_local_ip;
	} else {
		if(dst == 0 || dst->send_sock == 0 || dst->send_sock->sock_str.s == 0) {
			sto.fromip.len = snprintf(sto.fromip_buff, SIPTRACE_ADDR_MAX, "%s:%s:%d",
					siptrace_proto_name(msg->rcv.proto),
					ip_addr2a(&msg->rcv.dst_ip), (int)msg->rcv.dst_port);
			if(sto.fromip.len<0 || sto.fromip.len>=SIPTRACE_ADDR_MAX) {
				LM_ERR("failed to format toip buffer (%d)\n", sto.fromip.len);
				sto.fromip.s = SIPTRACE_ANYADDR;
				sto.fromip.len = SIPTRACE_ANYADDR_LEN;
			} else {
				sto.fromip.s = sto.fromip_buff;
			}
		} else {
			sto.fromip = dst->send_sock->sock_str;
		}
	}

	if(dst == 0) {
		sto.toip.s = SIPTRACE_ANYADDR;
		sto.toip.len = SIPTRACE_ANYADDR_LEN;
	} else {
		su2ip_addr(&to_ip, &dst->to);
		sto.toip.len = snprintf(sto.toip_buff, SIPTRACE_ADDR_MAX, "%s:%s:%d",
				siptrace_proto_name(dst->proto),
				ip_addr2a(&to_ip), (int)su_getport(&dst->to));
		if(sto.toip.len<0 || sto.toip.len>=SIPTRACE_ADDR_MAX) {
			LM_ERR("failed to format toip buffer (%d)\n", sto.toip.len);
			sto.toip.s = SIPTRACE_ANYADDR;
			sto.toip.len = SIPTRACE_ANYADDR_LEN;
		} else {
			sto.toip.s = sto.toip_buff;
		}
	}

	/* FIXME the callback is designed for outgoing requests but this along with
	 * the callback registration at the begining of the function it's for a special
	 * case - incoming CANCEL transactions; they were not traced before; TMCB_E2ECANCEL_IN
	 * will throw the incoming request through this function and the callback in the beginning
	 * will make sure the reply for this cancel is caught */
	if (unlikely(type == TMCB_E2ECANCEL_IN)) {
		sto.dir = "in";
	} else {
		sto.dir = "out";
	}

	sto.fromtag = get_from(msg)->tag_value;
	sto.totag = get_to(msg)->tag_value;

#ifdef STATISTICS
	sto.stat = siptrace_req;
#endif

	if (info->uriState == STRACE_RAW_URI) {
		LM_BUG("uriState must be either UNUSED or PARSED here! must be a bug! Message won't be traced!\n");
		return;
	}

	sip_trace_store(&sto, info->uriState == STRACE_PARSED_URI ? &info->u.dest_info : NULL, NULL);
	return;
}

static void trace_onreply_in(struct cell *t, int type, struct tmcb_params *ps)
{
	siptrace_data_t sto;
	siptrace_info_t* info;
	sip_msg_t *msg;
	sip_msg_t *req;
	char statusbuf[INT2STR_MAX_LEN];

	if(t == NULL || t->uas.request == 0 || ps == NULL) {
		LM_DBG("no uas request, local transaction\n");
		return;
	}

	req = ps->req;
	msg = ps->rpl;
	if((type != TMCB_ACK_NEG_IN) && (msg == NULL || req == NULL)) {
		LM_DBG("no reply\n");
		return;
	}
	info = (siptrace_info_t *)(*ps->param);

	memset(&sto, 0, sizeof(siptrace_data_t));

	if(traced_user_avp.n != 0)
		sto.avp = search_first_avp(traced_user_avp_type, traced_user_avp,
				&sto.avp_value, &sto.state);

	if((type != TMCB_ACK_NEG_IN) && ((sto.avp == NULL) && trace_is_off(req))) {
		LM_DBG("trace off... %d %d\n", sto.avp == NULL, trace_is_off(req));
		return;
	}

	if(sip_trace_prepare(msg) < 0)
		return;

	sto.body.s = msg->buf;
	sto.body.len = msg->len;

	sto.callid = msg->callid->body;

	sto.method = get_cseq(msg)->method;

	sto.status.s = int2strbuf(ps->code, statusbuf, INT2STR_MAX_LEN, &sto.status.len);
	if(sto.status.s == 0) {
		LM_ERR("failure to get the status string\n");
		return;
	}

	sto.fromip.len = snprintf(sto.fromip_buff, SIPTRACE_ADDR_MAX, "%s:%s:%d",
			siptrace_proto_name(msg->rcv.proto),
			ip_addr2a(&msg->rcv.src_ip), (int)msg->rcv.src_port);
	if(sto.fromip.len<0 || sto.fromip.len>=SIPTRACE_ADDR_MAX) {
		LM_ERR("failed to format fromip buffer (%d)\n", sto.fromip.len);
		sto.fromip.s = SIPTRACE_ANYADDR;
		sto.fromip.len = SIPTRACE_ANYADDR_LEN;
	} else {
		sto.fromip.s = sto.fromip_buff;
	}

	if(trace_local_ip.s && trace_local_ip.len > 0) {
		sto.toip = trace_local_ip;
	} else {
		sto.toip.len = snprintf(sto.toip_buff, SIPTRACE_ADDR_MAX, "%s:%s:%d",
				siptrace_proto_name(msg->rcv.proto),
				ip_addr2a(&msg->rcv.dst_ip), (int)msg->rcv.dst_port);
		if(sto.toip.len<0 || sto.toip.len>=SIPTRACE_ADDR_MAX) {
			LM_ERR("failed to format toip buffer (%d)\n", sto.toip.len);
			sto.toip.s = SIPTRACE_ANYADDR;
			sto.toip.len = SIPTRACE_ANYADDR_LEN;
		} else {
			sto.toip.s = sto.toip_buff;
		}
	}

	sto.dir = "in";

	sto.fromtag = get_from(msg)->tag_value;
	sto.totag = get_to(msg)->tag_value;
#ifdef STATISTICS
	sto.stat = siptrace_rpl;
#endif

	if (info->uriState == STRACE_RAW_URI) {
		LM_BUG("uriState must be either UNUSED or PARSED here! must be a bug! Message won't be traced!\n");
		return;
	}

	sip_trace_store(&sto, info->uriState == STRACE_PARSED_URI ? &info->u.dest_info : NULL, NULL);
	return;
}

static void trace_onreply_out(struct cell *t, int type, struct tmcb_params *ps)
{
	siptrace_data_t sto;
	siptrace_info_t* info;
	int faked = 0;
	int parsed_f = 0;
	struct sip_msg *msg;
	struct sip_msg *req;
	struct ip_addr to_ip;
	char statusbuf[INT2STR_MAX_LEN];
	dest_info_t *dst;

	if(t == NULL || t->uas.request == 0 || ps == NULL) {
		LM_DBG("no uas request, local transaction\n");
		return;
	}

	if(ps->flags & TMCB_RETR_F) {
		LM_DBG("retransmission\n");
		return;
	}

	info = (siptrace_info_t *)(*ps->param);

	memset(&sto, 0, sizeof(siptrace_data_t));

	/* can't(don't know) set FL_SIPTRACE flag from trace_onreq_out because
	 * there no access to CANCEL transaction there */
	if (likely(type != TMCB_RESPONSE_READY)) {
		if(traced_user_avp.n != 0)
			sto.avp = search_first_avp(traced_user_avp_type, traced_user_avp,
					&sto.avp_value, &sto.state);

		if((sto.avp == NULL) && trace_is_off(t->uas.request)) {
			LM_DBG("trace off...\n");
			return;
		}
	}

	req = ps->req;
	msg = ps->rpl;
	if(msg == NULL || msg == FAKED_REPLY) {
		msg = t->uas.request;
		faked = 1;
		/* check if from header has been already parsed.
		 * If not we have to parse it in pkg memory and free it at the end.
		 */
		if (msg->from && msg->from->parsed == NULL)
			parsed_f = 1;
	}

	if(sip_trace_prepare(msg) < 0)
		return;

	if(faked == 0) {
		if(ps->send_buf.len > 0) {
			sto.body = ps->send_buf;
		} else if(t->uas.response.buffer != NULL) {
			sto.body.s = t->uas.response.buffer;
			sto.body.len = t->uas.response.buffer_len;
		} else if(msg->len > 0) {
			sto.body.s = msg->buf;
			sto.body.len = msg->len;
		} else {
			sto.body.s = "No reply buffer";
			sto.body.len = sizeof("No reply buffer") - 1;
		}
	} else {
		if(ps->send_buf.len > 0) {
			sto.body = ps->send_buf;
		} else if(t->uas.response.buffer != NULL) {
			sto.body.s = t->uas.response.buffer;
			sto.body.len = t->uas.response.buffer_len;
		} else {
			sto.body.s = "No reply buffer";
			sto.body.len = sizeof("No reply buffer") - 1;
		}
	}

	sto.callid = msg->callid->body;
	sto.method = get_cseq(msg)->method;

	if(trace_local_ip.s && trace_local_ip.len > 0) {
		sto.fromip = trace_local_ip;
	} else {
		sto.fromip.len = snprintf(sto.fromip_buff, SIPTRACE_ADDR_MAX, "%s:%s:%d",
				siptrace_proto_name(msg->rcv.proto),
				ip_addr2a(&req->rcv.dst_ip), (int)req->rcv.dst_port);
		if(sto.fromip.len<0 || sto.fromip.len>=SIPTRACE_ADDR_MAX) {
			LM_ERR("failed to format fromip buffer (%d)\n", sto.fromip.len);
			sto.fromip.s = SIPTRACE_ANYADDR;
			sto.fromip.len = SIPTRACE_ANYADDR_LEN;
		} else {
			sto.fromip.s = sto.fromip_buff;
		}
	}

	sto.status.s = int2strbuf(ps->code, statusbuf, INT2STR_MAX_LEN, &sto.status.len);
	if(sto.status.s == 0) {
		LM_ERR("failure to get the status string\n");
		return;
	}

	memset(&to_ip, 0, sizeof(struct ip_addr));
	dst = ps->dst;
	if(dst == 0) {
		sto.toip.s = SIPTRACE_ANYADDR;
		sto.toip.len = SIPTRACE_ANYADDR_LEN;
	} else {
		su2ip_addr(&to_ip, &dst->to);
		sto.toip.len = snprintf(sto.toip_buff, SIPTRACE_ADDR_MAX, "%s:%s:%d",
				siptrace_proto_name(dst->proto),
				ip_addr2a(&to_ip), (int)su_getport(&dst->to));
		if(sto.toip.len<0 || sto.toip.len>=SIPTRACE_ADDR_MAX) {
			LM_ERR("failed to format toip buffer (%d)\n", sto.toip.len);
			sto.toip.s = SIPTRACE_ANYADDR;
			sto.toip.len = SIPTRACE_ANYADDR_LEN;
		} else {
			sto.toip.s = sto.toip_buff;
		}
	}

	sto.dir = "out";
	sto.fromtag = get_from(msg)->tag_value;
	sto.totag = get_to(msg)->tag_value;

#ifdef STATISTICS
	sto.stat = siptrace_rpl;
#endif

	if (info->uriState == STRACE_RAW_URI) {
		LM_BUG("uriState must be either UNUSED or PARSED here! must be a bug! Message won't be traced!\n");
		goto end;
	}

	sip_trace_store(&sto, info->uriState == STRACE_PARSED_URI ? &info->u.dest_info : NULL, NULL);

end:
	if (faked && parsed_f) {
		free_from(msg->from->parsed);
		msg->from->parsed = NULL;
	}
}

static void trace_tm_neg_ack_in(struct cell *t, int type, struct tmcb_params *ps)
{
	siptrace_info_t* info = (siptrace_info_t *)(*ps->param);

	LM_DBG("storing negative ack...\n");
	/* this condition should not exist but there seems to be a BUG in kamailio
	 * letting requests other than the ACK inside */
	if (ps->req->first_line.u.request.method_value != METHOD_ACK) {
		return;
	}

	if (info->uriState == STRACE_RAW_URI) {
		LM_BUG("uriState must be either UNUSED or PARSED here! must be a bug! Message won't be traced!\n");
		return;
	}

	if(trace_is_off(ps->req)) {
		LM_DBG("trace off...\n");
		return;
	}

	sip_trace(ps->req, (info->uriState == STRACE_PARSED_URI) ? &info->u.dest_info : NULL,
			NULL, NULL);
}

/**
 * if any param inside info structure is NULL or has 0 length it will not be added
 * if no param set data will have allocated 2 bytes and length 0
 * if at least one param has length > 0
 *
 * data format:
 *
 * | total length | duri_length | duri*   | corr id length | corr id*
 * | 2 bytes      | 2 bytes     | x bytes | 2 bytes        | x bytes
 * params marked with * are optional
 *
 */
static int add_info_xavp(siptrace_info_t* info)
{
	sr_xval_t xval;

	if (info == NULL) {
		LM_ERR("Nothing to serialize!\n");
		return -1;
	}

	if (info->uriState != STRACE_RAW_URI) {
		LM_BUG("URI should be in raw format here\n");
		return -1;
	}

	memset(&xval, 0, sizeof(sr_xval_t));
	xval.type = SR_XTYPE_VPTR;
	xval.v.vptr = (void *)info;

	/* save data into avp */
	if (xavp_add_value(&xavp_trace_info_name_s, &xval, NULL) == NULL) {
		shm_free(info);
		LM_ERR("Failed to add xavp!\n");
		return -1;
	}

	return 0;
}

static inline int parse_raw_uri(siptrace_info_t* info)
{
	dest_info_t dest_info;

	if (info == NULL) {
		LM_ERR("bad function call\n");
		return -1;
	}

	if (info->uriState != STRACE_RAW_URI) {
		LM_ERR("Invalid call! siptrace_info must contain a sip uri string!\n");
		return -1;
	}

	/* parse uri and get dest_info structure */
	if (parse_siptrace_uri(&info->u.dup_uri, &dest_info) < 0) {
		LM_ERR("failed to parse uri!\n");
		return -1;
	}

	info->u.dest_info = dest_info;
	info->uriState = STRACE_PARSED_URI;

	return 0;
}

static void trace_sl_ack_in(sl_cbp_t *slcbp)
{
	sip_msg_t *req;
	LM_DBG("storing ack...\n");
	req = slcbp->req;
	sip_trace(req, 0, NULL, NULL);
}

static void trace_sl_onreply_out(sl_cbp_t *slcbp)
{
	sip_msg_t *req;
	siptrace_data_t sto;
	sip_msg_t *msg;
	ip_addr_t to_ip;
	char statusbuf[INT2STR_MAX_LEN];

	if(slcbp == NULL || slcbp->req == NULL) {
		LM_ERR("bad parameters\n");
		return;
	}
	req = slcbp->req;

	memset(&sto, 0, sizeof(siptrace_data_t));
	if(traced_user_avp.n != 0)
		sto.avp = search_first_avp(traced_user_avp_type, traced_user_avp,
				&sto.avp_value, &sto.state);

	if((sto.avp == NULL) && trace_is_off(req)) {
		LM_DBG("trace off...\n");
		return;
	}

	msg = req;

	if(sip_trace_prepare(msg) < 0)
		return;

	sto.body.s = (slcbp->reply) ? slcbp->reply->s : "";
	sto.body.len = (slcbp->reply) ? slcbp->reply->len : 0;

	sto.callid = msg->callid->body;
	sto.method = msg->first_line.u.request.method;

	if(trace_local_ip.len > 0) {
		sto.fromip = trace_local_ip;
	} else {
		sto.fromip.len = snprintf(sto.fromip_buff, SIPTRACE_ADDR_MAX, "%s:%s:%d",
				siptrace_proto_name(req->rcv.proto),
				ip_addr2a(&req->rcv.dst_ip), req->rcv.dst_port);
		if(sto.fromip.len<0 || sto.fromip.len>=SIPTRACE_ADDR_MAX) {
			LM_ERR("failed to format toip buffer (%d)\n", sto.fromip.len);
			sto.fromip.s = SIPTRACE_ANYADDR;
			sto.fromip.len = SIPTRACE_ANYADDR_LEN;
		} else {
			sto.fromip.s = sto.fromip_buff;
		}
	}

	sto.status.s = int2strbuf(slcbp->code, statusbuf, INT2STR_MAX_LEN, &sto.status.len);
	if(sto.status.s == 0) {
		LM_ERR("failure to get the status string\n");
		return;
	}

	memset(&to_ip, 0, sizeof(struct ip_addr));
	if(slcbp->dst == 0) {
		sto.toip.s = SIPTRACE_ANYADDR;
		sto.toip.len = SIPTRACE_ANYADDR_LEN;
	} else {
		su2ip_addr(&to_ip, &slcbp->dst->to);
		sto.toip.len = snprintf(sto.toip_buff, SIPTRACE_ADDR_MAX, "%s:%s:%d",
				siptrace_proto_name(req->rcv.proto), ip_addr2a(&to_ip),
				(int)su_getport(&slcbp->dst->to));
		if(sto.toip.len<0 || sto.toip.len>=SIPTRACE_ADDR_MAX) {
			LM_ERR("failed to format toip buffer (%d)\n", sto.toip.len);
			sto.toip.s = SIPTRACE_ANYADDR;
			sto.toip.len = SIPTRACE_ANYADDR_LEN;
		} else {
			sto.toip.s = sto.toip_buff;
		}
	}

	sto.dir = "out";
	sto.fromtag = get_from(msg)->tag_value;
	sto.totag = get_to(msg)->tag_value;

#ifdef STATISTICS
	sto.stat = siptrace_rpl;
#endif

	sip_trace_store(&sto, NULL, NULL);
	return;
}

static void trace_transaction(sip_msg_t* msg, siptrace_info_t* info, int dlg_tran)
{
	if(msg == NULL) {
		LM_DBG("nothing to trace\n");
		return;
	}

	/* trace current message on out */
	msg->msg_flags |= FL_SIPTRACE;
	if (info->uriState == STRACE_RAW_URI) {
		if (parse_raw_uri(info) < 0) {
			LM_ERR("failed to parse trace destination uri!\n");
			return;
		}
	}

	if(tmb.register_tmcb(msg, 0, TMCB_REQUEST_SENT, trace_onreq_out, info, 0) <= 0) {
		LM_ERR("can't register trace_onreq_out\n");
		return;
	}

	/* trace reply on in */
	if(tmb.register_tmcb(msg, 0, TMCB_RESPONSE_IN, trace_onreply_in, info, 0) <= 0) {
		LM_ERR("can't register trace_onreply_in\n");
		return;
	}

	/* trace reply on out */
	if(tmb.register_tmcb(msg, 0, TMCB_RESPONSE_SENT, trace_onreply_out, info,
							dlg_tran ? 0 : free_trace_info)
			<= 0) {
		LM_ERR("can't register trace_onreply_out\n");
		return;
	}

	/* TODO */
	/* check the following callbacks: TMCB_REQUEST_PENDING, TMCB_RESPONSE_READY, TMCB_ACK_NEG_IN */
	/* trace reply on in */
	if(tmb.register_tmcb(msg, 0, TMCB_ACK_NEG_IN, trace_tm_neg_ack_in, info, 0) <= 0) {
		LM_ERR("can't register trace_onreply_in\n");
		return;
	}

	if(tmb.register_tmcb(msg, 0, TMCB_E2ECANCEL_IN, trace_onreq_out, info, 0) <= 0) {
		LM_ERR("can't register trace_onreply_in\n");
		return;
	}
}

//static void trace_dialog(sip_msg_t* msg, siptrace_info_t* info)
static void trace_dialog(struct dlg_cell* dlg, int type, struct dlg_cb_params *params)
{
	sr_xavp_t* xavp;

	if (!dlgb.get_dlg) {
		LM_ERR("Dialog API not loaded! Trace off...\n");
		return;
	}

	/* request - params->req */
	if (params == NULL || params->req == NULL) {
		LM_ERR("Invalid args!\n");
		return;
	}

	if (!(params->req->msg_flags & FL_SIPTRACE)) {
		LM_DBG("Trace is off for this request...\n");
		return;
	}

	xavp = xavp_get(&xavp_trace_info_name_s, NULL);
	if (!xavp) {
		/* this actually happens when only the transaction is traced
		 * FL_SIPTRACE is set from trace_onreq_out
		 * but xavp is set only for dialogs so this will avoid
		 * registering dialog callbacks which is the desired behavior */
		LM_DBG("%.*s xavp not registered! "
				"Probably incoming E2E CANCEL!\n", xavp_trace_info_name_s.len,
				xavp_trace_info_name_s.s);
		return;
	}

	if(dlgb.register_dlgcb(dlg, DLGCB_REQ_WITHIN,
				trace_dialog_transaction, xavp->val.v.vptr, 0) != 0) {
		LM_ERR("Failed to register DLGCB_REQ_WITHIN callback!\n");
		return;
	}

	if(dlgb.register_dlgcb(dlg, DLGCB_TERMINATED,
				trace_dialog_transaction, xavp->val.v.vptr, free_trace_info) != 0) {
		LM_ERR("Failed to register DLGCB_TERMINATED callback!\n");
		return;
	}

	return;
}


static void trace_dialog_transaction(struct dlg_cell* dlg, int type, struct dlg_cb_params *params)
{
	siptrace_info_t* info;

	/* coverity fix - there shouldn't be a scenario for this to happen */
	if (params == NULL || params->param == NULL) {
		LM_ERR("NULL dialog params!\n");
		return;
	}

	/**
	 * DUAL BYE - internally generated BYE from kamailio
	 * set flag to signal request_in callback which will register
	 * transaction callbacks to catch caller and callee BYEs and their
	 * responses
	 */
	if (params->req == NULL && params->rpl == NULL) {
		LM_DBG("dual bye!\n");
		return;
	}
	info = (siptrace_info_t *)*params->param;

	trace_transaction(params->req, info, 1);

	sip_trace(params->req, &info->u.dest_info, &info->correlation_id, NULL);
}

static void free_trace_info(void* trace_info)
{
	if (!trace_info) return;

	shm_free(trace_info);
}

/**
 *
 */
int siptrace_net_data_recv(sr_event_param_t *evp)
{
	sr_net_info_t *nd;
	siptrace_data_t sto;

	if(evp->data == 0)
		return -1;

	nd = (sr_net_info_t *)evp->data;
	if(nd->rcv == NULL || nd->data.s == NULL || nd->data.len <= 0)
		return -1;

	memset(&sto, 0, sizeof(siptrace_data_t));

	sto.body.s = nd->data.s;
	sto.body.len = nd->data.len;

	sto.fromip.len = snprintf(sto.fromip_buff, SIPTRACE_ADDR_MAX, "%s:%s:%d",
			siptrace_proto_name(nd->rcv->proto),
			ip_addr2strz(&nd->rcv->src_ip), (int)nd->rcv->src_port);
	if(sto.fromip.len<0 || sto.fromip.len>=SIPTRACE_ADDR_MAX) {
		LM_ERR("failed to format toip buffer (%d)\n", sto.fromip.len);
		sto.fromip.s = SIPTRACE_ANYADDR;
		sto.fromip.len = SIPTRACE_ANYADDR_LEN;
	} else {
		sto.fromip.s = sto.fromip_buff;
	}

	sto.toip.len = snprintf(sto.toip_buff, SIPTRACE_ADDR_MAX, "%s:%s:%d",
			siptrace_proto_name(nd->rcv->proto), ip_addr2strz(&nd->rcv->dst_ip),
			(int)nd->rcv->dst_port);
	if(sto.toip.len<0 || sto.toip.len>=SIPTRACE_ADDR_MAX) {
		LM_ERR("failed to format toip buffer (%d)\n", sto.toip.len);
		sto.toip.s = SIPTRACE_ANYADDR;
		sto.toip.len = SIPTRACE_ANYADDR_LEN;
	} else {
		sto.toip.s = sto.toip_buff;
	}

	sto.dir = "in";

	trace_send_hep_duplicate(&sto.body, &sto.fromip, &sto.toip, NULL, NULL);
	return 0;
}

/**
 *
 */
int siptrace_net_data_send(sr_event_param_t *evp)
{
	sr_net_info_t *nd;
	dest_info_t new_dst;
	siptrace_data_t sto;

	if(evp->data == 0)
		return -1;

	nd = (sr_net_info_t *)evp->data;
	if(nd->dst == NULL || nd->data.s == NULL || nd->data.len <= 0)
		return -1;

	new_dst = *nd->dst;

	if(new_dst.send_sock == 0) {
		new_dst.send_sock = get_send_socket(0, &nd->dst->to, nd->dst->proto);
	}

	memset(&sto, 0, sizeof(siptrace_data_t));

	sto.body.s = nd->data.s;
	sto.body.len = nd->data.len;

	if(unlikely(new_dst.send_sock == 0)) {
		LM_WARN("no sending socket found\n");
		strcpy(sto.fromip_buff, SIPTRACE_ANYADDR);
		sto.fromip.len = SIPTRACE_ANYADDR_LEN;
	} else {
		if(new_dst.send_sock->sock_str.len>=SIPTRACE_ADDR_MAX-1) {
			LM_ERR("socket string is too large: %d\n",
					new_dst.send_sock->sock_str.len);
			goto error;
		}
		strncpy(sto.fromip_buff, new_dst.send_sock->sock_str.s,
				new_dst.send_sock->sock_str.len);
		sto.fromip.len = new_dst.send_sock->sock_str.len;
	}
	sto.fromip.s = sto.fromip_buff;

	sto.toip.len = snprintf(sto.toip_buff, SIPTRACE_ADDR_MAX, "%s:%s:%d",
			siptrace_proto_name(new_dst.send_sock->proto),
			suip2a(&new_dst.to, sizeof(new_dst.to)),
			(int)su_getport(&new_dst.to));
	if(sto.toip.len<0 || sto.toip.len>=SIPTRACE_ADDR_MAX) {
		LM_ERR("failed to format toip buffer (%d)\n", sto.toip.len);
		sto.toip.s = SIPTRACE_ANYADDR;
		sto.toip.len = SIPTRACE_ANYADDR_LEN;
	} else {
		sto.toip.s = sto.toip_buff;
	}

	sto.dir = "out";

	trace_send_hep_duplicate(&sto.body, &sto.fromip, &sto.toip, NULL, NULL);
	return 0;

error:
	return -1;
}

/**
 *
 */
static int w_hlog1(struct sip_msg *msg, char *message, char *_)
{
	str smessage;
	if(fixup_get_svalue(msg, (gparam_t *)message, &smessage) != 0) {
		LM_ERR("unable to parse the message\n");
		return -1;
	}
	return hlog(msg, NULL, &smessage);
}

/**
 *
 */
static int ki_hlog(sip_msg_t *msg, str *message)
{
	return hlog(msg, NULL, message);
}

/**
 *
 */
static int w_hlog2(struct sip_msg *msg, char *correlationid, char *message)
{
	str scorrelationid, smessage;
	if(fixup_get_svalue(msg, (gparam_t *)correlationid, &scorrelationid) != 0) {
		LM_ERR("unable to parse the correlation id\n");
		return -1;
	}
	if(fixup_get_svalue(msg, (gparam_t *)message, &smessage) != 0) {
		LM_ERR("unable to parse the message\n");
		return -1;
	}
	return hlog(msg, &scorrelationid, &smessage);
}

/**
 *
 */
static int ki_hlog_cid(sip_msg_t *msg, str *correlationid, str *message)
{
	return hlog(msg, correlationid, message);
}

/**
 *
 */
static void siptrace_rpc_status(rpc_t *rpc, void *c)
{
	str status = {0, 0};

	if(rpc->scan(c, "S", &status) < 1) {
		rpc->fault(c, 500, "Not enough parameters (on, off or check)");
		return;
	}

	if(trace_on_flag == NULL) {
		rpc->fault(c, 500, "Internal error");
		return;
	}

	if(strncasecmp(status.s, "on", strlen("on")) == 0) {
		*trace_on_flag = 1;
		rpc->rpl_printf(c, "Enabled");
		return;
	}
	if(strncasecmp(status.s, "off", strlen("off")) == 0) {
		*trace_on_flag = 0;
		rpc->rpl_printf(c, "Disabled");
		return;
	}
	if(strncasecmp(status.s, "check", strlen("check")) == 0) {
		rpc->rpl_printf(c, *trace_on_flag ? "Enabled" : "Disabled");
		return;
	}
	rpc->fault(c, 500, "Bad parameter (on, off or check)");
	return;
}

static const char *siptrace_status_doc[2] = {
	"Get status or turn on/off siptrace. Parameters: on, off or check.",
	0
};

rpc_export_t siptrace_rpc[] = {
	{"siptrace.status", siptrace_rpc_status, siptrace_status_doc, 0},
	{0, 0, 0, 0}
};

static int siptrace_init_rpc(void)
{
	if(rpc_register_array(siptrace_rpc) != 0) {
		LM_ERR("failed to register RPC commands\n");
		return -1;
	}
	return 0;
}

/**
 *
 */
/* clang-format off */
static sr_kemi_t sr_kemi_siptrace_exports[] = {
	{ str_init("siptrace"), str_init("sip_trace"),
		SR_KEMIP_INT, ki_sip_trace,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("siptrace"), str_init("sip_trace_dst"),
		SR_KEMIP_INT, ki_sip_trace_dst,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("siptrace"), str_init("sip_trace_dst_cid"),
		SR_KEMIP_INT, ki_sip_trace_dst_cid,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("siptrace"), str_init("sip_trace_dst_cid_type"),
		SR_KEMIP_INT, ki_sip_trace_dst_cid_flag,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_STR,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("siptrace"), str_init("hlog"),
		SR_KEMIP_INT, ki_hlog,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("siptrace"), str_init("hlog_cid"),
		SR_KEMIP_INT, ki_hlog_cid,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("siptrace"), str_init("sip_trace_mode"),
		SR_KEMIP_INT, ki_sip_trace_mode,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},

	{ {0, 0}, {0, 0}, 0, NULL, { 0, 0, 0, 0, 0, 0 } }
};
/* clang-format on */

/**
 *
 */
int mod_register(char *path, int *dlflags, void *p1, void *p2)
{
	sr_kemi_modules_add(sr_kemi_siptrace_exports);
	return 0;
}
