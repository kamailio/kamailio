/*
 * $Id$
 *
 * UNIX Domain Socket Server
 *
 * Copyright (C) 2001-2004 Fhg Fokus
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
 */
/* History:
 *              created by janakj
 *  2004-03-03  added tcp init code (andrei)
 */

#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <signal.h>
#include <stdarg.h>
#include <time.h>
#include <fcntl.h>
#include "config.h"
#include "ut.h"
#include "globals.h"
#include "trim.h"
#include "pt.h"
#include "sr_module.h"
#include "mem/mem.h"
#include "fifo_server.h" /* CMD_SEPARATOR */
#include "unixsock_server.h"
#include "tsend.h"

#define UNIXSOCK_BUF_SIZE BUF_SIZE

char* unixsock_name = 0;
int unixsock_children = 1;

static int rx_sock, tx_sock;
static struct unixsock_cmd* cmd_list;
static char reply_buf[UNIXSOCK_BUF_SIZE];
static str reply_pos;
static struct sockaddr_un reply_addr;
static int reply_addr_len;

static time_t up_since;
static char up_since_ctime[MAX_CTIME_LEN];


#define PRINT_CMD "print"     /* Diagnostic command */
#define VERSION_CMD "version" /* Print the version of the server */
#define UPTIME_CMD "uptime"   /* Print server's uptime */
#define WHICH_CMD "which"     /* Print available FIFO commands */
#define PS_CMD "ps"           /* Print server's process table */
#define ARG_CMD "arg"         /* Print server's command line arguments */
#define PWD_CMD "pwd"         /* Get the current working directory */
#define KILL_CMD "kill"       /* Kill the server */


/* 
 * Diagnostic and hello-world command 
 */
static int print_cmd(str* msg)
{
	str line;
	int ret;

	ret = 0;

	if (unixsock_read_line(&line, msg) < 0) {
		unixsock_reply_asciiz("500 Error while reading text\n");
		ret = -1;
		goto end;
	}

	if (unixsock_reply_printf("200 OK\n%.*s\n", line.len, ZSW(line.s)) < 0) {
		unixsock_reply_reset();
		unixsock_reply_asciiz("500 Error while sending reply\n");
		ret = -1;
	}

 end:
	if (unixsock_reply_send() < 0) ret = -1;
	return ret;
}


/*
 * Print the version of the server
 */
static int version_cmd(str* msg)
{
	int ret;

	ret = 0;
	if (unixsock_reply_asciiz("200 OK\n" SERVER_HDR CRLF) < 0) ret = -1;
	if (unixsock_reply_send() < 0) ret = -1;
	return ret;
}


static int uptime_cmd(str* msg)
{
	time_t now;
	int ret;

	time(&now);
	ret = 0;
	
	if (unixsock_reply_printf("200 OK\nNow: %sUp Since: %sUp time: %.0f [sec]\n",
				  ctime(&now), up_since_ctime, difftime(now, up_since)) < 0) {
		unixsock_reply_reset();
		unixsock_reply_asciiz("500 Error while printing reply\n");
		ret = -1;
	}
	
	if (unixsock_reply_send() < 0) {
		ret = -1;
	}
	
	return ret;
}


static int which_cmd(str* msg)
{
	struct unixsock_cmd* c;
	int ret;

	ret = 0;
	unixsock_reply_asciiz("200 OK\n");

	for(c = cmd_list; c; c = c->next) {
		if (unixsock_reply_printf("%s\n", c->name) < 0) {
			unixsock_reply_reset();
			unixsock_reply_asciiz("500 Error while creating reply\n");
			ret = -1;
			break;
		}
	}
	
	if (unixsock_reply_send() < 0) {
		ret = -1;
	}
	return ret;
}


static int ps_cmd(str* msg)
{
	int p, ret;

	ret = 0;
	unixsock_reply_asciiz("200 OK\n");
	for (p = 0; p < process_count(); p++) {
		if (unixsock_reply_printf("%d\t%d\t%s\n", p, pt[p].pid, pt[p].desc) < 0) {
			unixsock_reply_reset();
			unixsock_reply_asciiz("500 Error while printing reply\n");
			ret = -1;
			break;
		}
	}
	
	if (unixsock_reply_send() < 0) {
		ret = -1;
	}
	return ret;
}


