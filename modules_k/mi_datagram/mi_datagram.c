/*
 * $Id: mi_datagram.c 1133 2007-04-02 17:31:13Z ancuta_onofrei $
 *
 * Copyright (C) 2007 Voice Sistem SRL
 *
 * This file is part of openser, a free SIP server.
 *
 * openser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * openser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 *
 * History:
 * ---------
 *  2007-06-25  first version (ancuta)
 */



#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdarg.h>
#include <time.h>
#include <stdio.h>
#include <fcntl.h>


#include "../../sr_module.h"
#include "../../resolve.h"
#include "../../dprint.h"
#include "../../ut.h"
#include "../../mem/mem.h"
#include "../../mem/shm_mem.h"
#include "../../mi/mi.h"
#include "../../ip_addr.h"
#include "mi_datagram.h"
#include "datagram_fnc.h"
#include "mi_datagram_parser.h"
#include "mi_datagram_writer.h"


/* AF_LOCAL is not defined on solaris */

#if !defined(AF_LOCAL)
#define AF_LOCAL AF_UNIX
#endif
#if !defined(PF_LOCAL)
#define PF_LOCAL PF_UNIX
#endif


#define MAX_CTIME_LEN 128
#define MAX_NB_PORT	  65535

static int mi_mod_init(void);
static int mi_child_init(int rank);
static int mi_destroy(void);

/* module-specific worker processes */
static pid_t * mi_socket_pid; /*children's pids*/

/* local variables */
static int mi_socket_domain =  AF_LOCAL;
static sockaddr_dtgram mi_dtgram_addr;

/* socket definition parameter */
static char *mi_socket = 0;
int mi_socket_timeout = 2000;

/* unixsock specific parameters */
static int  mi_unix_socket_uid = -1;
static char *mi_unix_socket_uid_s = 0;
static int  mi_unix_socket_gid = -1;
static char *mi_unix_socket_gid_s = 0;
static int mi_unix_socket_mode = S_IRUSR| S_IWUSR| S_IRGRP| S_IWGRP;

/* mi specific parameters */
static char *mi_reply_indent = DEFAULT_MI_REPLY_IDENT;
static int  mi_children_count  = MI_CHILD_NO;




MODULE_VERSION

static param_export_t mi_params[] = {
	{"children_count",      INT_PARAM,    &mi_children_count        },
	{"socket_name",         STR_PARAM,    &mi_socket                },
	{"socket_timeout",      INT_PARAM,    &mi_socket_timeout        },
	{"unix_socket_mode",    INT_PARAM,    &mi_unix_socket_mode      },
	{"unix_socket_group",   STR_PARAM,    &mi_unix_socket_gid_s     },
	{"unix_socket_group",   INT_PARAM,    &mi_unix_socket_gid       },
	{"unix_socket_user",    STR_PARAM,    &mi_unix_socket_uid_s     },
	{"unix_socket_user",    INT_PARAM,    &mi_unix_socket_uid       },
	{"reply_indent",        STR_PARAM,    &mi_reply_indent          },
	{0,0,0}
};



struct module_exports exports = {
	"mi_datagram",                 /* module name */
	DEFAULT_DLFLAGS,               /* dlopen flags */
	0,                             /* exported functions */
	mi_params,                     /* exported parameters */
	0,                             /* exported statistics */
	0,                             /* exported MI functions */
	0,                             /* exported pseudo-variables */
	0,                             /* extra processes */
	mi_mod_init,                   /* module initialization function */
	(response_function) 0,         /* response handling function */
	(destroy_function) mi_destroy, /* destroy function */
	mi_child_init                  /* per-child init function */
};


