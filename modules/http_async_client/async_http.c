/**
 * Copyright 2016 (C) Federico Cabiddu <federico.cabiddu@gmail.com>
 * Copyright 2016 (C) Giacomo Vacca <giacomo.vacca@gmail.com>
 * Copyright 2016 (C) Orange - Camille Oudot <camille.oudot@orange.com>
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 *
 * This file is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

/*! \file
 * \brief  Kamailio http_async_client :: Include file
 * \ingroup http_async_client
 */


#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>

#include <event2/event.h>

#include "../../sr_module.h"
#include "../../dprint.h"
#include "../../ut.h"
#include "../../cfg/cfg_struct.h"
#include "../../lib/kcore/faked_msg.h"
#include "../../modules/tm/tm_load.h"

#include "async_http.h"

/* tm */
extern struct tm_binds tmb;

struct sip_msg *ah_reply = NULL;
str ah_error = {NULL, 0};

async_http_worker_t *workers;
int num_workers = 1;

struct query_params ah_params;

int async_http_init_worker(int prank, async_http_worker_t* worker)
{
	LM_DBG("initializing worker process: %d\n", prank);
	worker->evbase = event_base_new();
	LM_DBG("base event %p created\n", worker->evbase);

	worker->g = shm_malloc(sizeof(struct http_m_global));
	memset(worker->g, 0, sizeof(http_m_global_t));
	LM_DBG("initialized global struct %p\n", worker->g);

	init_socket(worker);

	LM_INFO("started worker process: %d\n", prank);

	return 0;
}

void async_http_run_worker(async_http_worker_t* worker)
{
	init_http_multi(worker->evbase, worker->g);
	event_base_dispatch(worker->evbase);
}

int async_http_init_sockets(async_http_worker_t *worker)
{
	if (socketpair(PF_UNIX, SOCK_DGRAM, 0, worker->notication_socket) < 0) {
		LM_ERR("opening tasks dgram socket pair\n");
		return -1;
	}
	LM_INFO("inter-process event notification sockets initialized\n");
	return 0;
}

static inline char *strfindcasestrz(str *haystack, char *needlez)                                                                                                                                                                                                              
{
    int i,j;
    str needle;

    needle.s = needlez;
    needle.len = strlen(needlez);
    for(i=0;i<haystack->len-needle.len;i++) {
        for(j=0;j<needle.len;j++) {
            if ( !((haystack->s[i+j]==needle.s[j]) ||
                    ( isalpha((int)haystack->s[i+j])
                        && ((haystack->s[i+j])^(needle.s[j]))==0x20 )) )
                break;
        }
        if (j==needle.len)
            return haystack->s+i;
    }
    return 0;
}

void async_http_cb(struct http_m_reply *reply, void *param)
{
	async_query_t *aq;
	cfg_action_t *act;
	unsigned int tindex;
	unsigned int tlabel;
	struct cell *t = NULL;
	char *p;
	sip_msg_t *fmsg;

	if (reply->result != NULL) {
		LM_DBG("query result = %.*s [%d]\n", reply->result->len, reply->result->s, reply->result->len);
	}

	/* clean process-local result variables */
	ah_error.s = NULL;
	ah_error.len = 0;
	memset(ah_reply, 0, sizeof(struct sip_msg));

	/* set process-local result variables */
	if (reply->result == NULL) {
		/* error */
		ah_error.s = reply->error;
		ah_error.len = strlen(ah_error.s);
	} else {
		/* success */
		
		/* check for HTTP Via header
     	 * - HTTP Via format is different that SIP Via
     	 * - workaround: replace with Hia to be ignored by SIP parser
     	 */
    	if((p=strfindcasestrz(reply->result, "\nVia:"))!=NULL)
    	{
        	p++;
        	*p = 'H';
        	LM_DBG("replaced HTTP Via with Hia [[\n%.*s]]\n", reply->result->len, reply->result->s);
    	}

		ah_reply->buf = reply->result->s;
		ah_reply->len = reply->result->len;

		if (parse_msg(reply->result->s, reply->result->len, ah_reply) != 0) {
			LM_DBG("failed to parse the http_reply\n");
		} else {
			LM_DBG("successfully parsed http reply %p\n", ah_reply);
		}
	}

	aq = param;
	act = (cfg_action_t*)aq->param;
	if (aq->query_params.suspend_transaction) {
		tindex = aq->tindex;
		tlabel = aq->tlabel;

		if (tmb.t_lookup_ident(&t, tindex, tlabel) < 0) {
			LM_ERR("transaction not found %d:%d\n", tindex, tlabel);
			LM_DBG("freeing query %p\n", aq);
			free_async_query(aq);
			return;
		}
		// we bring the list of AVPs of the transaction to the current context
		set_avp_list(AVP_TRACK_FROM | AVP_CLASS_URI, &t->uri_avps_from);
		set_avp_list(AVP_TRACK_TO | AVP_CLASS_URI, &t->uri_avps_to);
		set_avp_list(AVP_TRACK_FROM | AVP_CLASS_USER, &t->user_avps_from);
		set_avp_list(AVP_TRACK_TO | AVP_CLASS_USER, &t->user_avps_to);
		set_avp_list(AVP_TRACK_FROM | AVP_CLASS_DOMAIN, &t->domain_avps_from);
		set_avp_list(AVP_TRACK_TO | AVP_CLASS_DOMAIN, &t->domain_avps_to);

		if (t)
			tmb.unref_cell(t);

		LM_DBG("resuming transaction (%d:%d)\n", tindex, tlabel);

		if(act!=NULL)
			tmb.t_continue(tindex, tlabel, act);
	} else {
		fmsg = faked_msg_next();
		if (run_top_route(act, fmsg, 0)<0)
			LM_ERR("failure inside run_top_route\n");
	}

	free_sip_msg(ah_reply);
	free_async_query(aq);

	return;
}

