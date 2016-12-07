/*
 * XMPP Module
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
 * \brief Kamailio XMPP :: XMPP Component interface support
 *  \ingroup xmpp
 *
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

struct xmpp_private_data {
	int fd;		/* socket */
	int running;
};

static int xode_send(int fd, xode x)
{
	char *str = xode_to_str(x);
	int len = strlen(str);
	
	LM_DBG("xode_send [%s]\n", str);

	if (net_send(fd, str, len) != len) {
		LM_ERR("send() error: %s\n", strerror(errno));
		return -1;
	}
	/* should str be freed?!?! */
	return len;
}

static void stream_node_callback(int type, xode node, void *arg) 
{
	struct xmpp_private_data *priv = (struct xmpp_private_data *) arg;
	char *id, *hash, *tag;
	char buf[4096];
	xode x;

	LM_DBG("stream callback: %d: %s\n", type, node ? xode_get_name(node) : "n/a");
	switch (type) {
	case XODE_STREAM_ROOT:
		id = xode_get_attrib(node, "id");
		snprintf(buf, sizeof(buf), "%s%s", id, xmpp_password);
		hash = shahash(buf);
		
		x = xode_new_tag("handshake");
		xode_insert_cdata(x, hash, -1);
		xode_send(priv->fd, x);
		xode_free(x);
		break;
	case XODE_STREAM_NODE:
		tag = xode_get_name(node);
		if (!strcmp(tag, "handshake")) {
			LM_DBG("handshake succeeded\n");
		} else if (!strcmp(tag, "message")) {
			LM_DBG("XMPP IM received\n");
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
			/* call presence callbacks */
			LM_DBG("XMPP Presence received\n");
			run_xmpp_callbacks(XMPP_RCV_PRESENCE, xode_to_str(node));
		}else if (!strcmp(tag, "iq")) {
			/* call presence callbacks */
			LM_DBG("XMPP IQ received\n");
			run_xmpp_callbacks(XMPP_RCV_IQ, xode_to_str(node));
		}
		break;
	case XODE_STREAM_ERROR:
		LM_ERR("stream error\n");
		/* fall-through */
	case XODE_STREAM_CLOSE:
		priv->running = 0;
		break;
	}
out:
	xode_free(node);
}

/*!
 *
 */
static int do_send_message_component(struct xmpp_private_data *priv,
		struct xmpp_pipe_cmd *cmd)
{
	xode x;

	LM_DBG("do_send_message_component from=[%s] to=[%s] body=[%s]\n",
			cmd->from, cmd->to, cmd->body);

	x = xode_new_tag("message");
	xode_put_attrib(x, "id", cmd->id); // XXX
	xode_put_attrib(x, "from", encode_uri_sip_xmpp(cmd->from));
	xode_put_attrib(x, "to", decode_uri_sip_xmpp(cmd->to));
	xode_put_attrib(x, "type", "chat");
	xode_insert_cdata(xode_insert_tag(x, "body"), cmd->body, -1);
			
	xode_send(priv->fd, x);
	xode_free(x);

	/* missing error handling here ?!?!*/
	return 0;
}

static int do_send_bulk_message_component(struct xmpp_private_data *priv,
		struct xmpp_pipe_cmd *cmd)
{
	int len;

	LM_DBG("do_send_bulk_message_component from=[%s] to=[%s] body=[%s]\n",
			cmd->from, cmd->to, cmd->body);
	len = strlen(cmd->body);
	if (net_send(priv->fd, cmd->body, len) != len) {
		LM_ERR("do_send_bulk_message_component: %s\n",strerror(errno));
		return -1;
	}
	return 0;
}


int xmpp_component_child_process(int data_pipe)
{
	int fd, maxfd, rv;
	fd_set fdset;
	xode_pool pool;
	xode_stream stream;
	struct xmpp_private_data priv;
	struct xmpp_pipe_cmd *cmd;
	
	while (1) {
		fd = net_connect(xmpp_host, xmpp_port);
		if (fd < 0) {
			sleep(3);
			continue;
		}
		
		priv.fd = fd;
		priv.running = 1;
		
		pool = xode_pool_new();
		stream = xode_stream_new(pool, stream_node_callback, &priv);
		
		net_printf(fd,
			"<?xml version='1.0'?>"
			"<stream:stream xmlns='jabber:component:accept' to='%s' "
			"version='1.0' xmlns:stream='http://etherx.jabber.org/streams'>",
			xmpp_domain);
		
		while (priv.running) {
			FD_ZERO(&fdset);
			FD_SET(data_pipe, &fdset);
			FD_SET(fd, &fdset);
			maxfd = fd > data_pipe ? fd : data_pipe;
			rv = select(maxfd + 1, &fdset, NULL, NULL, NULL);
			
			/* update the local config framework structures */
			cfg_update();

			if (rv < 0) {
				LM_ERR("select() failed: %s\n", strerror(errno));
			} else if (!rv) {
				/* timeout */
			} else if (FD_ISSET(fd, &fdset)) {
				char *buf = net_read_static(fd);

				if (!buf)
					/* connection closed */
					break;

				LM_DBG("server read\n[%s]\n", buf);
				xode_stream_eat(stream, buf, strlen(buf));
			} else if (FD_ISSET(data_pipe, &fdset)) {
				if (read(data_pipe, &cmd, sizeof(cmd)) != sizeof(cmd)) {
					LM_ERR("failed to read from command pipe: %s\n",
							strerror(errno));
				} else {
					LM_DBG("got pipe cmd %d\n", cmd->type);
					switch (cmd->type) {
					case XMPP_PIPE_SEND_MESSAGE:
						do_send_message_component(&priv, cmd);
						break;
					case XMPP_PIPE_SEND_PACKET:
					case XMPP_PIPE_SEND_PSUBSCRIBE:
					case XMPP_PIPE_SEND_PNOTIFY:
						do_send_bulk_message_component(&priv, cmd);
						break;
					}
					xmpp_free_pipe_cmd(cmd);
				}
			}
		}
		
		xode_pool_free(pool);
		
		close(fd);
	}
	return 0;
}

