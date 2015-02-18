/*
 * This file is part of Kamailio, a free SIP server.
 *
 * Copyright (C) 2006 Voice Sistem S.R.L.
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
 * Author: Andreea Spirea
 *
 */
/*! \file
 * \brief Kamailio XMPP :: XMPP server implementation (limited functionality)
 *  \ingroup xmpp
 */

/*
 * An inbound SIP message:
 *   from sip:user1@domain1 to sip:user2*domain2@gateway_domain
 * is translated to an XMPP message:
 *   from user1*domain1@xmpp_domain to user2@domain2
 *
 * An inbound XMPP message:
 *   from user1@domain1 to user2*domain2@xmpp_domain
 * is translated to a SIP message:
 *   from sip:user1*domain1@gateway_domain to sip:user2@domain2
 *
 * Where '*' is the domain_separator, and gateway_domain and
 * xmpp_domain are defined below.
 */

/*
 * 2-way dialback sequence with xmppd2:
 *
 *  Originating server (us)         Receiving server (them)      Authoritative server (us)
 *  -----------------------         -----------------------      -------------------------
 *           |                               |                               |
 *           |    establish connection       |                               |
 *           |------------------------------>|                               |
 *           |    send stream header         |                               |
 *           |------------------------------>|                               |
 *           |    send stream header         |                               |
 *           |<------------------------------|                               |
 *           |    send db:result request     |                               |
 *           |------------------------------>|                               |
 *                                           |    establish connection       |
 *                                           |------------------------------>|
 *                                           |    send stream header         |
 *                                           |------------------------------>|
 *                                           |    send stream header         |
 *                                           |<------------------------------|
 *                                           |    send db:result request     |
 *                                           |------------------------------>|
 *           |    send db:verify request     |
 *           |------------------------------>|
 *           |    send db:verify response    |
 *           |<------------------------------|
 *                                           |    send db:result response    |
 *                                           |------------------------------>|
 *                                           |    send db:verify request     |
 *                                           |<------------------------------|
 *                                           |    send db:verify response    |
 *                                           |------------------------------>|
 *           |    send db:result response    |
 *           |<------------------------------|
 *           :                               :                               :
 *           :                               :                               :
 *           |    outgoing <message/>        |                               :
 *           |------------------------------>|                               :
 *                                           |    incoming <message/>        |
 *                                           |------------------------------>|
 */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#include "../../sr_module.h"
#include "../../cfg/cfg_struct.h"

#include "xmpp.h"
#include "xmpp_api.h"
#include "network.h"
#include "xode.h"

#include <arpa/inet.h>

/* XXX hack */
#define DB_KEY	"this-be-a-random-key"

#define CONN_DEAD	0
#define CONN_INBOUND	1
#define CONN_OUTBOUND	2

struct xmpp_private_data {
	int fd;		/* outgoing stream socket */
	int listen_fd;	/* listening socket */
	int in_fd;	/* incoming stream socket */
	int running;
};

struct xmpp_connection {
	struct xmpp_connection *next;

	char *domain;
	int type;
	int fd;
	char *stream_id;
	xode_pool pool;
	xode_stream stream;
	xode todo;	/* backlog of outgoing messages, if any */
};

static char local_secret[64] = { 0, };

static void in_stream_node_callback(int type, xode node, void *arg);
static void out_stream_node_callback(int type, xode node, void *arg);

static struct xmpp_connection *conn_list = NULL;

static struct xmpp_connection *conn_new(int type, int fd, char *domain)
{
	struct xmpp_connection *conn = NULL;
	
	conn = malloc(sizeof(struct xmpp_connection));

	if(conn==NULL) 
	{
		LM_ERR("out of memory\n");
		return NULL;
	}
	
	memset(conn, 0, sizeof(struct xmpp_connection));
	conn->domain = domain ? strdup(domain) : NULL;
	conn->type = type;
	conn->fd = fd;
	conn->todo = xode_new_tag("todo");

	conn->pool = xode_pool_new();
	conn->stream = xode_stream_new(conn->pool,
		(type==CONN_INBOUND)?in_stream_node_callback:out_stream_node_callback,
		conn);
	
	conn->next = conn_list;
	conn_list = conn;
	return conn;
}