static int mi_mod_init(void)
{
	unsigned int port_no;
	int n;
	struct stat filestat;
	struct hostent * host;
	char *p, *host_s;
	str port_str;

	/* checking the mi_socket module param */
	DBG("DBG: mi_datagram: mi_mod_init: testing socket existance ...\n");

	if( mi_socket==NULL || *mi_socket == 0) {
		LOG(L_ERR, "ERROR:mi_datagram: mod_init: no DATAGRAM_ socket "
			"configured\n");
		return -1;
	}

	DBG("DBG:mi_datagram:mi_mod_init: the socket's name/addres is "
			"%s\n", mi_socket);

	memset( &mi_dtgram_addr, 0, sizeof(mi_dtgram_addr) );

	if(strncmp(mi_socket, "udp:",4) == 0)
	{
		/*for an UDP socket*/
		DBG("DBG:mi_datagram:mi_mod_init: we have an udp socket\n");
		/*separate proto and host */
		p = mi_socket+4;
		if( (*(p)) == '\0')
		{
			LOG(L_ERR,"ERROR:mi_datagram:mi_mod_init:malformed ip address\n");
			return -1;
		}
		host_s=p;
		DBG("DBG: mi_datagram:mi_mod_init: the remaining address "
			"after separating the protocol is %s\n",p);

		if( (p = strrchr(p+1, ':')) == 0 )
		{
			LOG(L_ERR,"ERROR:mi_datagram:mi_mod_init: no port specified\n");
			return -1;
		}

		/*the address contains a port number*/
		*p = '\0'; p++;
		port_str.s = p;
		port_str.len = strlen(p);
		DBG("DBG:mi_datagram:mi_mod_init: the port string is %s\n", p);
		if(str2int(&port_str, &port_no) != 0 ){
			LOG(L_ERR, "ERROR: mi_datagram:mi_mod_init: there is not "
				"a valid number port\n");
			return -1;
		}
		*p = '\0';
		if (port_no<1024  || port_no>MAX_NB_PORT)
		{
			LOG(L_ERR, "ERROR: mi_datagram:mi_mod_init: invalid port "
				"number; must be in [1024,%d]\n",MAX_NB_PORT);
			return -1;
		}
		
		if(! (host = resolvehost(host_s, 0)) ){
			LOG(L_ERR, "ERROR: mi_datagram:mi_mod_init failed to resolv %s"
				"\n", host_s);
			return -1;
		}
		DBG("DBG: mi_datagram:mi_init: the ip is %s\n",host_s);
		if(hostent2su( &(mi_dtgram_addr.udp_addr), host, 0, port_no ) !=0){
			LOG(L_ERR, "ERROR: mi_datagram:mi_mod_init failed to resolv %s"
				"\n", mi_socket);
			return -1;
		}
		mi_socket_domain = host->h_addrtype;
	}
	else
	{
		/*in case of a Unix socket*/
		DBG("DBG:mi_datagram:mi_mod_init we have an UNIX socket\n");
		
		n=stat(mi_socket, &filestat);
		if( n==0){
			LOG(L_INFO,"mi_datagram:mi_mid_init : the socket %s already exists"
				"trying to delete it...\n",mi_socket);
			if (unlink(mi_socket)<0){
				LOG(L_ERR, "ERROR: mi_datagram: mi_mod_init: cannot delete "
					"old socket (%s): %s\n", mi_socket, strerror(errno));
				return -1;
			}
		}else if (n<0 && errno!=ENOENT){
			LOG(L_ERR, "ERROR: mi_datagram: mi_mod_init: socket stat failed:"
				"%s\n", strerror(errno));
			return -1;
		}

		/* check mi_unix_socket_mode */
		if(!mi_unix_socket_mode){
			LOG(L_WARN, "WARNING:mi_datagram: mi_mod_init: cannot specify "
				"mi_unix_socket_mode = 0, forcing it to rw-------\n");
			mi_unix_socket_mode = S_IRUSR| S_IWUSR;
		}
	
		if (mi_unix_socket_uid_s){
			if (user2uid(&mi_unix_socket_uid, &mi_unix_socket_gid, 
					mi_unix_socket_uid_s)<0){
				LOG(L_ERR, "ERROR:mi_datagram: mi_mod_init:bad user name %s\n",
					mi_unix_socket_uid_s);
				return -1;
			}
		}
	
		if (mi_unix_socket_gid_s){
			if (group2gid(&mi_unix_socket_gid, mi_unix_socket_gid_s)<0){
				LOG(L_ERR,"ERROR:mi_datagram: mi_mod_init:bad group name %s\n",
					mi_unix_socket_gid_s);
				return -1;
			}
		}

		/*create the unix socket address*/
		mi_dtgram_addr.unix_addr.sun_family = AF_LOCAL;
		memcpy( mi_dtgram_addr.unix_addr.sun_path,
			mi_socket, strlen(mi_socket));
	}

	/* create the shared memory where the mi_socket_pids are kept */
	mi_socket_pid = (pid_t *)shm_malloc(mi_children_count * sizeof(pid_t));
	if(!mi_socket_pid){
		LOG(L_ERR, "ERROR:mi_datagram: mi_mod_init:cannot allocate shared "
			"memory for the mi_socket_pid\n");
		return -1;
	}
	memset(mi_socket_pid, 0, mi_children_count);

	return 0;
}


