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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
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

#include "xp_lib.h"

#define NO_SCRIPT -1

MODULE_VERSION

char *log_buf = NULL;

/** parameters */
int buf_size=4096;

/** module functions */
static int mod_init(void);
static int child_init(int);

static int xplog(struct sip_msg*, char*, char*);
static int xpdbg(struct sip_msg*, char*, char*);

static int xplog_fixup(void** param, int param_no);
static int xpdbg_fixup(void** param, int param_no);

static void destroy(void);

static cmd_export_t cmds[]={
	{"xplog",  xplog,  2, xplog_fixup, ANY_ROUTE},
	{"xpdbg",  xpdbg,  1, xpdbg_fixup, ANY_ROUTE},
	{"xbind",	(cmd_function)xl_bind, NO_SCRIPT, 0, 0},
	{"xprint",	(cmd_function)xl_print_log, NO_SCRIPT, 0, 0},
	{"xparse",	(cmd_function)xl_parse_format, NO_SCRIPT, 0, 0},
	{"shm_xparse",	(cmd_function)xl_shm_parse_format, NO_SCRIPT, 0, 0},
	{"xparse2",	(cmd_function)xl_parse_format2, NO_SCRIPT, 0, 0},
	{"shm_xparse2",	(cmd_function)xl_shm_parse_format2, NO_SCRIPT, 0, 0},
	{"xfree",	(cmd_function)xl_elog_free_all, NO_SCRIPT, 0, 0},
	{"shm_xfree",	(cmd_function)xl_elog_shm_free_all, NO_SCRIPT, 0, 0},
	{"xnulstr",	(cmd_function)xl_get_nulstr, NO_SCRIPT, 0, 0},
	{0,0,0,0,0}
};


static param_export_t params[]={
	{"buf_size",  PARAM_INT, &buf_size},
	{0,0,0}
};


/** module exports */
struct module_exports exports= {
	"xprint",
	cmds,
	0,        /* RPC methods */
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
	DBG("initializing ...\n");
	log_buf = (char*)pkg_malloc((buf_size+1)*sizeof(char));
	if(log_buf==NULL)
	{
		LOG(L_ERR, "mod_init: ERROR: no more memory\n");
		return -1;
	}

	return xl_mod_init();
}

/**
 * Initialize children
 */
static int child_init(int rank)
{
	DBG("init_child [%d]  pid [%d]\n", rank, getpid());
	return xl_child_init(rank);
}

/**
 */
static int xplog(struct sip_msg* msg, char* lev, char* frm)
{
	int log_len, level;

	if (get_int_fparam(&level, msg, (fparam_t *)lev)) {
		LOG(L_ERR, "xplog: cannot get log level\n");
		return -1;
	}
	if (level < L_ALERT)
		level = L_ALERT;
	else if (level > L_DBG)
		level = L_DBG;

	log_len = buf_size;

	if(xl_print_log(msg, (xl_elog_t*)frm, log_buf, &log_len)<0)
		return -1;

	/* log_buf[log_len] = '\0'; */
	LOG_(DEFAULT_FACILITY, level, "<script>: ", "%.*s", log_len, log_buf);

	return 1;
}

/**
 */
static int xpdbg(struct sip_msg* msg, char* frm, char* str2)
{
	int log_len;

	log_len = buf_size;

	if(xl_print_log(msg, (xl_elog_t*)frm, log_buf, &log_len)<0)
		return -1;

	/* log_buf[log_len] = '\0'; */
	LOG_(DEFAULT_FACILITY, L_DBG, "<script>: ", "%.*s", log_len, log_buf);

	return 1;
}

/**
 * destroy function
 */
static void destroy(void)
{
	DBG("destroy module ...\n");
	if(log_buf)
		pkg_free(log_buf);
}

static int xplog_fixup(void** param, int param_no)
{
	int level;
	fparam_t	*p;

	if(param_no==1)
	{
		if (*param == NULL) {
			LOG(L_ERR, "xplog_fixup: NULL parameter\n");
			return -1;
		}

		if ((((char*)(*param))[0] == '$')
			|| (((char*)(*param))[0] == '@')
		) {
			/* avp or select parameter */
			return fixup_var_int_1(param, 1);
		}

		if(strlen((char*)(*param))<3)
		{
			LOG(L_ERR, "xplog_fixup: wrong log level\n");
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
			LOG(L_ERR, "xplog_fixup: unknown log level\n");
			return E_UNSPEC;
		}
		/* constant parameter, but the fparam structure
		 * needs to be created */
		p = (fparam_t*)pkg_malloc(sizeof(fparam_t));
		if (!p) {
			LOG(L_ERR, "xplog_fixup: not enough memory\n");
			return -1;
		}
		p->v.i = level;
		p->type = FPARAM_INT;
		p->orig = *param;

		*param = p;
		return 0;
	}

	if(param_no==2)
		return xpdbg_fixup(param, 1);

	return 0;
}

static int xpdbg_fixup(void** param, int param_no)
{
	xl_elog_t *model;

	if(param_no==1)
	{
		if(*param)
		{
			if(xl_parse_format((char*)(*param), &model)<0)
			{
				LOG(L_ERR, "xpdbg_fixup: ERROR: wrong format[%s]\n",
					(char*)(*param));
				return E_UNSPEC;
			}

			*param = (void*)model;
			return 0;
		}
		else
		{
			LOG(L_ERR, "xpdbg_fixup: ERROR: null format\n");
			return E_UNSPEC;
		}
	}

	return 0;

}
