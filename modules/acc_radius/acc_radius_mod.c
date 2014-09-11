/*
 * $Id$
 * 
 * Accounting module
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
 * History:
 * -------
 * 2010-07-28 - moved out radius account out of acc module (daniel)
 */

/*! \file
 * \ingroup acc_radius
 * \brief Acc_radius:: Core module interface
 *
 * - Module: \ref acc_radius
 */

/*! \defgroup acc_radius ACC_RADIUS :: The Kamailio RADIUS accounting Module
 *            
 * The ACC_RADIUS module is used to account transactions information to
 *  RADIUS            
 */ 

#include <stdio.h>
#include <string.h>

#include "../../sr_module.h"
#include "../../dprint.h"
#include "../../mem/mem.h"
#include "../../parser/parse_to.h"
#include "../../lib/kcore/radius.h"
#include "../../modules/acc/acc_api.h"
#include "acc_radius_mod.h"

MODULE_VERSION

static int mod_init(void);
static void destroy(void);
static int child_init(int rank);

int acc_radius_init(acc_init_info_t *inf);
int acc_radius_send_request(struct sip_msg *req, acc_info_t *inf);

static int w_acc_radius_request(struct sip_msg *rq, char *comment, char *foo);
static int acc_api_fixup(void** param, int param_no);
static int free_acc_api_fixup(void** param, int param_no);

int init_acc_rad(acc_extra_t *leg_info, char *rad_cfg, int srv_type);
int extra2attrs(struct acc_extra *extra, struct attr *attrs, int offset);

/*! ACC API structure */
acc_api_t accb;
acc_engine_t _acc_radius_engine;

/* ----- RADIUS acc variables ----------- */
/*! \name AccRadiusVariables  Radius Variables */     
/*@{*/

static char *radius_config = 0;
int radius_flag = -1;
int radius_missed_flag = -1;
static int service_type = -1;
void *rh;
/* rad extra variables */
static char *rad_extra_str = 0;
acc_extra_t *rad_extra = 0;
/*@}*/


static cmd_export_t cmds[] = {
	{"acc_rad_request", (cmd_function)w_acc_radius_request, 1,
		acc_api_fixup, free_acc_api_fixup,
		ANY_ROUTE},
	{0, 0, 0, 0, 0, 0}
};



static param_export_t params[] = {
	{"radius_config",        PARAM_STRING, &radius_config        },
	{"radius_flag",          INT_PARAM, &radius_flag          },
	{"radius_missed_flag",   INT_PARAM, &radius_missed_flag   },
	{"service_type",         INT_PARAM, &service_type         },
	{"radius_extra",         PARAM_STRING, &rad_extra_str        },
	{0,0,0}
};


struct module_exports exports= {
	"acc_radius",
	DEFAULT_DLFLAGS, /* dlopen flags */
	cmds,       /* exported functions */
	params,     /* exported params */
	0,          /* exported statistics */
	0,          /* exported MI functions */
	0,          /* exported pseudo-variables */
	0,          /* extra processes */
	mod_init,   /* initialization module */
	0,          /* response function */
	destroy,    /* destroy function */
	child_init  /* per-child init function */
};



/************************** INTERFACE functions ****************************/

static int mod_init( void )
{
	if (radius_config==NULL || radius_config[0]=='\0') {
		LM_ERR("radius config file not set\n");
		return -1;
	}
	
	/* bind the ACC API */
	if (acc_load_api(&accb)<0) {
		LM_ERR("cannot bind to ACC API\n");
		return -1;
	}

	/* parse the extra string, if any */
	if (rad_extra_str && (rad_extra=accb.parse_extra(rad_extra_str))==0 ) {
		LM_ERR("failed to parse rad_extra param\n");
		return -1;
	}
		
	memset(&_acc_radius_engine, 0, sizeof(acc_engine_t));

	if(radius_flag != -1)
		_acc_radius_engine.acc_flag	   = 1<<radius_flag;
	if(radius_missed_flag != -1)
		_acc_radius_engine.missed_flag = 1<<radius_missed_flag;
	_acc_radius_engine.acc_req     = acc_radius_send_request;
	_acc_radius_engine.acc_init    = acc_radius_init;
	memcpy(_acc_radius_engine.name, "radius", 6);
	if(accb.register_engine(&_acc_radius_engine)<0)
	{
		LM_ERR("cannot register ACC RADIUS engine\n");
		return -1;
	}

	return 0;
}


static int child_init(int rank)
{
	if (rank==PROC_INIT || rank==PROC_MAIN || rank==PROC_TCP_MAIN)
		return 0; /* do nothing for the main process */

	return 0;
}


static void destroy(void)
{
}

static int acc_api_fixup(void** param, int param_no)
{
	struct acc_param *accp;
	char *p;

	p = (char*)*param;
	if (p==0 || p[0]==0) {
		LM_ERR("first parameter is empty\n");
		return E_SCRIPT;
	}

	if (param_no == 1) {
		accp = (struct acc_param*)pkg_malloc(sizeof(struct acc_param));
		if (!accp) {
			LM_ERR("no more pkg mem\n");
			return E_OUT_OF_MEM;
		}
		memset( accp, 0, sizeof(struct acc_param));
		accp->reason.s = p;
		accp->reason.len = strlen(p);
		/* any code? */
		if (accp->reason.len>=3 && isdigit((int)p[0])
		&& isdigit((int)p[1]) && isdigit((int)p[2]) ) {
			accp->code = (p[0]-'0')*100 + (p[1]-'0')*10 + (p[2]-'0');
			accp->code_s.s = p;
			accp->code_s.len = 3;
			accp->reason.s += 3;
			for( ; isspace((int)accp->reason.s[0]) ; accp->reason.s++ );
			accp->reason.len = strlen(accp->reason.s);
		}
		*param = (void*)accp;
#ifdef SQL_ACC
	} else if (param_no == 2) {
		/* only for db acc - the table name */
		if (db_url.s==0) {
			pkg_free(p);
			*param = 0;
		}
#endif
	}
	return 0;
}

