/**
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
#include "../../mem/mem.h"

#include "xl_lib.h"

#include "../../pvar.h"


MODULE_VERSION

char *log_buf = NULL;

/** parameters */
int buf_size=4096;
int force_color=0;

/** module functions */
static int mod_init(void);

static int xlog_1(struct sip_msg*, char*, char*);
static int xlog_2(struct sip_msg*, char*, char*);
static int xdbg(struct sip_msg*, char*, char*);

static int xlog_fixup(void** param, int param_no); 
static int xdbg_fixup(void** param, int param_no); 

void destroy(void);

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

static pv_export_t mod_items[] = {
	{ {"C", sizeof("C")-1}, 101, pv_get_color, 0,
		pv_parse_color_name, 0, 0, 0 },
	{ {0, 0}, 0, 0, 0, 0, 0, 0, 0 }
};


static cmd_export_t cmds[]={
	{"xlog",  (cmd_function)xlog_1,  1, xdbg_fixup, 0, 
		REQUEST_ROUTE | FAILURE_ROUTE |
		ONREPLY_ROUTE | BRANCH_ROUTE | ERROR_ROUTE | LOCAL_ROUTE},
	{"xlog",  (cmd_function)xlog_2,  2, xlog_fixup, 0, 
		REQUEST_ROUTE | FAILURE_ROUTE |
		ONREPLY_ROUTE | BRANCH_ROUTE | ERROR_ROUTE | LOCAL_ROUTE},
	{"xdbg",  (cmd_function)xdbg,    1, xdbg_fixup, 0, 
		REQUEST_ROUTE | FAILURE_ROUTE | 
		ONREPLY_ROUTE | BRANCH_ROUTE | ERROR_ROUTE | LOCAL_ROUTE},
	{0,0,0,0,0,0}
};


static param_export_t params[]={
	{"buf_size",     INT_PARAM, &buf_size},
	{"force_color",  INT_PARAM, &force_color},
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
	(response_function) 0,
	(destroy_function) destroy,
	0           /* per-child init function */
};

/**
 * init module function
 */
static int mod_init(void)
{
	log_buf = (char*)pkg_malloc((buf_size+1)*sizeof(char));
	if(log_buf==NULL)
	{
		LM_ERR("no pkg memory left\n");
		return -1;
	}
	return 0;
}

/**
 */
static int xlog_1(struct sip_msg* msg, char* frm, char* str2)
{
	int log_len;

	if(!is_printable(L_ERR))
		return 1;

	log_len = buf_size;

	if(xl_print_log(msg, (pv_elem_t*)frm, log_buf, &log_len)<0)
		return -1;

	/* log_buf[log_len] = '\0'; */
	LM_GEN1(L_ERR, "%.*s", log_len, log_buf);

	return 1;
}

/**
 */
static int xlog_2(struct sip_msg* msg, char* lev, char* frm)
{
	int log_len;
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

	log_len = buf_size;

	if(xl_print_log(msg, (pv_elem_t*)frm, log_buf, &log_len)<0)
		return -1;

	/* log_buf[log_len] = '\0'; */
	LM_GEN1((int)level, "%.*s", log_len, log_buf);

	return 1;
}

/**
 */
static int xdbg(struct sip_msg* msg, char* frm, char* str2)
{
	int log_len;

	if(!is_printable(L_DBG))
		return 1;

	log_len = buf_size;

	if(xl_print_log(msg, (pv_elem_t*)frm, log_buf, &log_len)<0)
		return -1;

	/* log_buf[log_len] = '\0'; */
	LM_GEN1(L_DBG, "%.*s", log_len, log_buf);

	return 1;
}

/**
 * destroy function
 */
void destroy(void)
{
	if(log_buf)
		pkg_free(log_buf);
}

static int xlog_fixup(void** param, int param_no)
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
				case 'C': xlp->v.level = L_CRIT; break;
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
		return xdbg_fixup(param, 1);

	return 0;
}

