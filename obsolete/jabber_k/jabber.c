/*
 * $Id$
 *
 * XJAB module
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * ---
 *
 * History
 * -------
 * 2003-02-28 connection management with ihttp implemented (dcm)
 * 2003-02-24 first version of callback functions for ihttp (dcm)
 * 2003-02-13 lot of comments enclosed in #ifdef XJ_EXTRA_DEBUG (dcm)
 * 2003-03-11 New module interface (janakj)
 * 2003-03-16 flags export parameter added (janakj)
 * 2003-04-06 rank 0 changed to 1 in child_init (janakj)
 * 2003-06-19 fixed too many Jabber workers bug (mostly on RH9.0) (dcm)
 * 2003-08-05 adapted to the new parse_content_type_hdr function (bogdan)
 * 2004-06-07 db API update (andrei)
 */


#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include "../../sr_module.h"
#include "../../error.h"
#include "../../ut.h"
#include "../../mem/shm_mem.h"
#include "../../mem/mem.h"
#include "../../globals.h"
#include "../../timer.h"
#include "../../parser/parse_uri.h"
#include "../../parser/parse_content.h"
#include "../../parser/parse_from.h"
#include "../../lib/srdb1/db.h"

#include "../../modules/tm/tm_load.h"

#ifdef HAVE_IHTTP
#include "../ihttp/ih_load.h"
#endif

#include "xjab_load.h"
#include "xjab_worker.h"
#include "xjab_util.h"


MODULE_VERSION

/** TM bind */
struct tm_binds tmb;

#ifdef HAVE_IHTTP
/** iHTTP bind */
struct ih_binds ihb;
/** iHTTP callback functions */
int xjab_mod_info(ih_req_p _irp, void *_p, char *_bb, int *_bl, 
		char *_hb, int *_hl);
int xjab_connections(ih_req_p _irp, void *_p, char *_bb, int *_bl, 
		char *_hb, int *_hl);
#endif

/** workers list */
xj_wlist jwl = NULL;

/** Structure that represents database connection */
static db1_con_t** db_con;
static db_func_t jabber_dbf;

/** parameters */

static str db_url   = str_init("mysql://root@127.0.0.1/sip_jab");
static str db_table = str_init("jusers");
char *registrar=NULL; /*"sip:registrar@example.org";*/

int nrw = 2;
int max_jobs = 10;

char *jaddress = "127.0.0.1";
int jport = 5222;

char *jaliases = NULL;
char *jdomain  = NULL;
char *proxy	   = NULL;

char* priority = "9";

int delay_time = 90;
int sleep_time = 20;
int cache_time = 600;
int check_time = 20;

int **pipes = NULL;

static int mod_init(void);
static int child_init(int rank);

int xjab_manage_sipmsg(struct sip_msg *msg, int type);
void xjab_check_workers(int mpid);

static int xj_send_message(struct sip_msg*, char*, char*);
static int xj_join_jconf(struct sip_msg*, char*, char*);
static int xj_exit_jconf(struct sip_msg*, char*, char*);
static int xj_go_online(struct sip_msg*, char*, char*);
static int xj_go_offline(struct sip_msg*, char*, char*);

static void destroy(void);

/*
 * Exported functions
 */
static cmd_export_t cmds[] = {
	{"jab_send_message",       (cmd_function)xj_send_message,
			0, 0, 0, REQUEST_ROUTE},
	{"jab_join_jconf",         (cmd_function)xj_join_jconf,
			0, 0, 0, REQUEST_ROUTE},
	{"jab_exit_jconf",         (cmd_function)xj_exit_jconf,
			0, 0, 0, REQUEST_ROUTE},
	{"jab_go_online",          (cmd_function)xj_go_online,
			0, 0, 0, REQUEST_ROUTE},
	{"jab_go_offline",         (cmd_function)xj_go_offline,
			0, 0, 0, REQUEST_ROUTE},
	{"jab_register_watcher",   (cmd_function)xj_register_watcher,
			XJ_NO_SCRIPT_F, 0, 0, 0            },
	{"jab_unregister_watcher", (cmd_function)xj_unregister_watcher,
			XJ_NO_SCRIPT_F, 0, 0, 0            },
	{"load_xjab",              (cmd_function)load_xjab,
			XJ_NO_SCRIPT_F, 0, 0, 0            },
	{0, 0, 0, 0, 0, 0}
};


