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
#include <signal.h>

#include "../../mem/shm_mem.h"
#include "../../mem/mem.h"
#include "../../sr_module.h"
#include "../../str.h"
#include "../../dprint.h"
#include "../../data_lump_rpl.h"
#include "../../fifo_server.h"
#include "../../parser/parse_uri.h"
#include "../../parser/parse_from.h"
#include "../../parser/parse_content.h"
#include "../../parser/parse_disposition.h"
#include "../../db/db.h"
#include "../tm/tm_load.h"
#include "cpl_run.h"
#include "cpl_db.h"
#include "cpl_loader.h"
#include "cpl_parser.h"
#include "cpl_nonsig.h"


#define MAX_PROXY_RECURSE  10


static char *DB_URL      = 0;  /* database url */
static char *DB_TABLE    = 0;  /* */
static pid_t aux_process = 0;  /* pid of the private aux. process */
static char *dtd_file    = 0;  /* name of the DTD file for CPL parser */


int    proxy_recurse     = 0;
char   *log_dir          = 0; /*directory where the user log should be dumped*/
int    cpl_cmd_pipe[2];
struct tm_binds cpl_tmb;

str    cpl_orig_tz = {0,0}; /* a copy of the original TZ; keept as a null
                             * terminated string in "TZ=value" format;
                             * used only by run_time_switch */

/* this vars are used outside only for loading scripts */
db_con_t* db_hdl   = 0;   /* this should be static !!!!*/

int (*sl_reply)(struct sip_msg* _m, char* _s1, char* _s2);


MODULE_VERSION


static int cpl_invoke_script(struct sip_msg* msg, char* str, char* str2);
static int cpl_process_register(struct sip_msg* msg, char* str, char* str2);
static int fixup_cpl_run_script(void** param, int param_no);
static int cpl_init(void);
static int cpl_child_init(int rank);
static int cpl_exit(void);


/*
 * Exported functions
 */
static cmd_export_t cmds[] = {
	{"cpl_run_script", cpl_invoke_script,1,fixup_cpl_run_script,REQUEST_ROUTE},
	{"cpl_process_register", cpl_process_register, 0, 0, REQUEST_ROUTE},
	{0, 0, 0, 0, 0}
};


/*
 * Exported parameters
 */
static param_export_t params[] = {
	{"cpl_db",        STR_PARAM, &DB_URL        },
	{"cpl_table",     STR_PARAM, &DB_TABLE      },
	{"cpl_dtd_file",  STR_PARAM, &dtd_file      },
	{"proxy_recurse", INT_PARAM, &proxy_recurse },
	{"log_dir",       STR_PARAM, &log_dir       },
	{0, 0, 0}
};


struct module_exports exports = {
	"cpl-c",
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
	int flag;

	if (param_no==1) {
		if (!strcasecmp( "incoming", *param))
			flag = CPL_RUN_INCOMING;
		else if (!strcasecmp( "outgoing", *param))
			flag = CPL_RUN_OUTGOING;
		else {
			LOG(L_ERR,"ERROR:fixup_cpl_run_script: script directive \"%s\""
				" unknown!\n",(char*)*param);
			return E_UNSPEC;
		}
		pkg_free(*param);
		*param=(void*)flag;
		return 0;
	}
	return 0;
}



