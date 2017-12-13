/**
 * Copyright (C) 2014-2017 Daniel-Constantin Mierla (asipto.com)
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
 */

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "../../core/trim.h"
#include "../../core/pt.h"
#include "../../core/sr_module.h"
#include "../../core/cfg/cfg_struct.h"
#include "../../core/resolve.h"
#include "../../core/ip_addr.h"

#include "jsonrpcs_mod.h"

/* DATAGRAM TRANSPORT */

typedef union{
	union sockaddr_union udp_addr;
	struct sockaddr_un   unix_addr;
} jsonrpc_dgram_sockaddr_t;

/* three types of sockaddr: UNIX, IPv4 and IPv6 */
typedef union{
	struct sockaddr_un  unix_v0;
	struct sockaddr_in  inet_v4;
	struct sockaddr_in6 inet_v6;
} jsonrpc_dgram_sock_t;

typedef struct{
	jsonrpc_dgram_sock_t sock;
	unsigned int domain;
	int address_len;
} jsonrpc_dgram_address_t;

typedef struct jsonrpc_dgram_rx_tx {
	int rx_sock;
} jsonrpc_dgram_rx_tx_t;

/* dgram variables */
static int jsonrpc_dgram_socket_domain =  AF_LOCAL;
static jsonrpc_dgram_sockaddr_t jsonrpc_dgram_addr;

/* dgram socket definition parameter */
static jsonrpc_dgram_rx_tx_t jsonrpc_dgram_sockets = { -1 };

/* dgram unixsock specific parameters */
char *jsonrpc_dgram_socket = NAME "_rpc.sock";
int jsonrpc_dgram_workers = 1;
int jsonrpc_dgram_timeout = 2000;
int  jsonrpc_dgram_unix_socket_uid = -1;
char *jsonrpc_dgram_unix_socket_uid_s = 0;
int  jsonrpc_dgram_unix_socket_gid = -1;
char *jsonrpc_dgram_unix_socket_gid_s = 0;
int jsonrpc_dgram_unix_socket_mode = S_IRUSR| S_IWUSR| S_IRGRP| S_IWGRP;

/* dgram function prototypes */
int jsonrpc_dgram_mod_init(void);
int jsonrpc_dgram_child_init(int rank);
int jsonrpc_dgram_destroy(void);


void jsonrpc_dgram_server(int rx_sock);
static int jsonrpc_dgram_init_socks(void);
static int jsonrpc_dgram_post_process(void);

