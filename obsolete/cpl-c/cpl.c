/*
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
 *
 * History:
 * -------
 * 2003-03-11: New module interface (janakj)
 * 2003-03-16: flags export parameter added (janakj)
 * 2003-11-11: build_lump_rpl() removed, add_lump_rpl() has flags (bogdan)
 * 2004-06-06  updated to the new DB api (andrei)
 * 2004-06-14: all global variables merged into cpl_env and cpl_fct;
 *             case_sensitive and realm_prefix added for building AORs - see
 *             build_userhost (bogdan)
 * 2004-10-09: added process_register_norpl to allow register processing
 *             without sending the reply(bogdan) - based on a patch sent by
 *             Christopher Crawford
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
#include "../../ut.h"
#include "../../dprint.h"
#include "../../data_lump_rpl.h"
#include "../../usr_avp.h"
#include "../../parser/parse_uri.h"
#include "../../parser/parse_from.h"
#include "../../parser/parse_content.h"
#include "../../parser/parse_disposition.h"
#include "../../lib/srdb2/db.h"
#include "../../cfg/cfg_struct.h"
#include "cpl_run.h"
#include "cpl_env.h"
#include "cpl_db.h"
#include "cpl_loader.h"
#include "cpl_parser.h"
#include "cpl_nonsig.h"
#include "cpl_rpc.h"
#include "loc_set.h"


#define MAX_PROXY_RECURSE  10
#define MAX_USERHOST_LEN    256


/* modules param variables */
static char *DB_URL        = 0;  /* database url */
static char *DB_TABLE      = 0;  /* */
static char *dtd_file      = 0;  /* name of the DTD file for CPL parser */
static char *lookup_domain = 0;
static pid_t aux_process   = 0;  /* pid of the private aux. process */
static char *timer_avp     = 0;  /* name of variable timer AVP */


struct cpl_enviroment    cpl_env = {
		0, /* no cpl logging */
		0, /* recurse proxy level is 0 */
		0, /* no script route to be run before proxy */
		6, /* nat flag */
		0, /* user part is not case sensitive */
		{0,0},   /* no domain prefix to be ignored */
		{-1,-1}, /* communication pipe to aux_process */
		{0,0},   /* original TZ \0 terminated "TZ=value" format */
		0, /* udomain */
		0, /* no branches on lookup */
		0, /* timer avp type */
		/*(int_str)*/{ 0 } /* timer avp name/ID */
};

struct cpl_functions  cpl_fct;


MODULE_VERSION


static int cpl_invoke_script (struct sip_msg* msg, char* str, char* str2);
static int w_process_register(struct sip_msg* msg, char* str, char* str2);
static int w_process_register_norpl(struct sip_msg* msg, char* str,char* str2);
static int cpl_process_register(struct sip_msg* msg, int no_rpl);
static int fixup_cpl_run_script(void** param, int param_no);
static int cpl_init(void);
static int cpl_child_init(int rank);
static int cpl_exit(void);


/*
 * Exported functions
 */
static cmd_export_t cmds[] = {
	{"cpl_run_script",cpl_invoke_script,2,fixup_cpl_run_script,REQUEST_ROUTE},
	{"cpl_process_register",w_process_register,0,0,REQUEST_ROUTE},
	{"cpl_process_register_norpl",w_process_register_norpl,0,0,REQUEST_ROUTE},
	{0, 0, 0, 0, 0}
};


/*
 * Exported parameters
 */
static param_export_t params[] = {
	{"cpl_db",         PARAM_STRING, &DB_URL      },
	{"cpl_table",      PARAM_STRING, &DB_TABLE    },
	{"cpl_dtd_file",   PARAM_STRING, &dtd_file    },
	{"proxy_recurse",  PARAM_INT,    &cpl_env.proxy_recurse  },
	{"proxy_route",    PARAM_INT,    &cpl_env.proxy_route    },
	{"nat_flag",       PARAM_INT,    &cpl_env.nat_flag       },
	{"log_dir",        PARAM_STRING, &cpl_env.log_dir        },
	{"case_sensitive", PARAM_INT,    &cpl_env.case_sensitive },
	{"realm_prefix",   PARAM_STR,    &cpl_env.realm_prefix   },
	{"lookup_domain",  PARAM_STRING, &lookup_domain          },
	{"lookup_append_branches", PARAM_INT, &cpl_env.lu_append_branches},
	{"timer_avp",      PARAM_STRING, &timer_avp   },
	{0, 0, 0}
};


