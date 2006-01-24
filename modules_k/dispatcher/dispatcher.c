/**
 * $Id$
 *
 * dispatcher module -- stateless load balancing
 *
 * Copyright (C) 2004-2006 FhG Fokus
 *
 * This file is part of openser, a free SIP server.
 *
 * openser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * openser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * History
 * -------
 * 2004-07-31  first version, by daniel
 * 
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>

#include "../../sr_module.h"
#include "../../dprint.h"
#include "../../error.h"
#include "../../ut.h"
#include "../../route.h"
#include "../../mem/mem.h"
#include "../../fifo_server.h"

#include "dispatch.h"

MODULE_VERSION

/** parameters */
char *dslistfile = CFG_DIR"dispatcher.list";
int  ds_force_dst   = 0;
int  ds_flags       = 0; 
int  ds_use_default = 0; 
int  dst_avp_id     = 271;
int  grp_avp_id     = 272;
int  cnt_avp_id     = 273;

/** module functions */
static int mod_init(void);
static int child_init(int);

static int w_ds_select_dst(struct sip_msg*, char*, char*);
static int w_ds_select_domain(struct sip_msg*, char*, char*);
static int w_ds_next_dst(struct sip_msg*, char*, char*);
static int w_ds_next_domain(struct sip_msg*, char*, char*);
static int w_ds_mark_dst0(struct sip_msg*, char*, char*);
static int w_ds_mark_dst1(struct sip_msg*, char*, char*);

void destroy(void);

static int ds_fixup(void** param, int param_no);

static int ds_fifo_set(FILE *stream, char *response_file);
static int ds_fifo_list(FILE *stream, char *response_file);

static cmd_export_t cmds[]={
	{"ds_select_dst",    w_ds_select_dst,    2, ds_fixup, REQUEST_ROUTE},
	{"ds_select_domain", w_ds_select_domain, 2, ds_fixup, REQUEST_ROUTE},
	{"ds_next_dst",      w_ds_next_dst,      0,        0, FAILURE_ROUTE},
	{"ds_next_domain",   w_ds_next_domain,   0,        0, FAILURE_ROUTE},
	{"ds_mark_dst",      w_ds_mark_dst0,     0,        0, FAILURE_ROUTE},
	{"ds_mark_dst",      w_ds_mark_dst1,     1,        0, FAILURE_ROUTE},
	{0,0,0,0,0}
};


static param_export_t params[]={
	{"list_file",      STR_PARAM, &dslistfile},
	{"force_dst",      INT_PARAM, &ds_force_dst},
	{"flags",          INT_PARAM, &ds_flags},
	{"use_default",    INT_PARAM, &ds_use_default},
	{"dst_avp_id",     INT_PARAM, &dst_avp_id},
	{"grp_avp_id",     INT_PARAM, &grp_avp_id},
	{"cnt_avp_id",     INT_PARAM, &cnt_avp_id},
	{0,0,0}
};


/** module exports */
struct module_exports exports= {
	"dispatcher",
	cmds,
	params,
	0,          /* exported statistics */
	mod_init,   /* module initialization function */
	(response_function) 0,
	(destroy_function) destroy,
	child_init  /* per-child init function */
};

/**
 * init module function
 */
static int mod_init(void)
{
	DBG("DISPATCHER: initializing ...\n");

	if(register_fifo_cmd(ds_fifo_set, "ds_set_state", 0)<0)
	{
		LOG(L_ERR,
			"DISPATCHER:mod_init:ERROR: cannot register fifo command!\n");
		return -1;
	}
	
	if(register_fifo_cmd(ds_fifo_list, "ds_list", 0)<0)
	{
		LOG(L_ERR,
			"DISPATCHER:mod_init:ERROR: cannot register fifo command!!\n");
		return -1;
	}
	
	if(ds_load_list(dslistfile)!=0)
	{
		LOG(L_ERR, "DISPATCHER:mod_init:ERROR -- couldn't load list file\n");
		return -1;
	}
	
	return 0;
}

/**
 * Initialize children
 */
static int child_init(int rank)
{
	DBG("DISPATCHER:init_child #%d / pid <%d>\n", rank, getpid());
	return 0;
}

static inline int ds_get_ivalue(struct sip_msg* msg, ds_param_p dp, int *val)
{
	xl_value_t value;
	if(dp->type==0) {
		*val = dp->v.id;
		return 0;
	}
	
	DBG("DISPATCHER:ds_get_ivalue: searching %d %d %d\n", dp->v.sp.type,
			dp->v.sp.p.hindex, dp->v.sp.p.hparam.len);
	if(xl_get_spec_value(msg, &dp->v.sp, &value)!=0 
			|| value.flags&XL_VAL_NULL || !(value.flags&XL_VAL_INT))
	{
		LOG(L_ERR,
			"DISPATCHER:ds_get_ivalue: no AVP found (error in scripts)\n");
		return -1;
	}
	*val = value.ri;
	return 0;
}

/**
 *
 */
static int w_ds_select_dst(struct sip_msg* msg, char* set, char* alg)
{
	int a, s;
	
	if(msg==NULL)
		return -1;
	if(ds_get_ivalue(msg, (ds_param_p)set, &s)!=0)
	{
		LOG(L_ERR,
			"DISPATCHER:w_ds_select_dst: no set value\n");
		return -1;
	}
	if(ds_get_ivalue(msg, (ds_param_p)alg, &a)!=0)
	{
		LOG(L_ERR,
			"DISPATCHER:ds_get_ivalue: no alg value\n");
		return -1;
	}

	return ds_select_dst(msg, s, a, 0 /*set dst uri*/);
}

