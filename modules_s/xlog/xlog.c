/**
 * $Id$
 *
 * Copyright (C) 2001-2003 Fhg Fokus
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
#include "../../mem/mem.h"

#include "xl_lib.h"


MODULE_VERSION


#define MAX_FORMATS 10


/** parameters */

char *formats[MAX_FORMATS] = { 
		"XLOG{0}: [%Tf] method:<%rm> r-uri:<%ru>\n",
		NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL };
int buf_size=4096;

char *log_buf = NULL;
int  log_len = 0;
xl_elog_t *models[MAX_FORMATS] = {
		NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL };

/** module functions */
static int mod_init(void);
static int child_init(int);

static int xlog(struct sip_msg*, char*, char*);
static int xdbg(struct sip_msg*, char*, char*);

void destroy(void);

static cmd_export_t cmds[]={
	{"xlog",  xlog,  2, 0, REQUEST_ROUTE | FAILURE_ROUTE},
	{"xdbg",  xdbg,  1, 0, REQUEST_ROUTE | FAILURE_ROUTE},
	{0,0,0,0,0}
};


static param_export_t params[]={
	{"f0",     STR_PARAM, &formats[0]},
	{"f1",     STR_PARAM, &formats[1]},
	{"f2",     STR_PARAM, &formats[2]},
	{"f3",     STR_PARAM, &formats[3]},
	{"f4",     STR_PARAM, &formats[4]},
	{"f5",     STR_PARAM, &formats[5]},
	{"f6",     STR_PARAM, &formats[6]},
	{"f7",     STR_PARAM, &formats[7]},
	{"f8",     STR_PARAM, &formats[8]},
	{"f9",     STR_PARAM, &formats[9]},
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
	int i;
	DBG("XLOG: initializing ...\n");
	log_buf = (char*)pkg_malloc((buf_size+1)*sizeof(char));
	if(log_buf==NULL)
	{
		LOG(L_ERR, "XLOG:mod_init: ERROR: no more memory\n");
		return -1;
	}
	
	for(i=0; i<MAX_FORMATS; i++)
	{
		if(formats[i])
		{
			if(xl_parse_format(formats[i], &models[i])<0)
				LOG(L_ERR, "XLOG:mod_init: ERROR: wrong format[%d]\n", i);
		}
	}

	return 0;
}

/**
 * Initialize childs
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
	int l=0, f=0;

	if(lev==NULL || strlen(lev)<3)
	{
		LOG(L_ERR, "XLOG:xlog: wrong log level\n");
		return -1;
	}
	switch(lev[2])
	{
		case 'A': l = L_ALERT; break;
        case 'C': l = L_CRIT; break;
        case 'E': l = L_ERR; break;
        case 'W': l = L_WARN; break;
        case 'N': l = L_NOTICE; break;
        case 'I': l = L_INFO; break;
        case 'D': l = L_DBG; break;
		default:
			LOG(L_ERR, "XLOG:xlog: unknown log level\n");
			return -1;
	}
	if(frm && frm[0]>'0' && frm[0]<='9')
		f = frm[0] - '0';

	DBG("XLOG:xlog: format[%d] level[%d] ...\n", f, l);
	log_len = buf_size;

	if(xl_print_log(msg, models[f], log_buf, &log_len)<0)
		return -1;

	log_buf[log_len] = '\0';
	LOG(l, log_buf);

	return 1;
}

/**
 */
static int xdbg(struct sip_msg* msg, char* frm, char* str2)
{
	int f=0;

	if(frm && frm[0]>'0' && frm[0]<='9')
		f = frm[0] - '0';

	DBG("XLOG:xdbg: format[%d]  ...\n", f);
	log_len = buf_size;

	if(xl_print_log(msg, models[f], log_buf, &log_len)<0)
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
	int i;
	DBG("XLOG: destroy module ...\n");
	if(log_buf)
		pkg_free(log_buf);
	for(i=0; i<MAX_FORMATS; i++)
		if(models[i])
			xl_elog_free_all(models[i]);
}