static int mi_child_init(int rank)
{
	int i, pid, res;
	rx_tx_sockets sockets;

	if (rank==PROC_TIMER || rank>0 ) {
		if(mi_datagram_writer_init( DATAGRAM_SOCK_BUF_SIZE ,
		mi_reply_indent )!= 0){
			LOG(L_CRIT, "CRITICAL:mi_datagram:mi_child_init: failed to "
				"initiate mi_datagram_writer\n");
			return -1;
		}
	}

	if(rank != 1)
		return 0;

	/*create the sockets*/
	res = mi_init_datagram_server(&mi_dtgram_addr, mi_socket_domain, &sockets,
								mi_unix_socket_mode, mi_unix_socket_uid, 
								mi_unix_socket_gid);

	if ( res ) {
		LOG(L_CRIT, "CRITICAL:mi_datagram:mi_child_init: The function "
			"mi_init_datagram_server returned with error!!!\n");
		return -1;
	}

	LOG(L_INFO,"INFO:mi_datagram:mi_child_init: forking %d workers\n",
		mi_children_count);

	for(i = 0;i<mi_children_count; i++)
	{
		pid = fork();

		if (pid < 0){
		/*error*/
			LOG(L_ERR, "ERROR:mi_datagram:mi_child_init: the process cannot "
				"fork!\n");
			return -1;
		}
		else if (pid==0) {
			/*child*/
			LOG(L_INFO,"INFO:mi_datagram:mi_child_init(%d): a new child:"
					"%d\n",rank, getpid());
			/*child's initial settings*/
			if( init_mi_child()!=0) {
				LOG(L_CRIT,"CRITICAL:mi_datagram:mi_child_init: failed to init"
					"the mi process\n");
				exit(-1);
			}
			if(mi_init_datagram_buffer()!=0){
				LOG(L_CRIT,"CRITICAL:mi_datagram:mi_child_init: failed to "
					"allocate datagram buffer\n");
				exit(-1);
			}
			
			mi_datagram_server(sockets.rx_sock, sockets.tx_sock);
			
			return 0;
		}
		/*parent*/
		DBG("DEBUG:mi_datagram:mi_child_init: new process with pid = %d "
				"created.\n",pid);
		/*put the child's pid in the shared memory*/
		mi_socket_pid[i] = pid;
	}
	/*close the parent's sockets*/
	close(sockets.rx_sock);
	close(sockets.tx_sock);

	return 0;

}


static int mi_destroy(void)
{
	int i, n;
	struct stat filestat;

	if(!mi_socket_pid){
		LOG(L_INFO, "INFO:mi_datagram:mi_destroy:memory for the child's "
			"mi_socket_pid was not allocated -> nothing to destroy\n");
		return 0;
	}

	/* killing the children */
	for(i=0;i<mi_children_count; i++)
	
		if (!mi_socket_pid[i]) {
			LOG(L_INFO,"INFO:mi_datagram:mi_destroy: processes %i "
					"hasn't been created -> nothing to kill\n",i);
		} 
		else {
			if (kill( mi_socket_pid[i], SIGKILL)!=0) {
				DBG("DBG:mi_datagram:mi_destroy: trying to kill "
					"the child %i\n",mi_socket_pid[i]);
				if (errno==ESRCH) {
					LOG(L_INFO,"INFO:mi_datagram:mi_destroy: seems that "
							"datagram child is already dead!\n");
				} 
				else {
					LOG(L_ERR,"ERROR:mi_datagram:mi_destroy: killing the "
							"aux. process failed! kill said: %s\n",
							strerror(errno));
					goto error;
				}
			}
			else
			{
				LOG(L_INFO,"INFO:mi_datagram:mi_destroy: datagram child "
						"successfully killed!\n");
			}		
		}

	/* destroying the socket descriptors */
	if(mi_socket_domain == AF_UNIX){
		n=stat(mi_socket, &filestat);
		if (n==0){
			if (unlink(mi_socket)<0){
				LOG(L_ERR, "ERROR: mi_datagram: mi_destroy: cannot delete the "
					"socket (%s): %s\n", mi_socket, strerror(errno));
				goto error;
			}
		} else if (n<0 && errno!=ENOENT) {
			LOG(L_ERR, "ERROR: mi_datagram: mi_destroy: SOCKET stat failed:	"
				"%s\n",	strerror(errno));
			goto error;
		}
	}
	/* freeing the shm shared memory */
	shm_free(mi_socket_pid);

	return 0;
error:
	/* freeing the shm shared memory */
	shm_free(mi_socket_pid);
	return -1;

}


