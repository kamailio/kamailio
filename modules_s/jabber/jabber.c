/*
 * $Id$
 *
 * JABBER module
 *
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>

#include "../../sr_module.h"
#include "../../error.h"
#include "../../ut.h"
#include "../im/im_funcs.h"
#include "../../mem/shm_mem.h"
#include "../../mem/mem.h"
#include "../../globals.h"

#include "sip2jabber.h"
#include "jc_pool.h"
#include "db.h"

jab_wlist jwl = NULL;

/* Structure that represents database connection */
db_con_t** db_con;

/** parameters */

char *db_url   = "sql://root@127.0.0.1/sip_jab";
char *db_table = "jusers";

int nrw = 2;
int max_jobs = 10;

char *contact = "sip:193.175.135.68:5060";
char *jaddress = "127.0.0.1";
int jport = 5222;

int sleep_time = 20;
int cache_time = 200;

int **pipes = NULL;

static int mod_init(void);
static int child_init(int rank);
static int jab_send_message(struct sip_msg*, char*, char* );

void destroy(void);

struct module_exports exports= {
	"jabber",
	(char*[]){
				"jab_send_message"
			},
	(cmd_function[]){
					jab_send_message
					},
	(int[]){
				0
			},
	(fixup_function[]){
				0
		},
	1,

	(char*[]) {   /* Module parameter names */
		"contact",
		"db_url",
		"jaddress",
		"jport",
		"workers",
		"max_jobs",
		"cache_time",
		"sleep_time"
	},
	(modparam_t[]) {   /* Module parameter types */
		STR_PARAM,
		STR_PARAM,
		STR_PARAM,
		INT_PARAM,
		INT_PARAM,
		INT_PARAM,
		INT_PARAM,
		INT_PARAM
	},
	(void*[]) {   /* Module parameter variable pointers */
		&contact,
		&db_url,
		&jaddress,
		&jport,
		&nrw,
		&max_jobs,
		&cache_time,
		&sleep_time
	},
	7,      /* Number of module paramers */
	
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
	int  i;

	DBG("JABBER: initializing\n");
	
	if (bind_dbmod())
	{
		DBG("JABBER: ERROR: Database module not found\n");
		return -1;
	}
	
	db_con = (db_con_t**)shm_malloc(nrw*sizeof(db_con_t*));
	if (db_con == NULL)
	{
		DBG("JABBER: Error while allocating db_con's\n");
		return -3;
	}
	pipes = (int**)pkg_malloc(nrw*sizeof(int*));
	if (pipes == NULL)
	{
		DBG("JABBER: Error while allocating pipes\n");
		return -3;
	}
	
	for(i=0; i<nrw; i++)
	{
		pipes[i] = (int*)pkg_malloc(2*sizeof(int));
		if (!pipes[i])
		{
			DBG("JABBER: Error while allocating pipes\n");
			return -3;
		}
	}
	
	for(i=0; i<nrw; i++)
	{
		db_con[i] = db_init(db_url);
		if (!db_con[i])
		{
			DBG("JABBER: Error while connecting database\n");
			return -3;
		}
		else
		{
			db_use_table(db_con[i], db_table);
			DBG("JABBER: Database connection opened successfuly\n");
		}
	}

	
	/** creating the pipees */
	
	for(i=0;i<nrw;i++)
	{
		/* create the pipe*/
		if (pipe(pipes[i])==-1) {
			DBG("JABBER: ERROR: mod_init: cannot create pipe!\n");
			return -4;
		}
		DBG("JABBER: INIT: pipe[%d] = <%d>-<%d>\n", i, pipes[i][0], pipes[i][1]);
	}
	
	if((jwl = jab_wlist_init(pipes, nrw, max_jobs)) == NULL)
	{
		DBG("JABBER: mod_init: error initializing workers list\n");
		return -1;
	}
	
	if((jab_wlist_init_ssock(jwl, "gorn.fokus.gmd.de", 5060) < 0) || (jab_wlist_init_contact(jwl, contact) < 0))
	{
		DBG("JABBER: mod_init: error workers list properties\n");
		return -1;
	}
	
	
	return 0;
}

/*
 * Initialize childs
 */
static int child_init(int rank)
{
	int i;
	int *pids = NULL;
	
	DBG("JABBER: Initializing child %d\n", rank);
	if(rank == 0)
	{
		pids = (int*)pkg_malloc(nrw*sizeof(int));
		if (pids == NULL)
		{
			DBG("JABBER: Error while allocating pid's\n");
			return -1;
		}
		/** launching the workers */
		for(i=0;i<nrw;i++)
		{
			if ( (pids[i]=fork())<0 )
			{
				DBG("JABBER: ERROR: mod_init cannot launch worker\n");
				return -1;
			}
			if (pids[i] == 0)
			{
				close(pipes[i][1]);
				worker_process(jwl, jaddress, jport, pipes[i][0], max_jobs, cache_time, sleep_time, db_con[i]);
				exit(0);
			}
		}
	
		if(jab_wlist_set_pids(jwl, pids, nrw) < 0)
		{
			DBG("JABBER: mod_init: error setting pid's\n");
			return -1;
		}
		if(pids)
			pkg_free(pids);
	}
	
	if(pipes)
	{
		for(i=0;i<nrw;i++)
			close(pipes[i][0]);
		for(i = 0; i < nrw; i++)
			pkg_free(pipes[i]);
		pkg_free(pipes);
	}
	return 0;
}

/**
 * send the SIP message through Jabber
 */
