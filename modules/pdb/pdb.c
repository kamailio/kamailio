/*
 * Copyright (C) 2009 1&1 Internet AG
 *
 * This file is part of sip-router, a free SIP server.
 *
 * sip-router is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * sip-router is distributed in the hope that it will be useful,
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


#include "../../sr_module.h"
#include "../../mem/mem.h"
#include "../../mem/shm_mem.h"
#include "../../lib/kmi/mi.h"
#include <sys/time.h>
#include <poll.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>

MODULE_VERSION


#define NETBUFSIZE 200


static char* modp_server = NULL;  /*!< format: \<host\>:\<port\>,... */
static int timeout = 50;  /*!< timeout for queries in milliseconds */
static int timeoutlogs = -10;  /*!< for aggregating timeout logs */
static int *active = NULL;


/*!
 * Generic parameter that holds a string, an int or an pseudo-variable
 * @todo replace this with gparam_t
 */
struct multiparam_t {
	enum {
		MP_INT,
		MP_STR,
		MP_AVP,
		MP_PVE,
	} type;
	union {
		int n;
		str s;
		struct {
			unsigned short flags;
			int_str name;
		} a;
		pv_elem_t *p;
	} u;
};


/* ---- exported commands: */
static int pdb_query(struct sip_msg *_msg, struct multiparam_t *_number, struct multiparam_t *_dstavp);

/* ---- fixup functions: */
static int pdb_query_fixup(void **arg, int arg_no);

/* ---- module init functions: */
static int mod_init(void);
static int child_init(int rank);
static int mi_child_init(void);
static void mod_destroy();

/* --- fifo functions */
struct mi_root * mi_pdb_status(struct mi_root* cmd, void* param);  /* usage: kamctl fifo pdb_status */
struct mi_root * mi_pdb_activate(struct mi_root* cmd, void* param);  /* usage: kamctl fifo pdb_activate */
struct mi_root * mi_pdb_deactivate(struct mi_root* cmd, void* param);  /* usage: kamctl fifo pdb_deactivate */


static cmd_export_t cmds[]={
	{ "pdb_query", (cmd_function)pdb_query, 2, pdb_query_fixup, 0, REQUEST_ROUTE | FAILURE_ROUTE },
	{0,0,0,0,0,0}
};


static param_export_t params[] = {
	{"server",      PARAM_STRING, &modp_server },
	{"timeout",     INT_PARAM, &timeout },
	{0, 0, 0 }
};


/* Exported MI functions */
static mi_export_t mi_cmds[] = {
	{ "pdb_status", mi_pdb_status, MI_NO_INPUT_FLAG, 0, mi_child_init },
	{ "pdb_activate", mi_pdb_activate, MI_NO_INPUT_FLAG, 0, mi_child_init },
	{ "pdb_deactivate", mi_pdb_deactivate, MI_NO_INPUT_FLAG, 0, mi_child_init },
	{ 0, 0, 0, 0, 0}
};


struct module_exports exports = {
	"pdb",
	DEFAULT_DLFLAGS, /* dlopen flags */
	cmds,       /* Exported functions */
	params,     /* Export parameters */
	0,          /* exported statistics */
	mi_cmds,    /* exported MI functions */
	0,          /* exported pseudo-variables */
	0,          /* extra processes */
	mod_init,   /* Module initialization function */
	0,          /* Response function */
	mod_destroy,/* Destroy function */
	child_init  /* Child initialization function */
};


struct server_item_t {
	struct server_item_t *next;
	char *host;
	unsigned short int port;
	struct sockaddr_in dstaddr;
	socklen_t dstaddrlen;
	int sock;
};


struct server_list_t {
	struct server_item_t *head;
	int nserver;
	struct pollfd *fds;
};


/*! global server list */
static struct server_list_t *server_list;


/*!
 * \return 1 if query for the number succeded and the avp with the corresponding carrier id was set,
 * -1 otherwise
 */