static int pwd_cmd(str* msg)
{
	char *cwd_buf;
	int max_len, ret;

	max_len = pathmax();
	cwd_buf = pkg_malloc(max_len);
	ret = 0;
	if (!cwd_buf) {
		LOG(L_ERR, "pwd_cmd: No memory left\n");
		unixsock_reply_asciiz("500 No Memory Left\n");
		ret = -1;
	}

	if (getcwd(cwd_buf, max_len)) {
		if (unixsock_reply_printf("200 OK\n%s\n", cwd_buf) < 0) {
			unixsock_reply_reset();
			unixsock_reply_asciiz("500 Error while sending reply\n");
			ret = -1;
		}
	} else {
		unixsock_reply_asciiz("500 getcwd Failed\n");
		ret = -1;
	}

	pkg_free(cwd_buf);
	if (unixsock_reply_send() < 0) {
		ret = -1;
	}
	return ret;
}


static int arg_cmd(str* msg)
{
	int p, ret;

	ret = 0;
	unixsock_reply_asciiz("200 OK\n");
	for (p = 0; p < my_argc; p++) {
		if (unixsock_reply_printf("%s\n", my_argv[p]) < 0) {
			unixsock_reply_reset();
			unixsock_reply_asciiz("500 Could not create reply\n");
			ret = -1;
			break;
		}
	}
			
	if (unixsock_reply_send() < 0) {
		ret = -1;
	}
	return ret;
}


static int kill_cmd(str* msg)
{
	unixsock_reply_asciiz("200 Killing now\n");
	unixsock_reply_send();
	kill(0, SIGTERM);
	return 0;
}


static int register_core_commands(void)
{
	if (unixsock_register_cmd(PRINT_CMD, print_cmd) < 0) {
		return -1;
	}

	if (unixsock_register_cmd(VERSION_CMD, version_cmd) < 0) {
		return -1;
	}

	if (unixsock_register_cmd(UPTIME_CMD, uptime_cmd) < 0) {
		return -1;
	}

	if (unixsock_register_cmd(WHICH_CMD, which_cmd) < 0) {
		return -1;
	}

	if (unixsock_register_cmd(PS_CMD, ps_cmd) < 0) {
		return -1;
	}

	if (unixsock_register_cmd(PWD_CMD, pwd_cmd) < 0) {
		return -1;
	}

	if (unixsock_register_cmd(ARG_CMD, arg_cmd) < 0) {
		return -1;
	}

	if (unixsock_register_cmd(KILL_CMD, kill_cmd) < 0) {
		return -1;
	}
	return 0;
}


/*
 * Create and bind local socket
 */
static int create_unix_socket(char* name)
{
	struct sockaddr_un addr;
	int len, flags;

	if (name == 0) {
		DBG("create_unix_socket: No unix domain socket"
		    " will be opened\n");
		return 1;
	}

	len = strlen(name);
	if (len == 0) {
		DBG("create_unix_socket: Unix domain socket server disabled\n");
		return 1;
	} else if (len > 107) {
		LOG(L_ERR, "create_unix_socket: Socket name too long\n");
		return -1;
	}

	DBG("create_unix_socket: Initializing Unix domain socket server\n");

	if (unlink(name) == -1) {
		if (errno != ENOENT) {
			LOG(L_ERR, "create_unix_socket: Error while unlinking "
			    "old socket (%s): %s\n", name, strerror(errno));
			return -1;
		}
	}

	rx_sock = socket(PF_LOCAL, SOCK_DGRAM, 0);
	if (rx_sock == -1) {
		LOG(L_ERR, "create_unix_socket: Cannot create RX socket: %s\n", 
		    strerror(errno));
		return -1;
	}

	memset(&addr, 0, sizeof(addr));
	addr.sun_family = PF_LOCAL;
	memcpy(addr.sun_path, name, len);

	if (bind(rx_sock, (struct sockaddr*)&addr, SUN_LEN(&addr)) == -1) {
		LOG(L_ERR, "create_unix_socket: bind: %s\n", strerror(errno));
		goto err_rx;
	}

	tx_sock = socket(PF_LOCAL, SOCK_DGRAM, 0);
	if (tx_sock == -1) {
		LOG(L_ERR, "create_unix_socket: Cannot create TX socket: %s\n",
		    strerror(errno));
		goto err_rx;
	}

	     /* Turn non-blocking mode on */
	flags = fcntl(tx_sock, F_GETFL);
	if (flags == -1){
		LOG(L_ERR, "create_unix_socket: fcntl failed: %s\n",
		    strerror(errno));
		goto err_both;
	}
		
	if (fcntl(tx_sock, F_SETFL, flags | O_NONBLOCK) == -1) {
		LOG(L_ERR, "create_unix_socket: fcntl: set non-blocking failed:"
		    " %s\n", strerror(errno));
		goto err_both;
	}
	
	return 0;
 err_both:
	close(tx_sock);
 err_rx:
	close(rx_sock);
	return -1;
}


