/**
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
 */
/*!
 * \file
 * \brief Module interface
 * \ingroup xlog
 * Module: \ref xlog
 */

/**
 * @defgroup xlog xlog :: Kamailio xlog module
 * @brief Kamailio xlog module
 * Extended logging from the configuration script using pv:s.
 * Can log to multiple channels as well as standard out.
 *
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <ctype.h>

#include "../../sr_module.h"
#include "../../dprint.h"
#include "../../error.h"
#include "../../cfg/cfg.h"
#include "../../mem/mem.h"
#include "../../parser/parse_param.h"

#include "xl_lib.h"

#include "../../pvar.h"

#define NOFACILITY -1

MODULE_VERSION

char *_xlog_buf = NULL;
char *_xlog_prefix = "<script>: ";

/** parameters */
static int buf_size=4096;
static int force_color=0;
static int long_format=0;
static int xlog_facility = DEFAULT_FACILITY;
static char *xlog_facility_name = NULL;

/** cfg dynamic parameters */
struct cfg_group_xlog {
	int methods_filter;
};
static struct cfg_group_xlog xlog_default_cfg = {
	-1	/* methods filter */
};
static void *xlog_cfg = &xlog_default_cfg;
static cfg_def_t xlog_cfg_def[] = {
	{"methods_filter",		CFG_VAR_INT | CFG_ATOMIC, 	0, 0, 0, 0,
		"Methods filter value for xlogm(...)."},
	{0, 0, 0, 0, 0, 0}
};

/** module functions */
static int mod_init(void);

static int xlog_1(struct sip_msg*, char*, char*);
static int xlog_2(struct sip_msg*, char*, char*);
static int xlog_3(struct sip_msg*, char*, char*, char*);
static int xdbg(struct sip_msg*, char*, char*);

static int xlogl_1(struct sip_msg*, char*, char*);
static int xlogl_2(struct sip_msg*, char*, char*);
static int xlogl_3(struct sip_msg*, char*, char*, char*);
static int xdbgl(struct sip_msg*, char*, char*);

static int xlogm_2(struct sip_msg*, char*, char*);

static int xlog_fixup(void** param, int param_no);
static int xlog3_fixup(void** param, int param_no);
static int xdbg_fixup(void** param, int param_no);
static int xlogl_fixup(void** param, int param_no);
static int xlogl3_fixup(void** param, int param_no);
static int xdbgl_fixup(void** param, int param_no);

static void destroy(void);

static int xlog_log_colors_param(modparam_t type, void *val);

int pv_parse_color_name(pv_spec_p sp, str *in);
static int pv_get_color(struct sip_msg *msg, pv_param_t *param, 
		pv_value_t *res);

typedef struct _xl_level
{
	int type;
	union {
		long level;
		pv_spec_t sp;
	} v;
} xl_level_t, *xl_level_p;

typedef struct _xl_msg
{
	pv_elem_t *m;
	struct action *a;
} xl_msg_t;

static pv_export_t mod_items[] = {
	{ {"C", sizeof("C")-1}, PVT_OTHER, pv_get_color, 0,
		pv_parse_color_name, 0, 0, 0 },
	{ {0, 0}, 0, 0, 0, 0, 0, 0, 0 }
};


static cmd_export_t cmds[]={
	{"xlog",   (cmd_function)xlog_1,   1, xdbg_fixup,  0, ANY_ROUTE},
	{"xlog",   (cmd_function)xlog_2,   2, xlog_fixup,  0, ANY_ROUTE},
	{"xlog",   (cmd_function)xlog_3,   3, xlog3_fixup, 0, ANY_ROUTE},
	{"xdbg",   (cmd_function)xdbg,     1, xdbg_fixup,  0, ANY_ROUTE},
	{"xlogl",  (cmd_function)xlogl_1,  1, xdbgl_fixup, 0, ANY_ROUTE},
	{"xlogl",  (cmd_function)xlogl_2,  2, xlogl_fixup, 0, ANY_ROUTE},
	{"xlogl",  (cmd_function)xlogl_3,  3, xlogl3_fixup,0, ANY_ROUTE},
	{"xdbgl",  (cmd_function)xdbgl,    1, xdbgl_fixup, 0, ANY_ROUTE},
	{"xlogm",  (cmd_function)xlogm_2,  2, xlog_fixup,  0, ANY_ROUTE},
	{0,0,0,0,0,0}
};


