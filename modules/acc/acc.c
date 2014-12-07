/*
 * $Id$
 *
 * Copyright (C) 2001-2003 FhG Fokus
 * Copyright (C) 2006 Voice Sistem SRL
 * Copyright (C) 2008 Juha Heinanen
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
 * History:
 * --------
 * 2003-04-04  grand acc cleanup (jiri)
 * 2003-11-04  multidomain support for mysql introduced (jiri)
 * 2004-06-06  updated to the new DB api, cleanup: acc_db_{bind, init,close)
 *              added (andrei)
 * 2005-05-30  acc_extra patch commited (ramona)
 * 2005-06-28  multi leg call support added (bogdan)
 * 2006-01-13  detect_direction (for sequential requests) added (bogdan)
 * 2006-09-08  flexible multi leg accounting support added,
 *             code cleanup for low level functions (bogdan)
 * 2006-09-19  final stage of a masive re-structuring and cleanup (bogdan)
 * 2008-09-03  added support for integer type Radius attributes (jh)
 */

/*! \file
 * \ingroup acc
 * \brief Acc:: Core accounting
 * 
 * Module: \ref acc
 */

#include <stdio.h>
#include <time.h>

#include "../../dprint.h"
#include "../../error.h"
#include "../../mem/mem.h"
#include "../../usr_avp.h"
#include "../../lib/srdb1/db.h"
#include "../../parser/hf.h"
#include "../../parser/msg_parser.h"
#include "../../parser/parse_from.h"
#include "../../parser/digest/digest.h"
#include "../../modules/tm/t_funcs.h"
#include "acc_mod.h"
#include "acc.h"
#include "acc_extra.h"
#include "acc_logic.h"
#include "acc_api.h"

#ifdef RAD_ACC
#include "../../lib/kcore/radius.h"
#endif

#ifdef DIAM_ACC
#include "diam_dict.h"
#include "diam_message.h"
#include "diam_tcp.h"
#endif

extern struct acc_extra *log_extra;
extern struct acc_extra *leg_info;
extern struct acc_enviroment acc_env;
extern char *acc_time_format;

#ifdef RAD_ACC
extern struct acc_extra *rad_extra;
#endif

#ifdef DIAM_ACC
extern char *diameter_client_host;
extern int diameter_client_port;
extern struct acc_extra *dia_extra;
#endif

#ifdef SQL_ACC
static db_func_t acc_dbf;
static db1_con_t* db_handle=0;
extern struct acc_extra *db_extra;
#endif

/* arrays used to collect the values before being
 * pushed to the storage backend (whatever used)
 * (3 = datetime + max 2 from time_mode) */
static str val_arr[ACC_CORE_LEN+MAX_ACC_EXTRA+MAX_ACC_LEG+3];
static int int_arr[ACC_CORE_LEN+MAX_ACC_EXTRA+MAX_ACC_LEG+3];
static char type_arr[ACC_CORE_LEN+MAX_ACC_EXTRA+MAX_ACC_LEG+3];

#define ACC_TIME_FORMAT_SIZE	128
static char acc_time_format_buf[ACC_TIME_FORMAT_SIZE];

/********************************************
 *        acc CORE function
 ********************************************/
#define get_ft_body( _ft_hdr) \
	((struct to_body*)_ft_hdr->parsed)

#define SET_EMPTY_VAL(_i) \
	do { \
		c_vals[_i].s = 0; \
		c_vals[_i].len = 0; \
	} while(0)

/* returns:
 * 		method name
 * 		from TAG
 * 		to TAG
 * 		callid
 * 		sip_code
 * 		sip_status
 * 		*/