static struct unixsock_cmd* lookup_cmd(str* cmd)
{
	struct unixsock_cmd* c;

	for(c = cmd_list; c; c = c->next) {
		if ((cmd->len == c->name.len) &&
		    (strncasecmp(c->name.s, cmd->s, cmd->len) == 0)) {
			return c;
		}
	}
	return 0;
}


static int parse_cmd(str* res, str* buffer)
{
	char* cmd_end;

	if (!res || !buffer) {
		LOG(L_ERR, "parse_cmd: Invalid parameter value\n");
		return -1;
	}

	if (buffer->len < 3) {
		LOG(L_ERR, "parse_cmd: Message too short\n");
		return -1;
	}

	if (buffer->s[0] != CMD_SEPARATOR) {
		LOG(L_ERR, "parse_cmd: Command must start with %c\n", 
		    CMD_SEPARATOR);
		return -1;
	}
	
	cmd_end = q_memchr(buffer->s + 1, CMD_SEPARATOR, buffer->len - 1);
	if (!cmd_end) {
		LOG(L_ERR, "parse_cmd: Closing '%c' missing\n", CMD_SEPARATOR);
		return -1;
	}

	res->s = buffer->s + 1;
	res->len = cmd_end - res->s;
	return 0;
} 


static void skip_line(str* buffer)
{
	if (!buffer) return;

	     /* Find \n */
	while (buffer->len && (buffer->s[0] != '\n')) {
		buffer->s++;
		buffer->len--;
	}

	if (buffer->len) {
		buffer->s++;
		buffer->len--;
	}

	     /* Skip CR following LF */
	while (buffer->len && (buffer->s[0] == '\r')) {
		buffer->s++;
		buffer->len--;
	}
}


static void unix_server_loop(void)
{
	int ret;
	str cmd, buffer;
	static char buf[UNIXSOCK_BUF_SIZE];
	struct unixsock_cmd* c;

	buffer.s = buf;
	buffer.len = 0;
	
	while(1) {
		reply_addr_len = sizeof(reply_addr);
		ret = recvfrom(rx_sock, buffer.s, UNIXSOCK_BUF_SIZE, 0, 
			       (struct sockaddr*)&reply_addr, &reply_addr_len);
		if (ret == -1) {
			LOG(L_ERR, "unix_server_loop: recvfrom: (%d) %s\n", 
			    errno, strerror(errno));
			if ((errno == EINTR) || 
			    (errno == EAGAIN) || 
			    (errno == EWOULDBLOCK) || 
			    (errno == ECONNREFUSED)) {
				DBG("unix_server_loop: Got %d (%s), going on\n",
				    errno, strerror(errno));
				continue;
			}
		}

		buffer.len = ret;
		unixsock_reply_reset();

		if (parse_cmd(&cmd, &buffer) < 0) {
			unixsock_reply_asciiz("400 First line malformed\n");
			unixsock_reply_send();
			continue;
		}

		buffer.s = cmd.s + cmd.len + 1;
		buffer.len -= cmd.len + 1 + 1;
		skip_line(&buffer); /* Skip the reply filename */

		c = lookup_cmd(&cmd);
		if (c == 0) {
			LOG(L_ERR, "unix_server_loop: Could not find "
			    "command '%.*s'\n", cmd.len, ZSW(cmd.s));
			unixsock_reply_printf("500 Command %.*s not found\n", cmd.len, ZSW(cmd.s));
			unixsock_reply_send();
			continue;
		}

		ret = c->f(&buffer);
		if (ret < 0) {
			LOG(L_ERR, "unix_server_loop: Command '%.*s' failed with "
			    "return value %d\n", cmd.len, ZSW(cmd.s), ret);
			     /* Note that we do not send reply here, the 
			      * function is supposed to do so, it knows the 
			      * reason of the failure better than us
			      */
		}
	}
}


