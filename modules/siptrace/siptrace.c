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
#include "../../sr_module.h"
#include "../../dprint.h"
#include "../../ut.h"
#include "../../ip_addr.h"
#include "../../mem/mem.h"
#include "../../mem/shm_mem.h"
#include "../../lib/kmi/mi.h"
#include "../../rpc.h"
#include "../../rpc_lookup.h"
#include "../../lib/srdb1/db.h"
#include "../../parser/parse_content.h"
#include "../../parser/parse_from.h"
#include "../../parser/parse_cseq.h"
#include "../../pvar.h"
#include "../../modules/tm/tm_load.h"
#include "../../modules/sl/sl.h"
#include "../../str.h"
#include "../../onsend.h"

#include "../../modules/sipcapture/hep.h"

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
	str totag;
	str toip;
	char toip_buff[IP_ADDR_MAX_STR_SIZE+12];
	char fromip_buff[IP_ADDR_MAX_STR_SIZE+12];
	struct timeval tv;
#ifdef STATISTICS
	stat_var *stat;
#endif
};

struct tm_binds tmb;

/** SL API structure */
sl_api_t slb;

/* module function prototypes */
static int mod_init(void);
static int siptrace_init_rpc(void);
static int child_init(int rank);
static void destroy(void);
static int sip_trace(struct sip_msg*, struct dest_info*, char*);
static int fixup_siptrace(void ** param, int param_no);

static int sip_trace_store_db(struct _siptrace_data* sto);
static int trace_send_duplicate(char *buf, int len, struct dest_info*);

static void trace_onreq_in(struct cell* t, int type, struct tmcb_params *ps);
static void trace_onreq_out(struct cell* t, int type, struct tmcb_params *ps);
static void trace_onreply_in(struct cell* t, int type, struct tmcb_params *ps);
static void trace_onreply_out(struct cell* t, int type, struct tmcb_params *ps);
static void trace_sl_onreply_out(sl_cbp_t *slcb);
static void trace_sl_ack_in(sl_cbp_t *slcb);

static int trace_send_hep_duplicate(str *body, str *from, str *to, struct dest_info*);
static int pipport2su (char *pipport, union sockaddr_union *tmp_su, unsigned int *proto);


static struct mi_root* sip_trace_mi(struct mi_root* cmd, void* param );

static str db_url             = str_init(DEFAULT_DB_URL);
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
static str time_us_column     = str_init("time_us");     /* 10 */
static str totag_column       = str_init("totag");       /* 11 */

#define NR_KEYS 12
#define SIP_TRACE_TABLE_VERSION 4

#define XHEADERS_BUFSIZE 512

int trace_flag = 0;
int trace_on   = 0;
int trace_sl_acks = 1;

int trace_to_database = 1;
int trace_delayed = 0;

int hep_version = 1;
int hep_capture_id = 1;

int xheaders_write = 0;
int xheaders_read = 0;

str force_send_sock_str = {0, 0};
struct sip_uri * force_send_sock_uri = 0;

str    dup_uri_str      = {0, 0};
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

db1_con_t *db_con = NULL; 		/*!< database connection */
db_func_t db_funcs;      		/*!< Database functions */

/*! \brief
 * Exported functions
 */
static cmd_export_t cmds[] = {
	{"sip_trace", (cmd_function)sip_trace, 0, 0, 0, ANY_ROUTE},
    {"sip_trace", (cmd_function)sip_trace, 1, fixup_siptrace, 0, ANY_ROUTE},
	{0, 0, 0, 0, 0, 0}
};


/*! \brief
 * Exported parameters
 */