static int pdb_query(struct sip_msg *_msg, struct multiparam_t *_number, struct multiparam_t *_dstavp)
{
	struct timeval tstart, tnow;
	struct server_item_t *server;
	short int carrierid, *_id;
	char buf[NETBUFSIZE+1+sizeof(carrierid)];
	size_t reqlen;
	int_str avp_val;
	struct usr_avp *avp;
	int i, ret, nflush;
	long int td;
	str number = STR_NULL;

	if ((active == NULL) || (*active == 0)) return -1;

	switch (_number->type) {
	case MP_STR:
		number = _number->u.s;
		break;
	case MP_AVP:
		avp = search_first_avp(_number->u.a.flags, _number->u.a.name, &avp_val, 0);
		if (!avp) {
			LM_ERR("cannot find AVP '%.*s'\n", _number->u.a.name.s.len, _number->u.a.name.s.s);
			return -1;
		}
		if ((avp->flags&AVP_VAL_STR)==0) {
			LM_ERR("cannot process integer value in AVP '%.*s'\n", _number->u.a.name.s.len, _number->u.a.name.s.s);
			return -1;
		}
		else number = avp_val.s;
		break;
	case MP_PVE:
		if (pv_printf_s(_msg, _number->u.p, &number)<0) {
			LM_ERR("cannot print the number\n");
			return -1;
		}
		break;
	default:
		LM_ERR("invalid number type\n");
		return -1;
	}

	LM_DBG("querying '%.*s'...\n", number.len, number.s);
	if (server_list == NULL) return -1;
	if (server_list->fds == NULL) return -1;

	if (gettimeofday(&tstart, NULL) != 0) {
		LM_ERR("gettimeofday() failed with errno=%d (%s)\n", errno, strerror(errno));
		return -1;
	}

	/* clear recv buffer */
	server = server_list->head;
	while (server) {
		nflush = 0;
		while (recv(server->sock, buf, NETBUFSIZE, MSG_DONTWAIT) > 0) {
			nflush++;
			if (gettimeofday(&tnow, NULL) != 0) {
				LM_ERR("gettimeofday() failed with errno=%d (%s)\n", errno, strerror(errno));
				return -1;
			}
			td=(tnow.tv_usec-tstart.tv_usec+(tnow.tv_sec-tstart.tv_sec)*1000000) / 1000;
			if (td > timeout) {
				LM_WARN("exceeded timeout while flushing recv buffer.\n");
				return -1;
			}
		}
		LM_DBG("flushed %d packets for '%s:%d'\n", nflush, server->host, server->port);
		server = server ->next;
	}

	/* prepare request */
	reqlen = number.len + 1; /* include null termination */
	if (reqlen > NETBUFSIZE) {
		LM_ERR("number too long '%.*s'.\n", number.len, number.s);
		return -1;
	}
	strncpy(buf, number.s, number.len);
	buf[number.len] = '\0';

	/* send request to all servers */
	server = server_list->head;
	while (server) {
		LM_DBG("sending request to '%s:%d'\n", server->host, server->port);
		ret=sendto(server->sock, buf, reqlen, MSG_DONTWAIT, (struct sockaddr *)&(server->dstaddr), server->dstaddrlen);
		if (ret < 0) {
			LM_ERR("sendto() failed with errno=%d (%s)\n", errno, strerror(errno));
		}
		server = server->next;
	}
		
	/* wait for response */
	for (;;) {
		if (gettimeofday(&tnow, NULL) != 0) {
			LM_ERR("gettimeofday() failed with errno=%d (%s)\n", errno, strerror(errno));
			return -1;
		}
		td=(tnow.tv_usec-tstart.tv_usec+(tnow.tv_sec-tstart.tv_sec)*1000000) / 1000;
		if (td > timeout) {
			timeoutlogs++;
			if (timeoutlogs<0) {
				LM_WARN("exceeded timeout while waiting for response.\n");
			}
			else if (timeoutlogs>1000) {
				LM_WARN("exceeded timeout %d times while waiting for response.\n", timeoutlogs);
				timeoutlogs=0;
			}
			return -1;
		}
		
		ret=poll(server_list->fds, server_list->nserver, timeout-td);
		for (i=0; i<server_list->nserver; i++) {
			if (server_list->fds[i].revents & POLLIN) {
				if (recv(server_list->fds[i].fd, buf, NETBUFSIZE, MSG_DONTWAIT) > 0) { /* do not block - just in case select/poll was wrong */
					buf[NETBUFSIZE] = '\0';
					if (strncmp(buf, number.s, number.len) == 0) {
						_id = (short int *)&(buf[reqlen]);
						carrierid=ntohs(*_id); /* convert to host byte order */
						goto found;
					}
				}
			}
			server_list->fds[i].revents = 0;
		}
	}

	found:
	if (timeoutlogs>0) {
		LM_WARN("exceeded timeout while waiting for response (buffered %d lines).\n", timeoutlogs);
		timeoutlogs=-10;
	}
	if (gettimeofday(&tnow, NULL) == 0) {
		LM_INFO("got an answer in %f ms\n", ((double)(tnow.tv_usec-tstart.tv_usec+(tnow.tv_sec-tstart.tv_sec)*1000000))/1000);
	}
	avp_val.n=carrierid;
	/* set avp ! */
	if (add_avp(_dstavp->u.a.flags, _dstavp->u.a.name, avp_val)<0) {
		LM_ERR("add AVP failed\n");
		return -1;
	}

	return 1;
}