static int get_uptime(void)
{
	char* t;

	time(&up_since);
	t = ctime(&up_since);
	if (strlen(t) + 1 >= MAX_CTIME_LEN) {
		LOG(L_ERR, "get_uptime: Too long date %d\n", (int)strlen(t));
		return -1;
	}
	memcpy(up_since_ctime, t, strlen(t) + 1);
	return 0;
}


/*
 * Initialize Unix domain socket server
 */
int init_unixsock_server(void)
{
	int ret, i;
	pid_t pid;
#ifdef USE_TCP
	int sockfd[2];
#endif
	
	
	ret = create_unix_socket(unixsock_name);
	if (ret < 0) {
		LOG(L_ERR, "init_unixsock_server: Error while creating "
		    "local socket\n");
		return -1;
	} else if (ret > 0) {
		return 1;
	}

	if (get_uptime() < 0) {
		return -1;
	}

        if (register_core_commands() < 0) {
		close(rx_sock);
		close(tx_sock);
		return -1;
	}

	for(i = 0; i < unixsock_children; i++) {
		process_no++;
#ifdef USE_TCP
		if(!tcp_disable){
 			if (socketpair(AF_UNIX, SOCK_STREAM, 0, sockfd)<0){
				LOG(L_ERR, "ERROR: init_unixsock_server: socketpair"
						" failed: %s\n", strerror(errno));
				return -1;
			}
		}
#endif
		pid = fork();
		if (pid < 0) {
			LOG(L_ERR, "init_unixsock_server: Unable to fork: %s\n",
			    strerror(errno));
			close(rx_sock);
			close(tx_sock);
			return -1;
		} else if (pid == 0) { /* child */
#ifdef USE_TCP
			if (!tcp_disable){
				close(sockfd[0]);
				unix_tcp_sock=sockfd[1];
			}
#endif
			if (init_child(PROC_UNIXSOCK) < 0) {
				LOG(L_ERR, "init_unixsock_server: Error in "
				    "init_child\n");
				close(rx_sock);
				close(tx_sock);
				return -1;
			}

			unix_server_loop(); /* Never returns */
		}

		     /* Parent */
		pt[process_no].pid = pid;
		strncpy(pt[process_no].desc, "unix domain socket server", 
			MAX_PT_DESC);
#ifdef USE_TCP
		if (!tcp_disable){
			close(sockfd[1]);
			pt[process_no].unix_sock=sockfd[0];
			pt[process_no].idx=-1; /* this is not a "tcp"
									  process*/
		}
#endif

	}

	return 1;
}


/*
 * Clean up
 */
void close_unixsock_server(void)
{
	struct unixsock_cmd* c;
	close(rx_sock);
	close(tx_sock);

	while(cmd_list) {
		c = cmd_list;
		cmd_list = cmd_list->next;
		pkg_free(c);
	}
}


/*
 * Register a new command
 */