int jsonrpc_dgram_mod_init(void)
{
	unsigned int port_no;
	int n;
	struct stat filestat;
	struct hostent *host;
	char *p, *host_s;
	str port_str;
	int len;
	int sep;

	/* checking the mi_socket module param */
	LM_DBG("testing socket existance...\n");

	if( jsonrpc_dgram_socket==NULL || *jsonrpc_dgram_socket == 0) {
		LM_ERR("no DATAGRAM_ socket configured\n");
		return -1;
	}

	LM_DBG("the socket's name/address is %s\n", jsonrpc_dgram_socket);

	memset( &jsonrpc_dgram_addr, 0, sizeof(jsonrpc_dgram_sockaddr_t) );

	if(strlen(jsonrpc_dgram_socket)<6) {
		LM_ERR("lenght of socket address is too short: %s\n",
				jsonrpc_dgram_socket);
		return -1;
	}
	if(strncmp(jsonrpc_dgram_socket, "udp:", 4) == 0) {
		/*for an UDP socket*/
		LM_DBG("udp socket provided\n");
		/*separate proto and host */
		p = jsonrpc_dgram_socket+4;
		if( (*(p)) == '\0') {
			LM_ERR("malformed ip address\n");
			return -1;
		}
		host_s=p;
		LM_DBG("remaining address after separating the protocol is %s\n", p);

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
		if (port_no<1024  || port_no>65535)
		{
			LM_ERR("invalid port number; must be in [1024,65535]\n");
			return -1;
		}

		if(! (host = resolvehost(host_s)) ) {
			LM_ERR("failed to resolve %s\n", host_s);
			return -1;
		}
		LM_DBG("the ip is %s\n",host_s);
		if(hostent2su( &(jsonrpc_dgram_addr.udp_addr), host, 0, port_no ) !=0) {
			LM_ERR("failed to resolve %s\n", jsonrpc_dgram_socket);
			return -1;
		}
		jsonrpc_dgram_socket_domain = host->h_addrtype;
		goto done;
	}
	/* in case of a Unix socket*/
	LM_DBG("UNIX socket provided\n");

	if(*jsonrpc_dgram_socket != '/') {
		if(runtime_dir!=NULL && *runtime_dir!=0) {
			len = strlen(runtime_dir);
			sep = 0;
			if(runtime_dir[len-1]!='/') {
				sep = 1;
			}
			len += sep + strlen(jsonrpc_dgram_socket);
			p = pkg_malloc(len + 1);
			if(p==NULL) {
				LM_ERR("no more pkg\n");
				return -1;
			}
			strcpy(p, runtime_dir);
			if(sep) strcat(p, "/");
			strcat(p, jsonrpc_dgram_socket);
			jsonrpc_dgram_socket = p;
			LM_DBG("unix socket path is [%s]\n", jsonrpc_dgram_socket);
		}
	}

	n=stat(jsonrpc_dgram_socket, &filestat);
	if( n==0) {
		LM_INFO("the socket %s already exists, trying to delete it...\n",
				jsonrpc_dgram_socket);
		if(config_check==0) {
			if (unlink(jsonrpc_dgram_socket)<0) {
				LM_ERR("cannot delete old socket: %s\n", strerror(errno));
				return -1;
			}
		}
	} else if (n<0 && errno!=ENOENT) {
		LM_ERR("socket stat failed:%s\n", strerror(errno));
		return -1;
	}

	/* check mi_unix_socket_mode */
	if(!jsonrpc_dgram_unix_socket_mode) {
		LM_WARN("cannot specify jsonrpc_dgram_unix_socket_mode = 0,"
				" forcing it to rw-------\n");
		jsonrpc_dgram_unix_socket_mode = S_IRUSR| S_IWUSR;
	}

	if (jsonrpc_dgram_unix_socket_uid_s) {
		if (user2uid(&jsonrpc_dgram_unix_socket_uid,
					&jsonrpc_dgram_unix_socket_gid,
					jsonrpc_dgram_unix_socket_uid_s)<0) {
			LM_ERR("bad user name %s\n", jsonrpc_dgram_unix_socket_uid_s);
			return -1;
		}
	}

	if (jsonrpc_dgram_unix_socket_gid_s) {
		if (group2gid(&jsonrpc_dgram_unix_socket_gid,
					jsonrpc_dgram_unix_socket_gid_s)<0) {
			LM_ERR("bad group name %s\n", jsonrpc_dgram_unix_socket_gid_s);
			return -1;
		}
	}

	/*create the unix socket address*/
	jsonrpc_dgram_addr.unix_addr.sun_family = AF_LOCAL;
	if(strlen(jsonrpc_dgram_socket)
			>= sizeof(jsonrpc_dgram_addr.unix_addr.sun_path)-1) {
		LM_ERR("socket path is too long\n");
		return -1;
	}
	memcpy(jsonrpc_dgram_addr.unix_addr.sun_path,
			jsonrpc_dgram_socket, strlen(jsonrpc_dgram_socket));

done:
	if(jsonrpc_dgram_init_socks()!=0) {
		LM_ERR("init datagram sockets function failed\n");
		return -1;
	}

	/* add space for extra processes */
	register_procs(jsonrpc_dgram_workers);
	/* add child to update local config framework structures */
	cfg_register_child(jsonrpc_dgram_workers);

	return 0;
}

#define JSONRPC_DGRAM_BUF_SIZE	65456
static unsigned int jsonrpc_dgram_write_buffer_len = JSONRPC_DGRAM_BUF_SIZE;

