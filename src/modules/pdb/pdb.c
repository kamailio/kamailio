/*
 * Copyright (C) 2009 1&1 Internet AG
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
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
 */

/*!
 * @file pdb.c
 * @brief Contains the functions exported by the module.
 */


#include "../../core/sr_module.h"
#include "../../core/cfg/cfg.h"
#include "../../core/cfg/cfg_ctx.h"
#include "../../core/mem/mem.h"
#include "../../core/mem/shm_mem.h"
#include "../../core/rpc_lookup.h"
#include "../../core/mod_fix.h"
#include "../../core/lvalue.h"
#include "../../core/kemi.h"
#include <sys/time.h>
#include <poll.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>

#include "common.h"
#include "config.h"

MODULE_VERSION

static char *modp_server = NULL; /*!< format: \<host\>:\<port\>,... */
static int timeoutlogs = -10;	 /*!< for aggregating timeout logs */
static int *active = NULL;
static uint16_t *global_id = NULL;

ksr_loglevels_t _ksr_loglevels_pdb = KSR_LOGLEVELS_DEFAULTS;

/*!
 * Generic parameter that holds a string, an int or a pseudo-variable
 * @todo replace this with gparam_t
 */
struct multiparam_t
{
	enum
	{
		MP_INT,
		MP_STR,
		MP_AVP,
		MP_PVE,
	} type;
	union
	{
		int n;
		str s;
		struct
		{
			avp_flags_t flags;
			avp_name_t name;
		} a;
		pv_elem_t *p;
	} u;
};


/* ---- exported commands: */
int pdb_query(sip_msg_t *_msg, str *_number, str *_dstvar);

/* ---- fixup functions: */
int pdb_query_fixup(void **arg, int arg_no);
int pdb_query_fixup_free(void **arg, int arg_no);

/* ---- KEMI related functions: */
int ki_pdb_query(sip_msg_t *_msg, str *number, str *dstvar);
int ki_pdb_query_helper(sip_msg_t *_msg, str *number, pv_spec_t *dvar);

/* ---- misc. functions: */
int do_pdb_query(str *number);


/* ---- module init functions: */
static int mod_init(void);
static int child_init(int rank);
static int rpc_child_init(void);
static void mod_destroy();

/* debug function for the new client <-> server protocol */
static void pdb_msg_dbg(struct pdb_msg *msg, char *dbg_msg);

/* build the new protocol message before transmission */
static int pdb_msg_format_send(struct pdb_msg *msg, uint8_t version,
		uint8_t type, uint8_t code, uint16_t id, char *payload,
		uint16_t payload_len);

static cmd_export_t cmds[] = {
		{"pdb_query", (cmd_function)pdb_query, 2, pdb_query_fixup,
				pdb_query_fixup_free, REQUEST_ROUTE | FAILURE_ROUTE},
		{0, 0, 0, 0, 0, 0}};


/* clang-format off */
static param_export_t params[] = {
	{"server", PARAM_STRING, &modp_server},
	{"timeout", PARAM_INT, &default_pdb_cfg.timeout},
	{"ll_info", PARAM_INT, &_ksr_loglevels_pdb.ll_info},

	{0, 0, 0}
};
/* clang-format on */

struct module_exports exports = {
		"pdb",			 /* module name */
		DEFAULT_DLFLAGS, /* dlopen flags */
		cmds,			 /* cmd (cfg function) exports */
		params,			 /* param exports */
		0,				 /* RPC method exports */
		0,				 /* pseudo-variables exports */
		0,				 /* response handling function */
		mod_init,		 /* Module initialization function */
		child_init,		 /* Child initialization function */
		mod_destroy		 /* Destroy function */
};


struct server_item_t
{
	struct server_item_t *next;
	char *host;
	unsigned short int port;
	struct sockaddr_in dstaddr;
	socklen_t dstaddrlen;
	int sock;
};


struct server_list_t
{
	struct server_item_t *head;
	int nserver;
	struct pollfd *fds;
};


/*! global server list */
static struct server_list_t *server_list;