/*
 * Exported parameters 
 */
static param_export_t params[] = {
	{"db_url",     PARAM_STRING, &db_url.s  },
	{"jaddress",   PARAM_STRING, &jaddress  },
	{"aliases",    PARAM_STRING, &jaliases  },
	{"proxy",      PARAM_STRING, &proxy     },
	{"jdomain",    PARAM_STRING, &jdomain   },
	{"registrar",  PARAM_STRING, &registrar },
	{"priority",   PARAM_STRING, &priority  },
	{"jport",      INT_PARAM, &jport     },
	{"workers",    INT_PARAM, &nrw       },
	{"max_jobs",   INT_PARAM, &max_jobs  },
	{"cache_time", INT_PARAM, &cache_time},
	{"delay_time", INT_PARAM, &delay_time},
	{"sleep_time", INT_PARAM, &sleep_time},
	{"check_time", INT_PARAM, &check_time},
	{0, 0, 0}
};


struct module_exports exports= {
	"jabber",
	DEFAULT_DLFLAGS, /* dlopen flags */
	cmds,       /* Exported functions */
	params,     /* Exported parameters */
	0,          /* exported statistics */
	0,          /* exported MI functions */
	0,          /* exported pseudo-variables */
	0,          /* extra processes */
	mod_init,   /* module initialization function */
	0,
	(destroy_function) destroy,
	child_init  /* per-child init function */
};

/**
 * init module function
 */
static int mod_init(void)
{
	LM_WARN("This module is deprecated and will be removed in the next release. Use the <purple> module.");
#ifdef HAVE_IHTTP
	load_ih_f load_ih;
#endif
	int  i;
	db_url.len = strlen(db_url.s);

	if(!jdomain)
	{
		LM_ERR("jdomain is NULL\n");
		return -1;
	}

	/* import mysql functions */
	if (db_bind_mod(&db_url, &jabber_dbf)<0)
	{
		LM_ERR("database module not found\n");
		return -1;
	}

	if (!DB_CAPABILITY(jabber_dbf, DB_CAP_QUERY)) {
		LM_ERR("database module does not implement 'query' function\n");
		return -1;
	}

	db_con = (db1_con_t**)shm_malloc(nrw*sizeof(db1_con_t*));
	if (db_con == NULL)
	{
		LM_ERR("no more shm memory\n");
		return -1;
	}

	/* load the TM API */
	if (load_tm_api(&tmb)!=0) {
		LM_ERR("can't load TM API\n");
		return -1;
	}

#ifdef HAVE_IHTTP
	/* import the iHTTP auto-loading function */
	if ( !(load_ih=(load_ih_f)find_export("load_ih", IH_NO_SCRIPT_F, 0))) {
		LM_ERR("can't import load_ih\n");
		return -1;
	}
	/* let the auto-loading function load all TM stuff */
	if (load_ih( &ihb )==-1)
		return -1;
#endif
	
	pipes = (int**)pkg_malloc(nrw*sizeof(int*));
	if (pipes == NULL)
	{
		LM_ERR("no more pkg memory (pipes)\n");
		return -1;
	}
	
	for(i=0; i<nrw; i++)
	{
		pipes[i] = (int*)pkg_malloc(2*sizeof(int));
		if (!pipes[i])
		{
			LM_ERR("no more pkg memory (pipes)\n");
			return -1;
		}
	}
	
	for(i=0; i<nrw; i++)
	{	
		db_con[i] = jabber_dbf.init(&db_url);
		if (!db_con[i])
		{
			LM_ERR("failed to connect to the database\n");
			return -1;
		}
		else
		{
			if (jabber_dbf.use_table(db_con[i], &db_table) < 0) {
				LM_ERR("use_table failed\n");
				return -1;
			}
			LM_DBG("database connection opened successfully\n");
		}
	}

	
	/** creating the pipes */
	
	for(i=0;i<nrw;i++)
	{
		/* create the pipe*/
		if (pipe(pipes[i])==-1) {
			LM_ERR("cannot create pipe!\n");
			return -1;
		}
		LM_DBG("pipe[%d] = <%d>-<%d>\n", i, pipes[i][0], pipes[i][1]);
	}
	
	if((jwl = xj_wlist_init(pipes,nrw,max_jobs,cache_time,sleep_time,
				delay_time)) == NULL)
	{
		LM_ERR("failed to initialize workers list\n");
		return -1;
	}
	
	if(xj_wlist_set_aliases(jwl, jaliases, jdomain, proxy) < 0)
	{
		LM_ERR("failed to set aliases and outbound proxy\n");
		return -1;
	}

	LM_DBG("initialized ...\n");	
	return 0;
}