int core2strar(struct sip_msg *req, str *c_vals, int *i_vals, char *t_vals)
{
	struct to_body *ft_body;
	struct hdr_field *from;
	struct hdr_field *to;

	/* method : request/reply - cseq parsed in acc_preparse_req() */
	c_vals[0] = get_cseq(req)->method;
	t_vals[0] = TYPE_STR;

	/* from/to URI and TAG */
	if (req->msg_flags&FL_REQ_UPSTREAM) {
		LM_DBG("the flag UPSTREAM is set -> swap F/T\n"); \
		from = acc_env.to;
		to = req->from;
	} else {
		from = req->from;
		to = acc_env.to;
	}

	if (from && (ft_body=get_ft_body(from)) && ft_body->tag_value.len) {
		c_vals[1] = ft_body->tag_value;
		t_vals[1] = TYPE_STR;
	} else {
		SET_EMPTY_VAL(1);
		t_vals[1] = TYPE_NULL;
	}

	if (to && (ft_body=get_ft_body(to)) && ft_body->tag_value.len) {
		c_vals[2] = ft_body->tag_value;
		t_vals[2] = TYPE_STR;
	} else {
		SET_EMPTY_VAL(2);
		t_vals[2] = TYPE_NULL;
	}

	/* Callid */
	if (req->callid && req->callid->body.len) {
		c_vals[3] = req->callid->body;
		t_vals[3] = TYPE_STR;
	} else {
		SET_EMPTY_VAL(3);
		t_vals[3] = TYPE_NULL;
	}

	/* SIP code */
	c_vals[4] = acc_env.code_s;
	i_vals[4] = acc_env.code;
	t_vals[4] = TYPE_INT;

	/* SIP status */
	c_vals[5] = acc_env.reason;
	t_vals[5] = TYPE_STR;

	gettimeofday(&acc_env.tv, NULL);
	acc_env.ts = acc_env.tv.tv_sec;

	return ACC_CORE_LEN;
}



/********************************************
 *        LOG  ACCOUNTING
 ********************************************/
static str log_attrs[ACC_CORE_LEN+MAX_ACC_EXTRA+MAX_ACC_LEG];

#define SET_LOG_ATTR(_n,_atr)  \
	do { \
		log_attrs[_n].s=A_##_atr; \
		log_attrs[_n].len=A_##_atr##_LEN; \
		n++; \
	} while(0)

void acc_log_init(void)
{
	struct acc_extra *extra;
	int n;

	n = 0;

	/* fixed core attributes */
	SET_LOG_ATTR(n,METHOD);
	SET_LOG_ATTR(n,FROMTAG);
	SET_LOG_ATTR(n,TOTAG);
	SET_LOG_ATTR(n,CALLID);
	SET_LOG_ATTR(n,CODE);
	SET_LOG_ATTR(n,STATUS);

	/* init the extra db keys */
	for(extra=log_extra; extra ; extra=extra->next)
		log_attrs[n++] = extra->name;

	/* multi leg call columns */
	for( extra=leg_info ; extra ; extra=extra->next)
		log_attrs[n++] = extra->name;
}


int acc_log_request( struct sip_msg *rq)
{
	static char log_msg[MAX_SYSLOG_SIZE];
	static char *log_msg_end=log_msg+MAX_SYSLOG_SIZE-2;
	char *p;
	int n;
	int m;
	int o;
	int i;
	struct tm *t;

	/* get default values */
	m = core2strar( rq, val_arr, int_arr, type_arr);

	/* get extra values */
	o = extra2strar( log_extra, rq, val_arr+m, int_arr+m, type_arr+m);
	m += o;

	for ( i=0,p=log_msg ; i<m ; i++ ) {
		if (p+1+log_attrs[i].len+1+val_arr[i].len >= log_msg_end) {
			LM_WARN("acc message too long, truncating..\n");
			p = log_msg_end;
			break;
		}
		*(p++) = A_SEPARATOR_CHR;
		memcpy(p, log_attrs[i].s, log_attrs[i].len);
		p += log_attrs[i].len;
		*(p++) = A_EQ_CHR;
		if (val_arr[i].s != NULL) {
			memcpy(p, val_arr[i].s, val_arr[i].len);
			p += val_arr[i].len;
		}
	}

	/* get per leg attributes */
	if ( leg_info ) {
		n = legs2strar(leg_info,rq,val_arr+m,int_arr+m,type_arr+m, 1);
		do {
			for (i=m; i<m+n; i++) {
				if (p+1+log_attrs[i].len+1+val_arr[i].len >= log_msg_end) {
					LM_WARN("acc message too long, truncating..\n");
					p = log_msg_end;
					break;
				}
				*(p++) = A_SEPARATOR_CHR;
				memcpy(p, log_attrs[i].s, log_attrs[i].len);
				p += log_attrs[i].len;
				*(p++) = A_EQ_CHR;
				if (val_arr[i].s != NULL) {
					memcpy(p, val_arr[i].s, val_arr[i].len);
					p += val_arr[i].len;
				}
			}
		} while (p!=log_msg_end && (n=legs2strar(leg_info,rq,val_arr+m,
							int_arr+m,type_arr+m,
							0))!=0);
	}

	/* terminating line */
	*(p++) = '\n';
	*(p++) = 0;

	if(acc_time_mode==1) {
		LM_GEN2(log_facility, log_level, "%.*stimestamp=%lu;%s=%u%s",
			acc_env.text.len, acc_env.text.s,(unsigned long)acc_env.ts,
			acc_time_exten.s, (unsigned int)acc_env.tv.tv_usec,
			log_msg);
	} else if(acc_time_mode==2) {
		LM_GEN2(log_facility, log_level, "%.*stimestamp=%lu;%s=%.3f%s",
			acc_env.text.len, acc_env.text.s,(unsigned long)acc_env.ts,
			acc_time_attr.s,
			(((double)(acc_env.tv.tv_sec * 1000)
							+ (acc_env.tv.tv_usec / 1000)) / 1000),
			log_msg);
	} else if(acc_time_mode==3 || acc_time_mode==4) {
		if(acc_time_mode==3) {
			t = localtime(&acc_env.ts);
		} else {
			t = gmtime(&acc_env.ts);
		}
		if(strftime(acc_time_format_buf, ACC_TIME_FORMAT_SIZE,
					acc_time_format, t)<=0) {
			acc_time_format_buf[0] = '\0';
		}
		LM_GEN2(log_facility, log_level, "%.*stimestamp=%lu;%s=%s%s",
			acc_env.text.len, acc_env.text.s,(unsigned long)acc_env.ts,
			acc_time_attr.s,
			acc_time_format_buf,
			log_msg);
	} else {
		LM_GEN2(log_facility, log_level, "%.*stimestamp=%lu%s",
			acc_env.text.len, acc_env.text.s,(unsigned long)acc_env.ts,
			log_msg);
	}
	/* free memory allocated by extra2strar */
	free_strar_mem( &(type_arr[m-o]), &(val_arr[m-o]), o, m);

	return 1;
}