/* debug function for the new client <-> server protocol */
static void pdb_msg_dbg(struct pdb_msg *msg, char *dbg_msg)
{
	int i;
	char buf[PAYLOADSIZE * 3 + 1];
	char *ptr = buf;

	if(msg->hdr.length > sizeof(msg->hdr)) {
		for(i = 0; i < msg->hdr.length - sizeof(msg->hdr); i++) {
			ptr += sprintf(ptr, "%02X ", msg->bdy.payload[i]);
		}
	} else {
		*ptr = '\0';
	}

	LM_DBG("%s\n"
		   "version = %d\ntype = %d\ncode = %d\nid = %d\nlen = %d\n"
		   "payload = %s\n",
			dbg_msg, msg->hdr.version, msg->hdr.type, msg->hdr.code,
			msg->hdr.id, msg->hdr.length, buf);
}

/* build the message before send */
static int pdb_msg_format_send(struct pdb_msg *msg, uint8_t version,
		uint8_t type, uint8_t code, uint16_t id, char *payload,
		uint16_t payload_len)
{
	msg->hdr.version = version;
	msg->hdr.type = type;
	msg->hdr.code = code;
	msg->hdr.id = id;

	if(payload == NULL) {
		/* just ignore the NULL buff (called when just want to set the len) */
		msg->hdr.length = sizeof(struct pdb_hdr);
		return 0;
	} else {
		msg->hdr.length = sizeof(struct pdb_hdr) + payload_len;
		memcpy(msg->bdy.payload, payload, payload_len);
		return 0;
	}

	return 0;
}


/* two chars to short-int without caring of memory alignment in char buffer */
#define PDB_BUFTOSHORT(_sv, _b, _n) \
	memcpy(&(_sv), (char *)(_b) + (_n), sizeof(short int))

/**
 *
 */
int ki_pdb_query(sip_msg_t *_msg, str *number, str *dstvar)
{
	pv_spec_t *dst;
	dst = pv_cache_get(dstvar);

	if(dst == NULL) {
		LM_ERR("failed to get pv spec for: %.*s\n", dstvar->len, dstvar->s);
		return -1;
	}

	if(dst->setf == NULL) {
		LM_ERR("target pv is not writable: %.*s\n", dstvar->len, dstvar->s);
		return -1;
	}

	return ki_pdb_query_helper(_msg, number, dst);
}


/**
 * Queries PDB service for given number and stores result in an AVP.
 *
 * @param _msg the current SIP message
 * @param _number the phone number to query
 * @param _dstavp the name of the AVP where to store the result
 *
 * @return 1 on success, -1 on failure
 */
int pdb_query(sip_msg_t *_msg, str *_number, str *_dstvar)
{
	str number;

	if(fixup_get_svalue(_msg, (gparam_t *)_number, &number) < 0) {
		LM_ERR("cannot print the number\n");
		return -1;
	}

	return ki_pdb_query_helper(_msg, &number, (pv_spec_t *)_dstvar);
}


/*!
 * \return 1 if query for the number succeeded and the avp with the corresponding carrier id was set,
 * -1 otherwise
 */
int ki_pdb_query_helper(sip_msg_t *_msg, str *number, pv_spec_t *dvar)
{
	pv_value_t val = {0};

	/* get carrier id */
	if((val.ri = do_pdb_query(number)) == 0) {
		LM_ERR("error in do_pdb_query");
		return -1;
	} else {
		/* set var */
		val.flags = PV_VAL_INT | PV_TYPE_INT;
		if(dvar->setf(_msg, &dvar->pvp, (int)EQ_T, &val) < 0) {
			LM_ERR("failed setting dst var\n");
			return -1;
		}
	}
	return 1;
}


/**
 * Internal function that actually queries the PDB service
 *
 * @param _number the phone number to query
 *
 * @return carrier-id on success, 0 on failure
 */