/**
 *
 */
static int w_ds_select_domain(struct sip_msg* msg, char* set, char* alg)
{
	int a, s;
	if(msg==NULL)
		return -1;

	if(ds_get_ivalue(msg, (ds_param_p)set, &s)!=0)
	{
		LOG(L_ERR,
			"DISPATCHER:w_ds_select_dst: no set value\n");
		return -1;
	}
	if(ds_get_ivalue(msg, (ds_param_p)alg, &a)!=0)
	{
		LOG(L_ERR,
			"DISPATCHER:ds_get_ivalue: no alg value\n");
		return -1;
	}

	return ds_select_dst(msg, s, a, 1/*set host port*/);
}

/**
 *
 */
static int w_ds_next_dst(struct sip_msg *msg, char *str1, char *str2)
{
	return ds_next_dst(msg, 0/*set dst uri*/);
}

/**
 *
 */
static int w_ds_next_domain(struct sip_msg *msg, char *str1, char *str2)
{
	return ds_next_dst(msg, 1/*set host port*/);
}

/**
 *
 */
static int w_ds_mark_dst0(struct sip_msg *msg, char *str1, char *str2)
{
	return ds_mark_dst(msg, 0);
}

/**
 *
 */
static int w_ds_mark_dst1(struct sip_msg *msg, char *str1, char *str2)
{
	if(str1 && (str1[0]=='i' || str1[0]=='I' || str1[0]=='0'))
		return ds_mark_dst(msg, 0);
	else
		return ds_mark_dst(msg, 1);
}

/**
 * destroy function
 */
void destroy(void)
{
	DBG("DISPATCHER: destroy module ...\n");
	ds_destroy_list();
}

/**
 *
 */
static int ds_fixup(void** param, int param_no)
{
	int err;
	ds_param_p dsp;
	
	if(param_no==1 || param_no==2)
	{
		dsp = (ds_param_p)pkg_malloc(sizeof(ds_param_t));
		if(dsp == NULL)
		{
			LOG(L_ERR, "DISPATCHER:ds_fixup: no more memory\n");
			return E_UNSPEC;
		}
		memset(dsp, 0, sizeof(ds_param_t));
		if(((char*)(*param))[0]=='$')
		{
			dsp->type = 1;
			if(xl_parse_spec((char*)*param, &dsp->v.sp,
					XL_THROW_ERROR|XL_DISABLE_MULTI|XL_DISABLE_COLORS)==NULL
				|| dsp->v.sp.type!=XL_AVP)
			{
				LOG(L_ERR,
					"DISPATCHER:ds_fixup: Unsupported User Field identifier\n");
				return E_UNSPEC;
			}
		} else {
			dsp->type = 0;
			dsp->v.id = str2s(*param, strlen(*param), &err);
			if (err == 0)
			{
				pkg_free(*param);
			} else {
				LOG(L_ERR, "DISPATCHER:ds_fixup: Bad number <%s>\n",
				    (char*)(*param));
				return E_UNSPEC;
			}
		}
		*param = (void*)dsp;
	}
	return 0;
}

/**
 *
 */
static int ds_fifo_set(FILE *stream, char *response_file)
{
	static char tbuf[256];
	str sp;
	int ret;
	unsigned int group, state;
	
	sp.s = tbuf;
	
	if(!read_line(sp.s, 255, stream, &sp.len) || sp.len==0)	
	{
		LOG(L_ERR, "DISPATCHER:ds_fifo_set: could not read state\n");
		fifo_reply(response_file, "500 ds_fifo_set - state not found\n");
		return 1;
	}

	if(sp.len<=0)
	{
		LOG(L_ERR, "DISPATCHER:ds_fifo_set: bad state value\n");
		fifo_reply(response_file, "500 ds_fifo_set - bad state value\n");
		return 1;
	}

	state = 1;
	if(sp.s[0]=='0' || sp.s[0]=='I' || sp.s[0]=='i')
		state = 0;

	if(!read_line(sp.s, 255, stream, &sp.len) || sp.len==0)	
	{
		LOG(L_ERR, "DISPATCHER:ds_fifo_set: could not read group\n");
		fifo_reply(response_file, "500 ds_fifo_set - group not found\n");
		return 1;
	}

	if(str2int(&sp, &group))
	{
		LOG(L_ERR, "DISPATCHER:ds_fifo_set: bad group value\n");
		fifo_reply(response_file, "500 ds_fifo_set - bad group value\n");
		return 1;
	}

	if(!read_line(sp.s, 255, stream, &sp.len) || sp.len==0)	
	{
		LOG(L_ERR, "DISPATCHER:ds_fifo_set: could not read dst address\n");
		fifo_reply(response_file, "500 ds_fifo_set - address not found\n");
		return 1;
	}

	if(state==1)
		ret = ds_set_state(group, &sp, DS_INACTIVE_DST, 0);
	else
		ret = ds_set_state(group, &sp, DS_INACTIVE_DST, 1);

	if(ret!=0)
	{
		fifo_reply(response_file, "404 ds_fifo_set - destination not found\n");
		return 1;
	}
	
	fifo_reply(response_file, "200 ds_fifo_set - state updated\n");
	return 0;
}

/**
 *
 */
static int ds_fifo_list(FILE *stream, char *response_file)
{
	FILE *freply=NULL;
	
	freply = open_reply_pipe(response_file);
	if(freply == NULL)
	{
		LOG(L_ERR, "DISPATCHER:ds_fifo_list: can't open reply fifo\n");
		return -1;
	}
	
	ds_print_list(freply);

	fclose(freply);
	return 0;
}