/********************************************
 *        SQL  ACCOUNTING
 ********************************************/

#ifdef SQL_ACC

/* caution: keys need to be aligned to core format
 * (3 = datetime + max 2 from time_mode) */
static db_key_t db_keys[ACC_CORE_LEN+3+MAX_ACC_EXTRA+MAX_ACC_LEG];
static db_val_t db_vals[ACC_CORE_LEN+3+MAX_ACC_EXTRA+MAX_ACC_LEG];


int acc_get_db_handlers(void **vf, void **vh) {
	if(db_handle==0)
		return -1;
	*vf = (void*)&acc_dbf;
	*vh = (void*)db_handle;
	return 0;
}

static void acc_db_init_keys(void)
{
	struct acc_extra *extra;
	int time_idx;
	int i;
	int n;

	/* init the static db keys */
	n = 0;
	/* caution: keys need to be aligned to core format */
	db_keys[n++] = &acc_method_col;
	db_keys[n++] = &acc_fromtag_col;
	db_keys[n++] = &acc_totag_col;
	db_keys[n++] = &acc_callid_col;
	db_keys[n++] = &acc_sipcode_col;
	db_keys[n++] = &acc_sipreason_col;
	db_keys[n++] = &acc_time_col;
	time_idx = n-1;
	if(acc_time_mode==1 || acc_time_mode==2
			|| acc_time_mode==3 || acc_time_mode==4) {
		db_keys[n++] = &acc_time_attr;
		if(acc_time_mode==1) {
			db_keys[n++] = &acc_time_exten;
		}
	}

	/* init the extra db keys */
	for(extra=db_extra; extra ; extra=extra->next)
		db_keys[n++] = &extra->name;

	/* multi leg call columns */
	for( extra=leg_info ; extra ; extra=extra->next)
		db_keys[n++] = &extra->name;

	/* init the values */
	for(i=0; i<n; i++) {
		VAL_TYPE(db_vals+i)=DB1_STR;
		VAL_NULL(db_vals+i)=0;
	}
	VAL_TYPE(db_vals+time_idx)=DB1_DATETIME;
	if(acc_time_mode==1) {
		VAL_TYPE(db_vals+time_idx+1)=DB1_INT;
		VAL_TYPE(db_vals+time_idx+2)=DB1_INT;
	} else if(acc_time_mode==2) {
		VAL_TYPE(db_vals+time_idx+1)=DB1_DOUBLE;
	} else if(acc_time_mode==3 || acc_time_mode==4) {
		VAL_TYPE(db_vals+time_idx+1)=DB1_STRING;
	}
}