static int free_acc_api_fixup(void** param, int param_no)
{
	if(*param)
	{
		pkg_free(*param);
		*param = 0;
	}
	return 0;
}


enum { RA_ACCT_STATUS_TYPE=0, RA_SERVICE_TYPE, RA_SIP_RESPONSE_CODE,
	RA_SIP_METHOD, RA_TIME_STAMP, RA_STATIC_MAX};
enum {RV_STATUS_START=0, RV_STATUS_STOP, RV_STATUS_ALIVE, RV_STATUS_FAILED,
	RV_SIP_SESSION, RV_STATIC_MAX};
static struct attr
	rd_attrs[RA_STATIC_MAX+ACC_CORE_LEN-2+MAX_ACC_EXTRA+MAX_ACC_LEG];
static struct val rd_vals[RV_STATIC_MAX];

int init_acc_rad(acc_extra_t *leg_info, char *rad_cfg, int srv_type)
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


int acc_radius_init(acc_init_info_t *inf)
{
	if (radius_config && radius_config[0]) {
		if (init_acc_rad(inf->leg_info, radius_config, service_type)!=0 ) {
			LM_ERR("failed to init radius\n");
			return -1;
		}
	}
	return 0;
}

static inline uint32_t rad_status( struct sip_msg *req, int code )
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

int acc_radius_send_request(struct sip_msg *req, acc_info_t *inf)
{
	int attr_cnt;
	VALUE_PAIR *send;
	uint32_t av_type;
	int offset;
	int i;

	send=NULL;

	attr_cnt = accb.get_core_attrs( req, inf->varr, inf->iarr, inf->tarr );
	/* not interested in the last 2 values */
	attr_cnt -= 2;

	av_type = rad_status( req, inf->env->code); /* RADIUS status */
	ADD_RAD_AVPAIR( RA_ACCT_STATUS_TYPE, &av_type, -1);

	av_type = rd_vals[RV_SIP_SESSION].v; /* session*/
	ADD_RAD_AVPAIR( RA_SERVICE_TYPE, &av_type, -1);

	av_type = (uint32_t)inf->env->code; /* status=integer */
	ADD_RAD_AVPAIR( RA_SIP_RESPONSE_CODE, &av_type, -1);

	av_type = req->REQ_METHOD; /* method */
	ADD_RAD_AVPAIR( RA_SIP_METHOD, &av_type, -1);

	/* unix time */
	av_type = (uint32_t)inf->env->ts;
	ADD_RAD_AVPAIR( RA_TIME_STAMP, &av_type, -1);

	/* add extra also */
	attr_cnt += accb.get_extra_attrs(rad_extra, req, inf->varr+attr_cnt,
				inf->iarr+attr_cnt, inf->tarr+attr_cnt);

	/* add the values for the vector - start from 1 instead of
	 * 0 to skip the first value which is the METHOD as string */
	offset = RA_STATIC_MAX-1;
	for( i=1; i<attr_cnt; i++) {
	    switch (inf->tarr[i]) {
	    case TYPE_STR:
		ADD_RAD_AVPAIR(offset+i, inf->varr[i].s, inf->varr[i].len);
		break;
	    case TYPE_INT:
		ADD_RAD_AVPAIR(offset+i, &(inf->iarr[i]), -1);
		break;
	    default:
		break;
	    }
	}

	/* call-legs attributes also get inserted */
	if ( inf->leg_info ) {
		offset += attr_cnt;
		attr_cnt = accb.get_leg_attrs(inf->leg_info,req,inf->varr,inf->iarr,inf->tarr,1);
		do {
			for (i=0; i<attr_cnt; i++)
				ADD_RAD_AVPAIR( offset+i, inf->varr[i].s, inf->varr[i].len );
		}while ( (attr_cnt=accb.get_leg_attrs(inf->leg_info,req,inf->varr,inf->iarr,
					      inf->tarr, 0))!=0 );
	}

	if (rc_acct(rh, SIP_PORT, send)!=OK_RC) {
		LM_ERR("radius-ing failed\n");
		goto error;
	}
	rc_avpair_free(send);
	return 1;

error:
	rc_avpair_free(send);
	return -1;
}

/*! \brief extra name is moved as string part of an attribute; str.len will contain an
 * index to the corresponding attribute
 */
int extra2attrs(struct acc_extra *extra, struct attr *attrs, int offset)
{
	int i;

	for(i=0 ; extra ; i++, extra=extra->next) {
		attrs[offset+i].n = extra->name.s;
	}
	return i;
}

/**
 *
 */
static int w_acc_radius_request(struct sip_msg *rq, char *comment, char *foo)
{
	return accb.exec(rq, &_acc_radius_engine, (acc_param_t*)comment);
}