/*
 * Initialize children
 */
static int child_init(int rank)
{
	int i, j, mpid, cpid;
	
	LM_DBG("initializing child <%d>\n", rank);
	     /* Rank 0 is main process now - 1 is the first child (janakj) */
	if(rank == 1)
	{
#ifdef HAVE_IHTTP
		/** register iHTTP callbacks -- go forward in any case*/
		ihb.reg_f("xjab", "XMPP Gateway", IH_MENU_YES,
				xjab_mod_info, NULL);
		ihb.reg_f("xjabc", "XMPP connections", IH_MENU_YES,
				xjab_connections, NULL);
#endif
		if((mpid=fork())<0 )
		{
			LM_ERR("cannot launch worker's manager\n");
			return -1;
		}
		if(mpid == 0)
		{
			/** launching the workers */
			for(i=0;i<nrw;i++)
			{
				if ( (cpid=fork())<0 )
				{
					LM_ERR("cannot launch worker\n");
					return -1;
				}
				if (cpid == 0)
				{
					for(j=0;j<nrw;j++)
						if(j!=i) close(pipes[j][0]);
					close(pipes[i][1]);
					if(xj_wlist_set_pid(jwl, getpid(), i) < 0)
					{
						LM_ERR("failed to set worker's pid\n");
						return -1;
					}
					xj_worker_process(jwl,jaddress,jport, priority, i, 
							db_con[i], &jabber_dbf);
					exit(0);
				}
			}

			mpid = getpid();
			while(1)
			{
				sleep(check_time);
				xjab_check_workers(mpid);
			}
		}
	}
	
	//if(pipes)
	//{
	//	for(i=0;i<nrw;i++)
	//		close(pipes[i][0]);
	//}
	return 0;
}

/**
 * send the SIP MESSAGE through Jabber
 */
static int xj_send_message(struct sip_msg *msg, char* foo1, char * foo2)
{
	LM_DBG("processing SIP MESSAGE\n");
	return xjab_manage_sipmsg(msg, XJ_SEND_MESSAGE);
}

/**
 * join a Jabber conference
 */
static int xj_join_jconf(struct sip_msg *msg, char* foo1, char * foo2)
{
	LM_DBG("join a Jabber conference\n");
	return xjab_manage_sipmsg(msg, XJ_JOIN_JCONF);
}

/**
 * exit from Jabber conference
 */
static int xj_exit_jconf(struct sip_msg *msg, char* foo1, char * foo2)
{
	LM_DBG("exit from a Jabber conference\n");
	return xjab_manage_sipmsg(msg, XJ_EXIT_JCONF);
}

/**
 * go online in Jabber network
 */
static int xj_go_online(struct sip_msg *msg, char* foo1, char * foo2)
{
	LM_DBG("go online in Jabber network\n");
	return xjab_manage_sipmsg(msg, XJ_GO_ONLINE);
}