/* binds to the corresponding database module
 * returns 0 on success, -1 on error */
int acc_db_init(const str* db_url)
{
	if (db_bind_mod(db_url, &acc_dbf)<0){
		LM_ERR("bind_db failed\n");
		return -1;
	}

	/* Check database capabilities */
	if (!DB_CAPABILITY(acc_dbf, DB_CAP_INSERT)) {
		LM_ERR("database module does not implement insert function\n");
		return -1;
	}

	acc_db_init_keys();

	return 0;
}


/* initialize the database connection
 * returns 0 on success, -1 on error */
int acc_db_init_child(const str *db_url)
{
	db_handle=acc_dbf.init(db_url);
	if (db_handle==0){
		LM_ERR("unable to connect to the database\n");
		return -1;
	}
	return 0;
}


/* close a db connection */
void acc_db_close(void)
{
	if (db_handle && acc_dbf.close)
		acc_dbf.close(db_handle);
}


int acc_db_request( struct sip_msg *rq)
{
	int m;
	int n;
	int i;
	int o;
	struct tm *t;

	/* formated database columns */
	m = core2strar( rq, val_arr, int_arr, type_arr );

	for(i=0; i<m; i++)
		VAL_STR(db_vals+i) = val_arr[i];
	/* time value */
	VAL_TIME(db_vals+(m++)) = acc_env.ts;
	/* extra time value */
	if(acc_time_mode==1) {
		VAL_INT(db_vals+(m++)) = (int)acc_env.tv.tv_sec;
		i++;
		VAL_INT(db_vals+(m++)) = (int)acc_env.tv.tv_usec;
		i++;
	} else if(acc_time_mode==2) {
		VAL_DOUBLE(db_vals+(m++)) = ((double)(acc_env.tv.tv_sec * 1000)
							+ (acc_env.tv.tv_usec / 1000)) / 1000;
		i++;
	} else if(acc_time_mode==3 || acc_time_mode==4) {
		if(acc_time_mode==3) {
			t = localtime(&acc_env.ts);
		} else {
			t = gmtime(&acc_env.ts);
		}
		if(strftime(acc_time_format_buf, ACC_TIME_FORMAT_SIZE,
					acc_time_format, t)<=0) {
			acc_time_format_buf[0] = '\0';
		}
		VAL_STRING(db_vals+(m++)) = acc_time_format_buf;
		i++;
	}

	/* extra columns */
	o = extra2strar( db_extra, rq, val_arr+m, int_arr+m, type_arr+m);
	m += o;

	for( i++ ; i<m; i++)
		VAL_STR(db_vals+i) = val_arr[i];

	if (acc_dbf.use_table(db_handle, &acc_env.text/*table*/) < 0) {
		LM_ERR("error in use_table\n");
		goto error;
	}

	/* multi-leg columns */
	if ( !leg_info ) {
		if(acc_db_insert_mode==1 && acc_dbf.insert_delayed!=NULL) {
			if (acc_dbf.insert_delayed(db_handle, db_keys, db_vals, m) < 0) {
				LM_ERR("failed to insert delayed into database\n");
				goto error;
			}
		} else if(acc_db_insert_mode==2 && acc_dbf.insert_async!=NULL) {
			if (acc_dbf.insert_async(db_handle, db_keys, db_vals, m) < 0) {
				LM_ERR("failed to insert async into database\n");
				goto error;
			}
		} else {
			if (acc_dbf.insert(db_handle, db_keys, db_vals, m) < 0) {
				LM_ERR("failed to insert into database\n");
				goto error;
			}
		}
	} else {
		n = legs2strar(leg_info,rq,val_arr+m,int_arr+m,type_arr+m,1);
		do {
			for (i=m; i<m+n; i++)
				VAL_STR(db_vals+i)=val_arr[i];
			if(acc_db_insert_mode==1 && acc_dbf.insert_delayed!=NULL) {
				if(acc_dbf.insert_delayed(db_handle,db_keys,db_vals,m+n)<0) {
					LM_ERR("failed to insert delayed into database\n");
					goto error;
				}
			} else if(acc_db_insert_mode==2 && acc_dbf.insert_async!=NULL) {
				if(acc_dbf.insert_async(db_handle,db_keys,db_vals,m+n)<0) {
					LM_ERR("failed to insert async into database\n");
					goto error;
				}
			} else {
				if (acc_dbf.insert(db_handle, db_keys, db_vals, m+n) < 0) {
					LM_ERR("failed to insert into database\n");
					goto error;
				}
			}
		}while ( (n=legs2strar(leg_info,rq,val_arr+m,int_arr+m,
				       type_arr+m,0))!=0 );
	}

	/* free memory allocated by extra2strar */
	free_strar_mem( &(type_arr[m-o]), &(val_arr[m-o]), o, m);
	return 1;
error:
	/* free memory allocated by extra2strar */
	free_strar_mem( &(type_arr[m-o]), &(val_arr[m-o]), o, m);
	return -1;
}

