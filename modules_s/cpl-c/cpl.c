/*
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
 *
 * History:
 * -------
 * 2003-03-11: New module interface (janakj)
 * 2003-03-16: flags export parameter added (janakj)
 */


#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

#include "../../mem/shm_mem.h"
#include "../../mem/mem.h"
#include "../../sr_module.h"
#include "../../str.h"
#include "../../dprint.h"
#include "../../error.h"
#include "../../fifo_server.h"
#include "../../parser/parse_uri.h"
#include "../../parser/parse_from.h"
#include "../../db/db.h"
#include "cpl_run.h"
#include "cpl_db.h"
#include "cpl_loader.h"
#include "cpl_nonsig.h"


static char *DB_URL       = 0;  /* database url */
static char *DB_TABLE     = 0;  /* */
char *dtd_file     = 0;
db_con_t* db_hdl   = 0;   /* this should be static !!!!*/
int  cache_timeout = 5;
cmd_function sl_send_rpl = 0;
static char *log_dir     = 0; /*directory where the user log should be dumped*/
int   cpl_cmd_pipe[2];


MODULE_VERSION



static int cpl_run_script(struct sip_msg* msg, char* str, char* str2);
static int fixup_cpl_run_script(void** param, int param_no);
static int cpl_init(void);
static int cpl_child_init(int rank);
static int cpl_exit(void);


/*
 * Exported functions
 */
static cmd_export_t cmds[] = {
	{"cpl_run_script", cpl_run_script, 1, fixup_cpl_run_script, REQUEST_ROUTE},
	{0, 0, 0, 0, 0}
};


/*
 * Exported parameters
 */
static param_export_t params[] = {
	{"cpl_db",        STR_PARAM, &DB_URL       },
	{"cpl_table",     STR_PARAM, &DB_TABLE     },
	{"cpl_dtd_file",  STR_PARAM, &dtd_file     },
	{"cache_timeout", INT_PARAM, &cache_timeout},
	{"log_dir",       STR_PARAM, &log_dir      },
	{0, 0, 0}
};


struct module_exports exports = {
	"cpl_c",
	cmds,     /* Exported functions */
	params,   /* Exported parameters */
	cpl_init, /* Module initialization function */
	(response_function) 0,
	(destroy_function) cpl_exit,
	0,
	(child_init_function) cpl_child_init /* per-child init function */
};



static int fixup_cpl_run_script(void** param, int param_no)
{
	int type;

	if (param_no==1) {
		if (!strcasecmp( "incoming", *param))
			type = CPL_INCOMING_TYPE;
		else if (!strcasecmp( "outgoing", *param))
			type = CPL_OUTGOING_TYPE;
		else {
			LOG(L_ERR,"ERROR:fixup_cpl_run_script: script directive \"%s\""
				" unknown!\n",(char*)*param);
			return E_UNSPEC;
		}
		pkg_free(*param);
		*param=(void*)type;
		return 0;
	}
	return 0;
}



static int cpl_init(void)
{
	struct stat stat_t;
	int val;

	LOG(L_INFO,"CPL - initializing\n");

	/* check the module params */
	if (DB_URL==0) {
		LOG(L_CRIT,"ERROR:cpl_init: mandatory parameter \"DB_URL\" "
			"found empty\n");
		goto error;
	}
	if (DB_TABLE==0) {
		LOG(L_CRIT,"ERROR:cpl_init: mandatory parameter \"DB_TABLE\" "
			"found empty\n");
		goto error;
	}
	if (dtd_file==0) {
		LOG(L_CRIT,"ERROR:cpl_init: mandatory parameter \"cpl_dtd_file\" "
			"found empty\n");
		goto error;
	} else {
		/* check if the dtd file exists */
		if (stat( dtd_file, &stat_t)==-1) {
			LOG(L_ERR,"ERROR:cpl_init: checking file \"%s\" status failed;"
				" stat returned %s\n",dtd_file,strerror(errno));
			goto error;
		}
		if ( !S_ISREG( stat_t.st_mode ) ) {
			LOG(L_ERR,"ERROR:cpl_init: dir \"%s\" is not a regular file!\n",
				dtd_file);
			goto error;
		}
		if (access( dtd_file, R_OK )==-1) {
			LOG(L_ERR,"ERROR:cpl_init: checking file \"%s\" for permissions "
				"failed; access returned %s\n",dtd_file,strerror(errno));
			goto error;
		}
	}
	if (log_dir==0) {
		LOG(L_WARN,"WARNING:cpl_init: log_dir param found void -> logging "
			" disabled!\n");
	} else {
		/* check if the dir exists */
		if (stat( log_dir, &stat_t)==-1) {
			LOG(L_ERR,"ERROR:cpl_init: checking dir \"%s\" status failed;"
				" stat returned %s\n",log_dir,strerror(errno));
			goto error;
		}
		if ( !S_ISDIR( stat_t.st_mode ) ) {
			LOG(L_ERR,"ERROR:cpl_init: dir \"%s\" is not a directory!\n",
				log_dir);
			goto error;
		}
		if (access( log_dir, R_OK|W_OK )==-1) {
			LOG(L_ERR,"ERROR:cpl_init: checking dir \"%s\" for permissions "
				"failed; access returned %s\n",log_dir,strerror(errno));
			goto error;
		}
	}

	/* bind to the mysql module */
	if (bind_dbmod()) {
		LOG(L_CRIT,"ERROR:cpl_init: cannot bind to database module! "
			"Did you forget to load a database module ?\n");
		goto error;
	}

	/* bind the sl_send_reply function */
	sl_send_rpl = find_export("sl_send_reply", 2, REQUEST_ROUTE);
	if (sl_send_rpl==0) {
		LOG(L_CRIT,"ERROR:cpl_init: cannot find \"sl_send_reply\" function! "
			"Did you forget to load the sl module ?\n");
		goto error;
	}

	/* register the fifo command */
	if (register_fifo_cmd( cpl_loader, "LOAD_CPL", 0)!=1) {
		LOG(L_CRIT,"ERROR:cpl_init: cannot register fifo command!\n");
		goto error;
	}

	/* build a pipe for sending commands to aux proccess */
	if ( pipe(cpl_cmd_pipe)==-1 ) {
		LOG(L_CRIT,"ERROR:cpl_init: cannot create command pipe: %s!\n",
			strerror(errno) );
		goto error;
	}
	/* set the writing non blocking */
	if ( (val=fcntl(cpl_cmd_pipe[1], F_GETFL, 0))<0 ) {
		LOG(L_ERR,"ERROR:cpl_init: getting flags from pipe[1] failed: fcntl "
			"said %s!\n",strerror(errno));
		goto error;
	}
	if ( fcntl(cpl_cmd_pipe[1], F_SETFL, val|O_NONBLOCK) ) {
		LOG(L_ERR,"ERROR:cpl_init: setting flags to pipe[1] failed: fcntl "
			"said %s!\n",strerror(errno));
		goto error;
	}

	return 0;
error:
	return -1;
}