/**
 * go offline in Jabber network
 */
static int xj_go_offline(struct sip_msg *msg, char* foo1, char * foo2)
{
	LM_DBG("go offline in Jabber network\n");
	return xjab_manage_sipmsg(msg, XJ_GO_OFFLINE);
}

/**
 * manage SIP message
 */
int xjab_manage_sipmsg(struct sip_msg *msg, int type)
{
	str body, dst, from_uri;
	xj_sipmsg jsmsg;
	int pipe, fl;
	t_xj_jkey jkey, *p;
	int mime;

	body.len = 0;
	body.s = 0;

	// extract message body - after that whole SIP MESSAGE is parsed
	if (type==XJ_SEND_MESSAGE)
	{
		/* get the message's body */
		body.s = get_body( msg );
		if(body.s==0) 
		{
			LM_ERR("cannot extract body from msg\n");
			goto error;
		}
		
		/* content-length (if present) must be already parsed */
		if(!msg->content_length)
		{
			LM_ERR("no Content-Length header found!\n");
			goto error;
		}
		body.len = get_content_length(msg);

		/* parse the content-type header */
		if((mime=parse_content_type_hdr(msg))<1)
		{
			LM_ERR("cannot parse Content-Type header\n");
			goto error;
		}

		/* check the content-type value */
		if(mime!=(TYPE_TEXT<<16)+SUBTYPE_PLAIN
			&& mime!=(TYPE_MESSAGE<<16)+SUBTYPE_CPIM)
		{
			LM_ERR("invalid content-type for"
				" a message request! type found=%d\n", mime);
			goto error;
		}
	}
	
	// check for TO and FROM headers - if is not SIP MESSAGE 
	if(parse_headers(msg,HDR_TO_F|HDR_FROM_F,0)==-1 || !msg->to || !msg->from)
	{
		LM_ERR("cannot find TO or FROM HEADERS!\n");
		goto error;
	}
	
	/* parsing from header */
	if ( parse_from_header( msg )<0 || msg->from->parsed==NULL) 
	{
		LM_DBG("cannot get FROM header\n");
		goto error;
	}
	from_uri.s = ((struct to_body*)msg->from->parsed)->uri.s;
	from_uri.len = ((struct to_body*)msg->from->parsed)->uri.len;
	if(xj_extract_aor(&from_uri, 0))
	{
		LM_DBG("cannot get AoR from FROM header\n");
		goto error;
	}

	jkey.hash = xj_get_hash(&from_uri, NULL);
	jkey.id = &from_uri;
	// get the communication pipe with the worker
	switch(type)
	{
		case XJ_SEND_MESSAGE:
		case XJ_JOIN_JCONF:
		case XJ_GO_ONLINE:
			if((pipe = xj_wlist_get(jwl, &jkey, &p)) < 0)
			{
				LM_DBG("cannot find pipe of the worker!\n");
				goto error;
			}
		break;
		case XJ_EXIT_JCONF:
		case XJ_GO_OFFLINE:
			if((pipe = xj_wlist_check(jwl, &jkey, &p)) < 0)
			{
				LM_DBG("no open Jabber session for"
						" <%.*s>!\n", from_uri.len, from_uri.s);
				goto error;
			}
		break;
		default:
			LM_DBG("error, strange SIP msg type!\n");
			goto error;
	}

	// if is for going ONLINE/OFFLINE we do not need the destination
	if(type==XJ_GO_ONLINE || type==XJ_GO_OFFLINE)
		goto prepare_job;
	
	// determination of destination
	// - try to get it from new_uri, r-uri or to hdr, but check it against
	// jdomain and aliases
	dst.len = 0;
	if( msg->new_uri.len > 0)
	{
		dst.s = msg->new_uri.s;
		dst.len = msg->new_uri.len;
		if(xj_wlist_check_aliases(jwl, &dst))
			dst.len = 0;
#ifdef XJ_EXTRA_DEBUG
		else
			LM_DBG("using NEW URI for destination\n");
#endif
	}
	
	if (dst.len == 0 &&  msg->first_line.u.request.uri.s != NULL
			&& msg->first_line.u.request.uri.len > 0 )
	{
		dst.s = msg->first_line.u.request.uri.s;
		dst.len = msg->first_line.u.request.uri.len;
		if(xj_wlist_check_aliases(jwl, &dst))
			dst.len = 0;
#ifdef XJ_EXTRA_DEBUG
		else
			LM_DBG("using R-URI for destination\n");
#endif
	}

	if(dst.len == 0 && msg->to->parsed)
	{
		dst.s = ((struct to_body*)msg->to->parsed)->uri.s;
		dst.len = ((struct to_body*)msg->to->parsed)->uri.len;
		if(dst.s == NULL || xj_wlist_check_aliases(jwl, &dst))
			dst.len = 0;
#ifdef XJ_EXTRA_DEBUG
		else
			LM_DBG("using TO-URI for destination\n");
#endif
	}
	
	if(dst.len == 0)
	{
		LM_DBG("destination not found in SIP message\n");
		goto error;
	}
	
	/** skip 'sip:' and parameters in destination address */
	if(xj_extract_aor(&dst, 1))
	{
		LM_ERR("cannot get AoR for destination\n");
		goto error;
	}
#ifdef XJ_EXTRA_DEBUG
	LM_DBG("destination after correction [%.*s].\n", dst.len, dst.s);
#endif
	
prepare_job:
	//putting the SIP message parts in share memory to be accessible by workers
    jsmsg = (xj_sipmsg)shm_malloc(sizeof(t_xj_sipmsg));
	memset(jsmsg, 0, sizeof(t_xj_sipmsg));
    if(jsmsg == NULL)
    	return -1;
	
	switch(type)
	{
		case XJ_SEND_MESSAGE:
			jsmsg->msg.len = body.len;
			if((jsmsg->msg.s = (char*)shm_malloc(jsmsg->msg.len+1)) == NULL)
			{
				shm_free(jsmsg);
				goto error;
			}
			strncpy(jsmsg->msg.s, body.s, jsmsg->msg.len);
		break;
		case XJ_GO_ONLINE:
		case XJ_GO_OFFLINE:
			dst.len = 0;
			dst.s = 0;
		case XJ_JOIN_JCONF:
		case XJ_EXIT_JCONF:
			jsmsg->msg.len = 0;
			jsmsg->msg.s = NULL;
		break;
		default:
			LM_DBG("this SHOULD NOT appear\n");
			shm_free(jsmsg);
			goto error;
	}
	if(dst.len>0)
	{
		jsmsg->to.len = dst.len;
		if((jsmsg->to.s = (char*)shm_malloc(jsmsg->to.len+1))==NULL)
		{
			if(type == XJ_SEND_MESSAGE)
				shm_free(jsmsg->msg.s);
			shm_free(jsmsg);
			goto error;
		}
		strncpy(jsmsg->to.s, dst.s, jsmsg->to.len);
	}
	else
	{
		jsmsg->to.len = 0;
		jsmsg->to.s   = 0;
	}

	jsmsg->jkey = p;
	jsmsg->type = type;
	//jsmsg->jkey->hash = jkey.hash;

	LM_DBG("sending <%p> to worker through <%d>\n",	jsmsg, pipe);
	// sending the SHM pointer of SIP message to the worker
	fl = write(pipe, &jsmsg, sizeof(jsmsg));
	if(fl != sizeof(jsmsg))
	{
		LM_ERR("failed to write to worker pipe!\n");
		if(type == XJ_SEND_MESSAGE)
			shm_free(jsmsg->msg.s);
		shm_free(jsmsg->to.s);
		shm_free(jsmsg);
		goto error;
	}
	
	return 1;
error:
	return -1;
}