int do_pdb_query(str *number)
{
	struct pdb_msg msg;
	struct timeval tstart, tnow;
	struct server_item_t *server;
	short int carrierid, _id;
	short int _idv;
	char buf[sizeof(struct pdb_msg)];
	size_t reqlen;
	int i, ret, nflush, bytes_received;
	long int td;

	if((active == NULL) || (*active == 0))
		return 0;

	LM_DBG("querying '%.*s'...\n", number->len, number->s);
	if(server_list == NULL)
		return 0;
	if(server_list->fds == NULL)
		return 0;

	if(gettimeofday(&tstart, NULL) != 0) {
		LM_ERR("gettimeofday() failed with errno=%d (%s)\n", errno,
				strerror(errno));
		return 0;
	}

	/* clear recv buffer */
	server = server_list->head;
	while(server) {
		nflush = 0;
		while(recv(server->sock, buf, sizeof(struct pdb_msg), MSG_DONTWAIT)
				> 0) {
			nflush++;
			if(gettimeofday(&tnow, NULL) != 0) {
				LM_ERR("gettimeofday() failed with errno=%d (%s)\n", errno,
						strerror(errno));
				return 0;
			}
			td = (tnow.tv_usec - tstart.tv_usec
						 + (tnow.tv_sec - tstart.tv_sec) * 1000000)
				 / 1000;
			if(td > cfg_get(pdb, pdb_cfg, timeout)) {
				LM_ERR("exceeded %d ms timeout while flushing recv buffer. "
					   "queried nr '%.*s'.\n",
						cfg_get(pdb, pdb_cfg, timeout), number->len, number->s);
				return 0;
			}
		}
		LM_DBG("flushed %d packets for '%s:%d'\n", nflush, server->host,
				server->port);
		server = server->next;
	}

	/* prepare request */
	reqlen = number->len + 1; /* include null termination */
	if(reqlen > PAYLOADSIZE) {
		LM_ERR("number too long '%.*s'.\n", number->len, number->s);
		return 0;
	}
	strncpy(buf, number->s, number->len);
	buf[number->len] = '\0';

	switch(PDB_VERSION) {
		case PDB_VERSION_1:
			pdb_msg_format_send(&msg, PDB_VERSION, PDB_TYPE_REQUEST_ID,
					PDB_CODE_DEFAULT, htons(*global_id), buf, reqlen);
			pdb_msg_dbg(&msg, "Kamailio pdb client sends:");

			/* increment msg id for the next request */
			*global_id = *global_id + 1;

			/* send request to all servers */
			server = server_list->head;
			while(server) {
				LM_DBG("sending request to '%s:%d'\n", server->host,
						server->port);
				ret = sendto(server->sock, (struct pdb_msg *)&msg,
						msg.hdr.length, MSG_DONTWAIT,
						(struct sockaddr *)&(server->dstaddr),
						server->dstaddrlen);
				if(ret < 0) {
					LM_ERR("sendto() failed with errno=%d (%s)\n", errno,
							strerror(errno));
				}
				server = server->next;
			}
			break;
		default:
			/* send request to all servers */
			server = server_list->head;
			while(server) {
				LM_DBG("sending request to '%s:%d'\n", server->host,
						server->port);
				ret = sendto(server->sock, buf, reqlen, MSG_DONTWAIT,
						(struct sockaddr *)&(server->dstaddr),
						server->dstaddrlen);
				if(ret < 0) {
					LM_ERR("sendto() failed with errno=%d (%s)\n", errno,
							strerror(errno));
				}
				server = server->next;
			}
			break;
	}

	memset(&msg, 0, sizeof(struct pdb_msg));
	/* wait for response */
	for(;;) {
		if(gettimeofday(&tnow, NULL) != 0) {
			LM_ERR("gettimeofday() failed with errno=%d (%s)\n", errno,
					strerror(errno));
			return 0;
		}
		td = (tnow.tv_usec - tstart.tv_usec
					 + (tnow.tv_sec - tstart.tv_sec) * 1000000)
			 / 1000;
		if(td > cfg_get(pdb, pdb_cfg, timeout)) {
			timeoutlogs++;
			if(timeoutlogs < 0) {
				LM_ERR("exceeded %d ms timeout while waiting for response. "
					   "queried nr '%.*s'.\n",
						cfg_get(pdb, pdb_cfg, timeout), number->len, number->s);
			} else if(timeoutlogs > 1000) {
				LM_ERR("exceeded %d ms timeout %d times while waiting for "
					   "response. queried nr '%.*s'.\n",
						cfg_get(pdb, pdb_cfg, timeout), timeoutlogs,
						number->len, number->s);
				timeoutlogs = 0;
			}
			return 0;
		}

		ret = poll(server_list->fds, server_list->nserver,
				cfg_get(pdb, pdb_cfg, timeout) - td);
		for(i = 0; i < server_list->nserver; i++) {
			if(server_list->fds[i].revents & POLLIN) {
				if((bytes_received = recv(server_list->fds[i].fd, buf,
							sizeof(struct pdb_msg), MSG_DONTWAIT))
						> 0) { /* do not block - just in case select/poll was wrong */
					switch(PDB_VERSION) {
						case PDB_VERSION_1:
							memcpy(&msg, buf, bytes_received);
							pdb_msg_dbg(&msg, "Kamailio pdb client receives:");

							_idv = msg.hdr.id; /* make gcc happy */
							msg.hdr.id = ntohs(_idv);

							switch(msg.hdr.code) {
								case PDB_CODE_OK:
									msg.bdy.payload[sizeof(struct pdb_bdy)
													- 1] = '\0';
									if(strcmp(msg.bdy.payload, number->s)
											== 0) {
										PDB_BUFTOSHORT(_id, msg.bdy.payload,
												reqlen); /* make gcc happy */
										carrierid = ntohs(
												_id); /* convert to host byte order */
										goto found;
									}
									break;
								case PDB_CODE_NOT_FOUND:
									LM_NOTICE("Number %s pdb_id not found\n",
											number->s);
									carrierid = -1;
									goto found;
								case PDB_CODE_NOT_NUMBER:
									LM_NOTICE("Number %s has letters in it\n",
											number->s);
									carrierid = -2;
									goto found;
								default:
									LM_NOTICE("Invalid code %d received\n",
											msg.hdr.code);
									carrierid = -3;
									goto found;
							}

							break;
						default:
							buf[sizeof(struct pdb_msg) - 1] = '\0';
							if(strncmp(buf, number->s, number->len) == 0) {
								PDB_BUFTOSHORT(
										_id, buf, reqlen); /* make gcc happy */
								carrierid = ntohs(
										_id); /* convert to host byte order */
								goto found;
							}
							break;
					}
				}
			}
			server_list->fds[i].revents = 0;
		}
	}

found:
	if(timeoutlogs > 0) {
		LM_ERR("exceeded %d timeout while waiting for response (buffered %d "
			   "lines). queried nr '%.*s'.\n",
				cfg_get(pdb, pdb_cfg, timeout), timeoutlogs, number->len,
				number->s);
		timeoutlogs = -10;
	}
	if(gettimeofday(&tnow, NULL) == 0) {
		LLM_INFO("got an answer in %f ms\n",
				((double)(tnow.tv_usec - tstart.tv_usec
						  + (tnow.tv_sec - tstart.tv_sec) * 1000000))
						/ 1000);
	}
	return carrierid;
}