static param_export_t params[]={
	{"buf_size",     INT_PARAM, &buf_size},
	{"force_color",  INT_PARAM, &force_color},
	{"long_format",  INT_PARAM, &long_format},
	{"prefix",       PARAM_STRING, &_xlog_prefix},
	{"log_facility", PARAM_STRING, &xlog_facility_name},
	{"log_colors",   PARAM_STRING|USE_FUNC_PARAM, (void*)xlog_log_colors_param},
	{"methods_filter",  PARAM_INT, &xlog_default_cfg.methods_filter},
	{0,0,0}
};


/** module exports */
struct module_exports exports= {
	"xlog",
	DEFAULT_DLFLAGS, /* dlopen flags */
	cmds,
	params,
	0,          /* exported statistics */
	0  ,        /* exported MI functions */
	mod_items,  /* exported pseudo-variables */
	0,          /* extra processes */
	mod_init,   /* module initialization function */
	0,
	(destroy_function) destroy,
	0           /* per-child init function */
};

/**
 * init module function
 */
static int mod_init(void)
{
	int lf;
	if(cfg_declare("xlog", xlog_cfg_def, &xlog_default_cfg,
				cfg_sizeof(xlog), &xlog_cfg)){
		LM_ERR("Fail to declare the xlog cfg framework structure\n");
		return -1;
	}
	if (xlog_facility_name!=NULL) {
		lf = str2facility(xlog_facility_name);
		if (lf != -1) {
			xlog_facility = lf;
		} else {
			LM_ERR("invalid syslog facility %s\n", xlog_facility_name);
			return -1;
		}
	}

	_xlog_buf = (char*)pkg_malloc((buf_size+1)*sizeof(char));
	if(_xlog_buf==NULL)
	{
		LM_ERR("no pkg memory left\n");
		return -1;
	}
	return 0;
}

static inline int xlog_helper(struct sip_msg* msg, xl_msg_t *xm,
		int level, int line, int facility)
{
	str txt;

	txt.len = buf_size;

	if(xl_print_log(msg, xm->m, _xlog_buf, &txt.len)<0)
		return -1;
	txt.s = _xlog_buf;
	/* if facility is not explicitely defined use the xlog default facility */
	if (facility==NOFACILITY) {
		facility = xlog_facility;
	} 

	if(line>0)
		if(long_format==1)
			LOG_(facility, level, _xlog_prefix,
				"%s:%d:%.*s",
				(xm->a)?(((xm->a->cfile)?xm->a->cfile:"")):"",
				(xm->a)?xm->a->cline:0, txt.len, txt.s);
		else
			LOG_(facility, level, _xlog_prefix,
				"%d:%.*s", (xm->a)?xm->a->cline:0, txt.len, txt.s);
	else
		LOG_(facility, level, _xlog_prefix,
			"%.*s", txt.len, txt.s);
	return 1;
}

/**
 * print log message to L_ERR level
 */
static int xlog_1_helper(struct sip_msg* msg, char* frm, char* str2, int mode, int facility)
{
	if(!is_printable(L_ERR))
		return 1;

	return xlog_helper(msg, (xl_msg_t*)frm, L_ERR, mode, facility);
}
static int xlog_1(struct sip_msg* msg, char* frm, char* str2)
{
	return xlog_1_helper(msg, frm, str2, 0, NOFACILITY);
}

/**
 * print log message to L_ERR level along with cfg line
 */
static int xlogl_1(struct sip_msg* msg, char* frm, char* str2)
{
	return xlog_1_helper(msg, frm, str2, 1, NOFACILITY);
}

static int xlog_2_helper(struct sip_msg* msg, char* lev, char* frm, int mode, int facility)
{
	long level;
	xl_level_p xlp;
	pv_value_t value;

	xlp = (xl_level_p)lev;
	if(xlp->type==1)
	{
		if(pv_get_spec_value(msg, &xlp->v.sp, &value)!=0 
			|| value.flags&PV_VAL_NULL || !(value.flags&PV_VAL_INT))
		{
			LM_ERR("invalid log level value [%d]\n", value.flags);
			return -1;
		}
		level = (long)value.ri;
	} else {
		level = xlp->v.level;
	}

	if(!is_printable((int)level))
		return 1;

	return xlog_helper(msg, (xl_msg_t*)frm, (int)level, mode, facility);
}