static param_export_t params[] = {
	{"db_url",             PARAM_STR, &db_url            },
	{"table",              PARAM_STR, &siptrace_table     },
	{"date_column",        PARAM_STR, &date_column        },
	{"callid_column",      PARAM_STR, &callid_column      },
	{"traced_user_column", PARAM_STR, &traced_user_column },
	{"msg_column",         PARAM_STR, &msg_column         },
	{"method_column",      PARAM_STR, &method_column      },
	{"status_column",      PARAM_STR, &status_column      },
	{"fromip_column",      PARAM_STR, &fromip_column      },
	{"toip_column",        PARAM_STR, &toip_column        },
	{"fromtag_column",     PARAM_STR, &fromtag_column     },
	{"totag_column",       PARAM_STR, &totag_column       },
	{"direction_column",   PARAM_STR, &direction_column   },
	{"trace_flag",         INT_PARAM, &trace_flag           },
	{"trace_on",           INT_PARAM, &trace_on             },
	{"traced_user_avp",    PARAM_STR, &traced_user_avp_str},
	{"trace_table_avp",    PARAM_STR, &trace_table_avp_str},
	{"duplicate_uri",      PARAM_STR, &dup_uri_str        },
	{"trace_to_database",  INT_PARAM, &trace_to_database    },
	{"trace_local_ip",     PARAM_STR, &trace_local_ip     },
	{"trace_sl_acks",      INT_PARAM, &trace_sl_acks        },
	{"xheaders_write",     INT_PARAM, &xheaders_write       },
	{"xheaders_read",      INT_PARAM, &xheaders_read        },
	{"hep_mode_on",        INT_PARAM, &hep_mode_on          },	 
    {"force_send_sock",    PARAM_STR, &force_send_sock_str	},
	{"hep_version",        INT_PARAM, &hep_version          },
	{"hep_capture_id",     INT_PARAM, &hep_capture_id       },	        
	{"trace_delayed",      INT_PARAM, &trace_delayed        },
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
	if(siptrace_init_rpc() != 0) 
	{
		LM_ERR("failed to register RPC commands\n");
		return -1;
	}

	if (trace_flag<0 || trace_flag>(int)MAX_FLAG)
	{
		LM_ERR("invalid trace flag %d\n", trace_flag);
		return -1;
	}
	trace_flag = 1<<trace_flag;

	trace_to_database_flag = (int*)shm_malloc(sizeof(int));
	if(trace_to_database_flag==NULL) {
		LM_ERR("no more shm memory left\n");
		return -1;
	}

	*trace_to_database_flag = trace_to_database;

	if(hep_version != 1 && hep_version != 2) {
	    LM_ERR("unsupported version of HEP");
	    return -1;
	}

	/* Find a database module if needed */
	if(trace_to_database_flag!=NULL && *trace_to_database_flag!=0) {
		if (db_bind_mod(&db_url, &db_funcs))
		{
			LM_ERR("unable to bind database module\n");
			return -1;
		}
		if (trace_to_database_flag && !DB_CAPABILITY(db_funcs, DB_CAP_INSERT))
		{
			LM_ERR("database modules does not provide all functions needed"
					" by module\n");
			return -1;
		}
	}

        if(hep_version != 1 && hep_version != 2) {
  
                  LM_ERR("unsupported version of HEP");
                  return -1;
        }                                          


	trace_on_flag = (int*)shm_malloc(sizeof(int));
	if(trace_on_flag==NULL) {
		LM_ERR("no more shm memory left\n");
		return -1;
	}

	*trace_on_flag = trace_on;

	xheaders_write_flag = (int*)shm_malloc(sizeof(int));
	xheaders_read_flag = (int*)shm_malloc(sizeof(int));
	if (!(xheaders_write_flag && xheaders_read_flag)) {
		LM_ERR("no more shm memory left\n");
		return -1;
	}
	*xheaders_write_flag = xheaders_write;
	*xheaders_read_flag = xheaders_read;

	/* register callbacks to TM */
	if (load_tm_api(&tmb)!=0) {
		LM_WARN("can't load tm api. Will not install tm callbacks.\n");
	} else if(tmb.register_tmcb( 0, 0, TMCB_REQUEST_IN, trace_onreq_in, 0, 0) <=0) {
		LM_ERR("can't register trace_onreq_in\n");
		return -1;
	}

	/* bind the SL API */
	if (sl_load_api(&slb)!=0) {
		LM_WARN("cannot bind to SL API. Will not install sl callbacks.\n");
	} else {
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
	}

	if(dup_uri_str.s!=0)
	{
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

	if(force_send_sock_str.s!=0)
	{
	    force_send_sock_str.len = strlen(force_send_sock_str.s);
	    force_send_sock_uri = (struct sip_uri *)pkg_malloc(sizeof(struct sip_uri));
	    if(force_send_sock_uri==0)
	    {
	        LM_ERR("no more pkg memory left\n");
	        return -1;
	    }
	    memset(force_send_sock_uri, 0, sizeof(struct sip_uri));
	    if(parse_uri(force_send_sock_str.s, force_send_sock_str.len, force_send_sock_uri)<0)
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

	if(trace_to_database_flag!=NULL && *trace_to_database_flag!=0) {
		db_con = db_funcs.init(&db_url);
		if (!db_con)
		{
			LM_ERR("unable to connect to database. Please check configuration.\n");
			return -1;
		}
		if (db_check_table_version(&db_funcs, db_con, &siptrace_table,
					   SIP_TRACE_TABLE_VERSION) < 0) {
			LM_ERR("error during table version check\n");
			db_funcs.close(db_con);		
			return -1;
		}
	}

	return 0;
}


static void destroy(void)
{
	if(trace_to_database_flag!=NULL && *trace_to_database_flag!=0) {
		if (db_con!=NULL)
			db_funcs.close(db_con);
	}

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
	} else if(proto==PROTO_WS) {
		strcpy(buf, "ws:");
	} else if(proto==PROTO_WSS) {
		strcpy(buf, "wss:");
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

	if(msg->cseq==NULL && ((parse_headers(msg, HDR_CSEQ_F, 0)==-1)
				|| (msg->cseq==NULL)))
	{
		LM_ERR("cannot parse cseq\n");
		goto error;
	}

	return 0;
error:
	return -1;
}

// Appends x-headers to the message in sto->body containing data from sto
static int sip_trace_xheaders_write(struct _siptrace_data *sto)
{
	char* buf = NULL;
	int bytes_written = 0;
	char* eoh = NULL;
	int eoh_offset = 0;
	char* new_eoh = NULL;

	if(xheaders_write_flag==NULL || *xheaders_write_flag==0)
		return 0;

	// Memory for the message with some additional headers.
	// It gets free()ed in sip_trace_xheaders_free().
	buf = pkg_malloc(sto->body.len + XHEADERS_BUFSIZE);
	if (buf == NULL) {
		LM_ERR("sip_trace_xheaders_write: out of memory\n");
		return -1;
	}

	// Copy the whole message to buf first; it must be \0-terminated for
	// strstr() to work. Then search for the end-of-header sequence.
	memcpy(buf, sto->body.s, sto->body.len);
	buf[sto->body.len] = '\0';
	eoh = strstr(buf, "\r\n\r\n");
	if (eoh == NULL) {
		LM_ERR("sip_trace_xheaders_write: malformed message\n");
		goto error;
	}
	eoh += 2; // the first \r\n belongs to the last header => skip it

	// Write the new headers a the end-of-header position. This overwrites
	// the \r\n terminating the old headers and the beginning of the message
	// body. Both will be recovered later.
	bytes_written = snprintf(eoh, XHEADERS_BUFSIZE,
			"X-Siptrace-Fromip: %.*s\r\n"
			"X-Siptrace-Toip: %.*s\r\n"
			"X-Siptrace-Time: %llu %llu\r\n"
			"X-Siptrace-Method: %.*s\r\n"
			"X-Siptrace-Dir: %s\r\n",
			sto->fromip.len, sto->fromip.s,
			sto->toip.len, sto->toip.s,
			(unsigned long long)sto->tv.tv_sec, (unsigned long long)sto->tv.tv_usec,
			sto->method.len, sto->method.s,
			sto->dir);
	if (bytes_written >= XHEADERS_BUFSIZE) {
		LM_ERR("sip_trace_xheaders_write: string too long\n");
		goto error;
	}

	// Copy the \r\n terminating the old headers and the message body from the
	// old buffer in sto->body.s to the new end-of-header in buf.
	eoh_offset = eoh - buf;
	new_eoh = eoh + bytes_written;
	memcpy(new_eoh, sto->body.s + eoh_offset, sto->body.len - eoh_offset);

	// Change sto to point to the new buffer.
	sto->body.s = buf;
	sto->body.len += bytes_written;
	return 0;
error:
	if(buf != NULL)
		pkg_free(buf);
	return -1;
}

// Parses x-headers, saves the data back to sto, and removes the x-headers
// from the message in sto->buf
static int sip_trace_xheaders_read(struct _siptrace_data *sto)
{
	char* searchend = NULL;
	char* eoh = NULL;
	char* xheaders = NULL;
	long long unsigned int tv_sec, tv_usec;

	if(xheaders_read_flag==NULL || *xheaders_read_flag==0)
		return 0;

	// Find the end-of-header marker \r\n\r\n
	searchend = sto->body.s + sto->body.len - 3;
	eoh = memchr(sto->body.s, '\r', searchend - eoh);
	while (eoh != NULL && eoh < searchend) {
		if (memcmp(eoh, "\r\n\r\n", 4) == 0)
			break;
		eoh = memchr(eoh + 1, '\r', searchend - eoh);
	}
	if (eoh == NULL) {
		LM_ERR("sip_trace_xheaders_read: malformed message\n");
		return -1;
	}

	// Find x-headers: eoh will be overwritten by \0 to allow the use of
	// strstr(). The byte at eoh will later be recovered, when the
	// message body is shifted towards the beginning of the message
	// to remove the x-headers.
	*eoh = '\0';
	xheaders = strstr(sto->body.s, "\r\nX-Siptrace-Fromip: ");
	if (xheaders == NULL) {
		LM_ERR("sip_trace_xheaders_read: message without x-headers "
				"from %.*s, callid %.*s\n",
				sto->fromip.len, sto->fromip.s, sto->callid.len, sto->callid.s);
		return -1;
	}

	// Allocate memory for new strings in sto
	// (gets free()ed in sip_trace_xheaders_free() )
	sto->fromip.s = pkg_malloc(51);
	sto->toip.s = pkg_malloc(51);
	sto->method.s = pkg_malloc(51);
	sto->dir = pkg_malloc(4);
	if (!(sto->fromip.s && sto->toip.s && sto->method.s && sto->dir)) {
		LM_ERR("sip_trace_xheaders_read: out of memory\n");
		goto erroraftermalloc;
	}

	// Parse the x-headers: scanf()
	if (sscanf(xheaders, "\r\n"
				"X-Siptrace-Fromip: %50s\r\n"
				"X-Siptrace-Toip: %50s\r\n"
				"X-Siptrace-Time: %llu %llu\r\n"
				"X-Siptrace-Method: %50s\r\n"
				"X-Siptrace-Dir: %3s",
				sto->fromip.s, sto->toip.s,
				&tv_sec, &tv_usec,
				sto->method.s,
				sto->dir) == EOF) {
		LM_ERR("sip_trace_xheaders_read: malformed x-headers\n");
		goto erroraftermalloc;
	}
	sto->fromip.len = strlen(sto->fromip.s);
	sto->toip.len = strlen(sto->toip.s);
	sto->tv.tv_sec = (time_t)tv_sec;
	sto->tv.tv_usec = (suseconds_t)tv_usec;
	sto->method.len = strlen(sto->method.s);

	// Remove the x-headers: the message body is shifted towards the beginning
	// of the message, overwriting the x-headers. Before that, the byte at eoh
	// is recovered.
	*eoh = '\r';
	memmove(xheaders, eoh, sto->body.len - (eoh - sto->body.s));
	sto->body.len -= eoh - xheaders;

	return 0;

erroraftermalloc:
	if (sto->fromip.s)
		pkg_free(sto->fromip.s);
	if (sto->toip.s)
		pkg_free(sto->toip.s);
	if (sto->method.s)
		pkg_free(sto->method.s);
	if (sto->dir)
		pkg_free(sto->dir);
	return -1;
}

// Frees the memory allocated by sip_trace_xheaders_{write,read}
static int sip_trace_xheaders_free(struct _siptrace_data *sto)
{
	if (xheaders_write_flag != NULL && *xheaders_write_flag != 0) {
		if(sto->body.s)
			pkg_free(sto->body.s);
	}

	if (xheaders_read_flag != NULL && *xheaders_read_flag != 0) {
		if(sto->fromip.s)
			pkg_free(sto->fromip.s);
		if(sto->toip.s)
			pkg_free(sto->toip.s);
		if(sto->dir)
			pkg_free(sto->dir);
	}

	return 0;
}

static int sip_trace_store(struct _siptrace_data *sto, struct dest_info *dst)
{
	if(sto==NULL)
	{
		LM_DBG("invalid parameter\n");
		return -1;
	}

	gettimeofday(&sto->tv, NULL);

	if (sip_trace_xheaders_read(sto) != 0)
		return -1;
	int ret = sip_trace_store_db(sto);

	if (sip_trace_xheaders_write(sto) != 0)
		return -1;

	if(hep_mode_on) trace_send_hep_duplicate(&sto->body, &sto->fromip, &sto->toip, dst);
    else trace_send_duplicate(sto->body.s, sto->body.len, dst);

	if (sip_trace_xheaders_free(sto) != 0)
		return -1;

	return ret;
}

static int sip_trace_store_db(struct _siptrace_data *sto)
{
	if(trace_to_database_flag==NULL || *trace_to_database_flag==0)
		goto done;

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

	if(trace_on_flag!=NULL && *trace_on_flag!=0) {
		db_vals[9].val.str_val.s   = "";
		db_vals[9].val.str_val.len = 0;

		LM_DBG("storing info...\n");
		if(trace_delayed!=0 && db_funcs.insert_delayed!=NULL)
		{
			if(db_funcs.insert_delayed(db_con, db_keys, db_vals, NR_KEYS)<0) {
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

	if(sto->avp==NULL)
		goto done;

	db_vals[9].val.str_val = sto->avp_value.s;

	LM_DBG("storing info...\n");
	if(trace_delayed!=0 && db_funcs.insert_delayed!=NULL)
	{
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
	while(sto->avp!=NULL) {
		db_vals[9].val.str_val = sto->avp_value.s;

		LM_DBG("storing info...\n");
		if(trace_delayed!=0 && db_funcs.insert_delayed!=NULL)
		{
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

static int fixup_siptrace(void** param, int param_no) {
	char *duri = (char*) *param;
	struct sip_uri dup_uri;
	struct dest_info *dst = NULL;
	struct proxy_l * p = NULL;
	str dup_uri_str = { 0, 0 };

	if (param_no != 1) {
		LM_DBG("params:%s\n", (char*)*param);
		return 0;
	}
	if (!(*duri)) {
		LM_ERR("invalid dup URI\n");
		return -1;
	}
	LM_DBG("sip_trace URI:%s\n", (char*)*param);

	dup_uri_str.s = duri;
	dup_uri_str.len = strlen(dup_uri_str.s);
	memset(&dup_uri, 0, sizeof(struct sip_uri));

	if (parse_uri(dup_uri_str.s, dup_uri_str.len, &dup_uri) < 0) {
		LM_ERR("bad dup uri\n");
		return -1;
	}

	dst = (struct dest_info *) pkg_malloc(sizeof(struct dest_info));
	if (dst == 0) {
		LM_ERR("no more pkg memory left\n");
		return -1;
	}
	init_dest_info(dst);
	/* create a temporary proxy*/
	dst->proto = PROTO_UDP;
	p = mk_proxy(&dup_uri.host, (dup_uri.port_no) ? dup_uri.port_no : SIP_PORT,
			dst->proto);
	if (p == 0) {
		LM_ERR("bad host name in uri\n");
		pkg_free(dst);
		return -1;
	}
	hostent2su(&dst->to, &p->host, p->addr_idx, (p->port) ? p->port : SIP_PORT);

	pkg_free(*param);
	/* free temporary proxy*/
	if (p) {
		free_proxy(p); /* frees only p content, not p itself */
		pkg_free(p);
	}

	*param = (void*) dst;
	return 0;
}

static int sip_trace(struct sip_msg *msg, struct dest_info * dst, char *dir)
{
	struct _siptrace_data sto;
	struct onsend_info *snd_inf = NULL;

	if (dst){
	    if (dst->send_sock == 0){
	        dst->send_sock=get_send_socket(0, &dst->to, dst->proto);
	        if (dst->send_sock==0){
	            LM_ERR("can't forward to af %d, proto %d no corresponding"
	                    " listening socket\n", dst->to.s.sa_family, dst->proto);
	            return -1;
	        }
	    }
	}

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

	sto.callid = msg->callid->body;

	if(msg->first_line.type==SIP_REQUEST) {
		sto.method = msg->first_line.u.request.method;
	} else {
		if(parse_headers(msg, HDR_CSEQ_F, 0) != 0 || msg->cseq==NULL
				|| msg->cseq->parsed==NULL) {
			LM_ERR("cannot parse cseq header\n");
			return -1;
		}
		sto.method = get_cseq(msg)->method;
	}

	if(msg->first_line.type==SIP_REPLY) {
		sto.status = msg->first_line.u.reply.status;
	} else {
		sto.status.s = "";
		sto.status.len = 0;
	}

	snd_inf=get_onsend_info();
	if(snd_inf==NULL) {
		sto.body.s = msg->buf;
		sto.body.len = msg->len;

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
	} else {
		sto.body.s   = snd_inf->buf;
		sto.body.len = snd_inf->len;

		strncpy(sto.fromip_buff, snd_inf->send_sock->sock_str.s,
				snd_inf->send_sock->sock_str.len);
		sto.fromip.s = sto.fromip_buff;
		sto.fromip.len = strlen(sto.fromip_buff);

		siptrace_copy_proto(snd_inf->send_sock->proto, sto.toip_buff);
		strcat(sto.toip_buff, suip2a(snd_inf->to, sizeof(*snd_inf->to)));
		strcat(sto.toip_buff,":");
		strcat(sto.toip_buff, int2str((int)su_getport(snd_inf->to), NULL));
		sto.toip.s = sto.toip_buff;
		sto.toip.len = strlen(sto.toip_buff);

		sto.dir = "out";
	}

	sto.fromtag = get_from(msg)->tag_value;
	sto.totag = get_to(msg)->tag_value;

#ifdef STATISTICS
	if(msg->first_line.type==SIP_REPLY) {
		sto.stat = siptrace_rpl;
	} else {
		sto.stat = siptrace_req;
	}
#endif
	return sip_trace_store(&sto, dst);
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
		/* check if it is outgoing cancel, t is INVITE
		 * and send_buf starts with CANCEL */
		if(is_invite(t) && ps->send_buf.len>7
				&& strncmp(ps->send_buf.s, "CANCEL ", 7)==0) {
			msg = t->uas.request;
			if(msg==NULL) {
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
	sto.totag = get_to(msg)->tag_value;

#ifdef STATISTICS
	sto.stat = siptrace_req;
#endif

	sip_trace_store(&sto, NULL);
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

	sto.method = get_cseq(msg)->method;

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
	sto.totag = get_to(msg)->tag_value;
#ifdef STATISTICS
	sto.stat = siptrace_rpl;
#endif

	sip_trace_store(&sto, NULL);
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
	sto.method = get_cseq(msg)->method;

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
	sto.totag = get_to(msg)->tag_value;

#ifdef STATISTICS
	sto.stat = siptrace_rpl;
#endif

	sip_trace_store(&sto, NULL);
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
	sto.totag = get_to(msg)->tag_value;

#ifdef STATISTICS
	sto.stat = siptrace_rpl;
#endif

	sip_trace_store(&sto, NULL);
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

static int trace_send_duplicate(char *buf, int len, struct dest_info *dst2)
{
	struct dest_info dst;
	struct proxy_l * p = NULL;

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

	if (!dst2){
	    init_dest_info(&dst);
	    /* create a temporary proxy*/
	    dst.proto = PROTO_UDP;
	    p=mk_proxy(&dup_uri->host, (dup_uri->port_no)?dup_uri->port_no:SIP_PORT,
	             dst.proto);
	    if (p==0){
	        LM_ERR("bad host name in uri\n");
	        return -1;
	    }
	    hostent2su(&dst.to, &p->host, p->addr_idx, (p->port)?p->port:SIP_PORT);

	    dst.send_sock=get_send_socket(0, &dst.to, dst.proto);
	    if (dst.send_sock==0){
	        LM_ERR("can't forward to af %d, proto %d no corresponding"
	                " listening socket\n", dst.to.s.sa_family, dst.proto);
	        goto error;
	    }
	}

	if (msg_send((dst2)?dst2:&dst, buf, len)<0)
	{
		LM_ERR("cannot send duplicate message\n");
		goto error;
	}

	if (p){
	    free_proxy(p); /* frees only p content, not p itself */
	    pkg_free(p);
	}
	return 0;
error:
    if (p){
	free_proxy(p); /* frees only p content, not p itself */
	pkg_free(p);
    }
	return -1;
}

static int trace_send_hep_duplicate(str *body, str *from, str *to, struct dest_info * dst2)
{
	struct dest_info dst;
	struct socket_info *si;
	struct dest_info* dst_fin = NULL;
	struct proxy_l * p=NULL /* make gcc happy */;
	void* buffer = NULL;
	union sockaddr_union from_su;
	union sockaddr_union to_su;
	unsigned int len, buflen, proto;
	struct hep_hdr hdr;
	struct hep_iphdr hep_ipheader;
	struct hep_timehdr hep_time;
	struct timeval tvb;
	struct timezone tz;
	                 
	struct hep_ip6hdr hep_ip6header;

	if(body->s==NULL || body->len <= 0)
		return -1;

	if(dup_uri_str.s==0 || dup_uri==NULL)
		return 0;


        gettimeofday( &tvb, &tz );
        

	/* message length */
	len = body->len 
		+ sizeof(struct hep_ip6hdr)
		+ sizeof(struct hep_hdr) + sizeof(struct hep_timehdr);;


	/* The packet is too big for us */
	if (unlikely(len>BUF_SIZE)){
		goto error;
	}

	/* Convert proto:ip:port to sockaddress union SRC IP */
	if (pipport2su(from->s, &from_su, &proto)==-1 || (pipport2su(to->s, &to_su, &proto)==-1))
		goto error;

	/* check if from and to are in the same family*/
	if(from_su.s.sa_family != to_su.s.sa_family) {
		LOG(L_ERR, "ERROR: trace_send_hep_duplicate: interworking detected ?\n");
		goto error;
	}

    if (!dst2){
	init_dest_info(&dst);
	/* create a temporary proxy*/
	dst.proto = PROTO_UDP;
	p=mk_proxy(&dup_uri->host, (dup_uri->port_no)?dup_uri->port_no:SIP_PORT,
			dst.proto);
	if (p==0)
	{
		LM_ERR("bad host name in uri\n");
		goto error;
	}

	hostent2su(&dst.to, &p->host, p->addr_idx, (p->port)?p->port:SIP_PORT);
	LM_DBG("setting up the socket_info\n");
	dst_fin = &dst;
    } else {
        dst_fin = dst2;
    }

    if (force_send_sock_str.s) {
        LM_DBG("force_send_sock activated, grep for the sock_info\n");
        si = grep_sock_info(&force_send_sock_uri->host,
                (force_send_sock_uri->port_no)?force_send_sock_uri->port_no:SIP_PORT,
                PROTO_UDP);
        if (!si) {
             LM_WARN("cannot grep socket info\n");
        } else {
            LM_DBG("found socket while grep: [%.*s] [%.*s]\n", si->name.len, si->name.s, si->address_str.len, si->address_str.s);
            dst_fin->send_sock = si;
        }
    }

    if (dst_fin->send_sock == 0) {
        dst_fin->send_sock=get_send_socket(0, &dst_fin->to, dst_fin->proto);
        if (dst_fin->send_sock == 0) {
            LM_ERR("can't forward to af %d, proto %d no corresponding"
                    " listening socket\n", dst_fin->to.s.sa_family, dst_fin->proto);
            goto error;
        }
    }

	/* Version && proto && length */
	hdr.hp_l = sizeof(struct hep_hdr);
	hdr.hp_v = hep_version;
	hdr.hp_p = proto;

	/* AND the last */
	if (from_su.s.sa_family==AF_INET){
		/* prepare the hep headers */

		hdr.hp_f = AF_INET;
		hdr.hp_sport = htons(from_su.sin.sin_port);
		hdr.hp_dport = htons(to_su.sin.sin_port);

		hep_ipheader.hp_src = from_su.sin.sin_addr;
		hep_ipheader.hp_dst = to_su.sin.sin_addr;

		len = sizeof(struct hep_iphdr);
	}
	else if (from_su.s.sa_family==AF_INET6){
		/* prepare the hep6 headers */

		hdr.hp_f = AF_INET6;

		hdr.hp_sport = htons(from_su.sin6.sin6_port);
		hdr.hp_dport = htons(to_su.sin6.sin6_port);

		hep_ip6header.hp6_src = from_su.sin6.sin6_addr;
		hep_ip6header.hp6_dst = to_su.sin6.sin6_addr;

		len = sizeof(struct hep_ip6hdr);
	}
	else {
		LOG(L_ERR, "ERROR: trace_send_hep_duplicate: Unsupported protocol family\n");
		goto error;;
	}

	hdr.hp_l +=len;
	if (hep_version == 2){
		len += sizeof(struct hep_timehdr);
	}
	len += sizeof(struct hep_hdr) + body->len;
	buffer = (void *)pkg_malloc(len+1);
	if (buffer==0){
		LOG(L_ERR, "ERROR: trace_send_hep_duplicate: out of memory\n");
		goto error;
	}

	/* Copy job */
	memset(buffer, '\0', len+1);

	/* copy hep_hdr */
	memcpy((void*)buffer, &hdr, sizeof(struct hep_hdr));
	buflen = sizeof(struct hep_hdr);

	/* hep_ip_hdr */
	if(from_su.s.sa_family==AF_INET) {
		memcpy((void*)buffer + buflen, &hep_ipheader, sizeof(struct hep_iphdr));
		buflen += sizeof(struct hep_iphdr);
	}
	else {
		memcpy((void*)buffer+buflen, &hep_ip6header, sizeof(struct hep_ip6hdr));
		buflen += sizeof(struct hep_ip6hdr);
	}

	if(hep_version == 2) {

                hep_time.tv_sec = tvb.tv_sec;
                hep_time.tv_usec = tvb.tv_usec;
                hep_time.captid = hep_capture_id;

                memcpy((void*)buffer+buflen, &hep_time, sizeof(struct hep_timehdr));
                buflen += sizeof(struct hep_timehdr);
        }

	/* PAYLOAD */
	memcpy((void*)(buffer + buflen) , (void*)body->s, body->len);
	buflen +=body->len;

	if (msg_send(dst_fin, buffer, buflen)<0)
	{
		LM_ERR("cannot send hep duplicate message\n");
		goto error;
	}

    if (p) {
	free_proxy(p); /* frees only p content, not p itself */
	pkg_free(p);
    }
	pkg_free(buffer);
	return 0;
error:
	if(p)
	{
		free_proxy(p); /* frees only p content, not p itself */
		pkg_free(p);
	}
	if(buffer) pkg_free(buffer);
	return -1;
}

/*!
 * \brief Convert a STR [proto:]ip[:port] into socket address.
 * [proto:]ip[:port]
 * \param pipport (udp:127.0.0.1:5060 or tcp:2001:0DB8:AC10:FE01:5060)
 * \param tmp_su target structure
 * \param proto uint protocol type
 * \return success / unsuccess
 */
static int pipport2su (char *pipport, union sockaddr_union *tmp_su, unsigned int *proto)
{
	unsigned int port_no, cutlen = 4;
	struct ip_addr *ip;
	char *p, *host_s;
	str port_str, host_uri;
	unsigned len = 0;
	char tmp_piport[256];

	/*parse protocol */
	if(strncmp(pipport, "udp:",4) == 0) *proto = IPPROTO_UDP;
	else if(strncmp(pipport, "tcp:",4) == 0) *proto = IPPROTO_TCP;
	else if(strncmp(pipport, "tls:",4) == 0) *proto = IPPROTO_IDP; /* fake proto type */
	else if(strncmp(pipport, "ws:",3) == 0) *proto = IPPROTO_IDP; /* fake proto type */
	else if(strncmp(pipport, "wss:",4) == 0) *proto = IPPROTO_IDP; /* fake proto type */
#ifdef USE_SCTP
	else if(strncmp(pipport, "sctp:",5) == 0) cutlen = 5, *proto = IPPROTO_SCTP;
#endif
	else if(strncmp(pipport, "any:",4) == 0) *proto = IPPROTO_UDP;
	else {
		LM_ERR("bad protocol %s\n", pipport);
		return -1;
	}
	
	if((len = strlen(pipport)) >= 256) {
		LM_ERR("too big pipport\n");
		goto error;
	}

	/* our tmp string */
        strncpy(tmp_piport, pipport, len+1);

	len = 0;

	/*separate proto and host */
	p = tmp_piport+cutlen;
	if( (*(p)) == '\0') {
		LM_ERR("malformed ip address\n");
		goto error;
	}
	host_s=p;

	if( (p = strrchr(p+1, ':')) == 0 ) {
		LM_DBG("no port specified\n");
		port_no = 0;
	}
	else {
        	/*the address contains a port number*/
        	*p = '\0';
        	p++;
        	port_str.s = p;
        	port_str.len = strlen(p);
        	LM_DBG("the port string is %s\n", p);
        	if(str2int(&port_str, &port_no) != 0 ) {
	        	LM_ERR("there is not a valid number port\n");
	        	goto error;
        	}
        	*p = '\0';
        }
        
	/* now IPv6 address has no brakets. It should be fixed! */
	if (host_s[0] == '[') {
		len = strlen(host_s + 1) - 1;
		if(host_s[len+1] != ']') {
			LM_ERR("bracket not closed\n");
			goto error;
		}
		memmove(host_s, host_s + 1, len);
		host_s[len] = '\0';
	}

	host_uri.s = host_s;
	host_uri.len = strlen(host_s);

	/* check if it's an ip address */
	if (((ip=str2ip(&host_uri))!=0)
			|| ((ip=str2ip6(&host_uri))!=0)
	   ) {
		ip_addr2su(tmp_su, ip, ntohs(port_no));
		return 0;	

	}

error:
	return -1;
}

static void siptrace_rpc_status (rpc_t* rpc, void* c) {
	str status = {0, 0};

	if (rpc->scan(c, "S", &status) < 1) {
		rpc->fault(c, 500, "Not enough parameters (on, off or check)");
		return;
	}

	if(trace_on_flag==NULL) {
		rpc->fault(c, 500, "Internal error");
		return;
	}

	if (strncasecmp(status.s, "on", strlen("on")) == 0) {
		*trace_on_flag = 1;
		rpc->rpl_printf(c, "Enabled");
		return;
	}
	if (strncasecmp(status.s, "off", strlen("off")) == 0) {
		*trace_on_flag = 0;
		rpc->rpl_printf(c, "Disabled");
		return;
	}
	if (strncasecmp(status.s, "check", strlen("check")) == 0) {
		rpc->rpl_printf(c, *trace_on_flag ? "Enabled" : "Disabled");
		return;
	} 
	rpc->fault(c, 500, "Bad parameter (on, off or check)");
	return;
}

static const char* siptrace_status_doc[2] = {
        "Get status or turn on/off siptrace. Parameters: on, off or check.",
        0
};

rpc_export_t siptrace_rpc[] = {
	{"siptrace.status", siptrace_rpc_status, siptrace_status_doc, 0},
	{0, 0, 0, 0}
};

static int siptrace_init_rpc(void)
{
	if (rpc_register_array(siptrace_rpc)!=0)
	{
		LM_ERR("failed to register RPC commands\n");
		return -1;
	}
	return 0;
}