void notification_socket_cb(int fd, short event, void *arg)
{
	(void)fd; /* unused */
	(void)event; /* unused */
	const async_http_worker_t *worker = (async_http_worker_t *) arg;

	int received;
	int i;
	async_query_t *aq;

	http_m_params_t query_params;

	str query;
	str post;

	if ((received = recvfrom(worker->notication_socket[0],
			&aq, sizeof(async_query_t*),
			0, NULL, 0)) < 0) {
		LM_ERR("failed to read from socket (%d: %s)\n", errno, strerror(errno));
		return;
	}

	if(received != sizeof(async_query_t*)) {
		LM_ERR("invalid query size %d\n", received);
		return;
	}

	query = ((str)aq->query);
	post = ((str)aq->post);

	query_params.timeout = aq->query_params.timeout;
	query_params.tls_verify_peer = aq->query_params.tls_verify_peer;
	query_params.tls_verify_host = aq->query_params.tls_verify_host;
	query_params.headers = NULL;
	for (i = 0 ; i < aq->query_params.headers.len ; i++) {
		query_params.headers = curl_slist_append(query_params.headers, aq->query_params.headers.t[i]);
	}
	query_params.method  = aq->query_params.method;

	query_params.tls_client_cert.s = NULL;
	query_params.tls_client_cert.len = 0;
	if (aq->query_params.tls_client_cert.s && aq->query_params.tls_client_cert.len > 0) {
		if (shm_str_dup(&query_params.tls_client_cert, &(aq->query_params.tls_client_cert)) < 0) {
			LM_ERR("Error allocating query_params.tls_client_cert\n");
			return;
		}
	}

	query_params.tls_client_key.s = NULL;
	query_params.tls_client_key.len = 0;
	if (aq->query_params.tls_client_key.s && aq->query_params.tls_client_key.len > 0) {
		if (shm_str_dup(&query_params.tls_client_key, &(aq->query_params.tls_client_key)) < 0) {
			LM_ERR("Error allocating query_params.tls_client_key\n");
			return;
		}
	}

	query_params.tls_ca_path.s = NULL;
	query_params.tls_ca_path.len = 0;
	if (aq->query_params.tls_ca_path.s && aq->query_params.tls_ca_path.len > 0) {
		if (shm_str_dup(&query_params.tls_ca_path, &(aq->query_params.tls_ca_path)) < 0) {
			LM_ERR("Error allocating query_params.tls_ca_path\n");
			return;
		}
	}

	LM_DBG("query received: [%.*s] (%p)\n", query.len, query.s, aq);

	if (new_request(&query, &post, &query_params, async_http_cb, aq) < 0) {
		LM_ERR("Cannot create request for %.*s\n", query.len, query.s);
		free_async_query(aq);
	}

	if (query_params.tls_client_cert.s && query_params.tls_client_cert.len > 0) {
		shm_free(query_params.tls_client_cert.s);
		query_params.tls_client_cert.s = NULL;
		query_params.tls_client_cert.len = 0;
	}
	if (query_params.tls_client_key.s && query_params.tls_client_key.len > 0) {
		shm_free(query_params.tls_client_key.s);
		query_params.tls_client_key.s = NULL;
		query_params.tls_client_key.len = 0;
	}
	if (query_params.tls_ca_path.s && query_params.tls_ca_path.len > 0) {
		shm_free(query_params.tls_ca_path.s);
		query_params.tls_ca_path.s = NULL;
		query_params.tls_ca_path.len = 0;
	}

	return;
}

