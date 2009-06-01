/*
 * $Id$
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * History:
 * --------
 *  2003-03-11  updated to the new module exports interface (andrei)
 *  2003-03-16  flags export parameter added (janakj)
 *  2003-03-19  all mallocs/frees replaced w/ pkg_malloc/pkg_free
 *  2005-03-01  force for stateless replies the incoming interface of
 *              the request (bogdan)
 *  2006-03-29  callbacks for sending replies added (bogdan)
 */

/*!
 * \file
 * \brief SL :: module definitions
 * \ingroup sl
 * - Module: \ref sl
 */

/*!
 * \defgroup sl SL :: The Kamailio SL Module
 *
 * The SL module allows Kamailio to act as a stateless UA server and
 * generate replies to SIP requests without keeping state. That is beneficial
 * in many scenarios, in which you wish not to burden server's memory and scale
 * well.
 */


#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "../../sr_module.h"
#include "../../dprint.h"
#include "../../error.h"
#include "../../ut.h"
#include "../../script_cb.h"
#include "../../mem/mem.h"
#include "../../pvar.h"

#include "../../modules/tm/tm_load.h"

#include "sl_funcs.h"
#include "sl_api.h"
#include "sl_cb.h"

MODULE_VERSION


static int w_sl_send_reply(struct sip_msg* msg, char* str1, char* str2);
static int w_send_reply(struct sip_msg* msg, char* str1, char* str2);
static int w_sl_reply_error(struct sip_msg* msg, char* str1, char* str2);
static int fixup_sl_send_reply(void** param, int param_no);
static int mod_init(void);
static void mod_destroy(void);
/* module parameter */
int sl_enable_stats = 1;
int sl_bind_tm = 1;

/* statistic variables */
stat_var *tx_1xx_rpls;
stat_var *tx_2xx_rpls;
stat_var *tx_3xx_rpls;
stat_var *tx_4xx_rpls;
stat_var *tx_5xx_rpls;
stat_var *tx_6xx_rpls;
stat_var *sent_rpls;
stat_var *sent_err_rpls;
stat_var *rcv_acks;

static struct tm_binds tmb;

static cmd_export_t cmds[]={
	{"sl_send_reply",   (cmd_function)w_sl_send_reply,
		2,  fixup_sl_send_reply, 0,
		REQUEST_ROUTE | ERROR_ROUTE },
	{"send_reply",   (cmd_function)w_send_reply,
		2,  fixup_sl_send_reply, 0,
		REQUEST_ROUTE|BRANCH_ROUTE|FAILURE_ROUTE|ERROR_ROUTE },
	{"sl_reply_error",  (cmd_function)w_sl_reply_error,
		0,  0, 0, REQUEST_ROUTE},
	{"register_slcb",  (cmd_function)register_slcb,
		0,  0, 0,
		0},
	{"load_sl",        (cmd_function)load_sl,
		0,  0, 0,
		0},
	{0,0,0,0,0,0}
};


static param_export_t mod_params[]={
	{ "enable_stats",  INT_PARAM, &sl_enable_stats },
	{ "bind_tm",       INT_PARAM, &sl_bind_tm },
	{ 0,0,0 }
};


stat_export_t mod_stats[] = {
	{"1xx_replies" ,       0,  &tx_1xx_rpls    },
	{"2xx_replies" ,       0,  &tx_2xx_rpls    },
	{"3xx_replies" ,       0,  &tx_3xx_rpls    },
	{"4xx_replies" ,       0,  &tx_4xx_rpls    },
	{"5xx_replies" ,       0,  &tx_5xx_rpls    },
	{"6xx_replies" ,       0,  &tx_6xx_rpls    },
	{"sent_replies" ,      0,  &sent_rpls      },
	{"sent_err_replies" ,  0,  &sent_err_rpls  },
	{"received_ACKs" ,     0,  &rcv_acks       },
	{0,0,0}
};


struct module_exports exports= {
	"sl",         /* module's name */
	DEFAULT_DLFLAGS, /* dlopen flags */
	cmds,         /* exported functions */
	mod_params,   /* param exports */
	mod_stats,    /* exported statistics */
	0,            /* exported MI functions */
	0,            /* exported pseudo-variables */
	0,            /* extra processes */
	mod_init,     /* module initialization function */
	0,            /* reply processing function */
	mod_destroy,
	0             /* per-child init function */
};


static int mod_init(void)
{
	load_tm_f load_tm;

	/* if statistics are disabled, prevent their registration to core */
	if (sl_enable_stats==0)
		exports.stats = 0;

#ifdef STATISTICS
	/* register statistics */
	if (sl_enable_stats!=0)
	{
		if (register_module_stats( exports.name, mod_stats)!=0 ) {
			LM_ERR("failed to register core statistics\n");
			return -1;
		}
	}
#endif

	/* filter all ACKs before script */
	if (register_script_cb(sl_filter_ACK, PRE_SCRIPT_CB|REQUEST_CB, 0 )!=0) {
		LM_ERR("register_script_cb failed\n");
		return -1;
	}

	/* init internal SL stuff */
	if (sl_startup()!=0) {
		LM_ERR("sl_startup failed\n");
		return -1;
	}

	if(sl_bind_tm!=0)
	{
		if ( (load_tm=(load_tm_f)find_export("load_tm", 0, 0)))
		{
			load_tm( &tmb );
		} else {
			LM_INFO("could not bind tm module - only stateless mode available\n");
			sl_bind_tm=0;
		}
	}

	return 0;
}