static int cpl_child_init(int rank)
{
	pid_t pid;

	/* don't do anything for main process and TCP manager process */
	if (rank==PROC_MAIN || rank==PROC_TCP_MAIN)
		return 0;

	/* only child 1 will fork the aux proccess */
	if (rank==1) {
		pid = fork();
		if (pid==-1) {
			LOG(L_CRIT,"ERROR:cpl_child_init(%d): cannot fork: %s!\n",
				rank, strerror(errno));
			goto error;
		} else if (pid==0) {
			/* I'm the child */
			cpl_aux_process( cpl_cmd_pipe[0], log_dir);
		}
	}

	if ( (db_hdl=db_init(DB_URL))==0 ) {
		LOG(L_CRIT,"ERROR:cpl_child_init: cannot initialize database "
			"connection\n");
		goto error;
	}
	if (db_use_table( db_hdl, DB_TABLE) < 0) {
		LOG(L_CRIT,"ERROR:cpl_child_init: cannot select table \"%s\"\n",
			DB_TABLE);
		goto error;
	}

	return 0;
error:
	if (db_hdl)
		db_close(db_hdl);
	return -1;
}



static int cpl_exit(void)
{
	return 0;
}



static int cpl_run_script(struct sip_msg* msg, char* str1, char* str2)
{
	struct cpl_interpreter  *cpl_intr;
	struct to_body          *from;
	struct sip_uri          uri;
	str                     script;

	script.s = 0;
	cpl_intr = 0;

	/* get the user_name */
	if ( (unsigned int)str1==CPL_INCOMING_TYPE ) {
		/* if it's incoming -> get the user_name from new_uri/RURI/To */
		DBG("DEBUG:cpl_run_script: tring to get user from new_uri\n");
		if ( !msg->new_uri.s||parse_uri( msg->new_uri.s,msg->new_uri.len,&uri)
		|| !uri.user.len )
		{
			DBG("DEBUG:cpl_run_script: tring to get user from R_uri\n");
			if ( parse_uri( msg->first_line.u.request.uri.s,
			msg->first_line.u.request.uri.len ,&uri)||!uri.user.len )
			{
				DBG("DEBUG:cpl_run_script: tring to get user from To\n");
				if (!msg->to || !get_to(msg) ||
				parse_uri( get_to(msg)->uri.s, get_to(msg)->uri.len, &uri)
				||!uri.user.len)
				{
					LOG(L_ERR,"ERROR:cpl_run_script: unable to extract user"
					" name from RURI or To header!\n");
					goto error;
				}
			}
		}
	} else {
		/* if it's outgoing -> get the user_name from From */
		/* parsing from header */
		if ( parse_from_header( msg )==-1 ) {
			LOG(L_ERR,"ERROR:cpl_run_script: unable to extract URI "
				"from FROM header\n");
			goto error;
		}
		from = (struct to_body*)msg->from->parsed;
		/* parse the extracted uri from From */
		if (parse_uri( from->uri.s, from->uri.len, &uri)||!uri.user.len) {
			LOG(L_ERR,"ERROR:cpl_run_script: unable to extract user name "
				"from URI (From header)\n");
			goto error;
		}
	}

	/* get the script for this user */
	if (get_user_script( db_hdl, &uri.user, &script)==-1)
		goto error;

	/* build a new script interpreter */
	if ( (cpl_intr=build_cpl_interpreter(msg,&script,(unsigned int)str1))==0 )
		goto error;

	/* run the script */
	switch (run_cpl_script( cpl_intr )) {
		case SCRIPT_END:
			free_cpl_interpreter( cpl_intr );
		case SCRIPT_TO_BE_CONTINUED:
			break;
		case -1:
			goto error;
	}

	return 1;
error:
	if (!cpl_intr && script.s)
		shm_free(script.s);
	if (cpl_intr)
		free_cpl_interpreter( cpl_intr );
	return -1;
}



