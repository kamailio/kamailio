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

#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <stdarg.h>
#include "ut.h"
#include "globals.h"
#include "trim.h"
#include "pt.h"
#include "sr_module.h"
#include "mem/mem.h"
#include "fifo_server.h" /* CMD_SEPARATOR */
#include "unixsock_server.h"

#define UNIXSOCK_BUF_SIZE BUF_SIZE

char* unixsock_name = 0;
int unixsock_children = 1;

static int sock;
static struct unixsock_cmd* cmd_list;
static char reply_buf[UNIXSOCK_BUF_SIZE];
static str reply_pos;
static struct sockaddr_un reply_addr;

#define CMD_NOT_FOUND "500 Command not found"
#define CMD_NOT_FOUND_LEN (sizeof(CMD_NOT_FOUND) - 1)

#define FLINE_ERR "400 First line malformed"
#define FLINE_ERR_LEN (sizeof(FLINE_ERR) - 1)


/*
 * Create and bind local socket
 */
static int create_unix_socket(char* name)
{
	struct sockaddr_un addr;
	int len;

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

	sock = socket(PF_LOCAL, SOCK_DGRAM, 0);
	if (sock == -1) {
		LOG(L_ERR, "create_unix_socket: Cannot create socket: %s\n", 
		    strerror(errno));
		return -1;
	}

	memset(&addr, 0, sizeof(addr));
	addr.sun_family = PF_LOCAL;
	memcpy(addr.sun_path, name, len);

	if (bind(sock, (struct sockaddr*)&addr, SUN_LEN(&addr)) == -1) {
		LOG(L_ERR, "create_unix_socket: bind: %s\n", strerror(errno));
		close(sock);
		return -1;
	}

	return 0;
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
	int addr_len, ret;
	str cmd, buffer;
	static char buf[UNIXSOCK_BUF_SIZE];
	struct unixsock_cmd* c;

	buffer.s = buf;
	buffer.len = 0;
	
	reply_pos.s = reply_buf;
	reply_pos.len = UNIXSOCK_BUF_SIZE;

	while(1) {
		addr_len = sizeof(reply_addr);
		ret = recvfrom(sock, buffer.s, UNIXSOCK_BUF_SIZE, 0, 
			       (struct sockaddr*)&reply_addr, &addr_len);
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

		if (parse_cmd(&cmd, &buffer) < 0) {
			unixsock_add_to_reply(FLINE_ERR, FLINE_ERR_LEN);
			unixsock_send_reply();
			continue;
		}

		buffer.s = cmd.s + cmd.len + 1;
		buffer.len -= cmd.len + 1 + 1;
		skip_line(&buffer); /* Skip the reply filename */

		c = lookup_cmd(&cmd);
		if (c == 0) {
			LOG(L_ERR, "unix_server_loop: Could not find "
			    "command '%.*s'\n", cmd.len, ZSW(cmd.s));
			unixsock_add_to_reply(CMD_NOT_FOUND, CMD_NOT_FOUND_LEN);
			unixsock_send_reply();
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


/*
 * Initialize Unix domain socket server
 */
int init_unixsock_server(void)
{
	int ret, i;
	pid_t pid;
	
	ret = create_unix_socket(unixsock_name);
	if (ret < 0) {
		LOG(L_ERR, "init_unixsock_server: Error while creating "
		    "local socket\n");
		return -1;
	} else if (ret > 0) {
		return 1;
	}

	for(i = 0; i < unixsock_children; i++) {
		process_no++;
		pid = fork();
		if (pid < 0) {
			LOG(L_ERR, "init_unixsock_server: Unable to fork: %s\n",
			    strerror(errno));
			close(sock);
			return -1;
		} else if (pid == 0) { /* child */
			if (init_child(PROC_UNIXSOCK) < 0) {
				LOG(L_ERR, "init_unixsock_server: Error in "
				    "init_child\n");
				close(sock);
				return -1;
			}

			unix_server_loop(); /* Never returns */
		}

		     /* Parent */
		pt[process_no].pid = pid;
		strncpy(pt[process_no].desc, "unix domain socket server", 
			MAX_PT_DESC);
	}
	return 1;
}


/*
 * Clean up
 */
void close_unixsock_server(void)
{
	struct unixsock_cmd* c;
	close(sock);

	while(cmd_list) {
		c = cmd_list;
		cmd_list = cmd_list->next;
		pkg_free(c);
	}
}


/*
 * Register a new command
 */
int unixsock_register_cmd(str* cmd, unixsock_f* f)
{
	struct unixsock_cmd* new_cmd;

	if (lookup_cmd(cmd)) {
		LOG(L_ERR, "unixsock_register_cmd: Function already exists\n");
		return -1;
	}

	new_cmd = pkg_malloc(sizeof(struct unixsock_cmd));
	if (new_cmd == 0) {
		LOG(L_ERR, "register_unixsock_cmd: Out of mem\n");
		return -1;
	}

	new_cmd->name = *cmd;
	new_cmd->f = f;

	new_cmd->next = cmd_list;
	cmd_list = new_cmd;
	
	DBG("unixsock_register_cmd: New command (%.*s) registered\n", 
	    cmd->len, ZSW(cmd->s));
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
ssize_t unixsock_send_reply(void)
{
	int ret;

	ret = sendto(sock, reply_buf, reply_pos.s - reply_buf, MSG_DONTWAIT, 
		     (struct sockaddr*)&reply_addr, SUN_LEN(&reply_addr));

	if (ret == -1) {
		LOG(L_ERR, "unixsock_send_reply: sendto: %s\n", 
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
	if (line->len) {
		trim_trailing(line);
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