struct module_exports exports = {
	"cpl-c",
	cmds,             /* Exported functions */
	cpl_rpc_methods,  /* RPC methods */
	params,           /* Exported parameters */
	cpl_init,         /* Module initialization function */
	(response_function) 0,
	(destroy_function) cpl_exit,
	0,
	(child_init_function) cpl_child_init /* per-child init function */
};



static int fixup_cpl_run_script(void** param, int param_no)
{
	long flag;

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
	} else if (param_no==2) {
		if ( !strcasecmp("is_stateless", *param) ) {
			flag = 0;
		} else if ( !strcasecmp("is_stateful", *param) ) {
			flag = CPL_IS_STATEFUL;
		} else if ( !strcasecmp("force_stateful", *param) ) {
			flag = CPL_FORCE_STATEFUL;
		} else {
			LOG(L_ERR,"ERROR:fixup_cpl_run_script: flag \"%s\" (second param)"
				" unknown!\n",(char*)*param);
			return E_UNSPEC;
		}
		pkg_free(*param);
		*param=(void*)flag;
	}
	return 0;
}



static int cpl_init(void)
{
	bind_usrloc_t bind_usrloc;
	load_tm_f     load_tm;
	struct stat   stat_t;
	char *ptr;
	int val;
	str foo;

	LOG(L_INFO,"CPL - initializing\n");

	/* check the module params */
	if (DB_URL==0) {
		LOG(L_CRIT,"ERROR:cpl_init: mandatory parameter \"cpl_db\" "
			"found empty\n");
		goto error;
	}

	if (DB_TABLE==0) {
		LOG(L_CRIT,"ERROR:cpl_init: mandatory parameter \"cpl_table\" "
			"found empty\n");
		goto error;
	}

	if (cpl_env.proxy_recurse>MAX_PROXY_RECURSE) {
		LOG(L_CRIT,"ERROR:cpl_init: value of proxy_recurse param (%d) exceeds "
			"the maximum safety value (%d)\n",
			cpl_env.proxy_recurse,MAX_PROXY_RECURSE);
		goto error;
	}

	/* fix the timer_avp name */
	if (timer_avp) {
		foo.s = timer_avp;
		foo.len = strlen(foo.s);
		if (parse_avp_spec(&foo,&cpl_env.timer_avp_type,&cpl_env.timer_avp,0)<0){
			LOG(L_CRIT,"ERROR:cpl_init: invalid timer AVP specs \"%s\"\n",
				timer_avp);
			goto error;
		}
		if (cpl_env.timer_avp_type&AVP_NAME_STR && cpl_env.timer_avp.s.s==foo.s) {
			cpl_env.timer_avp.s = foo;
		}
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

	if (cpl_env.log_dir==0) {
		LOG(L_INFO,"INFO:cpl_init: log_dir param found void -> logging "
			" disabled!\n");
	} else {
		if ( strlen(cpl_env.log_dir)>MAX_LOG_DIR_SIZE ) {
			LOG(L_ERR,"ERROR:cpl_init: dir \"%s\" has a too long name :-(!\n",
				cpl_env.log_dir);
			goto error;
		}
		/* check if the dir exists */
		if (stat( cpl_env.log_dir, &stat_t)==-1) {
			LOG(L_ERR,"ERROR:cpl_init: checking dir \"%s\" status failed;"
				" stat returned %s\n",cpl_env.log_dir,strerror(errno));
			goto error;
		}
		if ( !S_ISDIR( stat_t.st_mode ) ) {
			LOG(L_ERR,"ERROR:cpl_init: dir \"%s\" is not a directory!\n",
				cpl_env.log_dir);
			goto error;
		}
		if (access( cpl_env.log_dir, R_OK|W_OK )==-1) {
			LOG(L_ERR,"ERROR:cpl_init: checking dir \"%s\" for permissions "
				"failed; access returned %s\n",
				cpl_env.log_dir, strerror(errno));
			goto error;
		}
	}

	/* import the TM auto-loading function */
	if ( !(load_tm=(load_tm_f)find_export("load_tm", NO_SCRIPT, 0))) {
		LOG(L_ERR, "ERROR:cpl_c:cpl_init: cannot import load_tm\n");
		goto error;
	}
	/* let the auto-loading function load all TM stuff */
	if (load_tm( &(cpl_fct.tmb) )==-1)
		goto error;

	/* bind the SL API */
	if (sl_load_api(&cpl_fct.slb)!=0) {
		LM_ERR("cannot bind to SL API\n");
		return -1;
	}

	/* bind to usrloc module if requested */
	if (lookup_domain) {
		/* import all usrloc functions */
		bind_usrloc = (bind_usrloc_t)find_export("ul_bind_usrloc", 1, 0);
		if (!bind_usrloc) {
			LOG(L_ERR, "ERROR:cpl_c:cpl_init: Can't bind usrloc\n");
			goto error;
		}
		if (bind_usrloc( &(cpl_fct.ulb) ) < 0) {
			LOG(L_ERR, "ERROR:cpl_c:cpl_init: importing usrloc failed\n");
			goto error;
		}
		/* convert lookup_domain from char* to udomain_t* pointer */
		if (cpl_fct.ulb.register_udomain( lookup_domain, &cpl_env.lu_domain)
		< 0) {
			LOG(L_ERR, "ERROR:cpl_c:cpl_init: Error while registering domain "
				"<%s>\n",lookup_domain);
			goto error;
		}
	} else {
		LOG(L_NOTICE,"NOTICE:cpl_init: no lookup_domain given -> disable "
			" lookup node\n");
	}

	/* build a pipe for sending commands to aux process */
	if ( pipe( cpl_env.cmd_pipe )==-1 ) {
		LOG(L_CRIT,"ERROR:cpl_init: cannot create command pipe: %s!\n",
			strerror(errno) );
		goto error;
	}
	/* set the writing non blocking */
	if ( (val=fcntl(cpl_env.cmd_pipe[1], F_GETFL, 0))<0 ) {
		LOG(L_ERR,"ERROR:cpl_init: getting flags from pipe[1] failed: fcntl "
			"said %s!\n",strerror(errno));
		goto error;
	}
	if ( fcntl(cpl_env.cmd_pipe[1], F_SETFL, val|O_NONBLOCK) ) {
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
	cpl_env.orig_tz.len = 3/*"TZ="*/ + (ptr?(strlen(ptr)+1):0);
	if ( (cpl_env.orig_tz.s=shm_malloc( cpl_env.orig_tz.len ))==0 ) {
		LOG(L_ERR,"ERROR:cpl_init: no more shm mem. for saving TZ!\n");
		goto error;
	}
	memcpy(cpl_env.orig_tz.s,"TZ=",3);
	if (ptr)
		strcpy(cpl_env.orig_tz.s+3,ptr);

	/* convert realm_prefix from string null terminated to str */
	if (cpl_env.realm_prefix.s) {
		/* convert the realm_prefix to lower cases */
		strlower( &cpl_env.realm_prefix );
	}

	/* Register a child process that will keep updating
	 * its local configuration */
	cfg_register_child(1);

	return 0;
error:
	return -1;
}



static int cpl_child_init(int rank)
{
	pid_t pid;

	/* don't do anything for main process and TCP manager process */
	if (rank==PROC_INIT || rank==PROC_MAIN || rank==PROC_TCP_MAIN)
		return 0;

	/* only child 1 will fork the aux process */
	if (rank==1) {
		pid = fork();
		if (pid==-1) {
			LOG(L_CRIT,"ERROR:cpl_child_init(%d): cannot fork: %s!\n",
				rank, strerror(errno));
			goto error;
		} else if (pid==0) {
			/* I'm the child */

			/* initialize the config framework */
			if (cfg_child_init()) goto error;

			cpl_aux_process( cpl_env.cmd_pipe[0], cpl_env.log_dir);
		} else {
			LOG(L_INFO,"INFO:cpl_child_init(%d): I just gave birth to a child!"
				" I'm a PARENT!!\n",rank);
			/* I'm the parent -> remember the pid */
			aux_process = pid;
		}
	}

	return cpl_db_init(DB_URL, DB_TABLE);
error:
	return -1;
}



static int cpl_exit(void)
{
	/* free the TZ orig */
	if (cpl_env.orig_tz.s)
		shm_free(cpl_env.orig_tz.s);

	/* if still running, stop the aux process */
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



#define BUILD_UH_SHM      (1<<0)
#define BUILD_UH_ADDSIP   (1<<1)

static inline int build_userhost(struct sip_uri *uri, str *uh, int flg)
{
	static char buf[MAX_USERHOST_LEN];
	unsigned char do_strip;
	char *p;
	int i;

	/* do we need to strip realm prefix? */
	do_strip = 0;
	if (cpl_env.realm_prefix.len && cpl_env.realm_prefix.len<uri->host.len) {
		for( i=cpl_env.realm_prefix.len-1 ; i>=0 ; i-- )
			if ( cpl_env.realm_prefix.s[i]!=((uri->host.s[i])|(0x20)) )
				break;
		if (i==-1)
			do_strip = 1;
	}

	/* calculate the len (without terminating \0) */
	uh->len = 4*((flg&BUILD_UH_ADDSIP)!=0) + uri->user.len + 1 +
		uri->host.len - do_strip*cpl_env.realm_prefix.len;
	if (flg&BUILD_UH_SHM) {
		uh->s = (char*)shm_malloc( uh->len + 1 );
		if (!uh->s) {
			LOG(L_ERR,"ERROR:cpl-c:build_userhost: no more shm memory.\n");
			return -1;
		}
	} else {
		uh->s = buf;
		if ( uh->len > MAX_USERHOST_LEN ) {
			LOG(L_ERR,"ERROR:cpl-c:build_userhost: user+host longer than %d\n",
				MAX_USERHOST_LEN);
			return -1;
		}
	}

	/* build user@host */
	p = uh->s;
	if (flg&BUILD_UH_ADDSIP) {
		memcpy( uh->s, "sip:", 4);
		p += 4;
	}
	/* user part */
	if (cpl_env.case_sensitive) {
		memcpy( p, uri->user.s, uri->user.len);
		p += uri->user.len;
	} else {
		for(i=0;i<uri->user.len;i++)
			*(p++) = (0x20)|(uri->user.s[i]);
	}
	*(p++) = '@';
	/* host part in lower cases */
	for( i=do_strip*cpl_env.realm_prefix.len ; i< uri->host.len ; i++ )
		*(p++) = (0x20)|(uri->host.s[i]);
	*(p++) = 0;

	/* sanity check */
	if (p-uh->s!=uh->len+1) {
		LOG(L_CRIT,"BUG:cpl-c:build_userhost: buffer overflow l=%d,w=%ld\n",
			uh->len,(long)(p-uh->s));
		return -1;
	}
	return 0;
}



static inline int get_dest_user(struct sip_msg *msg, str *uh, int flg)
{
	struct sip_uri uri;

	/*  get the user_name from new_uri/RURI/To */
	DBG("DEBUG:cpl-c:get_dest_user: trying to get user from new_uri\n");
	if ( !msg->new_uri.s || parse_uri( msg->new_uri.s,msg->new_uri.len,&uri)==-1
	|| !uri.user.len )
	{
		DBG("DEBUG:cpl-c:get_dest_user: trying to get user from R_uri\n");
		if ( parse_uri( msg->first_line.u.request.uri.s,
		msg->first_line.u.request.uri.len ,&uri)==-1 || !uri.user.len )
		{
			DBG("DEBUG:cpl-c:get_dest_user: trying to get user from To\n");
			if ( (!msg->to&&( (parse_headers(msg,HDR_TO_F,0)==-1) ||
					!msg->to)) ||
				parse_uri( get_to(msg)->uri.s, get_to(msg)->uri.len, &uri)==-1
				|| !uri.user.len)
			{
				LOG(L_ERR,"ERROR:cpl-c:get_dest_user: unable to extract user"
					" name from RURI or To header!\n");
				return -1;
			}
		}
	}
	return build_userhost( &uri, uh, flg);
}



static inline int get_orig_user(struct sip_msg *msg, str *uh, int flg)
{
	struct to_body *from;
	struct sip_uri uri;

	/* if it's outgoing -> get the user_name from From */
	/* parsing from header */
	DBG("DEBUG:cpl-c:get_orig_user: trying to get user from From\n");
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
	return build_userhost( &uri, uh, flg);
}



/* Params:
 *   str1 - as unsigned int - can be CPL_RUN_INCOMING or CPL_RUN_OUTGOING
 *   str2 - as unsigned int - flags regarding state(less)|(ful)
 */
static int cpl_invoke_script(struct sip_msg* msg, char* str1, char* str2)
{
	struct cpl_interpreter  *cpl_intr;
	str  user;
	str  loc;
	str  script;

	/* get the user_name */
	if ( ((unsigned long)str1)&CPL_RUN_INCOMING ) {
		/* if it's incoming -> get the destination user name */
		if (get_dest_user( msg, &user, BUILD_UH_SHM)==-1)
			goto error0;
	} else {
		/* if it's outgoing -> get the origin user name */
		if (get_orig_user( msg, &user, BUILD_UH_SHM)==-1)
			goto error0;
	}

	/* get the script for this user */
	if (get_user_script(&user, &script, 1)==-1)
		goto error1;

	/* has the user a non-empty script? if not, return normally, allowing ser to
	 * continue its script */
	if ( !script.s || !script.len ) {
		shm_free(user.s);
		return 1;
	}

	/* build a new script interpreter */
	if ( (cpl_intr=new_cpl_interpreter(msg,&script))==0 )
		goto error2;
	/* set the flags */
	cpl_intr->flags =(unsigned int)((unsigned long)str1)|((unsigned long)str2);
	/* attache the user */
	cpl_intr->user = user;
	/* for OUTGOING we need also the destination user for init. with him
	 * the location set */
	if ( ((unsigned long)str1)&CPL_RUN_OUTGOING ) {
		if (get_dest_user( msg, &loc,BUILD_UH_ADDSIP)==-1)
			goto error3;
		if (add_location( &(cpl_intr->loc_set), &loc,10,CPL_LOC_DUPL)==-1)
			goto error3;
	}

	/* since the script interpretation can take some time, it will be better to
	 * send a 100 back to prevent the UAC to retransmit
	if ( cpl_tmb.t_reply( msg, (int)100, "Running cpl script" )!=1 ) {
		LOG(L_ERR,"ERROR:cpl_invoke_script: unable to send 100 reply!\n");
		goto error3;
	}
	* this should be done from script - it's much sooner ;-) */

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
			goto error3;
	}

	return 1;
error3:
	free_cpl_interpreter( cpl_intr );
	return -1;
error2:
	shm_free(script.s);
error1:
	shm_free(user.s);
error0:
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
	str  body = STR_NULL;
	str  user = STR_NULL;
	str  bin  = STR_NULL;
	str  log  = STR_NULL;

	/* content-length (if present) */
	if ( !msg->content_length &&
			((parse_headers(msg, HDR_CONTENTLENGTH_F, 0)==-1)
			 || !msg->content_length) )
	{
		LOG(L_ERR,"ERROR:cpl-c:do_script_action: no Content-Length "
			"hdr found!\n");
		goto error;
	}
	body.len = get_content_length( msg );

	/* get the user name */
	if (get_dest_user( msg, &user, 0)==-1)
		goto error;

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
			if (write_to_db(user.s, &body, &bin)!=1) {
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
			if (rmv_from_db(user.s)!=1) {
				cpl_err = &intern_err;
				goto error_1;
			}
			break;
	}

	if (log.s) pkg_free( log.s );
	return 0;
error_1:
	if (log.s) pkg_free( log.s );
error:
	return -1;
}



static inline int do_script_download(struct sip_msg *msg)
{
	str  user  = STR_NULL;
	str script = STR_NULL;

	/* get the destination user name */
	if (get_dest_user( msg, &user, 0)==-1)
		goto error;

	/* get the user's xml script from the database */
	if (get_user_script(&user, &script, 0)==-1)
		goto error;

	/* add a lump with content-type hdr */
	if (add_lump_rpl( msg, CONTENT_TYPE_HDR, CONTENT_TYPE_HDR_LEN,
	LUMP_RPL_HDR)==0) {
		LOG(L_ERR,"ERROR:cpl-c:do_script_download: cannot build hdr lump\n");
		cpl_err = &intern_err;
		goto error;
	}

	if (script.s!=0) {
		/*DBG("script len=%d\n--------\n%.*s\n--------\n",
			script.len, script.len, script.s);*/
		/* user has a script -> add a body lump */
		if ( add_lump_rpl( msg, script.s, script.len, LUMP_RPL_BODY)==0) {
			LOG(L_ERR,"ERROR:cpl-c:do_script_download: cannot build "
				"body lump\n");
			cpl_err = &intern_err;
			goto error;
		}
		/* build_lump_rpl duplicates the added text, so free the original */
		shm_free( script.s );
	}

	return 0;
error:
	if (script.s)
		shm_free(script.s);
	return -1;
}



static int w_process_register(struct sip_msg* msg, char* str, char* str2)
{
	return cpl_process_register( msg, 0);
}



static int w_process_register_norpl(struct sip_msg* msg, char* str,char* str2)
{
	return cpl_process_register( msg, 1);
}



static int cpl_process_register(struct sip_msg* msg, int no_rpl)
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
		/* can be an upload or remove -> check for the content-purpose and
		 * content-action headers */
		DBG("DEBUG:cpl_process_register: carrying CPL -> look at "
			"Content-Disposition\n");
		if (parse_content_disposition( msg )!=0) {
			LOG(L_ERR,"ERROR:cpl_process_register: Content-Disposition missing "
				"or corrupted\n");
			goto error;
		}
		disp = get_content_disposition(msg);
		print_disposition( disp ); /* just for DEBUG */
		/* check if the type of disposition is SCRIPT */
		if (disp->type.len!=CPL_SCRIPT_LEN ||
		strncasecmp(disp->type.s,CPL_SCRIPT,CPL_SCRIPT_LEN) ) {
			LOG(L_ERR,"ERROR:cpl_process_register: bogus message - Content-Type"
				"says CPL_SCRIPT, but Content-Disposition something else\n");
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

		/* do I have to send to reply? */
		if (no_rpl)
			goto resume_script;

		/* send a 200 OK reply back */
		cpl_fct.slb.zreply( msg, 200, "OK");
		/* I send the reply and I don't want to return to script execution, so
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
		/* no accept mime that matched cpl */
		goto resume_script;

	/* get the user name from msg, retrieve the script from db
	 * and appended to reply */
	if (do_script_download( msg )==-1)
		goto error;

	/* do I have to send to reply? */
	if (no_rpl)
		goto resume_script;

	/* send a 200 OK reply back */
	cpl_fct.slb.zreply( msg, 200, "OK");

stop_script:
	return 0;
resume_script:
	return 1;
error:
	/* send a error reply back */
	cpl_fct.slb.zreply( msg, cpl_err->err_code, cpl_err->err_msg);
	/* I don't want to return to script execution, so I return 0 to do break */
	return 0;
}



