/*
 * Accounting module with Diameter backend
 *
 * Copyright (C) 2001-2003 FhG Fokus
 * Copyright (C) 2006 Voice Sistem SRL
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
 * \ingroup acc_diameter
 * \brief Acc_diameter:: Core module interface
 *
 * - Module: \ref acc_diameter
 */

/*! \defgroup acc_diameter ACC_DIAMETER :: The Kamailio Diameter Accounting Module
 *
 * The ACC_DIAMETER module is used to account transactions information to
 * DIAMETER (beta version).
 *
 */

#include <stdio.h>
#include <string.h>

#include "../../core/sr_module.h"
#include "../../core/dprint.h"
#include "../../core/mem/mem.h"
#include "../../modules/tm/tm_load.h"
#include "../../core/str.h"
#include "../../core/mod_fix.h"
#include "../../modules/acc/acc_api.h"
#include "../../modules/acc/acc_extra.h"

#include "diam_dict.h"
#include "diam_tcp.h"
#include "diam_message.h"

MODULE_VERSION

static int mod_init(void);
static void destroy(void);
static int child_init(int rank);

int acc_diameter_init(acc_init_info_t *inf);
int acc_diameter_send_request(sip_msg_t *req, acc_info_t *inf);

static int w_acc_diameter_request(sip_msg_t *rq, char *comment, char *foo);
static int acc_api_fixup(void** param, int param_no);
static int free_acc_api_fixup(void** param, int param_no);

int init_acc_diameter(acc_extra_t *leg_info, char *rad_cfg, int srv_type);
int extra2int(struct acc_extra *extra, int *attrs);
int extra2strar(struct acc_extra *extra, sip_msg_t *rq, str *val_arr,
		int *int_arr, char *type_arr);

/*! ACC API structure */
acc_api_t accb;
acc_engine_t _acc_diameter_engine;

/* ----- DIAMETER acc variables ----------- */

/*! \name AccDiameterVariables  Radius Variables */
/*@{*/
int diameter_flag = -1;
int diameter_missed_flag = -1;
static char *diameter_extra_str = 0;		/*!< diameter extra variables */
struct acc_extra *diameter_extra = 0;
rd_buf_t *rb;				/*!< buffer used to read from TCP connection*/
char* diameter_client_host="localhost";
int diameter_client_port=3000;

/*@}*/

int sockfd = -1;

static cmd_export_t cmds[] = {
	{"acc_diam_request", (cmd_function)w_acc_diameter_request, 1,
		acc_api_fixup, free_acc_api_fixup,
		ANY_ROUTE},
	{0, 0, 0, 0, 0, 0}
};



static param_export_t params[] = {
	{"diameter_flag",        INT_PARAM, &diameter_flag        },
	{"diameter_missed_flag", INT_PARAM, &diameter_missed_flag },
	{"diameter_client_host", PARAM_STRING, &diameter_client_host },
	{"diameter_client_port", INT_PARAM, &diameter_client_port },
	{"diameter_extra",       PARAM_STRING, &diameter_extra_str },
	{0,0,0}
};


struct module_exports exports= {
	"acc_diameter",
	DEFAULT_DLFLAGS, /* dlopen flags */
	cmds,       /* exported functions */
	params,     /* exported params */
	0,          /* exported RPC methods */
	0,          /* exported pseudo-variables */
	0,          /* response function */
	mod_init,   /* initialization module */
	child_init, /* per-child init function */
	destroy    /* destroy function */
};


/************************** INTERFACE functions ****************************/