/**
 * destroy function of module
 */
static void destroy(void)
{
	int i;
#ifdef XJ_EXTRA_DEBUG
	LM_DBG("unloading module ...\n");
#endif
	if(pipes)
	{ // close the pipes
		for(i = 0; i < nrw; i++)
		{
			if(pipes[i])
			{
				close(pipes[i][0]);
				close(pipes[i][1]);
			}
			pkg_free(pipes[i]);
		}
		pkg_free(pipes);
	}
	// cleaning MySQL connections
	if(db_con != NULL)
	{
		for(i = 0; i<nrw; i++)
			jabber_dbf.close(db_con[i]);
		shm_free(db_con);
	}
			
	xj_wlist_free(jwl);
	LM_DBG("unloaded ...\n");
}

/**
 * register a watcher function for a Jabber user' presence
 */
void xj_register_watcher(str *from, str *to, void *cbf, void *pp)
{
	xj_sipmsg jsmsg = NULL;
	t_xj_jkey jkey, *jp;
	int pipe, fl;
	str from_uri, to_uri;

	if(!to || !from || !cbf)
		return;

#ifdef XJ_EXTRA_DEBUG
	LM_DBG("from=[%.*s] to=[%.*s]\n", from->len,
	    from->s, to->len, to->s);
#endif
	from_uri.s = from->s;
	from_uri.len = from->len;
	if(xj_extract_aor(&from_uri, 0))
	{
		LM_ERR("cannot get AoR from FROM header\n");
		goto error;
	}

	jkey.hash = xj_get_hash(&from_uri, NULL);
	jkey.id = &from_uri;

	if((pipe = xj_wlist_get(jwl, &jkey, &jp)) < 0)
	{
		LM_DBG("cannot find pipe of the worker!\n");
		goto error;
	}
	
	//putting the SIP message parts in share memory to be accessible by workers
	jsmsg = (xj_sipmsg)shm_malloc(sizeof(t_xj_sipmsg));
	memset(jsmsg, 0, sizeof(t_xj_sipmsg));
	if(jsmsg == NULL)
		goto error;
	
	jsmsg->msg.len = 0;
	jsmsg->msg.s = NULL;
	
	to_uri.s = to->s;
	to_uri.len = to->len;
	/** skip 'sip:' and parameters in destination address */
	if(xj_extract_aor(&to_uri, 1))
	{
		LM_ERR("cannot get AoR for destination\n");
		goto error;
	}
#ifdef XJ_EXTRA_DEBUG
	LM_DBG("destination after correction [%.*s].\n", to_uri.len, to_uri.s);
#endif

	jsmsg->to.len = to_uri.len;
	if((jsmsg->to.s = (char*)shm_malloc(jsmsg->to.len+1)) == NULL)
	{
		if(jsmsg->msg.s)
			shm_free(jsmsg->msg.s);
		shm_free(jsmsg);
		goto error;
	}
	strncpy(jsmsg->to.s, to_uri.s, jsmsg->to.len);
	jsmsg->to.s[jsmsg->to.len] = '\0';

	jsmsg->jkey = jp;
	jsmsg->type = XJ_REG_WATCHER;
	//jsmsg->jkey->hash = jkey.hash;
	
	jsmsg->cbf = (pa_callback_f)cbf;
	jsmsg->p = pp;

#ifdef XJ_EXTRA_DEBUG
	LM_DBG("sending <%p> to worker through <%d>\n", jsmsg, pipe);
#endif
	// sending the SHM pointer of SIP message to the worker
	fl = write(pipe, &jsmsg, sizeof(jsmsg));
	if(fl != sizeof(jsmsg))
	{
		LM_ERR("failed to write to worker pipe!\n");
		if(jsmsg->msg.s)
			shm_free(jsmsg->msg.s);
		shm_free(jsmsg->to.s);
		shm_free(jsmsg);
		goto error;
	}
	
 error:
	return;
}