static int jab_send_message(struct sip_msg *msg, char* foo1, char * foo2)
{
	str body, dst, /*host, user,*/ *p;
	jab_sipmsg jsmsg;
	struct to_body to, from;
	struct sip_uri _uri;
	int pipe, fl;

	// extract message body - after that whole SIP MESSAGE is parsed
	if ( !im_extract_body(msg,&body) )
	{
		DBG("JABBER: jab_send_message: cannot extract body from sip msg!\n");
		goto error;
	}
	// check for FROM header
	if(msg->from != NULL)
	{
		memset( &from , 0, sizeof(from) );
		parse_to(msg->from->body.s, msg->from->body.s + msg->from->body.len + 1, &from);
		if(from.error == PARSE_OK)
			DBG("JABBER: jab_send_message: From parsed OK.\n");
		else
		{
			DBG("JABER: jab_send_message: From NOT parsed\n");
			goto error;
		}
	}
	else
	{
		DBG("JABBER: jab_send_message: cannot find FROM HEADER!\n");
		goto error;
	}
	// get the communication pipe with the worker
	if((pipe = jab_wlist_get(jwl, &from.uri, &p)) < 0)
	{
		DBG("JABBER: jab_send_message: cannot find pipe of the worker!\n");
		goto error;
	}
	
	// determination of destination
	dst.len = 0;
	if( msg->new_uri.len > 0 )
	{
		DBG("JABBER: jab_send_message: using NEW URI for destination\n");
		dst.s = msg->new_uri.s;
		dst.len = msg->new_uri.len;
	} else if ( msg->first_line.u.request.uri.len > 0 )
	{
		DBG("JABBER: jab_send_message: parsing URI from first line\n");
		if(parse_uri(msg->first_line.u.request.uri.s, msg->first_line.u.request.uri.len, &_uri) < 0)
		{
			DBG("JABBER: jab_send_message: ERROR parsing URI from first line\n");
			goto error;
		}
		if(_uri.user.len > 0)
		{
			DBG("JABBER: jab_send_message: using URI for destination\n");
			dst.s = msg->first_line.u.request.uri.s;
			dst.len = msg->first_line.u.request.uri.len;
		}
		free_uri(&_uri);
	}
	if(dst.len == 0 && msg->to != NULL)
	{
		memset( &to , 0, sizeof(to) );
		parse_to(msg->to->body.s, msg->to->body.s + msg->to->body.len + 1, &to);
		if(to.uri.len > 0) // to.error == PARSE_OK)
		{
			DBG("JABBER: jab_send_message: TO parsed OK <%.*s>.\n", to.uri.len, to.uri.s);
			dst.s = to.uri.s;
			dst.len = to.uri.len;
		}
		else
		{
			DBG("JABER: jab_send_message: TO NOT parsed\n");
			goto error;
		}
	}
	if(dst.len == 0)
	{
		DBG("JABBER: jab_send_message: destination not found in SIP message\n");
		goto error;
	}
	
	/** skip 'sip:' in destination address */
	if(dst.s[0]=='s' && dst.s[1]=='i' && dst.s[2]=='p')
	{
		dst.s += 3;
		dst.len -= 3;
		fl = 1;
		while(*dst.s == ' ' || *dst.s == '\t' || *dst.s == ':')
		{
			dst.s++;
			dst.len--;
			fl = 0;
		}
		if(fl)
		{
			dst.s -= 3;
			dst.len += 3;
		}
		
		DBG("JABBER: DESTINATION corrected <%.*s>.\n", dst.len, dst.s);
	}
	
	// putting the SIP message parts in share memory to be accessible by workers
    jsmsg = (jab_sipmsg)shm_malloc(sizeof(t_jab_sipmsg));
    if(jsmsg == NULL)
    	return -1;
	jsmsg->to.len = dst.len;
	jsmsg->to.s = (char*)shm_malloc(jsmsg->to.len+1);
	if(jsmsg->to.s == NULL)
	{
		shm_free(jsmsg);
		goto error;
	}
	strncpy(jsmsg->to.s, dst.s, jsmsg->to.len);
	
	jsmsg->msg.len = body.len;
	jsmsg->msg.s = (char*)shm_malloc(jsmsg->msg.len+1);
	if(jsmsg->msg.s == NULL)
	{
		shm_free(jsmsg->to.s);
		shm_free(jsmsg);
		goto error;
	}
	strncpy(jsmsg->msg.s, body.s, jsmsg->msg.len);
	
	jsmsg->from = p;
	
	DBG("JABBER: jab_send_message[%d]: sending <%p> to worker through <%d>\n", getpid(), jsmsg, pipe);
	// sending the SHM pointer of SIP message to the worker
	if(write(pipe, &jsmsg, sizeof(jsmsg)) != sizeof(jsmsg))
	{
		DBG("JABBER: jab_send_message: error when writting to worker pipe!!!\n");
		goto error;
	}
	
	return 1;
error:
	return -1;
}

/**
 * destroy function of module
 */
void destroy(void)
{
	int i;
	DBG("JABBER: Unloading module ...\n");
	if(pipes)
	{
		for(i = 0; i < nrw; i++)
			pkg_free(pipes[i]);
		pkg_free(pipes);
	}
	if(db_con != NULL)
	{
		for(i = 0; i<nrw; i++)
			db_close(db_con[i]);
		shm_free(db_con);
	}
	if(jwl->ssock > 0)
		close(jwl->ssock);
			
	jab_wlist_free(jwl);
	DBG("JABBER: Unloaded\n");
}