/*! \brief reply socket security checks:
 * checks if fd is a socket, is not hardlinked and it's not a softlink
 * opened file descriptor + file name (for soft link check)
 * \return 0 if ok, <0 if not */
int jsonrpc_dgram_sock_check(int fd, char* fname)
{
	struct stat fst;
	struct stat lst;

	if (fstat(fd, &fst)<0){
		LM_ERR("fstat failed: %s\n",
		strerror(errno));
		return -1;
	}
	/* check if socket */
	if (!S_ISSOCK(fst.st_mode)){
		LM_ERR("%s is not a sock\n", fname);
		return -1;
	}
	/* check if hard-linked */
	if (fst.st_nlink>1){
		LM_ERR("security: %s is hard-linked %d times\n",
				fname, (unsigned)fst.st_nlink);
		return -1;
	}

	/* lstat to check for soft links */
	if (lstat(fname, &lst)<0){
		LM_ERR("lstat failed: %s\n", strerror(errno));
		return -1;
	}
	if (S_ISLNK(lst.st_mode)){
		LM_ERR("security: %s is a soft link\n", fname);
		return -1;
	}
	/* success */
	return 0;
}

#define jsonrpc_dgram_create_reply_socket(_socketfd,_socket_domain, _err) \
	_socketfd = socket(_socket_domain, SOCK_DGRAM, 0);\
	if (_socketfd == -1) {\
		LM_ERR("cannot create socket: %s\n", strerror(errno));\
		goto _err;\
	}\
	/* Turn non-blocking mode on for tx*/\
	flags = fcntl(_socketfd, F_GETFL);\
	if (flags == -1){\
		LM_ERR("fcntl failed: %s\n", strerror(errno));\
		goto _err;\
	}\
	if (fcntl(_socketfd, F_SETFL, flags | O_NONBLOCK) == -1) {\
		LM_ERR("fcntl: set non-blocking failed: %s\n", strerror(errno));\
		goto _err;\
	}

int jsonrpc_dgram_init_server(jsonrpc_dgram_sockaddr_t *addr,
		unsigned int socket_domain,
		jsonrpc_dgram_rx_tx_t *socks,
		int mode, int uid, int gid )
{
	char *socket_name;
	int flags;
	int optval;

	/* create sockets for rx and tx ... */
	jsonrpc_dgram_socket_domain = socket_domain;

	socks->rx_sock = socket(socket_domain, SOCK_DGRAM, 0);
	if (socks->rx_sock == -1) {
		LM_ERR("cannot create RX socket: %s\n", strerror(errno));
		return -1;
	}

	switch(socket_domain)
	{
	case AF_LOCAL:
			LM_DBG("we have a unix socket: %s\n", addr->unix_addr.sun_path);
			socket_name = addr->unix_addr.sun_path;
			if(bind(socks->rx_sock,(struct sockaddr*)&addr->unix_addr,
						SUN_LEN(&addr->unix_addr))< 0) {
				LM_ERR("bind: %s\n", strerror(errno));
				goto err_rx;
			}
			if(jsonrpc_dgram_sock_check(socks->rx_sock, socket_name)!=0)
				goto err_rx;
			/* change permissions */
			if (mode){
				if (chmod(socket_name, mode)<0){
					LM_ERR("failed to change the permissions for %s to %04o:"
						"%s[%d]\n",socket_name, mode, strerror(errno), errno);
					goto err_rx;
				}
			}
			/* change ownership */
			if ((uid!=-1) || (gid!=-1)){
				if (chown(socket_name, uid, gid)<0){
					LM_ERR("failed to change the owner/group for %s  to %d.%d;"
					"%s[%d]\n", socket_name, uid, gid, strerror(errno), errno);
					goto err_rx;
				}
			}
			break;

	case AF_INET:
			if (bind(socks->rx_sock, &addr->udp_addr.s,
						sockaddru_len(addr->udp_addr))< 0) {
				LM_ERR("bind: %s\n", strerror(errno));
				goto err_rx;
			}
			break;
	case AF_INET6:
			if(bind(socks->rx_sock, (struct sockaddr*)&addr->udp_addr.sin6,
						sizeof(addr->udp_addr)) < 0) {
				LM_ERR("bind: %s\n", strerror(errno));
				goto err_rx;
			}
			break;
	default:
			LM_ERR("domain not supported\n");
			goto err_rx;

	}

	optval = 64 * 1024;
	if (setsockopt(socks->rx_sock, SOL_SOCKET, SO_SNDBUF,
					(void*)&optval, sizeof(optval)) ==-1){
		LM_ERR("failed to increse send buffer size via setsockopt "
				" SO_SNDBUF (%d) - %d: %s\n", optval,
				errno, strerror(errno));
		/* continue, non-critical */
	}

	/* Turn non-blocking mode on for tx*/
	flags = fcntl(socks->rx_sock, F_GETFL);
	if (flags == -1){
		LM_ERR("fcntl failed: %s\n", strerror(errno));
		goto err_rx;
	}
	if (fcntl(socks->rx_sock, F_SETFL, flags | O_NONBLOCK) == -1) {
		LM_ERR("fcntl: set non-blocking failed: %s\n", strerror(errno));
		goto err_rx;
	}
	return 0;

err_rx:
	if(socks->rx_sock>=0) close(socks->rx_sock);
	return -1;
}