static int mod_init( void )
{
	/* bind the ACC API */
	if (acc_load_api(&accb)<0) {
		LM_ERR("cannot bind to ACC API\n");
		return -1;
	}

	/* parse the extra string, if any */
	if (diameter_extra_str
			&& (diameter_extra=accb.parse_extra(diameter_extra_str))==0 ) {
		LM_ERR("failed to parse diameter_extra param\n");
		return -1;
	}

	memset(&_acc_diameter_engine, 0, sizeof(acc_engine_t));

	if(diameter_flag != -1)
		_acc_diameter_engine.acc_flag	   = diameter_flag;
	if(diameter_missed_flag != -1)
		_acc_diameter_engine.missed_flag = diameter_missed_flag;
	_acc_diameter_engine.acc_req     = acc_diameter_send_request;
	_acc_diameter_engine.acc_init    = acc_diameter_init;
	memcpy(_acc_diameter_engine.name, "diameter", 8);
	if(accb.register_engine(&_acc_diameter_engine)<0)
	{
		LM_ERR("cannot register ACC DIAMETER engine\n");
		return -1;
	}

	return 0;
}


static int child_init(int rank)
{
	if (rank==PROC_INIT || rank==PROC_MAIN || rank==PROC_TCP_MAIN)
		return 0; /* do nothing for the main process */

	/* open TCP connection */
	LM_DBG("initializing TCP connection\n");

	sockfd = init_mytcp(diameter_client_host, diameter_client_port);
	if(sockfd==-1)
	{
		LM_ERR("TCP connection not established\n");
		return -1;
	}

	LM_DBG("a TCP connection was established on sockfd=%d\n", sockfd);

	/* every child with its buffer */
	rb = (rd_buf_t*)pkg_malloc(sizeof(rd_buf_t));
	if(!rb)
	{
		PKG_MEM_ERROR;
		return -1;
	}
	rb->buf = 0;

	return 0;
}


static void diam_destroy_extras(acc_extra_t *extra)
{
	acc_extra_t *foo;

	while (extra) {
		foo = extra;
		extra = extra->next;
		pkg_free(foo);
	}
}

static void destroy(void)
{
	close_tcp_connection(sockfd);
	if (diameter_extra)
		diam_destroy_extras(diameter_extra);
}

/************************** FIXUP functions ****************************/

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
			PKG_MEM_ERROR;
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


/************************** DIAMETER ACC ****************************/

#define AA_REQUEST 265
#define AA_ANSWER  265

#define ACCOUNTING_REQUEST 271
#define ACCOUNTING_ANSWER  271

static int diam_attrs[ACC_CORE_LEN+MAX_ACC_EXTRA+MAX_ACC_LEG];

int acc_diam_init(acc_extra_t *leg_info)
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

	m = extra2int( diameter_extra, diam_attrs+n);
	if (m<0) {
		LM_ERR("extra names for DIAMETER must be integer AVP codes\n");
		return -1;
	}
	n += m;

	m = extra2int( leg_info, diam_attrs+n);
	if (m<0) {
		LM_ERR("leg info names for DIAMETER must be integer AVP codes\n");
		return -1;
	}
	n += m;

	return 0;
}


int diam_status(struct sip_msg *rq, int code)
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