/**
 * print log message to level given in parameter
 */
static int xlog_2(struct sip_msg* msg, char* lev, char* frm)
{
	return xlog_2_helper(msg, lev, frm, 0, NOFACILITY);
}

/**
 * print log message to level given in parameter along with cfg line
 */
static int xlogl_2(struct sip_msg* msg, char* lev, char* frm)
{
	return xlog_2_helper(msg, lev, frm, 1, NOFACILITY);
}

/**
 * print log message to level given in parameter applying methods filter
 */
static int xlogm_2(struct sip_msg* msg, char* lev, char* frm)
{
	int mfilter;

	mfilter = cfg_get(xlog, xlog_cfg, methods_filter);

	if(mfilter==-1)
		return 1;

	if(msg->first_line.type==SIP_REQUEST) {
		if (msg->first_line.u.request.method_value & mfilter) {
			return 1;
		}
	} else {
		if (parse_headers(msg, HDR_CSEQ_F, 0) != 0 || msg->cseq==NULL) {
			LM_ERR("cannot parse cseq header\n");
			return -1;
		}
		if (get_cseq(msg)->method_id & mfilter) {
			return 1;
		}
	}

	return xlog_2_helper(msg, lev, frm, 0, NOFACILITY);
}

static int xlog_3_helper(struct sip_msg* msg, char* fac, char* lev, char* frm, int mode)
{
	long level;
	int facility;
	xl_level_p xlp;
	pv_value_t value;

	xlp = (xl_level_p)lev;
	if(xlp->type==1)
	{
		if(pv_get_spec_value(msg, &xlp->v.sp, &value)!=0 
			|| value.flags&PV_VAL_NULL || !(value.flags&PV_VAL_INT))
		{
			LM_ERR("invalid log level value [%d]\n", value.flags);
			return -1;
		}
		level = (long)value.ri;
	} else {
		level = xlp->v.level;
	}
	facility = *(int*)fac;

	if(!is_printable((int)level))
		return 1;

	return xlog_helper(msg, (xl_msg_t*)frm, (int)level, mode, facility);
}

/**
 * print log message to level given in parameter
 * add dedicated logfacility
 */
static int xlog_3(struct sip_msg* msg, char* fac, char* lev, char* frm)
{
	return xlog_3_helper(msg, fac, lev, frm, 0);
}

/**
 * print log message to level given in parameter along with cfg line
 * add dedicated logfacility
 */
static int xlogl_3(struct sip_msg* msg, char* fac, char* lev, char* frm)
{
	return xlog_3_helper(msg, fac, lev, frm, 1);
}

static int xdbg_helper(struct sip_msg* msg, char* frm, char* str2, int mode, int facility)
{
	if(!is_printable(L_DBG))
		return 1;
	return xlog_helper(msg, (xl_msg_t*)frm, L_DBG, mode, facility);
}

/**
 * print log message to L_DBG level
 */
static int xdbg(struct sip_msg* msg, char* frm, char* str2)
{
	return xdbg_helper(msg, frm, str2, 0, NOFACILITY);
}

/**
 * print log message to L_DBG level along with cfg line
 */
static int xdbgl(struct sip_msg* msg, char* frm, char* str2)
{
	return xdbg_helper(msg, frm, str2, 1, NOFACILITY);
}

/**
 * module destroy function
 */
static void destroy(void)
{
	if(_xlog_buf)
		pkg_free(_xlog_buf);
}

static int xdbg_fixup_helper(void** param, int param_no, int mode)
{
	xl_msg_t *xm;
	str s;

	xm = (xl_msg_t*)pkg_malloc(sizeof(xl_msg_t));
	if(xm==NULL)
	{
		LM_ERR("no more pkg\n");
		return -1;
	}
	memset(xm, 0, sizeof(xl_msg_t));
	if(mode==1)
		xm->a = get_action_from_param(param, param_no);
	s.s = (char*)(*param); s.len = strlen(s.s);

	if(pv_parse_format(&s, &xm->m)<0)
	{
		LM_ERR("wrong format[%s]\n", (char*)(*param));
		return E_UNSPEC;
	}
	*param = (void*)xm;
	return 0;
}

