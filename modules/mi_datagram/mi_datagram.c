/*
 * $Id: mi_datagram.c 1133 2007-04-02 17:31:13Z ancuta_onofrei $
 *
 * Copyright (C) 2007 Voice Sistem SRL
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * Kamailio is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Kamailio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 *
 * History:
 * ---------
 *  2007-06-25  first version (ancuta)
 */

/*!
 * \file
 * \brief MI_DATAGRAM :: Unix socket and network socket (UDP) API for the Kamailio manager interface
 * \ingroup mi
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
#include "../../lib/kmi/mi.h"
#include "../../ip_addr.h"
#include "../../pt.h"
#include "../../globals.h"
#include "../../cfg/cfg_struct.h"
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


#define MAX_CTIME_LEN 	  128
#define MAX_NB_PORT	  65535

static int mi_mod_init(void);
static int mi_child_init(int rank);
static int mi_destroy(void);
static int pre_datagram_process(void);
static int post_datagram_process(void);
static void datagram_process(int rank);

/* local variables */
static int mi_socket_domain =  AF_LOCAL;
static sockaddr_dtgram mi_dtgram_addr;

/* socket definition parameter */
static char *mi_socket = 0;
int mi_socket_timeout = 2000;
static rx_tx_sockets sockets;

/* unixsock specific parameters */
static int  mi_unix_socket_uid = -1;
static char *mi_unix_socket_uid_s = 0;
static int  mi_unix_socket_gid = -1;
static char *mi_unix_socket_gid_s = 0;
static int mi_unix_socket_mode = S_IRUSR| S_IWUSR| S_IRGRP| S_IWGRP;

/* mi specific parameters */
static char *mi_reply_indent = DEFAULT_MI_REPLY_IDENT;



MODULE_VERSION


static proc_export_t mi_procs[] = {
	{"MI Datagram",  pre_datagram_process,  post_datagram_process,
			datagram_process, MI_CHILD_NO },
	{0,0,0,0,0}
};


static param_export_t mi_params[] = {
	{"children_count",      INT_PARAM,    &mi_procs[0].no           },
	{"socket_name",         PARAM_STRING,    &mi_socket                },
	{"socket_timeout",      INT_PARAM,    &mi_socket_timeout        },
	{"unix_socket_mode",    INT_PARAM,    &mi_unix_socket_mode      },
	{"unix_socket_group",   PARAM_STRING,    &mi_unix_socket_gid_s     },
	{"unix_socket_group",   INT_PARAM,    &mi_unix_socket_gid       },
	{"unix_socket_user",    PARAM_STRING,    &mi_unix_socket_uid_s     },
	{"unix_socket_user",    INT_PARAM,    &mi_unix_socket_uid       },
	{"reply_indent",        PARAM_STRING,    &mi_reply_indent          },
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
	mi_procs,                      /* extra processes */
	mi_mod_init,                   /* module initialization function */
	0,                             /* response handling function */
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
	LM_DBG("testing socket existance...\n");

	if( mi_socket==NULL || *mi_socket == 0) {
		LM_ERR("no DATAGRAM_ socket configured\n");
		return -1;
	}

	LM_DBG("the socket's name/addres is %s\n", mi_socket);

	memset( &mi_dtgram_addr, 0, sizeof(mi_dtgram_addr) );

	if(strncmp(mi_socket, "udp:",4) == 0)
	{
		/*for an UDP socket*/
		LM_DBG("we have an udp socket\n");
		/*separate proto and host */
		p = mi_socket+4;
		if( (*(p)) == '\0') {
			LM_ERR("malformed ip address\n");
			return -1;
		}
		host_s=p;
		LM_DBG("the remaining address after separating the protocol is %s\n",p);

		if( (p = strrchr(p+1, ':')) == 0 ) {
			LM_ERR("no port specified\n");
			return -1;
		}

		/*the address contains a port number*/
		*p = '\0';
		p++;
		port_str.s = p;
		port_str.len = strlen(p);
		LM_DBG("the port string is %s\n", p);
		if(str2int(&port_str, &port_no) != 0 ) {
			LM_ERR("there is not a valid number port\n");
			return -1;
		}
		*p = '\0';
		if (port_no<1024  || port_no>MAX_NB_PORT)
		{
			LM_ERR("invalid port number; must be in [1024,%d]\n",MAX_NB_PORT);
			return -1;
		}
		
		if(! (host = resolvehost(host_s)) ) {
			LM_ERR("failed to resolve %s\n", host_s);
			return -1;
		}
		LM_DBG("the ip is %s\n",host_s);
		if(hostent2su( &(mi_dtgram_addr.udp_addr), host, 0, port_no ) !=0){
			LM_ERR("failed to resolve %s\n", mi_socket);
			return -1;
		}
		mi_socket_domain = host->h_addrtype;
		goto done;
	} 
	/* in case of a Unix socket*/
	LM_DBG("we have an UNIX socket\n");
		
	n=stat(mi_socket, &filestat);
	if( n==0) {
		LM_INFO("the socket %s already exists, trying to delete it...\n", mi_socket);
		if(config_check==0) {
			if (unlink(mi_socket)<0) {
				LM_ERR("cannot delete old socket: %s\n", strerror(errno));
				return -1;
			}
		}
	} else if (n<0 && errno!=ENOENT) {
		LM_ERR("socket stat failed:%s\n", strerror(errno));
		return -1;
	}

	/* check mi_unix_socket_mode */
	if(!mi_unix_socket_mode) {
		LM_WARN("cannot specify mi_unix_socket_mode = 0, forcing it to rw-------\n");
		mi_unix_socket_mode = S_IRUSR| S_IWUSR;
	}
	
	if (mi_unix_socket_uid_s) {
		if (user2uid(&mi_unix_socket_uid, &mi_unix_socket_gid, mi_unix_socket_uid_s)<0) {
			LM_ERR("bad user name %s\n", mi_unix_socket_uid_s);
			return -1;
		}
	}
	
	if (mi_unix_socket_gid_s) {
		if (group2gid(&mi_unix_socket_gid, mi_unix_socket_gid_s)<0) {
			LM_ERR("bad group name %s\n", mi_unix_socket_gid_s);
			return -1;
		}
	}

	/*create the unix socket address*/
	mi_dtgram_addr.unix_addr.sun_family = AF_LOCAL;
	memcpy( mi_dtgram_addr.unix_addr.sun_path, mi_socket, strlen(mi_socket));

done:
	/* add space for extra processes */
	register_procs(mi_procs[0].no);
	/* add child to update local config framework structures */
	cfg_register_child(mi_procs[0].no);

	return 0;
}