int unixsock_register_cmd(char* command, unixsock_f* f)
{
	str cmd;
	struct unixsock_cmd* new_cmd;

	cmd.s = command;
	cmd.len = strlen(command);

	if (lookup_cmd(&cmd)) {
		LOG(L_ERR, "unixsock_register_cmd: Function already exists\n");
		return -1;
	}

	new_cmd = pkg_malloc(sizeof(struct unixsock_cmd));
	if (new_cmd == 0) {
		LOG(L_ERR, "register_unixsock_cmd: Out of mem\n");
		return -1;
	}

	new_cmd->name = cmd;
	new_cmd->f = f;

	new_cmd->next = cmd_list;
	cmd_list = new_cmd;
	
	DBG("unixsock_register_cmd: New command (%.*s) registered\n", 
	    cmd.len, ZSW(cmd.s));
	return 1;
}


int unixsock_add_to_reply(const char* buf, size_t len)
{
	if (reply_pos.len < len) {
		LOG(L_ERR, "unixsock_add_to_reply: Buffer too small\n");
		return -1;
	}

	memcpy(reply_pos.s, buf, len);
	reply_pos.s += len;
	reply_pos.len -= len;
	return 0;
}


/*
 * Send a reply
 */
ssize_t unixsock_reply_send(void)
{
	int ret;

	ret = sendto(tx_sock, reply_buf, reply_pos.s - reply_buf, MSG_DONTWAIT, 
		     (struct sockaddr*)&reply_addr, reply_addr_len);

	if (ret == -1) {
		LOG(L_ERR, "unixsock_reply_send: sendto: %s\n", 
		    strerror(errno));
	}

	return ret;
}


/*
 * Send a reply
 */
ssize_t unixsock_reply_sendto(struct sockaddr_un* to)
{
	int ret;

	if (!to) {
		LOG(L_ERR, "unixsock_reply_sendto: Invalid parameter value\n");
		return -1;
	}

	ret = sendto(tx_sock, reply_buf, reply_pos.s - reply_buf, MSG_DONTWAIT, 
		     (struct sockaddr*)to, SUN_LEN(to));

	if (ret == -1) {
		LOG(L_ERR, "unixsock_reply_sendto: sendto: %s\n", 
		    strerror(errno));
	}

	return ret;
}


/*
 * Read a line, the result will be stored in line
 * parameter, the data is not copied, it's just
 * a pointer to an existing buffer
 */
int unixsock_read_line(str* line, str* source)
{
	if (!line || !source) {
		LOG(L_ERR, "unixsock_read_line: Invalid parameter value\n");
		return -1;
	}

	*line = *source;
	skip_line(source);
	line->len = source->s - line->s;
	trim_trailing(line);
	if (line->len) {
		return 0;
	} else {
		return 1;
	}
}


/*
 * Read body until the closing .CRLF, no CRLF recovery
 * is done so no additional buffer is necessary, body will
 * point to an existing buffer
 */
int unixsock_read_body(str* body, str* source)
{
	int i, state, last_dot;

	enum states {
		ST_BEGIN,
		ST_CRLF,
		ST_DATA,
		ST_NEWLINE
	};

	if (!body || !source) {
		LOG(L_ERR, "unixsock_read_body: Invalid parameter value\n");
		return -1;
	}

	if (source->len < 2) {
		LOG(L_ERR, "unixsock_read_body: Not enough input data "
		    "(malformed message ?)\n");
		return -1;
	}

	state = ST_BEGIN;
	body->s = source->s;
	last_dot = 0;
	for(i = 0; i < source->len; i++) {
		switch(state) {
		case ST_BEGIN:
			if (source->s[i] == '.') {
				last_dot = i;
				state = ST_CRLF;
			} else if (source->s[i] == '\n') {
				state = ST_NEWLINE;
			} else {
				state = ST_DATA;
			}
			break;
			
		case ST_CRLF:
			if (source->s[i] == '\n') {
				body->len = last_dot;
				return 0;
			} else if (source->s[i] != '\r') {
				state = ST_DATA;
			}
			break;

		case ST_DATA:
			if (source->s[i] == '\n') {
				state = ST_NEWLINE;
			}
			break;

		case ST_NEWLINE:
			if (source->s[i] == '.') {
				last_dot = i;
				state = ST_CRLF;
			}
			break;
		}
	}

	LOG(L_ERR, "unixsock_read_body: Could not find the end of the body\n");
	return -1;
}