/**
 * Fixes the module functions' parameters.
 *
 * @param arg the parameter
 * @param arg_no the number of the parameter
 *
 * @return 0 on success, -1 on failure
 */
int pdb_query_fixup(void **arg, int arg_no)
{
	if(arg_no == 1) {
		/* phone number */
		if(fixup_spve_null(arg, 1) != 0) {
			LM_ERR("cannot fixup parameter %d\n", arg_no);
			return -1;
		}
	} else if(arg_no == 2) {
		/* destination avp name */
		if(fixup_pvar_null(arg, 1) != 0) {
			LM_ERR("cannot fixup parameter %d\n", arg_no);
			return -1;
		}
		if(((pv_spec_t *)(*arg))->setf == NULL) {
			LM_ERR("dst var is not writeble\n");
			return -1;
		}
	}

	return 0;
}


/**
 *
 */
int pdb_query_fixup_free(void **arg, int arg_no)
{
	if(arg_no == 1) {
		/* phone number */
		return fixup_free_spve_null(arg, 1);
	}

	if(arg_no == 2) {
		/* destination var name */
		return fixup_free_pvar_null(arg, 1);
	}

	LM_ERR("invalid parameter number <%d>\n", arg_no);
	return -1;
}


/*!
 * Adds new server structure to server list.
 * \return 0 on success -1 otherwise
 */
static int add_server(char *host, char *port)
{
	long int ret;
	struct server_item_t *server;

	LM_DBG("adding server '%s:%s'\n", host, port);
	server = pkg_malloc(sizeof(struct server_item_t));
	if(server == NULL) {
		PKG_MEM_ERROR;
		return -1;
	}
	memset(server, 0, sizeof(struct server_item_t));

	server->next = server_list->head;
	server_list->head = server;

	server->host = pkg_malloc(strlen(host) + 1);
	if(server->host == NULL) {
		PKG_MEM_ERROR;
		pkg_free(server);
		return -1;
	}
	strcpy(server->host, host);

	ret = strtol(port, NULL, 10);
	if((ret < 0) || (ret > 65535)) {
		LM_ERR("invalid port '%s'\n", port);
		return -1;
	}
	server->port = ret;

	return 0;
}


/*!
 * Prepares data structures for all configured servers.
 *  \return 0 on success, -1 otherwise
 */