static int jsonrpc_dgram_init_socks(void)
{
	int res;

	/*create the sockets*/
	res = jsonrpc_dgram_init_server(&jsonrpc_dgram_addr,
			jsonrpc_dgram_socket_domain, &jsonrpc_dgram_sockets,
			jsonrpc_dgram_unix_socket_mode, jsonrpc_dgram_unix_socket_uid,
			jsonrpc_dgram_unix_socket_gid);

	if ( res ) {
		LM_CRIT("initializing datagram server function returned error\n");
		return -1;
	}

	return 0;
}

static char *jsonrpc_dgram_buf = NULL;
int jsonrpc_dgram_init_buffer(void) {

	jsonrpc_dgram_buf = pkg_malloc(JSONRPC_DGRAM_BUF_SIZE);
	if (jsonrpc_dgram_buf==NULL) {
		LM_ERR("no more pkg memory\n");
		return -1;
	}
	return 0;
}

static void jsonrpc_dgram_process(int rank)
{
	LM_INFO("a new child %d/%d\n", rank, getpid());

	if (jsonrpc_dgram_init_buffer()!=0) {
		LM_ERR("failed to allocate datagram buffer\n");
		exit(-1);
	}

	jsonrpc_dgram_write_buffer_len = JSONRPC_DGRAM_BUF_SIZE ;

	jsonrpc_dgram_server(jsonrpc_dgram_sockets.rx_sock);

	exit(-1);
}


int jsonrpc_dgram_child_init(int rank)
{
	int i;
	int pid;

	if (rank==PROC_MAIN) {
		for(i=0; i<jsonrpc_dgram_workers; i++) {
			pid=fork_process(PROC_RPC, "JSONRPCS DATAGRAM", 1);
			if (pid<0)
				return -1; /* error */
			if(pid==0) {
				/* child */

				/* initialize the config framework */
				if (cfg_child_init())
					return -1;

				jsonrpc_dgram_process(i);
				return 0;
			}
		}
		if(jsonrpc_dgram_post_process()!=0) {
			LM_ERR("post-fork function failed\n");
			return -1;
		}
	}
	return 0;
}


static int jsonrpc_dgram_post_process(void)
{
	/* close the sockets */
	if(jsonrpc_dgram_sockets.rx_sock>=0) close(jsonrpc_dgram_sockets.rx_sock);
	return 0;
}