/**
 * unregister a watcher for a Jabber user' presence
 */
void xj_unregister_watcher(str *from, str *to, void *cbf, void *pp)
{
	if(!to || !from)
		return;
}

/**
 * check if all SER2Jab workers are still alive
 * - if not, try to launch new ones
 */
void xjab_check_workers(int mpid)
{
	int i, n, stat;
	//LM_DBG("time=%d\n", get_ticks());
	if(!jwl || jwl->len <= 0)
		return;
	for(i=0; i < jwl->len; i++)
	{
		if(jwl->workers[i].pid > 0)
		{
			stat = 0;
			n = waitpid(jwl->workers[i].pid, &stat, WNOHANG);
			if(n == 0 || n!=jwl->workers[i].pid)
				continue;
		
			LM_ERR("worker[%d][pid=%d] has exited - status=%d err=%d"
					"errno=%d\n", i, jwl->workers[i].pid, stat, n, errno);
			xj_wlist_clean_jobs(jwl, i, 1);
			xj_wlist_set_pid(jwl, -1, i);
		}
		
#ifdef XJ_EXTRA_DEBUG
		LM_DBG("create a new worker[%d]\n", i);
#endif
		if ( (stat=fork())<0 )
		{
#ifdef XJ_EXTRA_DEBUG
			LM_DBG("cannot launch new worker[%d]\n", i);
#endif
			LM_ERR("worker[%d] lost forever \n", i);
			return;
		}
		if (stat == 0)
		{
			if(xj_wlist_set_pid(jwl, getpid(), i) < 0)
			{
				LM_ERR("failed to set new worker's pid - w[%d]\n", i);
				return;
			}
			xj_worker_process(jwl,jaddress,jport,priority, i,
					db_con[i], &jabber_dbf);
			exit(0);
		}
	}			
}