static int prepare_server(void)
{
	char *p, *dst, *end, *sep, *host, *port;

	if(modp_server == NULL) {
		LM_ERR("server parameter missing.\n");
		return -1;
	}

	/* Remove white space from db_sources */
	for(p = modp_server, dst = modp_server; *p != '\0'; ++p, ++dst) {
		while(isspace(*p))
			++p;
		*dst = *p;
	}
	*dst = '\0';

	p = modp_server;
	end = p + strlen(p);

	while(p < end) {
		sep = strchr(p, ':');
		if(sep == NULL) {
			LM_ERR("syntax error in sources parameter.\n");
			return -1;
		}
		host = p;
		*sep = '\0';
		p = sep + 1;

		sep = strchr(p, ',');
		if(sep == NULL)
			sep = end;
		port = p;
		*sep = '\0';
		p = sep + 1;

		if(add_server(host, port) != 0)
			return -1;
	}

	return 0;
}


static void destroy_server_list(void)
{
	if(server_list) {
		while(server_list->head) {
			struct server_item_t *server = server_list->head;
			server_list->head = server->next;
			if(server->host)
				pkg_free(server->host);
			pkg_free(server);
		}
		pkg_free(server_list);
		server_list = NULL;
	}
}


/*!
 * Allocates memory and builds a list of all servers defined in module parameter.
 * \return 0 on success, -1 otherwise
 */
static int init_server_list(void)
{
	server_list = pkg_malloc(sizeof(struct server_list_t));
	if(server_list == NULL) {
		PKG_MEM_ERROR;
		return -1;
	}
	memset(server_list, 0, sizeof(struct server_list_t));

	if(prepare_server() != 0) {
		destroy_server_list();
		return -1;
	}

	return 0;
}


/*!
 * Initializes sockets for all servers in server list.
 * \return 0 on success, -1 otherwise
 */
static int init_server_socket(void)
{
	struct server_item_t *server;
	struct hostent *hp;
	int i;

	if(server_list) {
		server_list->nserver = 0;
		server = server_list->head;
		while(server) {
			LM_DBG("initializing socket for '%s:%d'\n", server->host,
					server->port);
			server->sock = socket(AF_INET, SOCK_DGRAM, 0);
			if(server->sock < 0) {
				LM_ERR("socket() failed with errno=%d (%s).\n", errno,
						strerror(errno));
				return -1;
			}

			memset(&(server->dstaddr), 0, sizeof(server->dstaddr));
			server->dstaddr.sin_family = AF_INET;
			server->dstaddr.sin_port = htons(server->port);
			hp = gethostbyname(server->host);
			if(hp == NULL) {
				LM_ERR("gethostbyname(%s) failed with h_errno=%d.\n",
						server->host, h_errno);
				close(server->sock);
				server->sock = 0;
				return -1;
			}
			memcpy(&(server->dstaddr.sin_addr.s_addr), hp->h_addr,
					hp->h_length);
			server->dstaddrlen = sizeof(server->dstaddr);

			server = server->next;
			server_list->nserver++;
		}

		LM_DBG("got %d server in list\n", server_list->nserver);
		server_list->fds =
				pkg_malloc(sizeof(struct pollfd) * server_list->nserver);
		if(server_list->fds == NULL) {
			PKG_MEM_ERROR;
			return -1;
		}
		memset(server_list->fds, 0,
				sizeof(struct pollfd) * server_list->nserver);

		i = 0;
		server = server_list->head;
		while(server) {
			server_list->fds[i].fd = server->sock;
			server_list->fds[i].events = POLLIN;
			server = server->next;
			i++;
		}
	}
	return 0;
}


/*!
 * Destroys sockets for all servers in server list.
 */
static void destroy_server_socket(void)
{
	if(server_list) {
		struct server_item_t *server = server_list->head;
		while(server) {
			if(server->sock > 0)
				close(server->sock);
			server = server->next;
		}
		if(server_list->fds)
			pkg_free(server_list->fds);
	}
}


static void pdb_rpc_status(rpc_t *rpc, void *ctx)
{
	void *vh;
	if(active == NULL) {
		rpc->fault(ctx, 500, "Active field not initialized");
		return;
	}
	if(rpc->add(ctx, "{", &vh) < 0) {
		rpc->fault(ctx, 500, "Server error");
		return;
	}
	rpc->struct_add(vh, "ds", "active", *active, "status",
			(*active) ? "active" : "inactive");
}