static int xlog_fixup_helper(void** param, int param_no, int mode)
{
	xl_level_p xlp;
	str s;
	
	if(param_no==1)
	{
		s.s = (char*)(*param);
		if(s.s==NULL || strlen(s.s)<2)
		{
			LM_ERR("wrong log level\n");
			return E_UNSPEC;
		}

		xlp = (xl_level_p)pkg_malloc(sizeof(xl_level_t));
		if(xlp == NULL)
		{
			LM_ERR("no more memory\n");
			return E_UNSPEC;
		}
		memset(xlp, 0, sizeof(xl_level_t));
		if(s.s[0]==PV_MARKER)
		{
			xlp->type = 1;
			s.len = strlen(s.s);
			if(pv_parse_spec(&s, &xlp->v.sp)==NULL)
			{
				LM_ERR("invalid level param\n");
				return E_UNSPEC;
			}
		} else {
			xlp->type = 0;
			switch(((char*)(*param))[2])
			{
				case 'A': xlp->v.level = L_ALERT; break;
				case 'B': xlp->v.level = L_BUG; break;
				case 'C': xlp->v.level = L_CRIT2; break;
				case 'E': xlp->v.level = L_ERR; break;
				case 'W': xlp->v.level = L_WARN; break;
				case 'N': xlp->v.level = L_NOTICE; break;
				case 'I': xlp->v.level = L_INFO; break;
				case 'D': xlp->v.level = L_DBG; break;
				default:
					LM_ERR("unknown log level\n");
					return E_UNSPEC;
			}
		}
		pkg_free(*param);
		*param = (void*)xlp;
		return 0;
	}

	if(param_no==2)
		return xdbg_fixup_helper(param, 2, mode);

	return 0;
}

/*
 * fixup log facility
 */
static int xlog3_fixup_helper(void** param, int param_no)
{
	int *facility;
	str s;

	s.s = (char*)(*param);
	if(s.s==NULL)
	{
		LM_ERR("wrong log facility\n");
		return E_UNSPEC;
	}
	facility = (int*)pkg_malloc(sizeof(int));
	if(facility == NULL)
	{
		LM_ERR("no more memory\n");
		return E_UNSPEC;
	}
	*facility = str2facility(s.s);
	if (*facility == -1) {
		LM_ERR("invalid syslog facility %s\n", s.s);
		pkg_free(facility);
		return E_UNSPEC;
	}

	pkg_free(*param);
	*param = (void*)facility;
	return 0;
}

static int xlog_fixup(void** param, int param_no)
{
	if(param==NULL || *param==NULL)
	{
		LM_ERR("invalid parameter number %d\n", param_no);
		return E_UNSPEC;
	}
	return xlog_fixup_helper(param, param_no, 0);
}

static int xlog3_fixup(void** param, int param_no)
{
	if(param==NULL || *param==NULL)
	{
		LM_ERR("invalid parameter number %d\n", param_no);
		return E_UNSPEC;
	}
	/* fixup loglevel */
	if (param_no == 2) {
		return xlog_fixup_helper(param, 1, 0);
	}
	/* fixup log message */
	if (param_no == 3) {
		return xdbg_fixup_helper(param, 3, 0);
	}
	/* fixup facility */
	return xlog3_fixup_helper(param, param_no);
}

static int xdbg_fixup(void** param, int param_no)
{
	if(param_no!=1 || param==NULL || *param==NULL)
	{
		LM_ERR("invalid parameter number %d\n", param_no);
		return E_UNSPEC;
	}
	return xdbg_fixup_helper(param, param_no, 0);
}

static int xlogl3_fixup(void** param, int param_no)
{
	if(param==NULL || *param==NULL)
	{
		LM_ERR("invalid parameter number %d\n", param_no);
		return E_UNSPEC;
	}
	/* fixup loglevel */
	if (param_no == 2) {
		return xlog_fixup_helper(param, 1, 1);
	}
	/* fixup log message */
	if (param_no == 3) {
		return xdbg_fixup_helper(param, 3, 1);
	}
	/* fixup facility */
	return xlog3_fixup_helper(param, param_no);
}

static int xlogl_fixup(void** param, int param_no)
{
	if(param==NULL || *param==NULL)
	{
		LM_ERR("invalid parameter number %d\n", param_no);
		return E_UNSPEC;
	}
	return xlog_fixup_helper(param, param_no, 1);
}