#ifdef HAVE_IHTTP
/**
 * Module's information retrieval - function to use with iHttp module
 *
 */ 
int xjab_mod_info(ih_req_p _irp, void *_p, char *_bb, int *_bl, 
		char *_hb, int *_hl)
{
	if(!_irp || !_bb || !_bl || *_bl <= 0 || !_hb || !_hl || *_hl <= 0)
		return -1;
	*_hl = 0;
	*_hb = 0;
	
	strcpy(_bb, "<h4>SER2Jabber Gateway</h4>");
	strcat(_bb, "<br>Module parameters:<br>");
	strcat(_bb, "<br> -- db table = ");
	strcat(_bb, db_table);
	strcat(_bb, "<br> -- workers = ");
	strcat(_bb, int2str(nrw, NULL));
	strcat(_bb, "<br> -- max jobs per worker = ");
	strcat(_bb, int2str(max_jobs, NULL));

	strcat(_bb, "<br> -- jabber server address = ");
	strcat(_bb, jaddress);
	strcat(_bb, "<br> -- jabber server port = ");
	strcat(_bb, int2str(jport, NULL));

	strcat(_bb, "<br> -- aliases = ");
	strcat(_bb, (jaliases)?jaliases:"NULL");
	strcat(_bb, "<br> -- jabber domain = ");
	strcat(_bb, (jdomain)?jdomain:"NULL");
	strcat(_bb, "<br> -- proxy address = ");
	strcat(_bb, (proxy)?proxy:"NULL");

	strcat(_bb, "<br> -- delay time = ");
	strcat(_bb, int2str(delay_time, NULL));
	strcat(_bb, "<br> -- sleep time = ");
	strcat(_bb, int2str(sleep_time, NULL));
	strcat(_bb, "<br> -- cache time = ");
	strcat(_bb, int2str(cache_time, NULL));
	strcat(_bb, "<br> -- check time = ");
	strcat(_bb, int2str(check_time, NULL));
	
	*_bl = strlen(_bb);

	return 0;
}

/**
 * SER2Jab connection management - function to use with iHttp module
 * - be aware of who is able to use the ihttp because he can close any 
 *   open connection between SER and Jabber server
 */ 