#endif


/************ RADIUS & DIAMETER helper functions **************/
#if defined(RAD_ACC) || defined (DIAM_ACC)
#ifndef UINT4
#define UINT4 uint32_t
#endif
inline static UINT4 phrase2code(str *phrase)
{
	UINT4 code;
	int i;

	if (phrase->len<3) return 0;
	code=0;
	for (i=0;i<3;i++) {
		if (!(phrase->s[i]>='0' && phrase->s[i]<'9'))
				return 0;
		code=code*10+phrase->s[i]-'0';
	}
	return code;
}
#endif


/********************************************
 *        RADIUS  ACCOUNTING
 ********************************************/
#ifdef RAD_ACC
enum { RA_ACCT_STATUS_TYPE=0, RA_SERVICE_TYPE, RA_SIP_RESPONSE_CODE,
	RA_SIP_METHOD, RA_TIME_STAMP, RA_STATIC_MAX};
enum {RV_STATUS_START=0, RV_STATUS_STOP, RV_STATUS_ALIVE, RV_STATUS_FAILED,
	RV_SIP_SESSION, RV_STATIC_MAX};
static struct attr
	rd_attrs[RA_STATIC_MAX+ACC_CORE_LEN-2+MAX_ACC_EXTRA+MAX_ACC_LEG];
static struct val rd_vals[RV_STATIC_MAX];

int init_acc_rad(char *rad_cfg, int srv_type)
{
	int n;

	memset(rd_attrs, 0, sizeof(rd_attrs));
	memset(rd_vals, 0, sizeof(rd_vals));
	rd_attrs[RA_ACCT_STATUS_TYPE].n  = "Acct-Status-Type";
	rd_attrs[RA_SERVICE_TYPE].n      = "Service-Type";
	rd_attrs[RA_SIP_RESPONSE_CODE].n = "Sip-Response-Code";
	rd_attrs[RA_SIP_METHOD].n        = "Sip-Method";
	rd_attrs[RA_TIME_STAMP].n        = "Event-Timestamp";
	n = RA_STATIC_MAX;
	/* caution: keep these aligned to core acc output */
	rd_attrs[n++].n                  = "Sip-From-Tag";
	rd_attrs[n++].n                  = "Sip-To-Tag";
	rd_attrs[n++].n                  = "Acct-Session-Id";

	rd_vals[RV_STATUS_START].n        = "Start";
	rd_vals[RV_STATUS_STOP].n         = "Stop";
	rd_vals[RV_STATUS_ALIVE].n        = "Alive";
	rd_vals[RV_STATUS_FAILED].n       = "Failed";
	rd_vals[RV_SIP_SESSION].n         = "Sip-Session";

	/* add and count the extras as attributes */
	n += extra2attrs( rad_extra, rd_attrs, n);
	/* add and count the legs as attributes */
	n += extra2attrs( leg_info, rd_attrs, n);

	/* read config */
	if ((rh = rc_read_config(rad_cfg)) == NULL) {
		LM_ERR("failed to open radius config file: %s\n", rad_cfg );
		return -1;
	}
	/* read dictionary */
	if (rc_read_dictionary(rh, rc_conf_str(rh, "dictionary"))!=0) {
		LM_ERR("failed to read radius dictionary\n");
		return -1;
	}

	INIT_AV(rh, rd_attrs, n, rd_vals, RV_STATIC_MAX, "acc", -1, -1);

	if (srv_type != -1)
		rd_vals[RV_SIP_SESSION].v = srv_type;

	return 0;
}


static inline UINT4 rad_status( struct sip_msg *req, int code )
{
        str tag;
        unsigned int in_dialog_req = 0;

        tag = get_to(req)->tag_value;
        if(tag.s!=0 && tag.len!=0)
		in_dialog_req = 1;

	if (req->REQ_METHOD==METHOD_INVITE && in_dialog_req == 0
	            && code>=200 && code<300)
 		return rd_vals[RV_STATUS_START].v;
 	if ((req->REQ_METHOD==METHOD_BYE || req->REQ_METHOD==METHOD_CANCEL))
 		return rd_vals[RV_STATUS_STOP].v;
	if (in_dialog_req != 0)
		return rd_vals[RV_STATUS_ALIVE].v;
 	return rd_vals[RV_STATUS_FAILED].v;
 }