static int cpl_init(void)
{
	load_tm_f  load_tm;
	struct stat stat_t;
	char *ptr;
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

	if (proxy_recurse>MAX_PROXY_RECURSE) {
		LOG(L_CRIT,"ERROR:cpl_init: value of proxy_recurse param ()%d exceeds "
			"the maximum safty value (%d)\n",proxy_recurse,MAX_PROXY_RECURSE);
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
		if ( strlen(log_dir)>MAX_LOG_DIR_SIZE ) {
			LOG(L_ERR,"ERROR:cpl_init: dir \"%s\" has a too long name :-(!\n",
				log_dir);
			goto error;
		}
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

	/* import the TM auto-loading function */
	if ( !(load_tm=(load_tm_f)find_export("load_tm", NO_SCRIPT, 0))) {
		LOG(L_ERR, "ERROR:cpl_c:cpl_init: cannot import load_tm\n");
		goto error;
	}
	/* let the auto-loading function load all TM stuff */
	if (load_tm( &cpl_tmb )==-1)
		goto error;

	/* load the send_reply function from sl module */
	if ((sl_reply=find_export("sl_send_reply", 2, 0))==0) {
		LOG(L_ERR, "ERROR:cpl_c:cpl_init: cannot import sl_send_reply; maybe "
			"you forgot to load the sl module\n");
		goto error;
	}

	/* register the fifo commands */
	if (register_fifo_cmd( cpl_load, "LOAD_CPL", 0)!=1) {
		LOG(L_CRIT,"ERROR:cpl_init: cannot register LOAD_CPL fifo cmd!\n");
		goto error;
	}
	if (register_fifo_cmd( cpl_remove, "REMOVE_CPL", 0)!=1) {
		LOG(L_CRIT,"ERROR:cpl_init: cannot register REMOVE_CPL fifo cmd!\n");
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

	/* init the CPL parser */
	if (init_CPL_parser( dtd_file )!=1 ) {
		LOG(L_ERR,"ERROR:cpl_init: init_CPL_parser failed!\n");
		goto error;
	}

	/* make a copy of the original TZ env. variable */
	ptr = getenv("TZ");
	cpl_orig_tz.len = 3/*"TZ="*/ + (ptr?(strlen(ptr)+1):0);
	if ( (cpl_orig_tz.s=shm_malloc(cpl_orig_tz.len))==0 ) {
		LOG(L_ERR,"ERROR:cpl_init: no more shm mem. for saving TZ!\n");
		goto error;
	}
	memcpy(cpl_orig_tz.s,"TZ=",3);
	if (ptr)
		strcpy(cpl_orig_tz.s+3,ptr);

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
		} else {
			LOG(L_INFO,"INFO:cpl_child_init(%d): I just gave birth to a child!"
				" I'm a PARENT!!\n",rank);
			/* I'm the parent -> remember the pid */
			aux_process = pid;
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
	/* free the TZ orig */
	if (cpl_orig_tz.s)
		shm_free(cpl_orig_tz.s);

	/* if still runnigng, stop the aux process */
	if (!aux_process) {
		LOG(L_INFO,"INFO:cpl_c:cpl_exit: aux process hasn't been created -> "
			"nothing to kill :-(\n");
	} else {
		/* kill the auxiliary process */
		if (kill( aux_process, SIGKILL)!=0) {
			if (errno==ESRCH) {
				LOG(L_INFO,"INFO:cpl_c:cpl_exit: seems that my child is "
					"already dead! :-((\n");
			} else {
				LOG(L_ERR,"ERROR:cpl_c:cpl_exit: killing the aux. process "
					"failed! kill said: %s\n",strerror(errno));
				return -1;
			}
		} else {
			LOG(L_INFO,"INFO:cl_c:cpl_exit: I have blood on my hands!! I just"
				" killed my own child!");
		}
	}
	return 0;
}



static inline int get_dest_user(struct sip_msg *msg, str *user)
{
	struct sip_uri uri;

	/*  get the user_name from new_uri/RURI/To */
	DBG("DEBUG:cpl-c:get_dest_user: tring to get user from new_uri\n");
	if ( !msg->new_uri.s || parse_uri( msg->new_uri.s,msg->new_uri.len,&uri)==-1
	|| !uri.user.len )
	{
		DBG("DEBUG:cpl-c:get_dest_user: tring to get user from R_uri\n");
		if ( parse_uri( msg->first_line.u.request.uri.s,
		msg->first_line.u.request.uri.len ,&uri)==-1 || !uri.user.len )
		{
			DBG("DEBUG:cpl-c:get_dest_user: tring to get user from To\n");
			if ( ((!msg->to&&parse_headers(msg,HDR_TO,0)==-1) || !msg->to ) ||
			parse_uri( get_to(msg)->uri.s, get_to(msg)->uri.len, &uri)==-1
			|| !uri.user.len)
			{
				LOG(L_ERR,"ERROR:cpl-c:get_dest_user: unable to extract user"
					" name from RURI or To header!\n");
				return -1;
			}
		}
	}
	*user = uri.user;
	return 0;
}



static inline int get_orig_user(struct sip_msg *msg, str *user)
{
	struct to_body *from;
	struct sip_uri uri;
	
	/* if it's outgoing -> get the user_name from From */
	/* parsing from header */
	DBG("DEBUG:cpl-c:get_orig_user: tring to get user from From\n");
	if ( parse_from_header( msg )==-1 ) {
		LOG(L_ERR,"ERROR:cpl-c:get_orig_user: unable to extract URI "
			"from FROM header\n");
		return -1;
	}
	from = (struct to_body*)msg->from->parsed;
	/* parse the extracted uri from From */
	if (parse_uri( from->uri.s, from->uri.len, &uri)||!uri.user.len) {
		LOG(L_ERR,"ERROR:cpl-c:get_orig_user: unable to extract user name "
			"from URI (From header)\n");
		return -1;
	}
	*user = uri.user;
	return 0;
}



/* Params: str1 - as unsigned int - can be CPL_RUN_INCOMING
 * or CPL_RUN_OUTGOING */
static int cpl_invoke_script(struct sip_msg* msg, char* str1, char* str2)
{
	struct cpl_interpreter  *cpl_intr;
	str  user;
	str  script;

	script.s = 0;
	cpl_intr = 0;

	/* get the user_name */
	if ( ((unsigned int)str1)&CPL_RUN_INCOMING ) {
		/* if it's incoming -> get the destination user name */
		if (get_dest_user( msg, &user)==-1)
			goto error;
	} else {
		/* if it's outgoing -> get the origin user name */
		if (get_orig_user( msg, &user)==-1)
			goto error;
	}

	/* get the script for this user */
	if (get_user_script( db_hdl, &user, &script, "cpl_bin")==-1)
		goto error;

	/* has the user a non-empty script? if not, return normaly, allowing ser to
	 * continue its script */
	if ( !script.s || !script.len )
		return 1;

	/* build a new script interpreter */
	if ( (cpl_intr=new_cpl_interpreter(msg,&script))==0 )
		goto error;
	/* set the flags */
	cpl_intr->flags = (unsigned int)str1;
	/* attache the user */
	cpl_intr->user = user;

	/* since the script interpretation can take some time, it will be better to
	 * send a 100 back to prevent the UAC to retransmit */
	if ( cpl_tmb.t_reply( msg, (int)100, "Running cpl script" )!=1 ) {
		LOG(L_ERR,"ERROR:cpl_invoke_script: unable to send 100 reply!\n");
		goto error;
	}

	/* run the script */
	switch (cpl_run_script( cpl_intr )) {
		case SCRIPT_DEFAULT:
			free_cpl_interpreter( cpl_intr );
			return 1; /* execution of ser's script will continue */
		case SCRIPT_END:
			free_cpl_interpreter( cpl_intr );
		case SCRIPT_TO_BE_CONTINUED:
			return 0; /* break the SER script */
		case SCRIPT_RUN_ERROR:
		case SCRIPT_FORMAT_ERROR:
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


#define CPL_SCRIPT          "script"
#define CPL_SCRIPT_LEN      (sizeof(CPL_SCRIPT)-1)
#define ACTION_PARAM        "action"
#define ACTION_PARAM_LEN    (sizeof(ACTION_PARAM)-1)
#define STORE_ACTION        "store"
#define STORE_ACTION_LEN    (sizeof(STORE_ACTION)-1)
#define REMOVE_ACTION       "remove"
#define REMOVE_ACTION_LEN   (sizeof(REMOVE_ACTION)-1)

#define REMOVE_SCRIPT       0xcaca
#define STORE_SCRIPT        0xbebe

#define CONTENT_TYPE_HDR      ("Content-Type: application/cpl-xml"CRLF)
#define CONTENT_TYPE_HDR_LEN  (sizeof(CONTENT_TYPE_HDR)-1)

struct cpl_error {
	int   err_code;
	char *err_msg;
};

static struct cpl_error bad_req = {400,"Bad request"};
static struct cpl_error intern_err = {500,"Internal server error"};
static struct cpl_error bad_cpl = {400,"Bad CPL script"};

static struct cpl_error *cpl_err = &bad_req;


static inline int do_script_action(struct sip_msg *msg, int action)
{
	str  body = {0,0};
	str  user = {0,0};
	str  bin  = {0,0};
	str  log  = {0,0};
	char foo;

	/* content-length (if present) */
	if ( (!msg->content_length && parse_headers(msg,HDR_CONTENTLENGTH,0)==-1)
	|| !msg->content_length) {
		LOG(L_ERR,"ERROR:cpl-c:do_script_action: no Content-Length "
			"hdr found!\n");
		goto error;
	}
	body.len = get_content_length( msg );

	/* get the user name */
	if (get_dest_user( msg, &user)==-1)
		goto error;
	/* make the user zero terminated -  it's safe - it'a an incomming request,
	 * so I'm the only process working with it (no sync problems) and user
	 * points inside the message and all the time it's something after it (no
	 * buffer overflow)*/
	foo = user.s[user.len];
	user.s[user.len] = 0;

	/* we have the script and the user */
	switch (action) {
		case STORE_SCRIPT :
			/* check the len -> it must not be 0 */
			if (body.len==0) {
				LOG(L_ERR,"ERROR:cpl-c:do_script_action: 0 content-len found "
					"for store\n");
				goto error_1;
			}
			/* get the message's body */
			body.s = get_body( msg );
			if (body.s==0) {
				LOG(L_ERR,"ERROR:cpl-c:do_script_action: cannot extract "
					"body from msg!\n");
				goto error_1;
			}
			/* now compile the script and place it into database */
			/* get the binary coding for the XML file */
			if ( encodeCPL( &body, &bin, &log)!=1) {
				cpl_err = &bad_cpl;
				goto error_1;
			}

			/* write both the XML and binary formats into database */
			if (write_to_db( db_hdl, user.s, &body, &bin)!=1) {
				cpl_err = &intern_err;
				goto error_1;
			}
			break;
		case REMOVE_SCRIPT:
			/* check the len -> it must be 0 */
			if (body.len!=0) {
				LOG(L_ERR,"ERROR:cpl-c:do_script_action: non-0 content-len "
					"found for remove\n");
				goto error_1;
			}
			/* remove the script for the user */
			if (rmv_from_db( db_hdl, user.s)!=1) {
				cpl_err = &intern_err;
				goto error_1;
			}
			break;
	}

	if (log.s) pkg_free( log.s );
	user.s[user.len] = foo;
	return 0;
error_1:
	if (log.s) pkg_free( log.s );
	user.s[user.len] = foo;
error:
	return -1;
}



static inline int do_script_download(struct sip_msg *msg)
{
	struct lump_rpl *ct_type;
	struct lump_rpl *body;
	str  user;
	str script;

	/* get the destination user name */
	if (get_dest_user( msg, &user)==-1)
		goto error;

	/* get the user's xml script from the database */
	if (get_user_script( db_hdl, &user, &script, "cpl_xml")==-1)
		goto error;

	/* add a lump with content-type hdr */
	ct_type = build_lump_rpl( CONTENT_TYPE_HDR, CONTENT_TYPE_HDR_LEN,
		LUMP_RPL_HDR);
	if (ct_type==0) {
		LOG(L_ERR,"ERROR:cpl-c:do_script_download: cannot build hdr lump\n");
		cpl_err = &intern_err;
		goto error;
	}
	add_lump_rpl(  msg, ct_type);

	if (script.len!=0 && script.s!=0) {
		/*DBG("script len=%d\n--------\n%.*s\n--------\n",
			script.len, script.len, script.s);*/
		/* user has a script -> add a body lump */
		body = build_lump_rpl( script.s, script.len, LUMP_RPL_BODY);
		if (body==0) {
			LOG(L_ERR,"ERROR:cpl-c:do_script_download: cannot build "
				"body lump\n");
			cpl_err = &intern_err;
			goto error;
		}
		if (add_lump_rpl( msg, body)==-1) {
			LOG(L_CRIT,"BUG:cpl-c:do_script_download: body lump "
				"already added\n");
			cpl_err = &intern_err;
			goto error;
		}
	}

	return 0;
error:
	return -1;
}



static int cpl_process_register(struct sip_msg* msg, char* str1, char* str2)
{
	struct disposition *disp;
	struct disposition_param *param;
	int  ret;
	int  mime;
	int  *mimes;

	/* make sure that is a REGISTER ??? */

	/* here should be the CONTACT- hack */

	/* is there a CONTENT-TYPE hdr ? */
	mime = parse_content_type_hdr( msg );
	if (mime==-1)
		goto error;

	/* check the mime type */
	DBG("DEBUG:cpl_process_register: Content-Type mime found %u, %u\n",
		mime>>16,mime&0x00ff);
	if ( mime && mime==(TYPE_APPLICATION<<16)+SUBTYPE_CPLXML ) {
		/* can be an upload or remove -> check for the content-purpos and
		 * content-action headers */
		DBG("DEBUG:cpl_process_register: carrying CPL -> look at "
			"Content-Disposition\n");
		if (parse_content_disposition( msg )!=0) {
			LOG(L_ERR,"ERROR:cpl_process_register: Content-Disposition missing "
				"or corruped\n");
			goto error;
		}
		disp = get_content_disposition(msg);
		print_disposition( disp ); /* just for DEBUG */
		/* check if the type of dispostion is SCRIPT */
		if (disp->type.len!=CPL_SCRIPT_LEN ||
		strncasecmp(disp->type.s,CPL_SCRIPT,CPL_SCRIPT_LEN) ) {
			LOG(L_ERR,"ERROR:cpl_process_register: bogus message - Content-Type"
				"says CPL_SCRIPT, but Content-Disposition someting else\n");
			goto error;
		}
		/* disposition type is OK -> look for action parameter */
		for(param=disp->params;param;param=param->next) {
			if (param->name.len==ACTION_PARAM_LEN &&
			!strncasecmp(param->name.s,ACTION_PARAM,ACTION_PARAM_LEN))
				break;
		}
		if (param==0) {
			LOG(L_ERR,"ERROR:cpl_process_register: bogus message - "
				"Content-Disposition has no action param\n");
			goto error;
		}
		/* action param found -> check its value: store or remove */
		if (param->body.len==STORE_ACTION_LEN &&
		!strncasecmp( param->body.s, STORE_ACTION, STORE_ACTION_LEN)) {
			/* it's a store action -> get the script from body message and store
			 * it into database (CPL and BINARY format) */
			if (do_script_action( msg, STORE_SCRIPT)==-1)
				goto error;
		} else
		if (param->body.len==REMOVE_ACTION_LEN &&
		!strncasecmp( param->body.s, REMOVE_ACTION, REMOVE_ACTION_LEN)) {
			/* it's a remove action -> remove the script from database */
			if (do_script_action( msg, REMOVE_SCRIPT)==-1)
				goto error;
		} else {
			LOG(L_ERR,"ERROR:cpl_process_register: unknown action <%.*s>\n",
				param->body.len,param->body.s);
			goto error;
		}
		/* send a 200 OK reply back */
		sl_reply( msg, (char*)200, "OK");
		/* I send the reply and I don't want to resturn to script execution, so
		 * I return 0 to do break */
		goto stop_script;
	}

	/* is there an ACCEPT hdr ? */
	if ( (ret=parse_accept_hdr(msg))==-1)
		goto error;
	if (ret==0 || (mimes=get_accept(msg))==0 )
		/* accept header not present or no mimes found */
		goto resume_script;

	/* looks if the REGISTER accepts cpl-xml or * */
	while (*mimes) {
		DBG("DEBUG: accept mime found %u, %u\n",
			(*mimes)>>16,(*mimes)&0x00ff);
		if (*mimes==(TYPE_ALL<<16)+SUBTYPE_ALL ||
		*mimes==(TYPE_APPLICATION<<16)+SUBTYPE_CPLXML )
			break;
		mimes++;
	}
	if (*mimes==0)
		/* no accept mime that mached cpl */
		goto resume_script;

	/* get the user name from msg, retrive the script from db
	 * and appended to reply */
	if (do_script_download( msg )==-1)
		goto error;

	/* send a 200 OK reply back */
	sl_reply( msg, (char*)200, "OK");

stop_script:
	return 0;
resume_script:
	return 1;
error:
	/* send a error reply back */
	sl_reply( msg, (char*)cpl_err->err_code, cpl_err->err_msg);
	/* I don't want to resturn to script execution, so I return 0 to do break */
	return 0;
}