int init_socket(async_http_worker_t *worker)
{
	worker->socket_event = event_new(worker->evbase, worker->notication_socket[0], EV_READ|EV_PERSIST, notification_socket_cb, worker);
	event_add(worker->socket_event, NULL);
	return (0);
}

int async_send_query(sip_msg_t *msg, str *query, str *post, cfg_action_t *act)
{
	async_query_t *aq;
	unsigned int tindex = 0;
	unsigned int tlabel = 0;
	short suspend = 0;
	int dsize;
	tm_cell_t *t = 0;

	if(query==0) {
		LM_ERR("invalid parameters\n");
		return -1;
	}

	t = tmb.t_gett();
	if (t==NULL || t==T_UNDEFINED) {
		LM_DBG("no pre-existing transaction, switching to transaction-less behavior\n");
	} else if (!ah_params.suspend_transaction) {
		LM_DBG("transaction won't be suspended\n");
	} else {
		if(tmb.t_suspend==NULL) {
			LM_ERR("http async query is disabled - tm module not loaded\n");
			return -1;
		}

		if(tmb.t_suspend(msg, &tindex, &tlabel)<0) {
			LM_ERR("failed to suspend request processing\n");
			return -1;
		}

		suspend = 1;

		LM_DBG("transaction suspended [%u:%u]\n", tindex, tlabel);
	}
	dsize = sizeof(async_query_t);
	aq = (async_query_t*)shm_malloc(dsize);

	if(aq==NULL)
	{
		LM_ERR("no more shm\n");
		goto error;
	}
	memset(aq,0,dsize);

	if(shm_str_dup(&aq->query, query)<0) {
		goto error;
	}

	if (post != NULL) {

		if(shm_str_dup(&aq->post, post)<0) {
			goto error;
		}
	}

	aq->param = act;
	aq->tindex = tindex;
	aq->tlabel = tlabel;
	
	aq->query_params.tls_verify_peer = ah_params.tls_verify_peer;
	aq->query_params.tls_verify_host = ah_params.tls_verify_host;
	aq->query_params.suspend_transaction = suspend;
	aq->query_params.timeout = ah_params.timeout;
	aq->query_params.headers = ah_params.headers;
	aq->query_params.method = ah_params.method;

	aq->query_params.tls_client_cert.s = NULL;
	aq->query_params.tls_client_cert.len = 0;
	if (ah_params.tls_client_cert.s && ah_params.tls_client_cert.len > 0) {
		if (shm_str_dup(&aq->query_params.tls_client_cert, &(ah_params.tls_client_cert)) < 0) {
			LM_ERR("Error allocating aq->query_params.tls_client_cert\n");
			goto error;
		}
	}

	aq->query_params.tls_client_key.s = NULL;
	aq->query_params.tls_client_key.len = 0;
	if (ah_params.tls_client_key.s && ah_params.tls_client_key.len > 0) {
		if (shm_str_dup(&aq->query_params.tls_client_key, &(ah_params.tls_client_key)) < 0) {
			LM_ERR("Error allocating aq->query_params.tls_client_key\n");
			goto error;
		}
	}

	aq->query_params.tls_ca_path.s = NULL;
	aq->query_params.tls_ca_path.len = 0;
	if (ah_params.tls_ca_path.s && ah_params.tls_ca_path.len > 0) {
		if (shm_str_dup(&aq->query_params.tls_ca_path, &(ah_params.tls_ca_path)) < 0) {
			LM_ERR("Error allocating aq->query_params.tls_ca_path\n");
			goto error;
		}
	}

	set_query_params(&ah_params);

	if(async_push_query(aq)<0) {
		LM_ERR("failed to relay query: %.*s\n", query->len, query->s);
		goto error;
	}

	if (suspend)  {
		/* force exit in config */
		return 0;
	}
	
	/* continue route processing */
	return 1;

error:

	if (suspend) {
		tmb.t_cancel_suspend(tindex, tlabel);
	}
	free_async_query(aq);
	return -1;
}