static void conn_free(struct xmpp_connection *conn)
{
	struct xmpp_connection **last_p, *link;
	
	last_p = &conn_list;
	for (link = conn_list; link; link = link->next) {
		if (link == conn) {
			*last_p = link->next;
			break;
		}
		last_p = &link->next;
	}

	if (conn->todo)
		xode_free(conn->todo);
	xode_pool_free(conn->pool);
	if (conn->fd != -1)
		close(conn->fd);
	if (conn->stream_id)
		free(conn->stream_id);
	if (conn->domain)
		free(conn->domain);
	free(conn);
}

static struct xmpp_connection *conn_find_domain(char *domain, int type)
{
	struct xmpp_connection *conn;
	
	for (conn = conn_list; conn; conn = conn->next)
		if (conn->domain && !strcasecmp(conn->domain, domain) 
				&& conn->type == type)
			return conn;
	return NULL;
}

/*
static struct xmpp_connection *conn_find_fd(int fd)
{
	struct xmpp_connection *conn;
	
	for (conn = conn_list; conn; conn = conn->next)
		if (conn->fd == fd)
			return conn;
	return NULL;
}
*/

/*****************************************************************************/

static int xode_send(int fd, xode x)
{
	char *str = xode_to_str(x);
	int len = strlen(str);
	
	LM_DBG("xode_send->%d [%s]\n", fd, str);

	if (net_send(fd, str, len) != len) {
		LM_ERR("send() failed: %s\n", strerror(errno));
		return -1;
	}
	return len;
}

static int xode_send_domain(char *domain, xode x)
{
	struct xmpp_connection *conn;

	if ((conn = conn_find_domain(domain, CONN_OUTBOUND))) {
		xode_send(conn->fd, x);
		xode_free(x);
	} else {
		if((conn = conn_new(CONN_OUTBOUND, -1, domain))==0)
			return -1;
		xode_insert_node(conn->todo, x);
	}
	return 1;
}

static void out_stream_node_callback(int type, xode node, void *arg)
{
	struct xmpp_connection *conn = (struct xmpp_connection *) arg;
	struct xmpp_connection *in_conn = NULL;
	char *tag;
	xode x;

	LM_DBG("outstream callback: %d: %s\n", type, 
			node?xode_get_name(node):"n/a");

	if (conn->domain)
		in_conn = conn_find_domain(conn->domain, CONN_INBOUND);

	switch (type) {
	case XODE_STREAM_ROOT:
		x = xode_new_tag("db:result");
		xode_put_attrib(x, "xmlns:db", "jabber:server:dialback");
		xode_put_attrib(x, "from", xmpp_domain);
		xode_put_attrib(x, "to", conn->domain);
		//xode_insert_cdata(x, DB_KEY, -1);
		xode_insert_cdata(x, db_key(local_secret, conn->domain,
					xode_get_attrib(node, "id")), -1);
		xode_send(conn->fd, x);
		xode_free(x);

		break;
	case XODE_STREAM_NODE:
		tag = xode_get_name(node);

		if (!strcmp(tag, "db:verify")) {
			char *from = xode_get_attrib(node, "from");
			char *to = xode_get_attrib(node, "to");
			char *id = xode_get_attrib(node, "id");
			char *type = xode_get_attrib(node, "type");
			/* char *cdata = xode_get_data(node); */
			
			if (!strcmp(type, "valid") || !strcmp(type, "invalid")) {
				/* got a reply, report it */
				x = xode_new_tag("db:result");
				xode_put_attrib(x, "xmlns:db", "jabber:server:dialback");
				xode_put_attrib(x, "from", to);
				xode_put_attrib(x, "to", from);
				xode_put_attrib(x, "id", id);
				xode_put_attrib(x, "type", type);
				if (in_conn)
					xode_send(in_conn->fd, x);
				else
					LM_ERR("need to send reply to domain '%s', but no inbound"
							" connection found\n", from);
				xode_free(x);
			}
		} else if (!strcmp(tag, "db:result")) {
			char *type = xode_get_attrib(node, "type");
			
			if (type && !strcmp(type, "valid")) {
				/* the remote server has successfully authenticated us,
				 * we can now send data */
				for (x = xode_get_firstchild(conn->todo); x;
						x = xode_get_nextsibling(x)) {
					LM_DBG("sending todo tag '%s'\n", xode_get_name(x));
					xode_send(conn->fd, x);
				}
				xode_free(conn->todo);
				conn->todo = NULL;
			}
		}
		break;
	case XODE_STREAM_ERROR:
		LM_ERR("outstream error\n");
		/* fall-through */
	case XODE_STREAM_CLOSE:
		conn->type = CONN_DEAD;
		break;
	}
	xode_free(node);
}