int jsonrpc_dgram_destroy(void)
{
	int n;
	struct stat filestat;

	if(jsonrpc_dgram_socket==NULL) {
		return 0;
	}
	/* destroying the socket descriptors */
	if(jsonrpc_dgram_socket_domain == AF_UNIX) {
		n=stat(jsonrpc_dgram_socket, &filestat);
		if (n==0) {
			if(config_check==0) {
				if (unlink(jsonrpc_dgram_socket)<0){
					LM_ERR("cannot delete the socket (%s): %s\n",
						jsonrpc_dgram_socket, strerror(errno));
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

static union {
	struct sockaddr_un un;
	struct sockaddr_in in;
} jsonrpc_dgram_reply_addr;
static unsigned int jsonrpc_dgram_reply_addr_len;

/* this function sends the reply over the reply socket */
static int jsonrpc_dgram_send_data(int fd, char* buf, unsigned int len,
				const struct sockaddr* to, int tolen, int timeout)
{
	int n;
	unsigned int optlen = sizeof(int);
	int optval = 0;

	if(len == 0 || tolen ==0)
		return -1;

	/*LM_DBG("destination address length is %i\n", tolen);*/
	n=sendto(fd, buf, len, 0, to, tolen);
	if(n!=len) {
		if(getsockopt(fd, SOL_SOCKET, SO_SNDBUF, (int*)&optval, &optlen)==-1) {
			LM_ERR("getsockopt failed\n");
		}
		LM_ERR("failed to send the response - ret: %d, len: %d (%d),"
				" err: %d - %s)\n",
				n, len, optval, errno, strerror(errno));
		return n;
	}
	LM_DBG("rpc response sent out\n");
	return n;
}

void jsonrpc_dgram_server(int rx_sock)
{
	int ret;
	str scmd;
	jsonrpc_plain_reply_t* jr = NULL;
	fd_set readfds;
	int n;

	ret = 0;

	while(1) { /*read the datagram*/
		/* update the local config framework structures */
		cfg_update();
		memset(jsonrpc_dgram_buf, 0, JSONRPC_DGRAM_BUF_SIZE);
		jsonrpc_dgram_reply_addr_len = sizeof(jsonrpc_dgram_reply_addr);

		FD_ZERO(&readfds);
		FD_SET(rx_sock, &readfds);
		n = select(rx_sock+1, &readfds, 0, 0, 0);
		if(n < 0) {
			LM_ERR("failure in select: (%d) %s\n", errno, strerror(errno));
			continue;
		}
		if(!FD_ISSET(rx_sock, &readfds)) {
			/* no data on udp socket */
			continue;
		}

		/* get the client's address */
		ret = recvfrom(rx_sock, jsonrpc_dgram_buf, JSONRPC_DGRAM_BUF_SIZE, 0,
				(struct sockaddr*)&jsonrpc_dgram_reply_addr,
				&jsonrpc_dgram_reply_addr_len);

		if (ret == -1) {
			LM_ERR("recvfrom: (%d) %s\n", errno, strerror(errno));
			if ((errno == EINTR) ||
				(errno == EAGAIN) ||
				(errno == EWOULDBLOCK) ||
				(errno == ECONNREFUSED)) {
				LM_DBG("got %d (%s), going on\n", errno, strerror(errno));
				continue;
			}
			LM_DBG("error in recvfrom\n");
			continue;
		}

		if(ret == 0)
			continue;

		LM_DBG("received %.*s\n", ret, jsonrpc_dgram_buf);

		if(ret> JSONRPC_DGRAM_BUF_SIZE) {
			LM_ERR("buffer overflow\n");
			continue;
		}

		scmd.s = jsonrpc_dgram_buf;
		scmd.len = ret;
		trim(&scmd);

		LM_DBG("buf is %s and we have received %i bytes\n",
				scmd.s, scmd.len);
		if(jsonrpc_exec_ex(&scmd, NULL)<0) {
			LM_ERR("failed to execute the json document from datagram\n");
			continue;
		}

		jr = jsonrpc_plain_reply_get();
		LM_DBG("command executed - result: [%d] [%p] [%.*s]\n",
				jr->rcode, jr->rbody.s,
				jr->rbody.len, jr->rbody.s);

		jsonrpc_dgram_send_data(rx_sock, jr->rbody.s, jr->rbody.len,
						  (struct sockaddr*)&jsonrpc_dgram_reply_addr,
						  jsonrpc_dgram_reply_addr_len,
						  jsonrpc_dgram_timeout);
	}
}
