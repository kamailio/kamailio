/*
 * $Id$
 *
 * UNIX Domain Socket Server
 *
 * Copyright (C) 2001-2004 FhG Fokus
 * Copyright (C) 2005 iptelorg GmbH
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
/* History:
 *              created by janakj
 *  2004-03-03  added tcp init code (andrei)
 *  2004-04-29  added chmod(sock_perm) & chown(sock_user,sock_group)  (andrei)
 */

#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <time.h>
#include <fcntl.h>
#include "../../config.h"
#include "../../ut.h"
#include "../../globals.h"
#include "../../trim.h"
#include "../../pt.h"
#include "../../sr_module.h"
#include "../../mem/mem.h"
#include "unixsock_server.h"
#include "../../tsend.h"


/* AF_LOCAL is not defined on solaris */
#if !defined(AF_LOCAL)
#define AF_LOCAL AF_UNIX
#endif
#if !defined(PF_LOCAL)
#define PF_LOCAL PF_UNIX
#endif


/* solaris doesn't have SUN_LEN */
#ifndef SUN_LEN
#define SUN_LEN(sa)	 ( strlen((sa)->sun_path) + \
					 (size_t)(((struct sockaddr_un*)0)->sun_path) )
#endif


#define UNIXSOCK_BUF_SIZE BUF_SIZE

char* unixsock_name = 0;
char* unixsock_user = 0;
char* unixsock_mode = 0;
char* unixsock_group = 0;
unsigned int unixsock_children = 1;
unsigned int unixsock_tx_timeout = 2000; /* Timeout for sending replies in milliseconds */

static int rx_sock, tx_sock;
static char reply_buf[UNIXSOCK_BUF_SIZE];
static str reply_pos;
static struct sockaddr_un reply_addr;
static unsigned int reply_addr_len;

static int parse_cmd(str* cmd, str* buffer)
{
	return 1;
}


/*
 * Create and bind local socket
 */
int init_unixsock_socket(void)
{
	struct sockaddr_un addr;
	int len, flags;

	if (unixsock_name == 0) {
		DBG("No unix domain socket will be opened\n");
		return 1;
	}

	len = strlen(unixsock_name);
	if (len == 0) {
		DBG("Unix domain socket server disabled\n");
		return 1;
	} else if (len > 107) {
		ERR("Socket name too long\n");
		return -1;
	}

	DBG("Initializing Unix domain socket server @ %s\n", 
	    unixsock_name);

	if (unlink(unixsock_name) == -1) {
		if (errno != ENOENT) {
			ERR(L_ERR, "Error while unlinking "
			    "old socket (%s): %s\n", unixsock_name, strerror(errno));
			return -1;
		}
	}

	rx_sock = socket(PF_LOCAL, SOCK_DGRAM, 0);
	if (rx_sock == -1) {
		ERR("Cannot create listening socket: %s\n", strerror(errno));
		return -1;
	}

	memset(&addr, 0, sizeof(addr));
	addr.sun_family = PF_LOCAL;
	memcpy(addr.sun_path, unixsock_name, len);

	if (bind(rx_sock, (struct sockaddr*)&addr, SUN_LEN(&addr)) == -1) {
		ERR("bind: %s\n", strerror(errno));
		goto err_rx;
	}
	/* try to change permissions */
	if (sock_mode){ /* sock_mode==0 doesn't make sense, nobody can read/write*/
		if (chmod(unixsock_name, sock_mode)<0){
			ERR("Failed to change the"
			    " permissions for %s to %04o: %s[%d]\n",
			    unixsock_name, sock_mode, strerror(errno), errno);
			goto err_rx;
		}
	}
	/* try to change the ownership */
	if ((sock_uid!=-1) || (sock_gid!=-1)){
		if (chown(unixsock_name, sock_uid, sock_gid)<0){
			ERR("Failed to change the"
			    " owner/group for %s  to %d.%d; %s[%d]\n",
			    unixsock_name, sock_uid, sock_gid, strerror(errno), errno);
			goto err_rx;
		}
	}

	tx_sock = socket(PF_LOCAL, SOCK_DGRAM, 0);
	if (tx_sock == -1) {
		ERR("Cannot create sending socket:"
		    " %s\n", strerror(errno));
		goto err_rx;
	}
	
	     /* Turn non-blocking mode on */
	flags = fcntl(tx_sock, F_GETFL);
	if (flags == -1){
		ERR("fcntl failed: %s\n", strerror(errno));
		goto err_both;
	}
		
	if (fcntl(tx_sock, F_SETFL, flags | O_NONBLOCK) == -1) {
		ERR("fcntl: set non-blocking failed: %s\n", strerror(errno));
		goto err_both;
	}
	
	return 1;
 err_both:
	close(tx_sock);
 err_rx:
	close(rx_sock);
	return -1;
}