/*!
 * fixes the module functions' parameters if it is a phone number.
 * supports string, pseudo-variables and AVPs.
 *
 * @param param the parameter
 * @return 0 on success, -1 on failure
 */
static int mp_fixup(void ** param) {
	pv_spec_t avp_spec;
	struct multiparam_t *mp;
	str s;

	mp = (struct multiparam_t *)pkg_malloc(sizeof(struct multiparam_t));
	if (mp == NULL) {
		PKG_MEM_ERROR;
		return -1;
	}
	memset(mp, 0, sizeof(struct multiparam_t));
	
	s.s = (char *)(*param);
	s.len = strlen(s.s);

	if (s.s[0]!='$') {
		/* This is string */
		mp->type=MP_STR;
		mp->u.s=s;
	}
	else {
		/* This is a pseudo-variable */
		if (pv_parse_spec(&s, &avp_spec)==0) {
			LM_ERR("pv_parse_spec failed for '%s'\n", (char *)(*param));
			pkg_free(mp);
			return -1;
		}
		if (avp_spec.type==PVT_AVP) {
			/* This is an AVP - could be an id or name */
			mp->type=MP_AVP;
			if(pv_get_avp_name(0, &(avp_spec.pvp), &(mp->u.a.name), &(mp->u.a.flags))!=0) {
				LM_ERR("Invalid AVP definition <%s>\n", (char *)(*param));
				pkg_free(mp);
				return -1;
			}
		} else {
			mp->type=MP_PVE;
			if(pv_parse_format(&s, &(mp->u.p))<0) {
				LM_ERR("pv_parse_format failed for '%s'\n", (char *)(*param));
				pkg_free(mp);
				return -1;
			}
		}
	}
	*param = (void*)mp;

	return 0;
}


/*!
 * fixes the module functions' parameters in case of AVP names.
 *
 * @param param the parameter
 * @return 0 on success, -1 on failure
 */
static int avp_name_fixup(void ** param) {
	pv_spec_t avp_spec;
	struct multiparam_t *mp;
	str s;

	s.s = (char *)(*param);
	s.len = strlen(s.s);
	if (s.len <= 0) return -1;
	if (pv_parse_spec(&s, &avp_spec)==0 || avp_spec.type!=PVT_AVP) {
		LM_ERR("Malformed or non AVP definition <%s>\n", (char *)(*param));
		return -1;
	}
	
	mp = (struct multiparam_t *)pkg_malloc(sizeof(struct multiparam_t));
	if (mp == NULL) {
		PKG_MEM_ERROR;
		return -1;
	}
	memset(mp, 0, sizeof(struct multiparam_t));
	
	mp->type=MP_AVP;
	if(pv_get_avp_name(0, &(avp_spec.pvp), &(mp->u.a.name), &(mp->u.a.flags))!=0) {
		LM_ERR("Invalid AVP definition <%s>\n", (char *)(*param));
		pkg_free(mp);
		return -1;
	}

	*param = (void*)mp;
	
	return 0;
}


static int pdb_query_fixup(void **arg, int arg_no)
{
	if (arg_no == 1) {
		/* phone number */
		if (mp_fixup(arg) < 0) {
			LM_ERR("cannot fixup parameter %d\n", arg_no);
			return -1;
		}
	}
	else if (arg_no == 2) {
		/* destination avp name */
		if (avp_name_fixup(arg) < 0) {
			LM_ERR("cannot fixup parameter %d\n", arg_no);
			return -1;
		}
	}

	return 0;
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
	server= pkg_malloc(sizeof(struct server_item_t));
	if (server == NULL) {
		PKG_MEM_ERROR;
		return -1;
	}
	memset(server, 0, sizeof(struct server_item_t));

	server->next = server_list->head;
	server_list->head = server;

	server->host = pkg_malloc(strlen(host)+1);
	if (server->host == NULL) {
		PKG_MEM_ERROR;
		return -1;
	}
	strcpy(server->host, host);

	ret=strtol(port, NULL, 10);
	if ((ret<0) || (ret>65535)) {
		LM_ERR("invalid port '%s'\n", port);
		return -1;
	}
	server->port=ret;

	return 0;
}


/*!
 * Prepares data structures for all configured servers.
 *  \return 0 on success, -1 otherwise
 */