#define ADD_RAD_AVPAIR(_attr,_val,_len)		\
    do {								\
	if (!rc_avpair_add(rh, &send, rd_attrs[_attr].v, _val, _len, 0)) { \
	    LM_ERR("failed to add %s, %d\n", rd_attrs[_attr].n, _attr);	\
	    goto error;							\
	} \
    }while(0)

int acc_rad_request( struct sip_msg *req )
{
	int attr_cnt;
	VALUE_PAIR *send;
	UINT4 av_type;
	int offset;
	int i;
	int m;
	int o;

	send=NULL;

	attr_cnt = core2strar( req, val_arr, int_arr, type_arr );
	/* not interested in the last 2 values */
	attr_cnt -= 2;

	av_type = rad_status( req, acc_env.code); /* RADIUS status */
	ADD_RAD_AVPAIR( RA_ACCT_STATUS_TYPE, &av_type, -1);

	av_type = rd_vals[RV_SIP_SESSION].v; /* session*/
	ADD_RAD_AVPAIR( RA_SERVICE_TYPE, &av_type, -1);

	av_type = (UINT4)acc_env.code; /* status=integer */
	ADD_RAD_AVPAIR( RA_SIP_RESPONSE_CODE, &av_type, -1);

	av_type = req->REQ_METHOD; /* method */
	ADD_RAD_AVPAIR( RA_SIP_METHOD, &av_type, -1);

	/* unix time */
	av_type = (UINT4)acc_env.ts;
	ADD_RAD_AVPAIR( RA_TIME_STAMP, &av_type, -1);

	/* add extra also */
	o = extra2strar(rad_extra, req, val_arr+attr_cnt,
				int_arr+attr_cnt, type_arr+attr_cnt);
	attr_cnt += o;
	m = attr_cnt;

	/* add the values for the vector - start from 1 instead of
	 * 0 to skip the first value which is the METHOD as string */
	offset = RA_STATIC_MAX-1;
	for( i=1; i<attr_cnt; i++) {
	    switch (type_arr[i]) {
	    case TYPE_STR:
		ADD_RAD_AVPAIR(offset+i, val_arr[i].s, val_arr[i].len);
		break;
	    case TYPE_INT:
		ADD_RAD_AVPAIR(offset+i, &(int_arr[i]), -1);
		break;
	    default:
		break;
	    }
	}

	/* call-legs attributes also get inserted */
	if ( leg_info ) {
		offset += attr_cnt;
		attr_cnt = legs2strar(leg_info,req,val_arr,int_arr,type_arr,1);
		do {
			for (i=0; i<attr_cnt; i++)
				ADD_RAD_AVPAIR( offset+i, val_arr[i].s, val_arr[i].len );
		}while ( (attr_cnt=legs2strar(leg_info,req,val_arr,int_arr,
					      type_arr, 0))!=0 );
	}

	if (rc_acct(rh, SIP_PORT, send)!=OK_RC) {
		LM_ERR("radius-ing failed\n");
		goto error;
	}
	rc_avpair_free(send);
	/* free memory allocated by extra2strar */
	free_strar_mem( &(type_arr[m-o]), &(val_arr[m-o]), o, m);
	return 1;

error:
	rc_avpair_free(send);
	/* free memory allocated by extra2strar */
	free_strar_mem( &(type_arr[m-o]), &(val_arr[m-o]), o, m);
	return -1;
}

#endif


/********************************************
 *        DIAMETER  ACCOUNTING
 ********************************************/
#ifdef DIAM_ACC

#define AA_REQUEST 265
#define AA_ANSWER  265

#define ACCOUNTING_REQUEST 271
#define ACCOUNTING_ANSWER  271

static int diam_attrs[ACC_CORE_LEN+MAX_ACC_EXTRA+MAX_ACC_LEG];