static void unix_server_loop(void)
{
	int ret;
	str cmd, buffer;
	static char buf[UNIXSOCK_BUF_SIZE];
	struct unixsock_cmd* c = NULL;
	
	while(1) {
		reply_addr_len = sizeof(reply_addr);
		ret = recvfrom(rx_sock, buf, UNIXSOCK_BUF_SIZE, 0, 
			       (struct sockaddr*)&reply_addr, &reply_addr_len);
		if (ret == -1) {
			ERR("recvfrom: (%d) %s\n", errno, strerror(errno));
			if ((errno == EINTR) || 
			    (errno == EAGAIN) || 
			    (errno == EWOULDBLOCK) || 
			    (errno == ECONNREFUSED)) {
				DBG("Got %d (%s), going on\n", errno, strerror(errno));
				continue;
			}
			ERR("BUG: unexpected recvfrom error\n");
			continue;
		}

		buffer.s = buf;
		buffer.len = ret;
		unixsock_reply_reset();

		if (parse_cmd(&cmd, &buffer) < 0) {
			     /*			unixsock_reply_asciiz("400 First line malformed\n"); */
			unixsock_reply_send();
			continue;
		}

		buffer.s = cmd.s + cmd.len + 1;
		buffer.len -= cmd.len + 1 + 1;

		     /*		c = lookup_cmd(&cmd); */
		if (c == 0) {
			ERR("Could not find "
			    "command '%.*s'\n", cmd.len, ZSW(cmd.s));
			     /*			unixsock_reply_printf("500 Command %.*s not found\n", cmd.len, ZSW(cmd.s)); */
			unixsock_reply_send();
			continue;
		}

		     /*		ret = c->f(&buffer); */
		if (ret < 0) {
			ERR("Command '%.*s' failed with "
			    "return value %d\n", cmd.len, ZSW(cmd.s), ret);
			     /* Note that we do not send reply here, the 
			      * function is supposed to do so, it knows the 
			      * reason of the failure better than us
			      */
		}
	}
}


/*
 * Spawn listeners
 */
int init_unixsock_children(void)
{
	int i;
	pid_t pid;

	if (!unixsock_name || *unixsock_name == '\0') {
		return 1;
	}

	for(i = 0; i < unixsock_children; i++) {
		pid = fork_process(PROC_UNIXSOCK,"unix domain socket",1);
		if (pid < 0) {
			ERR("Unable to fork: %s\n", strerror(errno));
			close(rx_sock);
			close(tx_sock);
			return -1;
		} else if (pid == 0) { /* child */
			unix_server_loop(); /* Never returns */
		}
		/* Parent */
	}

	DBG("Unix domain socket server successfully initialized @ %s\n", unixsock_name);
	return 1;
}


/*
 * Clean up
 */
void close_unixsock_server(void)
{
	close(rx_sock);
	close(tx_sock);
}

/*
 * Send a reply
 */
ssize_t unixsock_reply_send(void)
{
	return tsend_dgram(tx_sock, 
			   reply_buf, reply_pos.s - reply_buf,
			   (struct sockaddr*)&reply_addr, reply_addr_len, 
			   unixsock_tx_timeout);
}


/*
 * Send a reply
 */
ssize_t unixsock_reply_sendto(struct sockaddr_un* to)
{
	if (!to) {
	        ERR("Invalid parameter value\n");
		return -1;
	}

	return tsend_dgram(tx_sock, 
			   reply_buf, reply_pos.s - reply_buf, 
			   (struct sockaddr*)to, SUN_LEN(to), 
			   unixsock_tx_timeout);
}


/*
 * Reset the reply buffer -- start to write
 * at the beginning
 */
void unixsock_reply_reset(void)
{
	reply_pos.s = reply_buf;
	reply_pos.len = UNIXSOCK_BUF_SIZE;
}


/*
 * Return the address of the sender
 */
struct sockaddr_un* unixsock_sender_addr(void)
{
	return &reply_addr;
}