int acc_diameter_send_request(sip_msg_t *req, acc_info_t *inf)
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

	attr_cnt =  accb.get_core_attrs(req, inf->varr, inf->iarr, inf->tarr);
	/* last value is not used */
	attr_cnt--;

	if ( (send=AAAInMessage(ACCOUNTING_REQUEST, AAA_APP_NASREQ))==NULL) {
		LM_ERR("failed to create new AAA request\n");
		return -1;
	}

	m = 0;
	o = 0;
	/* AVP_ACCOUNTIG_RECORD_TYPE */
	if( (status = diam_status(req, inf->env->code))<0) {
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
	o = accb.get_extra_attrs(diameter_extra, req, inf->varr+attr_cnt,
			inf->iarr+attr_cnt, inf->tarr+attr_cnt);
	attr_cnt += o;
	m = attr_cnt;

	/* add attributes */
	for(i=0; i<attr_cnt; i++) {
		if((avp=AAACreateAVP(diam_attrs[i], 0,0, inf->varr[i].s, inf->varr[i].len,
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
	if ( inf->leg_info ) {
		cnt = accb.get_leg_attrs(inf->leg_info,req,inf->varr,inf->iarr,inf->tarr,1);
		do {
			for (i=0; i<cnt; i++) {
				if((avp=AAACreateAVP(diam_attrs[attr_cnt+i], 0, 0,
								inf->varr[i].s, inf->varr[i].len, AVP_DUPLICATE_DATA)) == 0) {
					LM_ERR("failed to create AVP: no more free memory!\n");
					goto error;
				}
				if( AAAAddAVPToMessage(send, avp, 0)!= AAA_ERR_SUCCESS) {
					LM_ERR("avp not added \n");
					AAAFreeAVP(&avp);
					goto error;
				}
			}
		} while ( (attr_cnt=accb.get_leg_attrs(inf->leg_info,req,inf->varr,inf->iarr,
						inf->tarr, 0))!=0 );
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
	free_strar_mem( &(inf->tarr[m-o]), &(inf->varr[m-o]), o, m);
	return 1;

error:
	AAAFreeMessage(&send);
	/* free memory allocated by extra2strar */
	free_strar_mem( &(inf->tarr[m-o]), &(inf->varr[m-o]), o, m);
	return -1;
}

int acc_diameter_init(acc_init_info_t *inf)
{
	if(acc_diam_init(inf->leg_info)<0) {
		LM_ERR("failed to init diameter acc\n");
		return -1;
	}
	return 0;
}

/*! \brief converts the name of the extra from str to integer
 * and stores it over str.len ; str.s is freed and made zero
 */
int extra2int(struct acc_extra *extra, int *attrs)
{
	unsigned int ui;
	int i;

	for( i=0 ; extra ; i++,extra=extra->next ) {
		if (str2int( &extra->name, &ui)!=0) {
			LM_ERR("<%s> is not a number\n", extra->name.s);
			return -1;
		}
		attrs[i] = (int)ui;
	}
	return i;
}

int extra2strar(struct acc_extra *extra, sip_msg_t *rq, str *val_arr,
		int *int_arr, char *type_arr)
{
	pv_value_t value;
	int n;
	int i;

	n = 0;
	i = 0;

	while (extra) {
		/* get the value */
		if (pv_get_spec_value( rq, &extra->spec, &value)!=0) {
			LM_ERR("failed to get '%.*s'\n", extra->name.len,extra->name.s);
		}

		/* check for overflow */
		if (n==MAX_ACC_EXTRA) {
			LM_WARN("array to short -> omitting extras for accounting\n");
			goto done;
		}

		if(value.flags&PV_VAL_NULL) {
			/* convert <null> to empty to have consistency */
			val_arr[n].s = 0;
			val_arr[n].len = 0;
			type_arr[n] = TYPE_NULL;
		} else {
			val_arr[n].s = (char *)pkg_malloc(value.rs.len);
			if (val_arr[n].s == NULL ) {
				PKG_MEM_ERROR;
				/* Cleanup already allocated memory and
				 * return that we didn't do anything */
				for (i = 0; i < n ; i++) {
					if (NULL != val_arr[i].s){
						pkg_free(val_arr[i].s);
						val_arr[i].s = NULL;
					}
				}
				n = 0;
				goto done;
			}
			memcpy(val_arr[n].s, value.rs.s, value.rs.len);
			val_arr[n].len = value.rs.len;
			if (value.flags&PV_VAL_INT) {
				int_arr[n] = value.ri;
				type_arr[n] = TYPE_INT;
			} else {
				type_arr[n] = TYPE_STR;
			}
		}
		n++;

		extra = extra->next;
	}

done:
	return n;
}

/**
 *
 */
static int w_acc_diameter_request(sip_msg_t *rq, char *comment, char *foo)
{
	return accb.exec(rq, &_acc_diameter_engine, (acc_param_t*)comment);
}