int acc_diam_init()
{
	int n;
	int m;

	n = 0;
	/* caution: keep these aligned to core acc output */
	diam_attrs[n++] = AVP_SIP_METHOD;
	diam_attrs[n++] = AVP_SIP_FROM_TAG;
	diam_attrs[n++] = AVP_SIP_TO_TAG;
	diam_attrs[n++] = AVP_SIP_CALLID;
	diam_attrs[n++] = AVP_SIP_STATUS;

	m = extra2int( dia_extra, diam_attrs+n);
	if (m<0) {
		LM_ERR("extra names for DIAMETER must be integer AVP codes\n");
		return -1;
	}
	n += m;

	m = extra2int( leg_info, diam_attrs+n);
	if (m<0) {
		LM_ERR("leg info names for DIAMTER must be integer AVP codes\n");
		return -1;
	}
	n += m;

	return 0;
}


inline unsigned long diam_status(struct sip_msg *rq, int code)
{
	if ((rq->REQ_METHOD==METHOD_INVITE || rq->REQ_METHOD==METHOD_ACK)
				&& code>=200 && code<300) 
		return AAA_ACCT_START;

	if ((rq->REQ_METHOD==METHOD_BYE || rq->REQ_METHOD==METHOD_CANCEL))
		return AAA_ACCT_STOP;

	if (code>=200 && code <=300)  
		return AAA_ACCT_EVENT;

	return -1;
}


int acc_diam_request( struct sip_msg *req )
{
	int attr_cnt;
	int cnt;
	AAAMessage *send = NULL;
	AAA_AVP *avp;
	struct sip_uri puri;
	str *uri;
	int ret;
	int i;
	int status;
	char tmp[2];
	unsigned int mid;
	int m;
	int o;

	attr_cnt = core2strar( req, val_arr, int_arr, type_arr );
	/* last value is not used */
	attr_cnt--;

	if ( (send=AAAInMessage(ACCOUNTING_REQUEST, AAA_APP_NASREQ))==NULL) {
		LM_ERR("failed to create new AAA request\n");
		return -1;
	}

	m = 0;
	o = 0;
	/* AVP_ACCOUNTIG_RECORD_TYPE */
	if( (status = diam_status(req, acc_env.code))<0) {
		LM_ERR("status unknown\n");
		goto error;
	}
	tmp[0] = status+'0';
	tmp[1] = 0;
	if( (avp=AAACreateAVP(AVP_Accounting_Record_Type, 0, 0, tmp,
	1, AVP_DUPLICATE_DATA)) == 0) {
		LM_ERR("failed to create AVP:no more free memory!\n");
		goto error;
	}
	if( AAAAddAVPToMessage(send, avp, 0)!= AAA_ERR_SUCCESS) {
		LM_ERR("avp not added \n");
		AAAFreeAVP(&avp);
		goto error;
	}
	/* SIP_MSGID AVP */
	mid = req->id;
	if( (avp=AAACreateAVP(AVP_SIP_MSGID, 0, 0, (char*)(&mid), 
	sizeof(mid), AVP_DUPLICATE_DATA)) == 0) {
		LM_ERR("failed to create AVP:no more free memory!\n");
		goto error;
	}
	if( AAAAddAVPToMessage(send, avp, 0)!= AAA_ERR_SUCCESS) {
		LM_ERR("avp not added \n");
		AAAFreeAVP(&avp);
		goto error;
	}

	/* SIP Service AVP */
	if( (avp=AAACreateAVP(AVP_Service_Type, 0, 0, SIP_ACCOUNTING, 
	SERVICE_LEN, AVP_DUPLICATE_DATA)) == 0) {
		LM_ERR("failed to create AVP:no more free memory!\n");
		goto error;
	}
	if( AAAAddAVPToMessage(send, avp, 0)!= AAA_ERR_SUCCESS) {
		LM_ERR("avp not added \n");
		AAAFreeAVP(&avp);
		goto error;
	}

	/* also the extra attributes */
	o = extra2strar( dia_extra, req, val_arr, int_arr, type_arr);
	attr_cnt += o;
	m = attr_cnt;

	/* add attributes */
	for(i=0; i<attr_cnt; i++) {
		if((avp=AAACreateAVP(diam_attrs[i], 0,0, val_arr[i].s, val_arr[i].len,
		AVP_DUPLICATE_DATA)) == 0) {
			LM_ERR("failed to create AVP: no more free memory!\n");
			goto error;
		}
		if( AAAAddAVPToMessage(send, avp, 0)!= AAA_ERR_SUCCESS) {
			LM_ERR("avp not added \n");
			AAAFreeAVP(&avp);
			goto error;
		}
	}

	/* and the leg attributes */
	if ( leg_info ) {
	        cnt = legs2strar(leg_info,req,val_arr,int_arr,type_arr,1);
		do {
			for (i=0; i<cnt; i++) {
				if((avp=AAACreateAVP(diam_attrs[attr_cnt+i], 0, 0,
				val_arr[i].s, val_arr[i].len, AVP_DUPLICATE_DATA)) == 0) {
					LM_ERR("failed to create AVP: no more free memory!\n");
					goto error;
				}
				if( AAAAddAVPToMessage(send, avp, 0)!= AAA_ERR_SUCCESS) {
					LM_ERR("avp not added \n");
					AAAFreeAVP(&avp);
					goto error;
				}
			}
		} while ( (cnt=legs2strar(leg_info,req,val_arr,int_arr,
					  type_arr,0))!=0 );
	}

	if (get_uri(req, &uri) < 0) {
		LM_ERR("failed to get uri, From/To URI not found\n");
		goto error;
	}

	if (parse_uri(uri->s, uri->len, &puri) < 0) {
		LM_ERR("failed to parse From/To URI\n");
		goto error;
	}

	/* Destination-Realm AVP */
	if( (avp=AAACreateAVP(AVP_Destination_Realm, 0, 0, puri.host.s,
	puri.host.len, AVP_DUPLICATE_DATA)) == 0) {
		LM_ERR("failed to create AVP:no more free memory!\n");
		goto error;
	}

	if( AAAAddAVPToMessage(send, avp, 0)!= AAA_ERR_SUCCESS) {
		LM_ERR("avp not added \n");
		AAAFreeAVP(&avp);
		goto error;
	}

	/* prepare the message to be sent over the network */
	if(AAABuildMsgBuffer(send) != AAA_ERR_SUCCESS) {
		LM_ERR("message buffer not created\n");
		goto error;
	}

	if(sockfd==AAA_NO_CONNECTION) {
		sockfd = init_mytcp(diameter_client_host, diameter_client_port);
		if(sockfd==AAA_NO_CONNECTION) {
			LM_ERR("failed to reconnect to Diameter client\n");
			goto error;
		}
	}

	/* send the message to the DIAMETER client */
	ret = tcp_send_recv(sockfd, send->buf.s, send->buf.len, rb, req->id);
	if(ret == AAA_CONN_CLOSED) {
		LM_NOTICE("connection to Diameter client closed.It will be "
				"reopened by the next request\n");
		close(sockfd);
		sockfd = AAA_NO_CONNECTION;
		goto error;
	}

	if(ret != ACC_SUCCESS) {
		/* a transmission error occurred */
		LM_ERR("message sending to the DIAMETER backend authorization "
				"server failed\n");
		goto error;
	}

	AAAFreeMessage(&send);
	/* free memory allocated by extra2strar */
	free_strar_mem( &(type_arr[m-o]), &(val_arr[m-o]), o, m);
	return 1;

error:
	AAAFreeMessage(&send);
	/* free memory allocated by extra2strar */
	free_strar_mem( &(type_arr[m-o]), &(val_arr[m-o]), o, m);
	return -1;
}

