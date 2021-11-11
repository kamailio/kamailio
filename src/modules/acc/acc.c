/*
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
 */

/*! \file
 * \ingroup acc
 * \brief Acc:: Core accounting
 *
 * Module: \ref acc
 */

#include <stdio.h>
#include <time.h>

#include "../../core/dprint.h"
#include "../../core/error.h"
#include "../../core/mem/mem.h"
#include "../../core/usr_avp.h"
#include "../../core/async_task.h"
#include "../../lib/srdb1/db.h"
#include "../../core/parser/hf.h"
#include "../../core/parser/msg_parser.h"
#include "../../core/parser/parse_from.h"
#include "../../core/parser/digest/digest.h"
#include "../../modules/tm/t_funcs.h"
#include "acc_mod.h"
#include "acc.h"
#include "acc_extra.h"
#include "acc_logic.h"
#include "acc_api.h"

extern struct acc_extra *log_extra;
extern struct acc_extra *leg_info;
extern struct acc_enviroment acc_env;
extern char *acc_time_format;
extern int acc_extra_nullable;

static db_func_t acc_dbf;
static db1_con_t* db_handle=0;
extern struct acc_extra *db_extra;

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

	LM_DBG("default - totag[%.*s]\n", c_vals[2].len, c_vals[2].s);
	if (c_vals[2].len == 0 && acc_env.to_tag.s && acc_env.to_tag.len > 0) {
		LM_DBG("extra [%p] totag[%.*s]\n", acc_env.to_tag.s, acc_env.to_tag.len, acc_env.to_tag.s);
		c_vals[2].len = acc_env.to_tag.len;
		c_vals[2].s = acc_env.to_tag.s;
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
	struct tm t;
	double dtime;

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
		dtime = (double)acc_env.tv.tv_usec;
		dtime = (dtime / 1000000) + (double)acc_env.tv.tv_sec;
		LM_GEN2(log_facility, log_level, "%.*stimestamp=%lu;%s=%.3f%s",
				acc_env.text.len, acc_env.text.s,(unsigned long)acc_env.ts,
				acc_time_attr.s, dtime, log_msg);
	} else if(acc_time_mode==3 || acc_time_mode==4) {
		if(acc_time_mode==3) {
			localtime_r(&acc_env.ts, &t);
		} else {
			gmtime_r(&acc_env.ts, &t);
		}
		if(strftime(acc_time_format_buf, ACC_TIME_FORMAT_SIZE,
					acc_time_format, &t)<=0) {
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

int acc_is_db_ready(void)
{
	if(db_handle!=0)
		return 1;

	return 0;
}

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
	struct tm t;
	double dtime;

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
		dtime = (double)acc_env.tv.tv_usec;
		dtime = (dtime / 1000000) + (double)acc_env.tv.tv_sec;
		VAL_DOUBLE(db_vals+(m++)) = dtime;
		i++;
	} else if(acc_time_mode==3 || acc_time_mode==4) {
		if(acc_time_mode==3) {
			localtime_r(&acc_env.ts, &t);
		} else {
			gmtime_r(&acc_env.ts, &t);
		}
		if(strftime(acc_time_format_buf, ACC_TIME_FORMAT_SIZE,
					acc_time_format, &t)<=0) {
			acc_time_format_buf[0] = '\0';
		}
		VAL_STRING(db_vals+(m++)) = acc_time_format_buf;
		i++;
	}

	/* extra columns */
	o = extra2strar( db_extra, rq, val_arr+m, int_arr+m, type_arr+m);
	m += o;

	for( i++ ; i<m; i++) {
		if (acc_extra_nullable == 1 && type_arr[i] == TYPE_NULL) {
			LM_DBG("attr[%d] is NULL\n", i);
			VAL_NULL(db_vals + i) = 1;
		} else {
			VAL_STR(db_vals+i) = val_arr[i];
		}
	}

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
		} else if(acc_db_insert_mode==2 && acc_dbf.insert_async!=NULL
				&& async_task_workers_active()) {
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
			for (i=m; i<m+n; i++) {
			if (acc_extra_nullable == 1 && type_arr[i] == TYPE_NULL) {
					VAL_NULL(db_vals + i) = 1;
				} else {
					VAL_STR(db_vals+i)=val_arr[i];
				}
			}
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

/**
 * @brief test if acc flag from enternal engines is set
 */
int is_eng_acc_on(sip_msg_t *msg)
{
	acc_engine_t *e;

	e = acc_api_get_engines();

	if(e==NULL) {
		return 0;
	}
	while(e) {
		if(e->flags & 1) {
			if(isflagset(msg, e->acc_flag) == 1) {
				return 1;
			}
		}
		e = e->next;
	}
	return 0;
}

/**
 * @brief test if acc flag from enternal engines is set
 */
int is_eng_mc_on(sip_msg_t *msg)
{
	acc_engine_t *e;

	e = acc_api_get_engines();

	if(e==NULL) {
		return 0;
	}
	while(e) {
		if(e->flags & 1) {
			if(isflagset(msg, e->missed_flag) == 1) {
				return 1;
			}
		}
		e = e->next;
	}
	return 0;
}

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
			if((type==0) && isflagset(msg, e->acc_flag) == 1) {
				LM_DBG("acc event for engine: %s\n", e->name);
				e->acc_req(msg, &inf);
				if(reset) *reset |= 1 << e->acc_flag;
			}
			if((type==1) && isflagset(msg, e->missed_flag) == 1) {
				LM_DBG("missed event for engine: %s\n", e->name);
				e->acc_req(msg, &inf);
				if(reset) *reset |= 1 << e->missed_flag;
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