int async_push_query(async_query_t *aq)
{
	int len;
	int worker;
	static unsigned long rr = 0; /* round robin */

	str query;

	query = ((str)aq->query);

	worker = rr++ % num_workers;
	len = write(workers[worker].notication_socket[1], &aq, sizeof(async_query_t*));
	if(len<=0) {
		LM_ERR("failed to pass the query to async workers\n");
		return -1;
	}
	LM_DBG("query sent [%.*s] (%p) to worker %d\n", query.len, query.s, aq, worker + 1);
	return 0;
}

void init_query_params(struct query_params *p) {
	memset(&ah_params, 0, sizeof(struct query_params));
	set_query_params(p);
}

void set_query_params(struct query_params *p) {
	p->headers.len = 0;
	p->headers.t = NULL;
	p->tls_verify_host = tls_verify_host;
	p->tls_verify_peer = tls_verify_peer;
	p->suspend_transaction = 1;
	p->timeout = http_timeout;
	p->method = AH_METH_DEFAULT;

	if (p->tls_client_cert.s && p->tls_client_cert.len > 0) {
		shm_free(p->tls_client_cert.s);
		p->tls_client_cert.s = NULL;
		p->tls_client_cert.len = 0;
	}
	if (tls_client_cert.s && tls_client_cert.len > 0) {
		if (shm_str_dup(&p->tls_client_cert, &tls_client_cert) < 0) {
			LM_ERR("Error allocating tls_client_cert\n");
			return;
		}
	}

	if (p->tls_client_key.s && p->tls_client_key.len > 0) {
		shm_free(p->tls_client_key.s);
		p->tls_client_key.s = NULL;
		p->tls_client_key.len = 0;
	}
	if (tls_client_key.s && tls_client_key.len > 0) {
		if (shm_str_dup(&p->tls_client_key, &tls_client_key) < 0) {
			LM_ERR("Error allocating tls_client_key\n");
			return;
		}
	}

	if (p->tls_ca_path.s && p->tls_ca_path.len > 0) {
		shm_free(p->tls_ca_path.s);
		p->tls_ca_path.s = NULL;
		p->tls_ca_path.len = 0;
	}
	if (tls_ca_path.s && tls_ca_path.len > 0) {
		if (shm_str_dup(&p->tls_ca_path, &tls_ca_path) < 0) {
			LM_ERR("Error allocating tls_ca_path\n");
			return;
		}
	}
}

int header_list_add(struct header_list *hl, str* hdr) {
	char *tmp;

	hl->len++;
	hl->t = shm_realloc(hl->t, hl->len * sizeof(char*));
	if (!hl->t) {
		LM_ERR("shm memory allocation failure\n");
		return -1;
	}
	hl->t[hl->len - 1] = shm_malloc(hdr->len + 1);
	tmp = hl->t[hl->len - 1];
	if (!tmp) {
		LM_ERR("shm memory allocation failure\n");
		return -1;
	}
	memcpy(tmp, hdr->s, hdr->len);
	*(tmp + hdr->len) = '\0';

	LM_DBG("stored new http header: [%s]\n", tmp);
	return 1;
}

int query_params_set_method(struct query_params *qp, str *meth) {
	if (strncasecmp(meth->s, "GET", meth->len) == 0) {
		qp->method = AH_METH_GET;
	} else if (strncasecmp(meth->s, "POST",meth->len) == 0) {
		qp->method = AH_METH_POST;
	} else if (strncasecmp(meth->s, "PUT", meth->len) == 0) {
		qp->method = AH_METH_PUT;
	} else if (strncasecmp(meth->s, "DELETE", meth->len) == 0) {
		qp->method = AH_METH_DELETE;
	} else {
		LM_ERR("Unsupported method: %.*s\n", meth->len, meth->s);
		return -1;
	}
	return 1;
}