#endif

/**
 * @brief execute all acc engines for a SIP request event
 */
int acc_run_engines(struct sip_msg *msg, int type, int *reset)
{
	acc_info_t inf;
	acc_engine_t *e;

	e = acc_api_get_engines();

	if(e==NULL)
		return 0;

	memset(&inf, 0, sizeof(acc_info_t));
	inf.env  = &acc_env;
	inf.varr = val_arr;
	inf.iarr = int_arr;
	inf.tarr = type_arr;
	inf.leg_info = leg_info;
	while(e) {
		if(e->flags & 1) {
			if((type==0) && (msg->flags&(e->acc_flag))) {
				LM_DBG("acc event for engine: %s\n", e->name);
				e->acc_req(msg, &inf);
				if(reset) *reset |= e->acc_flag;
			}
			if((type==1) && (msg->flags&(e->missed_flag))) {
				LM_DBG("missed event for engine: %s\n", e->name);
				e->acc_req(msg, &inf);
				if(reset) *reset |= e->missed_flag;
			}
		}
		e = e->next;
	}
	return 0;
}

/**
 * @brief set hooks to acc_info_t attributes
 */
void acc_api_set_arrays(acc_info_t *inf)
{
	inf->varr = val_arr;
	inf->iarr = int_arr;
	inf->tarr = type_arr;
	inf->leg_info = leg_info;
}