static void mod_destroy(void)
{
	sl_shutdown();
	destroy_slcb_lists();

}

/*!
 * \brief Fixup function for sl_send_reply
 */
static int fixup_sl_send_reply(void** param, int param_no)
{
	pv_elem_t *model=NULL;
	str s;

	/* convert to str */
	s.s = (char*)*param;
	s.len = strlen(s.s);

	model=NULL;
	if (param_no==1 || param_no==2)
	{
		if(s.len==0)
		{
			LM_ERR("no param %d!\n", param_no);
			return E_UNSPEC;
		}

		if(pv_parse_format(&s ,&model) || model==NULL)
		{
			LM_ERR("wrong format [%s] for param no %d!\n", s.s, param_no);
			return E_UNSPEC;
		}
		if(model->spec.getf==NULL)
		{
			if(param_no==1)
			{
			   if(str2int(&s,
					(unsigned int*)&model->spec.pvp.pvn.u.isname.name.n)!=0
					   || model->spec.pvp.pvn.u.isname.name.n<100
					   || model->spec.pvp.pvn.u.isname.name.n>699)
			   {
					LM_ERR("wrong value [%s] for param no %d!\n",
						s.s, param_no);
					LM_ERR("allowed values: 1xx - 6xx only!\n");
					return E_UNSPEC;
			   }
			}
		}
		*param = (void*)model;
	}

	return 0;
}


/*!
 * \brief Small wrapper around sl_send_reply
 */
static int w_sl_reply_error( struct sip_msg* msg, char* str1, char* str2)
{
	return sl_reply_error( msg );
}


/*!
 * \brief Wrapper around sl_send_reply
 *
 * Wrapper around sl_send_reply, evaluate pseudo-variables.
 */
static int w_sl_send_reply(struct sip_msg* msg, char* str1, char* str2)
{
	str code_s;
	unsigned int code_i;

	if(((pv_elem_p)str1)->spec.getf!=NULL)
	{
		if(pv_printf_s(msg, (pv_elem_p)str1, &code_s)!=0)
			return -1;
		if(str2int(&code_s, &code_i)!=0 || code_i<100 || code_i>699)
			return -1;
	} else {
		code_i = ((pv_elem_p)str1)->spec.pvp.pvn.u.isname.name.n;
	}
	
	if(((pv_elem_p)str2)->spec.getf!=NULL)
	{
		if(pv_printf_s(msg, (pv_elem_p)str2, &code_s)!=0 || code_s.len <=0)
			return -1;
	} else {
		code_s = ((pv_elem_p)str2)->text;
	}

	return sl_send_reply(msg, code_i, &code_s);
}

int send_reply(struct sip_msg *msg, int code, str *text)
{
	struct cell * t;
	char rbuf[256];
	if(sl_bind_tm!=0)
	{
		t = tmb.t_gett();
		if(t!= NULL && t!=T_UNDEFINED)
		{
			if(text->len>=256)
			{
				LM_ERR("reason phrase too long (tm)\n");
				return -1;
			}
			strncpy(rbuf,  text->s,  text->len);			
			rbuf[text->len] = '\0';
			if(tmb.t_reply(msg, code, rbuf)< 0)
			{
				LM_ERR("failed to reply stateful (tm)\n");
				return -1;
			}
			LM_DBG("reply in stateful mode (tm)\n");
			return 1;
		}
	}

	LM_DBG("reply in stateless mode (sl)\n");
	return sl_send_reply(msg, code, text);
}

static int w_send_reply(struct sip_msg* msg, char* str1, char* str2)
{
	str code_s;
	unsigned int code_i;

	if(((pv_elem_p)str1)->spec.getf!=NULL)
	{
		if(pv_printf_s(msg, (pv_elem_p)str1, &code_s)!=0)
			return -1;
		if(str2int(&code_s, &code_i)!=0 || code_i<100 || code_i>699)
			return -1;
	} else {
		code_i = ((pv_elem_p)str1)->spec.pvp.pvn.u.isname.name.n;
	}
	
	if(((pv_elem_p)str2)->spec.getf!=NULL)
	{
		if(pv_printf_s(msg, (pv_elem_p)str2, &code_s)!=0 || code_s.len <=0)
			return -1;
	} else {
		code_s = ((pv_elem_p)str2)->text;
	}
	return send_reply(msg, code_i, &code_s);
}


int get_reply_totag(struct sip_msg *msg, str *totag)
{
	struct cell * t;
	if(msg==NULL || totag==NULL)
		return -1;
	if(sl_bind_tm!=0)
	{
		t = tmb.t_gett();
		if(t!= NULL && t!=T_UNDEFINED)
		{
			if(tmb.t_get_reply_totag(msg, totag)< 0)
			{
				LM_ERR("failed to get totag (tm)\n");
				return -1;
			}
			LM_DBG("totag stateful mode (tm)\n");
			return 1;
		}
	}

	LM_DBG("totag stateless mode (sl)\n");
	return sl_get_reply_totag(msg, totag);
}

/*!
 * \brief Helper function for loading the SL API
 * \param slb sl_bind structure
 * \return -1 on parameter errors, 1 otherwise
 */
int load_sl( struct sl_binds *slb)
{
	if(slb==NULL)
		return -1;

	slb->reply      = sl_send_reply;
	slb->reply_dlg  = sl_send_reply_dlg;
	slb->sl_get_reply_totag = sl_get_reply_totag;
	slb->send_reply = send_reply;
	slb->get_reply_totag = get_reply_totag;


	return 1;
}