static int mi_child_init(int rank)
{
	int i;
	int pid;

	if (rank==PROC_TIMER || rank>0 ) {
		if(mi_datagram_writer_init( DATAGRAM_SOCK_BUF_SIZE , mi_reply_indent )!= 0) {
			LM_CRIT("failed to initiate mi_datagram_writer\n");
			return -1;
		}
	}
	if (rank==PROC_MAIN) {
		if(pre_datagram_process()!=0)
		{
			LM_ERR("pre-fork function failed\n");
			return -1;
		}
		for(i=0; i<mi_procs[0].no; i++)
		{
			pid=fork_process(PROC_NOCHLDINIT, "MI DATAGRAM", 1);
			if (pid<0)
				return -1; /* error */
			if(pid==0) {
				/* child */

				/* initialize the config framework */
				if (cfg_child_init())
					return -1;

				datagram_process(i);
				return 0;
			}
		}
		if(post_datagram_process()!=0)
		{
			LM_ERR("post-fork function failed\n");
			return -1;
		}
	}
	return 0;
}


static int pre_datagram_process(void)
{
	int res;

	/*create the sockets*/
	res = mi_init_datagram_server(&mi_dtgram_addr, mi_socket_domain, &sockets,
					mi_unix_socket_mode, mi_unix_socket_uid, mi_unix_socket_gid);

	if ( res ) {
		LM_CRIT("function mi_init_datagram_server returned with error!!!\n");
		return -1;
	}

	return 0;
}


static void datagram_process(int rank)
{
	LM_INFO("a new child %d/%d\n", rank, getpid());

	/*child's initial settings*/
	if ( init_mi_child(PROC_NOCHLDINIT, 1)!=0) {
		LM_CRIT("failed to init the mi process\n");
		exit(-1);
	}
	if (mi_init_datagram_buffer()!=0) {
		LM_ERR("failed to allocate datagram buffer\n");
		exit(-1);
	}

	if (mi_datagram_writer_init( DATAGRAM_SOCK_BUF_SIZE , mi_reply_indent )!= 0) {
		LM_CRIT("failed to initiate mi_datagram_writer\n");
		exit(-1);
	}

	mi_datagram_server(sockets.rx_sock, sockets.tx_sock);

	exit(-1);
}


static int post_datagram_process(void)
{
	/* close the sockets */
	close(sockets.rx_sock);
	close(sockets.tx_sock);
	return 0;
}


static int mi_destroy(void)
{
	int n;
	struct stat filestat;

	/* destroying the socket descriptors */
	if(mi_socket_domain == AF_UNIX) {
		n=stat(mi_socket, &filestat);
		if (n==0) {
			if(config_check==0) {
				if (unlink(mi_socket)<0){
					LM_ERR("cannot delete the socket (%s): %s\n", 
						mi_socket, strerror(errno));
					goto error;
				}
			}
		} else if (n<0 && errno!=ENOENT) {
			LM_ERR("socket stat failed: %s\n", strerror(errno));
			goto error;
		}
	}

	return 0;
error:
	return -1;

}