int xjab_connections(ih_req_p _irp, void *_p, char *_bb, int *_bl, 
		char *_hb, int *_hl)
{
	t_xj_jkey jkey, *p;
	str _u;
	ih_param_p _ipp = NULL;
	int idx, i, maxcount;
	char *cp;

	if(!_irp || !_bb || !_bl || *_bl <= 0 || !_hb || !_hl || *_hl <= 0)
		return -1;
	
	*_hl = 0;
	*_hb = 0;
	idx = -1;
	strcpy(_bb, "<h4>Active XMPP connections</h4>");
	
	if(_irp->params)
	{
		strcat(_bb, "<br><b>Close action is alpha release!</b><br>");
		_ipp = _irp->params;
		i = 0;
		while(_ipp)
		{
			switch(_ipp->name[0])
			{
				case 'w':
					idx = 0;
					cp = _ipp->value;
					while(*cp && *cp>='0' && *cp<='9')
					{
						idx = idx*10 + *cp-'0';
						cp++;
					}
					i++;
				break;
				case 'u':
					_u.s = _ipp->value;
					_u.len = strlen(_ipp->value);
					jkey.id = &_u;
					i++;
				break;
				case 'i':
					jkey.hash = 0;
					cp = _ipp->value;
					while(*cp && *cp>='0' && *cp<='9')
					{
						jkey.hash = jkey.hash*10 + *cp-'0';
						cp++;
					}
					i++;
				break;
				
			}
			_ipp = _ipp->next;
		}
		if(i!=3 || idx < 0 || idx >= jwl->len)
		{
			strcat(_bb, "<br><b><i>Bad parameters!</i></b>\n");
		}
		else
		{
			strcat(_bb, "<br><b><i>The connection of [");
			strcat(_bb, _u.s);

			if(xj_wlist_set_flag(jwl, &jkey, XJ_FLAG_CLOSE) < 0)
				strcat(_bb, "] does not exist!</i></b>\n");
			else
				strcat(_bb, "] was scheduled for closing!</i></b>\n");
		}
		*_bl = strlen(_bb);

		return 0;
	}
	
	if(jwl!=NULL && jwl->len > 0 && jwl->workers!=NULL)
	{
		for(idx=0; idx<jwl->len; idx++)
		{
			strcat(_bb, "<br><b><i>Worker[");
			strcat(_bb, int2str(idx, NULL));
			strcat(_bb, "]</i></b> &nbsp;&nbsp;pid=");
			strcat(_bb, int2str(jwl->workers[idx].pid, NULL));
			strcat(_bb, " &nbsp;&nbsp;nr of jobs=");
			strcat(_bb, int2str(jwl->workers[idx].nr, NULL));
			if(!jwl->workers[idx].sip_ids)
				continue;
			lock_set_get(jwl->sems, idx);
			maxcount = count234(jwl->workers[idx].sip_ids);
			for (i = 0; i < maxcount; i++) 
			{
				p = (xj_jkey)index234(jwl->workers[idx].sip_ids, i);
				if(p == NULL)
					continue;
				strcat(_bb, "<br>&nbsp;&nbsp;&nbsp;");
				strcat(_bb, int2str(i, NULL));
				strcat(_bb, ".&nbsp;&nbsp;&nbsp;");
				strcat(_bb, "<a href=\"xjabc?w=");
				strcat(_bb, int2str(idx, NULL));
				strcat(_bb, "&i=");
				strcat(_bb, int2str(p->hash, NULL));
				strcat(_bb, "&u=");
				strncat(_bb, p->id->s, p->id->len);
				strcat(_bb, "\">close</a>");
				strcat(_bb, "&nbsp;&nbsp;&nbsp;");
				strcat(_bb, int2str(p->hash, NULL));
				strcat(_bb, "&nbsp;&nbsp;&nbsp;");
				strncat(_bb, p->id->s, p->id->len);
			}
			lock_set_release(jwl->sems, idx);
		}
	}
	
	*_bl = strlen(_bb);

	return 0;
}

#endif // HAVE_IHTTP