static int xdbg_fixup(void** param, int param_no)
{
	pv_elem_t *model;
	str s;

	if(param_no==1)
	{
		if(*param)
		{
			s.s = (char*)(*param); s.len = strlen(s.s);
			if(log_stderr!=0 || (log_stderr==0 && force_color!=0))
			{
				if(pv_parse_format(&s, &model)<0)
				{
					LM_ERR("ERROR: wrong format[%s]\n",
						(char*)(*param));
					return E_UNSPEC;
				}
			} else {
				if(pv_parse_format(&s, &model)<0)
				{
					LM_ERR("ERROR: wrong format[%s]!\n",
						(char*)(*param));
					return E_UNSPEC;
				}
			}
			
			*param = (void*)model;
			return 0;
		}
		else
		{
			LM_ERR("ERROR: null format\n");
			return E_UNSPEC;
		}
	}

	return 0;
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

#define COL_BUF 10

#define append_sstring(p, end, s) \
        do{\
                if ((p)+(sizeof(s)-1)<=(end)){\
                        memcpy((p), s, sizeof(s)-1); \
                        (p)+=sizeof(s)-1; \
                }else{ \
                        /* overflow */ \
                        LM_ERR("append_sstring overflow\n"); \
                        goto error;\
                } \
        } while(0) 


static int pv_get_color(struct sip_msg *msg, pv_param_t *param,
		pv_value_t *res)
{
	static char color[COL_BUF];
	char* p;
	char* end;
	str s;

	if(log_stderr==0 && force_color==0)
	{
		s.s = "";
		s.len = 0;
		return pv_get_strval(msg, param, res, &s);
	}

	p = color;
	end = p + COL_BUF;
        
	/* excape sequenz */
	append_sstring(p, end, "\033[");
        
	if(param->pvn.u.isname.name.s.s[0]!='_')
	{
		if (islower((int)param->pvn.u.isname.name.s.s[0]))
		{
			/* normal font */
			append_sstring(p, end, "0;");
		} else {
			/* bold font */
			append_sstring(p, end, "1;");
			param->pvn.u.isname.name.s.s[0] += 32;
		}
	}
         
	/* foreground */
	switch(param->pvn.u.isname.name.s.s[0])
	{
		case 'x':
			append_sstring(p, end, "39;");
		break;
		case 's':
			append_sstring(p, end, "30;");
		break;
		case 'r':
			append_sstring(p, end, "31;");
		break;
		case 'g':
			append_sstring(p, end, "32;");
		break;
		case 'y':
			append_sstring(p, end, "33;");
		break;
		case 'b':
			append_sstring(p, end, "34;");
		break;
		case 'p':
			append_sstring(p, end, "35;");
		break;
		case 'c':
			append_sstring(p, end, "36;");
		break;
		case 'w':
			append_sstring(p, end, "37;");
		break;
		default:
			LM_ERR("invalid foreground\n");
			return pv_get_null(msg, param, res);
	}
         
	/* background */
	switch(param->pvn.u.isname.name.s.s[1])
	{
		case 'x':
			append_sstring(p, end, "49");
		break;
		case 's':
			append_sstring(p, end, "40");
		break;
		case 'r':
			append_sstring(p, end, "41");
		break;
		case 'g':
			append_sstring(p, end, "42");
		break;
		case 'y':
			append_sstring(p, end, "43");
		break;
		case 'b':
			append_sstring(p, end, "44");
		break;
		case 'p':
			append_sstring(p, end, "45");
		break;
		case 'c':
			append_sstring(p, end, "46");
		break;
		case 'w':
			append_sstring(p, end, "47");
		break;
		default:
			LM_ERR("invalid background\n");
			return pv_get_null(msg, param, res);
	}

	/* end */
	append_sstring(p, end, "m");

	s.s = color;
	s.len = p-color;
	return pv_get_strval(msg, param, res, &s);

error:
	return -1;
}