static int xdbgl_fixup(void** param, int param_no)
{
	if(param_no!=1 || param==NULL || *param==NULL)
	{
		LM_ERR("invalid parameter number %d\n", param_no);
		return E_UNSPEC;
	}
	return xdbg_fixup_helper(param, param_no, 1);
}

int pv_parse_color_name(pv_spec_p sp, str *in)
{

	if(in==NULL || in->s==NULL || sp==NULL)
		return -1;

	if(in->len != 2)
	{
		LM_ERR("color name must have two chars\n");
		return -1;
	}
	
	/* foreground */
	switch(in->s[0])
	{
		case 'x':
		case 's': case 'r': case 'g':
		case 'y': case 'b': case 'p':
		case 'c': case 'w': case 'S':
		case 'R': case 'G': case 'Y':
		case 'B': case 'P': case 'C':
		case 'W':
		break;
		default: 
			goto error;
	}
                               
	/* background */
	switch(in->s[1])
	{
		case 'x':
		case 's': case 'r': case 'g':
		case 'y': case 'b': case 'p':
		case 'c': case 'w':
		break;   
		default: 
			goto error;
	}
	
	sp->pvp.pvn.type = PV_NAME_INTSTR;
	sp->pvp.pvn.u.isname.type = AVP_NAME_STR;
	sp->pvp.pvn.u.isname.name.s = *in;

	sp->getf = pv_get_color;

	/* force the color PV type */
	sp->type = PVT_COLOR;
	return 0;
error:
	LM_ERR("invalid color name\n");
	return -1;
}

static int pv_get_color(struct sip_msg *msg, pv_param_t *param,
		pv_value_t *res)
{
	str s = {"", 0};

	if(log_stderr==0 && force_color==0)
	{
		LM_DBG("ignoring colors\n");
		return pv_get_strval(msg, param, res, &s);
	}

	dprint_term_color(param->pvn.u.isname.name.s.s[0],
			param->pvn.u.isname.name.s.s[1], &s);
	return pv_get_strval(msg, param, res, &s);
}

/**
 *
 */
static int xlog_log_colors_param(modparam_t type, void *val)
{
	param_t* params_list = NULL;
	param_hooks_t phooks;
	param_t *pit=NULL;
	str s;
	int level;

	if(val==NULL)
		goto error;

	s.s = (char*)val;
	s.len = strlen(s.s);

	if(s.len<=0)
		goto error;

	if(s.s[s.len-1]==';')
		s.len--;
	if (parse_params(&s, CLASS_ANY, &phooks, &params_list)<0)
		goto error;

	for (pit = params_list; pit; pit=pit->next)
	{
		if (pit->name.len==7
				&& strncasecmp(pit->name.s, "l_alert", 7)==0) {
			level = L_ALERT;
		} else if (pit->name.len==5
				&& strncasecmp(pit->name.s, "l_bug", 5)==0) {
			level = L_BUG;
		} else if (pit->name.len==7
				&& strncasecmp(pit->name.s, "l_crit2", 7)==0) {
			level = L_CRIT2;
		} else if (pit->name.len==6
				&& strncasecmp(pit->name.s, "l_crit", 6)==0) {
			level = L_CRIT;
		} else if (pit->name.len==5
				&& strncasecmp(pit->name.s, "l_err", 5)==0) {
			level = L_ERR;
		} else if (pit->name.len==6
				&& strncasecmp(pit->name.s, "l_warn", 6)==0) {
			level = L_WARN;
		} else if (pit->name.len==8
				&& strncasecmp(pit->name.s, "l_notice", 8)==0) {
			level = L_NOTICE;
		} else if (pit->name.len==6
				&& strncasecmp(pit->name.s, "l_info", 6)==0) {
			level = L_INFO;
		} else if (pit->name.len==5
				&& strncasecmp(pit->name.s, "l_dbg", 5)==0) {
			level = L_DBG;
		} else {
			LM_ERR("invalid level name %.*s\n",
					pit->name.len, pit->name.s);
			goto error;
		}
			
		if(pit->body.len!=2) {
			LM_ERR("invalid color spec for level %.*s (%.*s)\n",
					pit->name.len, pit->name.s,
					pit->body.len, pit->body.s);
			goto error;
		}
		dprint_color_update(level, pit->body.s[0], pit->body.s[1]);
	}

	if(params_list!=NULL)
		free_params(params_list);
	return 0;

error:
	if(params_list!=NULL)
		free_params(params_list);
	return -1;

}
