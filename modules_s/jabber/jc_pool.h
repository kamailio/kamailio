/*
 * $Id$
 *
 * JABBER module
 *
 */

#ifndef _JC_POOL_h_
#define _JC_POOL_h_

#include<sys/types.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<netdb.h>
#include<sys/un.h>

#include "sip2jabber.h"
#include "tree234.h"
#include "../../str.h"
#include "db.h"
#include "lock.h"

/**********             ***/
typedef struct _jab_worker
{
	int pid;			// process id
	int pipe;			// communication pipe
	int nr;				// number of jobs
	tree234 *sip_ids;   // sip ids allocated for the worker
} t_jab_worker, *jab_worker;

typedef struct _jab_wlist
{
	int len;   			// length of the list
	int maxj;			// maximum jobs / worker
	int ssock;			// sip response socket
	struct sockaddr_in sserver;  // sip server address
	str *contact_h;	// contact header
    //int semid;          // semaphore for mutual access to the list
    smart_lock *sems;	//
	jab_worker workers; // the list of workers
} t_jab_wlist, *jab_wlist;

/**********             ***/

typedef struct _jab_sipmsg
{
	str *from; // pointer to FROM
	str to;    // destination
	str msg;   // message body
} t_jab_sipmsg, *jab_sipmsg;

/**********             ***/

typedef struct _open_jc
{
	str *id;			// id of connection
	int expire;         // time when the open connection is expired
	int ready;          // time when the connection is ready for sending messages
	jbconnection jbc;   // connection to the Jabber server
} t_open_jc, *open_jc;

/**********             ***/

typedef struct _jmsg_queue
{
	int len;			// maximum size of the queue
	int size;           // numebr of elements in the queue
	int head;           // index of the first element in queue
	int tail;           // index of the last element in queue
	jab_sipmsg *jsm;    // pointer to the message
	open_jc *ojc;       // pointer to the connection which will be used on sending
} t_jmsg_queue, *jmsg_queue;

/**********             ***/

typedef struct _jc_pool
{
	int len;				// maximum len of the pool
	open_jc *ojc;           // the connections to the Jabber
	t_jmsg_queue jmqueue;   // messages queue
} t_jc_pool, *jc_pool;

/**********   LOOK AT IMPLEMENTATION OF FUNCTIONS FOR DESCRIPTION    ***/

jab_wlist jab_wlist_init(int **, int, int);
int jab_wlist_init_ssock(jab_wlist, char *, int);
int jab_wlist_init_contact(jab_wlist, char *);
int jab_wlist_set_pids(jab_wlist, int *, int);
int jab_wlist_get(jab_wlist, str *, str **);
void jab_wlist_del(jab_wlist, str *, int);
void jab_wlist_free(jab_wlist);

int worker_process(jab_wlist, char*, int, int, int, int, int, db_con_t*);

int jab_send_sip_msg(str *, str *, str *, str *);

/**********             ***/

jc_pool jc_pool_init(int, int);
int jc_pool_add(jc_pool, open_jc);
open_jc jc_pool_get(jc_pool, str*);
int jc_pool_del(jc_pool, str*);
void jc_pool_free(jc_pool);
void jc_pool_print(jc_pool);
int jc_pool_add_jmsg(jc_pool, jab_sipmsg, open_jc);
int jc_pool_del_jmsg(jc_pool);

/**********             ***/

open_jc open_jc_create(str*, jbconnection, int);
int open_jc_update(open_jc, int);
void open_jc_free(open_jc);

/**********             ***/

#endif