static int prepare_server(void)
{
	char *p, *dst, *end, *sep, *host, *port;

	if (modp_server == NULL) {
		LM_ERR("server parameter missing.\n");
		return -1;
	}

	/* Remove white space from db_sources */
	for (p = modp_server, dst = modp_server; *p != '\0'; ++p, ++dst) {
		while (isspace(*p)) ++p;
		*dst = *p;
	}
	*dst = '\0';

	p = modp_server;
	end = p + strlen(p);

	while (p < end) {
		sep = strchr(p, ':');
		if (sep == NULL) {
			LM_ERR("syntax error in sources parameter.\n");
			return -1;
		}
		host = p;
		*sep = '\0';
		p = sep + 1;

		sep = strchr(p, ',');
		if (sep == NULL) sep = end;
		port = p;
		*sep = '\0';
		p = sep + 1;

		if (add_server(host, port) != 0)  return -1;
	}

	return 0;
}


static void destroy_server_list(void)
{
	if (server_list) {
		while (server_list->head) {
			struct server_item_t *server = server_list->head;
			server_list->head = server->next;
			if (server->host) pkg_free(server->host);
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
	if (server_list == NULL) {
		PKG_MEM_ERROR;
		return -1;
	}
	memset(server_list, 0, sizeof(struct server_list_t));

	if (prepare_server() != 0) {
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

	if (server_list) {
		server_list->nserver=0;
		server = server_list->head;
		while (server) {
			LM_DBG("initializing socket for '%s:%d'\n", server->host, server->port);
			server->sock = socket(AF_INET, SOCK_DGRAM, 0);
			if (server->sock<0) {
				LM_ERR("socket() failed with errno=%d (%s).\n", errno, strerror(errno));
				return -1;
			}

			memset(&(server->dstaddr), 0, sizeof(server->dstaddr));
			server->dstaddr.sin_family = AF_INET;
			server->dstaddr.sin_port = htons(server->port);
			hp = gethostbyname(server->host);
			if (hp == NULL) {
				LM_ERR("gethostbyname(%s) failed with h_errno=%d.\n", server->host, h_errno);
				close(server->sock);
				server->sock=0;
				return -1;
			}
			memcpy(&(server->dstaddr.sin_addr.s_addr), hp->h_addr, hp->h_length);
			server->dstaddrlen=sizeof(server->dstaddr);

			server = server->next;
			server_list->nserver++;
		}

		LM_DBG("got %d server in list\n", server_list->nserver);
		server_list->fds = pkg_malloc(sizeof(struct pollfd)*server_list->nserver);
		if (server_list->fds == NULL) {
			PKG_MEM_ERROR;
			return -1;
		}
		memset(server_list->fds, 0, sizeof(struct pollfd)*server_list->nserver);

		i=0;
		server = server_list->head;
		while (server) {
			server_list->fds[i].fd=server->sock;
			server_list->fds[i].events=POLLIN;
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
	if (server_list) {
		struct server_item_t *server = server_list->head;
		while (server) {
			if (server->sock>0) close(server->sock);
			server = server->next;
		}
		if (server_list->fds) pkg_free(server_list->fds);
	}
}


struct mi_root * mi_pdb_status(struct mi_root* cmd, void* param)
{
	struct mi_root * root = NULL;
	struct mi_node * node = NULL;

	if (active == NULL) return init_mi_tree(500, "NULL pointer", 12);

	root = init_mi_tree( 200, MI_OK_S, MI_OK_LEN);
	if (root == NULL) return NULL;

	if (*active) node = addf_mi_node_child(&root->node, 0, 0, 0, "pdb is active");
	else node = addf_mi_node_child(&root->node, 0, 0, 0, "pdb is deactivated");
	if (node == NULL) {
		free_mi_tree(root);
		return NULL;
	}

	return root;
}


struct mi_root * mi_pdb_deactivate(struct mi_root* cmd, void* param)
{
	if (active == NULL) return init_mi_tree(500, "NULL pointer", 12);

	*active=0;
	return init_mi_tree(200, MI_OK_S, MI_OK_LEN);
}


struct mi_root * mi_pdb_activate(struct mi_root* cmd, void* param)
{
	if (active == NULL) return init_mi_tree(500, "NULL pointer", 12);

	*active=1;
	return init_mi_tree( 200, MI_OK_S, MI_OK_LEN);
}


static int mod_init(void)
{
	active = shm_malloc(sizeof(*active));
	if (active == NULL) {
		SHM_MEM_ERROR;
		return -1;
	}
	*active=1;

	if (init_server_list() != 0) {
		shm_free(active);
		return -1;
	}
	return 0;
}

static int child_init (int rank)
{
	if(rank==PROC_INIT || rank==PROC_TCP_MAIN)
		return 0;
	return mi_child_init();
}


static int pdb_child_initialized = 0;

static int mi_child_init(void)
{
	if(pdb_child_initialized)
		return 0;
	if (init_server_socket() != 0) return -1;
	pdb_child_initialized = 1;
	return 0;
}


static void mod_destroy(void)
{
	destroy_server_socket();
	destroy_server_list();
	if (active) shm_free(active);
}