/*
 * Read a set of lines, the functions performs CRLF recovery,
 * therefore lineset must point to an additional buffer
 * to which the data will be copied. Initial lineset->len contains
 * the size of the buffer
 */
int unixsock_read_lineset(str* lineset, str* source)
{
	int i, state, len;

	enum states {
		ST_BEGIN,
		ST_CRLF,
		ST_DATA,
		ST_NEWLINE
	};

	if (!lineset || !source) {
		LOG(L_ERR, "unixsock_read_lineset: Invalid parameter value\n");
		return -1;
	}

	if (source->len < 2) {
		LOG(L_ERR, "unixsock_read_lineset: Not enough input "
		    "data (malformed message ?)\n");
		return -1;
	}                 

	state = ST_BEGIN;
	len = 0;
	for(i = 0; i < source->len; i++) {
		if (source->s[i] == '\r') {
			     /* Filter out CR */
			continue;
		}

		switch(state) {
		case ST_BEGIN:
			if (source->s[i] == '.') {
				state = ST_CRLF;
			} else if (source->s[i] == '\n') {
				lineset->s[len++] = '\r';
				lineset->s[len++] = '\n';
				state = ST_NEWLINE;
			} else {
				lineset->s[len++] = source->s[i];
			}
			break;
			
		case ST_CRLF:
			if (source->s[i] == '\n') {
				lineset->len = len;
				return 0;
			} else {
				lineset->s[len++] = '.';
				lineset->s[len++] = source->s[i];
				state = ST_DATA;
			}
			break;

		case ST_DATA:
			if (source->s[i] == '\n') {
				lineset->s[len++] = '\r';
				lineset->s[len++] = '\n';
				state = ST_NEWLINE;
			} else {
				lineset->s[len++] = source->s[i];
			}
			break;

		case ST_NEWLINE:
			if (source->s[i] == '.') {
				state = ST_CRLF;
			} else {
				lineset->s[len++] = source->s[i];
				state = ST_DATA;
			}
			break;
		}
	}

	LOG(L_ERR, "unixsock_read_body: Could not find the end of the body\n");
	return -1;
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
 * Add ASCIIZ to the reply buffer
 */
int unixsock_reply_asciiz(char* str)
{
	int len;
	
	if (!str) {
		LOG(L_ERR, "unixsock_reply_asciiz: Invalid parameter value\n");
		return -1;
	}

	len = strlen(str);

	if (reply_pos.len < len) {
		LOG(L_ERR, "unixsock_reply_asciiz: Buffer too small\n");
		return -1;
	}

	memcpy(reply_pos.s, str, len);
	reply_pos.s += len;
	reply_pos.len -= len;
	return 0;
}


/*
 * Add a string represented by str structure
 * to the reply buffer
 */
int unixsock_reply_str(str* s)
{
	if (!s) {
		LOG(L_ERR, "unixsock_reply_str: Invalid parameter value\n");
		return -1;
	}

	if (reply_pos.len < s->len) {
		LOG(L_ERR, "unixsock_reply_str: Buffer too small\n");
		return -1;
	}
	
	memcpy(reply_pos.s, s->s, s->len);
	reply_pos.s += s->len;
	reply_pos.len -= s->len;
	return 0;
}


/*
 * Printf-like reply function
 */
int unixsock_reply_printf(char* fmt, ...)
{
	va_list ap;
	int ret;

	if (!fmt) {
		LOG(L_ERR, "unixsock_reply_printf: Invalid parameter value\n");
		return -1;
	}

	va_start(ap, fmt);
	ret = vsnprintf(reply_pos.s, reply_pos.len, fmt, ap);
	if ((ret == -1) || (ret >= reply_pos.len)) {
		LOG(L_ERR, "unixsock_reply_printf: Buffer too small\n");
		return -1;
	}

	va_end(ap);
	reply_pos.s += ret;
	reply_pos.len -= ret;
	return 0;
}


/*
 * Return the address of the sender
 */
struct sockaddr_un* unix_sender_addr(void)
{
	return &reply_addr;
}