static void in_stream_node_callback(int type, xode node, void *arg) 
{
	struct xmpp_connection *conn = (struct xmpp_connection *) arg;
	char *tag;
	xode x;

	LM_DBG("instream callback: %d: %s\n",
			type, node ? xode_get_name(node) : "n/a");
	switch (type) {
	case XODE_STREAM_ROOT:
		conn->stream_id = strdup(random_secret());
		net_printf(conn->fd,
			"<?xml version='1.0'?>"
			"<stream:stream xmlns:stream='http://etherx.jabber.org/streams' xmlns='jabber:server' version='1.0'"
			" xmlns:db='jabber:server:dialback' id='%s' from='%s'>", conn->stream_id, xmpp_domain);
		net_printf(conn->fd,"<stream:features xmlns:stream='http://etherx.jabber.org/streams'/>");
		break;
	case XODE_STREAM_NODE:
		tag = xode_get_name(node);

		if (!strcmp(tag, "db:result")) {
			char *from = xode_get_attrib(node, "from");
			char *to = xode_get_attrib(node, "to");
			/* char *id = xode_get_attrib(node, "id"); */
			char *type = xode_get_attrib(node, "type");
			char *cdata = xode_get_data(node);
			
			if (!type) {
				if (conn->domain) {
					LM_DBG("connection %d has old domain '%s'\n",conn->fd,
							conn->domain);
					free(conn->domain);
				}
				conn->domain = strdup(from);
				LM_DBG("connection %d set domain '%s'\n",
						conn->fd, conn->domain);

				/* it's a request; send verification over outgoing connection */
				x = xode_new_tag("db:verify");
				xode_put_attrib(x, "xmlns:db", "jabber:server:dialback");
				xode_put_attrib(x, "from", to);
				xode_put_attrib(x, "to", from);
				//xode_put_attrib(x, "id", "someid"); /* XXX fix ID */
				xode_put_attrib(x, "id", conn->stream_id);
				xode_insert_cdata(x, cdata, -1);
				xode_send_domain(from, x);
			}			
		} else if (!strcmp(tag, "db:verify")) {
			char *from = xode_get_attrib(node, "from");
			char *to = xode_get_attrib(node, "to");
			char *id = xode_get_attrib(node, "id");
			char *type = xode_get_attrib(node, "type");
			char *cdata = xode_get_data(node);
			
			if (!type) {
				/* it's a request */
				x = xode_new_tag("db:verify");
				xode_put_attrib(x, "xmlns:db", "jabber:server:dialback");
				xode_put_attrib(x, "from", to);
				xode_put_attrib(x, "to", from);
				xode_put_attrib(x, "id", id);
				//if (cdata && !strcmp(cdata, DB_KEY)) {
				if (cdata && !strcmp(cdata, db_key(local_secret, from, id))) {
					xode_put_attrib(x, "type", "valid");
				} else {
					xode_put_attrib(x, "type", "invalid");
				}
				xode_send(conn->fd, x);
				xode_free(x);
			}			
		} else if (!strcmp(tag, "message")) {
			char *from = xode_get_attrib(node, "from");
			char *to = xode_get_attrib(node, "to");
			char *type = xode_get_attrib(node, "type");
			xode body = xode_get_tag(node, "body");
			char *msg;
			
			if (!type)
				type = "chat";
			if (!strcmp(type, "error")) {	
				LM_DBG("received message error stanza\n");
				goto out;
			}
			
			if (!from || !to || !body) {
				LM_DBG("invalid <message/> attributes\n");
				goto out;
			}

			if (!(msg = xode_get_data(body)))
				msg = "";
			xmpp_send_sip_msg(
				encode_uri_xmpp_sip(from),
				decode_uri_xmpp_sip(to),
				msg);
		} else if (!strcmp(tag, "presence")) {
			/* run presence callbacks */
		}
		break;

		break;
	case XODE_STREAM_ERROR:
		LM_ERR("instream error\n");
		/* fall-through */
	case XODE_STREAM_CLOSE:
		conn->type = CONN_DEAD;
		break;
	}
out:
	xode_free(node);
}