static void pdb_rpc_timeout(rpc_t *rpc, void *ctx)
{
	void *vh;
	int new_val = -1;
	int old_val = cfg_get(pdb, pdb_cfg, timeout);

	str gname = str_init("pdb");
	str vname = str_init("timeout");

	if(rpc->add(ctx, "{", &vh) < 0) {
		rpc->fault(ctx, 500, "Server error");
		return;
	}

	if(rpc->scan(ctx, "*d", &new_val) < 1) {
		if(rpc->struct_add(vh, "d", "timeout", old_val) < 0) {
			rpc->fault(ctx, 500, "Internal error reply structure");
		}
		return;
	}

	if(rpc->struct_add(vh, "dd", "old_timeout", old_val, "new_timeout", new_val)
			< 0) {
		rpc->fault(ctx, 500, "Internal error reply structure");
		return;
	}

	cfg_set_now_int(ctx, &gname, NULL, &vname, new_val);
}

static void pdb_rpc_activate(rpc_t *rpc, void *ctx)
{
	if(active == NULL) {
		rpc->fault(ctx, 500, "Active field not initialized");
		return;
	}
	*active = 1;
}

static void pdb_rpc_deactivate(rpc_t *rpc, void *ctx)
{
	if(active == NULL) {
		rpc->fault(ctx, 500, "Active field not initialized");
		return;
	}
	*active = 0;
}

static const char *pdb_rpc_status_doc[2] = {"Get the pdb status.", 0};

static const char *pdb_rpc_timeout_doc[2] = {"Set the pdb_query timeout.", 0};

static const char *pdb_rpc_activate_doc[2] = {"Activate pdb.", 0};

static const char *pdb_rpc_deactivate_doc[2] = {"Deactivate pdb.", 0};

rpc_export_t pdb_rpc[] = {{"pdb.status", pdb_rpc_status, pdb_rpc_status_doc, 0},
		{"pdb.timeout", pdb_rpc_timeout, pdb_rpc_timeout_doc, 0},
		{"pdb.activate", pdb_rpc_activate, pdb_rpc_activate_doc, 0},
		{"pdb.deactivate", pdb_rpc_deactivate, pdb_rpc_deactivate_doc, 0},
		{0, 0, 0, 0}};

static int pdb_rpc_init(void)
{
	if(rpc_register_array(pdb_rpc) != 0) {
		LM_ERR("failed to register RPC commands\n");
		return -1;
	}
	return 0;
}

static int mod_init(void)
{
	if(pdb_rpc_init() < 0) {
		LM_ERR("failed to register RPC commands\n");
		return -1;
	}
	active = shm_malloc(sizeof(*active));
	if(active == NULL) {
		SHM_MEM_ERROR;
		return -1;
	}
	*active = 1;

	if(init_server_list() != 0) {
		shm_free(active);
		return -1;
	}

	global_id = (uint16_t *)shm_malloc(sizeof(uint16_t));
	if(!global_id) {
		SHM_MEM_ERROR;
		shm_free(active);
		return -1;
	}

	if(cfg_declare("pdb", pdb_cfg_def, &default_pdb_cfg, cfg_sizeof(pdb),
			   &pdb_cfg)) {
		LM_ERR("Failed to declare the configuration\n");
		return -1;
	}

	return 0;
}

static int child_init(int rank)
{
	if(rank == PROC_INIT || rank == PROC_TCP_MAIN)
		return 0;
	return rpc_child_init();
}


static int pdb_child_initialized = 0;

static int rpc_child_init(void)
{
	if(pdb_child_initialized)
		return 0;
	if(init_server_socket() != 0)
		return -1;
	pdb_child_initialized = 1;
	return 0;
}


static void mod_destroy(void)
{
	destroy_server_socket();
	destroy_server_list();
	if(active)
		shm_free(active);
}


/**
 *
 */
/* clang-format off */
static sr_kemi_t sr_kemi_pdb_exports[] = {
    { str_init("pdb"), str_init("pdb_query"),
        SR_KEMIP_INT, ki_pdb_query,
        { SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_NONE,
            SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
    },

    { {0, 0}, {0, 0}, 0, NULL, { 0, 0, 0, 0, 0, 0 } }
};
/* clang-format on */


/**
 *
 */
int mod_register(char *path, int *dlflags, void *p1, void *p2)
{
	sr_kemi_modules_add(sr_kemi_pdb_exports);
	return 0;
}
