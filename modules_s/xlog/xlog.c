/**
 * $Id$
 *
 * Copyright (C) 2001-2003 FhG Fokus
 *
 * This file is part of ser, a free SIP server.
 *
 * ser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * For a license to use the ser software under conditions
 * other than those described here, or to purchase support for this
 * software, please contact iptel.org by e-mail at the following addresses:
 *    info@iptel.org
 *
 * ser is distributed in the hope that it will be useful,
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

#include "../../sr_module.h"
#include "../../dprint.h"
#include "../../error.h"
#include "../../mem/mem.h"

#include "xl_lib.h"


MODULE_VERSION

char *log_buf = NULL;

/** parameters */
int buf_size=4096;

/** module functions */
static int mod_init(void);
static int child_init(int);

static int xlog(struct sip_msg*, char*, char*);
static int xdbg(struct sip_msg*, char*, char*);

static int xlog_fixup(void** param, int param_no); 
static int xdbg_fixup(void** param, int param_no); 

void destroy(void);

static cmd_export_t cmds[]={
	{"xlog",  xlog,  2, xlog_fixup, REQUEST_ROUTE | FAILURE_ROUTE |
		 ONREPLY_ROUTE},
	{"xdbg",  xdbg,  1, xdbg_fixup, REQUEST_ROUTE | FAILURE_ROUTE | 
		ONREPLY_ROUTE},
	{0,0,0,0,0}
};


static param_export_t params[]={
	{"buf_size",  INT_PARAM, &buf_size},
	{0,0,0}
};


/** module exports */
struct module_exports exports= {
	"xlog",
	cmds,
	params,
	
	mod_init,   /* module initialization function */
	(response_function) 0,
	(destroy_function) destroy,
	0,
	child_init  /* per-child init function */
};

/**
 * init module function
 */
static int mod_init(void)
{
	DBG("XLOG: initializing ...\n");
	log_buf = (char*)pkg_malloc((buf_size+1)*sizeof(char));
	if(log_buf==NULL)
	{
		LOG(L_ERR, "XLOG:mod_init: ERROR: no more memory\n");
		return -1;
	}

	return 0;
}

/**
 * Initialize children
 */
static int child_init(int rank)
{
	DBG("XLOG: init_child [%d]  pid [%d]\n", rank, getpid());
	return 0;
}

/**
 */
static int xlog(struct sip_msg* msg, char* lev, char* frm)
{
	int log_len;

	log_len = buf_size;

	if(xl_print_log(msg, (xl_elog_t*)frm, log_buf, &log_len)<0)
		return -1;

	log_buf[log_len] = '\0';
	LOG((int)(long)lev, log_buf);

	return 1;
}

/**
 */
static int xdbg(struct sip_msg* msg, char* frm, char* str2)
{
	int log_len;

	log_len = buf_size;

	if(xl_print_log(msg, (xl_elog_t*)frm, log_buf, &log_len)<0)
		return -1;

	log_buf[log_len] = '\0';
	DBG(log_buf);

	return 1;
}

/**
 * destroy function
 */
void destroy(void)
{
	DBG("XLOG: destroy module ...\n");
	if(log_buf)
		pkg_free(log_buf);
}

static int xlog_fixup(void** param, int param_no)
{
	long level;
	
	if(param_no==1)
	{
		if(*param==NULL || strlen((char*)(*param))<3)
		{
			LOG(L_ERR, "XLOG:xlog_fixup: wrong log level\n");
			return E_UNSPEC;
		}
		switch(((char*)(*param))[2])
		{
			case 'A': level = L_ALERT; break;
	        case 'C': level = L_CRIT; break;
    	    case 'E': level = L_ERR; break;
        	case 'W': level = L_WARN; break;
        	case 'N': level = L_NOTICE; break;
        	case 'I': level = L_INFO; break;
	        case 'D': level = L_DBG; break;
			default:
				LOG(L_ERR, "XLOG:xlog_fixup: unknown log level\n");
				return E_UNSPEC;
		}
		pkg_free(*param);
		*param = (void*)level;
		return 0;
	}

	if(param_no==2)
		return xdbg_fixup(param, 1);
	
	return 0;			
}

static int xdbg_fixup(void** param, int param_no)
{
	xl_elog_t *model;

	if(param_no==1)
	{
		if(*param)
		{
			if(xl_parse_format((char*)(*param), &model)<0)
			{
				LOG(L_ERR, "XLOG:xdbg_fixup: ERROR: wrong format[%s]\n",
					(char*)(*param));
				return E_UNSPEC;
			}
			
			*param = (void*)model;
			return 0;
		}
		else
		{
			LOG(L_ERR, "XLOG:xdbg_fixup: ERROR: null format\n");
			return E_UNSPEC;
		}
	}

	return 0;

}