static void do_send_message_server(struct xmpp_pipe_cmd *cmd)
{
	char *domain;
	xode x;

	LM_DBG("rom=[%s] to=[%s] body=[%s]\n", cmd->from,cmd->to, cmd->body);

	x = xode_new_tag("message");
	xode_put_attrib(x, "xmlns", "jabber:client");
	xode_put_attrib(x, "id", cmd->id); // XXX
	xode_put_attrib(x, "from", encode_uri_sip_xmpp(cmd->from));
	xode_put_attrib(x, "to", decode_uri_sip_xmpp(cmd->to));
	xode_put_attrib(x, "type", "chat");
	xode_insert_cdata(xode_insert_tag(x, "body"), cmd->body, -1);

	domain = extract_domain(decode_uri_sip_xmpp(cmd->to));
	xode_send_domain(domain, x);
}

int xmpp_server_child_process(int data_pipe)
{
	int rv;
	int listen_fd;
	fd_set fdset;
	struct xmpp_connection *conn;
	
	snprintf(local_secret, sizeof(local_secret), "%s", random_secret());

	while ((listen_fd = net_listen(xmpp_domain, xmpp_port)) < 0) {
		/* ugh. */
		sleep(3);
	}

	while (1) {
		FD_ZERO(&fdset);
		FD_SET(data_pipe, &fdset);
		FD_SET(listen_fd, &fdset);
		
		/* check for dead connections */
		for (conn = conn_list; conn; ) {
			struct xmpp_connection *next = conn->next;

			if (conn->type == CONN_DEAD)
				conn_free(conn);
			conn = next;
		}

		for (conn = conn_list; conn; conn = conn->next) {
			/* check if we need to set up a connection */
			if (conn->type == CONN_OUTBOUND && conn->fd == -1) {
				if ((conn->fd = net_connect(conn->domain, xmpp_port)) >= 0)
				{
					net_printf(conn->fd,
						"<?xml version='1.0'?>"
						"<stream:stream xmlns:stream='http://etherx.jabber.org/streams' xmlns='jabber:server' version='1.0' "
						"xmlns:db='jabber:server:dialback' to='%s' from='%s'>",
						conn->domain, xmpp_domain);
					net_printf(conn->fd,
                           "<stream:features xmlns:stream='http://etherx.jabber.org/streams'/>");
				} else {
					conn->type = CONN_DEAD;
				}
			}		

			if (conn->fd != -1)
				FD_SET(conn->fd, &fdset);
		}

		rv = select(FD_SETSIZE, &fdset, NULL, NULL, NULL);

		/* update the local config framework structures */
		cfg_update();

		if (rv < 0) {
			LM_ERR("select() failed: %s\n", strerror(errno));
		} else if (!rv) {
			/* timeout */
		} else {
			for (conn = conn_list; conn; conn = conn->next) {
				if (conn->fd != -1 && FD_ISSET(conn->fd, &fdset)) {
					char *buf = net_read_static(conn->fd);
					if (!buf) {
						conn->type = CONN_DEAD;
					} else {
						LM_DBG("stream (fd %d, domain '%s') read\n[%s]\n",
								conn->fd, conn->domain, buf);
						xode_stream_eat(conn->stream, buf, strlen(buf));
					}
				}
			}

			if (FD_ISSET(listen_fd, &fdset)) {
				struct sockaddr_in sin;
				unsigned int len = sizeof(sin);
				int fd;

				if ((fd = accept(listen_fd,(struct sockaddr*)&sin, &len))<0) {
					LM_ERR("accept() failed: %s\n", strerror(errno));
				} else {
					LM_DBG("accept()ed connection from %s:%d\n",
							inet_ntoa(sin.sin_addr), ntohs(sin.sin_port));
					conn_new(CONN_INBOUND, fd, NULL);
				}
			}

			if (FD_ISSET(data_pipe, &fdset)) {
				struct xmpp_pipe_cmd *cmd;

				if (read(data_pipe, &cmd, sizeof(cmd)) != sizeof(cmd)) {
					LM_ERR("failed to read from command pipe: %s\n",
							strerror(errno));
				} else {
					LM_DBG("got pipe cmd %d\n", cmd->type);
					switch (cmd->type) {
					case XMPP_PIPE_SEND_MESSAGE:
						do_send_message_server(cmd);
						break;
					case XMPP_PIPE_SEND_PACKET:
					case XMPP_PIPE_SEND_PSUBSCRIBE:
					case XMPP_PIPE_SEND_PNOTIFY:
						break;
					}
					xmpp_free_pipe_cmd(cmd);
				}
			}
		}
	}
	return 0;
}

