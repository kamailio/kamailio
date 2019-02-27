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

#include "../../core/sr_module.h"
#include "../../core/dprint.h"
#include "../../core/ut.h"
#include "../../core/cfg/cfg_struct.h"
#include "../../core/receive.h"
#include "../../core/fmsg.h"
#include "../../core/kemi.h"
#include "../../modules/tm/tm_load.h"

#include "async_http.h"

/* tm */
extern struct tm_binds tmb;

struct sip_msg *ah_reply = NULL;
str ah_error = {NULL, 0};

async_http_worker_t *workers = NULL;
int num_workers = 1;

struct query_params ah_params;
unsigned int q_idx;
char q_id[MAX_ID_LEN+1];

int async_http_init_worker(int prank, async_http_worker_t* worker)
{
	LM_DBG("initializing worker process: %d\n", prank);
	worker->evbase = event_base_new();
	LM_DBG("base event %p created\n", worker->evbase);

	worker->g = shm_malloc(sizeof(struct http_m_global));
	if(worker->g==NULL) {
		LM_ERR("out of shared memory\n");
		return -1;
	}
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
	async_query_t *aq = NULL;
	cfg_action_t *act = NULL;
	int ri;
	unsigned int tindex;
	unsigned int tlabel;
	struct cell *t = NULL;
	char *p;
	str newbuf = {0, 0};
	sip_msg_t *fmsg = NULL;
	sr_kemi_eng_t *keng = NULL;
	str cbname = {0, 0};
	str evname = str_init("http_async_client:callback");

	aq = param;

	/* clean process-local result variables */
	ah_error.s = NULL;
	ah_error.len = 0;
	memset(ah_reply, 0, sizeof(struct sip_msg));

	keng = sr_kemi_eng_get();
	if(keng==NULL) {
		ri = route_lookup(&main_rt, aq->cbname);
		if(ri<0) {
			LM_ERR("unable to find route block [%s]\n", aq->cbname);
			goto done;
		}
		act = main_rt.rlist[ri];
		if(act==NULL) {
			LM_ERR("empty action lists in route block [%s]\n", aq->cbname);
			goto done;
		}
	}

	if (reply->result != NULL) {
		LM_DBG("query result = %.*s [%d]\n", reply->result->len, reply->result->s, reply->result->len);
	}

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
			if (ah_reply->first_line.u.reply.statuscode == 100) {
				newbuf.s = get_body( ah_reply );
				newbuf.len = reply->result->s + reply->result->len - newbuf.s;

				if (!(newbuf.len < 0)) {	
					memset(ah_reply, 0, sizeof(struct sip_msg));
					ah_reply->buf = newbuf.s;
					ah_reply->len = newbuf.len;

					if (parse_msg(ah_reply->buf, ah_reply->len, ah_reply) != 0) {
						LM_DBG("failed to parse the http_reply\n");
					} else {
						LM_DBG("successfully parsed http reply %p\n", ah_reply);
					}
				} else {
					/* this should not happen! */
					LM_WARN("something got wrong parsing the 100 Continue: got %d len\n", newbuf.len);
				}
				
			} else {
				LM_DBG("successfully parsed http reply %p\n", ah_reply);
			}
		}
	}

	strncpy(q_id, aq->id, strlen(aq->id));
	
	q_id[strlen(aq->id)] = '\0';

	cfg_update();

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

		if(keng==NULL) {
			if(act!=NULL) {
				tmb.t_continue(tindex, tlabel, act);
			}
		} else {
			cbname.s = aq->cbname;
			cbname.len = aq->cbname_len;
			tmb.t_continue_cb(tindex, tlabel, &cbname, &evname);
		}
	} else {
		fmsg = faked_msg_next();
		if(keng==NULL) {
			if(act!=NULL) {
				if (run_top_route(act, fmsg, 0)<0) {
					LM_ERR("failure inside run_top_route\n");
				}
			}
		} else {
			cbname.s = aq->cbname;
			cbname.len = aq->cbname_len;
			if(sr_kemi_route(keng, fmsg, EVENT_ROUTE, &cbname, &evname)<0) {
				LM_ERR("error running event route kemi callback\n");
			}
		}
		ksr_msg_env_reset();
	}

done:
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
	int i, len;
	async_query_t *aq;

	http_m_params_t query_params;

	str query;

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

	memset(&query_params, 0, sizeof(http_m_params_t));
	query_params.timeout = aq->query_params.timeout;
	query_params.tls_verify_peer = aq->query_params.tls_verify_peer;
	query_params.tls_verify_host = aq->query_params.tls_verify_host;
	query_params.authmethod = aq->query_params.authmethod;
	query_params.tcp_keepalive = aq->query_params.tcp_keepalive;
	query_params.tcp_ka_idle = aq->query_params.tcp_ka_idle;
	query_params.tcp_ka_interval = aq->query_params.tcp_ka_interval;

	for (i = 0 ; i < aq->query_params.headers.len ; i++) {
		query_params.headers = curl_slist_append(query_params.headers, aq->query_params.headers.t[i]);
	}
	query_params.method  = aq->query_params.method;

	if (aq->query_params.tls_client_cert) {
		len = strlen(aq->query_params.tls_client_cert);
		query_params.tls_client_cert = shm_malloc(len+1);

		if(query_params.tls_client_cert == NULL) {
			LM_ERR("Error allocating query_params.tls_client_cert\n");
			goto done;
		}

		strncpy(query_params.tls_client_cert, aq->query_params.tls_client_cert, len);
		query_params.tls_client_cert[len] = '\0';
	}

	if (aq->query_params.tls_client_key) {
		len = strlen(aq->query_params.tls_client_key);
		query_params.tls_client_key = shm_malloc(len+1);

		if(query_params.tls_client_key == NULL) {
			LM_ERR("Error allocating query_params.tls_client_key\n");
			goto done;
		}

		strncpy(query_params.tls_client_key, aq->query_params.tls_client_key, len);
		query_params.tls_client_key[len] = '\0';
	}

	if (aq->query_params.tls_ca_path) {
		len = strlen(aq->query_params.tls_ca_path);
		query_params.tls_ca_path = shm_malloc(len+1);

		if(query_params.tls_ca_path == NULL) {
			LM_ERR("Error allocating query_params.tls_ca_path\n");
			goto done;
		}

		strncpy(query_params.tls_ca_path, aq->query_params.tls_ca_path, len);
		query_params.tls_ca_path[len] = '\0';
	}

	if (aq->query_params.body.s && aq->query_params.body.len > 0) {
		if (shm_str_dup(&query_params.body, &(aq->query_params.body)) < 0) {
			LM_ERR("Error allocating query_params.body\n");
			goto done;
		}
	}

	if (aq->query_params.username) {
		len = strlen(aq->query_params.username);
		query_params.username = shm_malloc(len+1);

		if(query_params.username == NULL) {
			LM_ERR("error in shm_malloc\n");
			goto done;
		}

		strncpy(query_params.username, aq->query_params.username, len);
		query_params.username[len] = '\0';
	}

	if (aq->query_params.password) {
		len = strlen(aq->query_params.password);
		query_params.password = shm_malloc(len+1);

		if(query_params.password == NULL) {
			LM_ERR("error in shm_malloc\n");
			goto done;
		}

		strncpy(query_params.password, aq->query_params.password, len);
		query_params.password[len] = '\0';
	}

	LM_DBG("query received: [%.*s] (%p)\n", query.len, query.s, aq);

	if (new_request(&query, &query_params, async_http_cb, aq) < 0) {
		LM_ERR("Cannot create request for %.*s\n", query.len, query.s);
		free_async_query(aq);
	}

done:
	if (query_params.tls_client_cert) {
		shm_free(query_params.tls_client_cert);
		query_params.tls_client_cert = NULL;
	}
	if (query_params.tls_client_key) {
		shm_free(query_params.tls_client_key);
		query_params.tls_client_key = NULL;
	}
	if (query_params.tls_ca_path) {
		shm_free(query_params.tls_ca_path);
		query_params.tls_ca_path = NULL;
	}
	if (query_params.body.s && query_params.body.len > 0) {
		shm_free(query_params.body.s);
		query_params.body.s = NULL;
		query_params.body.len = 0;
	}

	if (query_params.username) {
		shm_free(query_params.username);
		query_params.username = NULL;
	}

	if (query_params.password) {
		shm_free(query_params.password);
		query_params.password = NULL;
	}

	return;
}

int init_socket(async_http_worker_t *worker)
{
	worker->socket_event = event_new(worker->evbase, worker->notication_socket[0], EV_READ|EV_PERSIST, notification_socket_cb, worker);
	event_add(worker->socket_event, NULL);
	return (0);
}

int async_send_query(sip_msg_t *msg, str *query, str *cbname)
{
	async_query_t *aq;
	unsigned int tindex = 0;
	unsigned int tlabel = 0;
	short suspend = 0;
	int dsize;
	int len;
	tm_cell_t *t = 0;

	if(query==0) {
		LM_ERR("invalid parameters\n");
		return -1;
	}
	if(cbname->len>=MAX_CBNAME_LEN-1) {
		LM_ERR("callback name is too long: %d / %.*s\n", cbname->len,
				cbname->len, cbname->s);
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

	memcpy(aq->cbname, cbname->s, cbname->len);
	aq->cbname[cbname->len] = '\0';
	aq->cbname_len = cbname->len;
	aq->tindex = tindex;
	aq->tlabel = tlabel;
	
	aq->query_params.tls_verify_peer = ah_params.tls_verify_peer;
	aq->query_params.tls_verify_host = ah_params.tls_verify_host;
	aq->query_params.suspend_transaction = suspend;
	aq->query_params.timeout = ah_params.timeout;
	aq->query_params.tcp_keepalive = ah_params.tcp_keepalive;
	aq->query_params.tcp_ka_idle = ah_params.tcp_ka_idle;
	aq->query_params.tcp_ka_interval = ah_params.tcp_ka_interval;
	aq->query_params.headers = ah_params.headers;
	aq->query_params.method = ah_params.method;
	aq->query_params.authmethod = ah_params.authmethod;
	
	q_idx++;
	snprintf(q_id, MAX_ID_LEN+1, "%u-%u", (unsigned int)getpid(), q_idx);
	strncpy(aq->id, q_id, strlen(q_id));

	aq->query_params.tls_client_cert = NULL;
	if (ah_params.tls_client_cert) {
		len = strlen(ah_params.tls_client_cert);
		aq->query_params.tls_client_cert = shm_malloc(len+1);

		if(aq->query_params.tls_client_cert == NULL) {
			LM_ERR("Error allocating aq->query_params.tls_client_cert\n");
			goto error;
		}

		strncpy(aq->query_params.tls_client_cert, ah_params.tls_client_cert, len);
		aq->query_params.tls_client_cert[len] = '\0';
	}

	aq->query_params.tls_client_key = NULL;
	if (ah_params.tls_client_key) {
		len = strlen(ah_params.tls_client_key);
		aq->query_params.tls_client_key = shm_malloc(len+1);

		if(aq->query_params.tls_client_key == NULL) {
			LM_ERR("Error allocating aq->query_params.tls_client_key\n");
			goto error;
		}

		strncpy(aq->query_params.tls_client_key, ah_params.tls_client_key, len);
		aq->query_params.tls_client_key[len] = '\0';
	}

	aq->query_params.tls_ca_path = NULL;
	if (ah_params.tls_ca_path) {
		len = strlen(ah_params.tls_ca_path);
		aq->query_params.tls_ca_path = shm_malloc(len+1);

		if(aq->query_params.tls_ca_path == NULL) {
			LM_ERR("Error allocating aq->query_params.tls_ca_path\n");
			goto error;
		}

		strncpy(aq->query_params.tls_ca_path, ah_params.tls_ca_path, len);
		aq->query_params.tls_ca_path[len] = '\0';
	}

	aq->query_params.body.s = NULL;
	aq->query_params.body.len = 0;
	if (ah_params.body.s && ah_params.body.len > 0) {
		if (shm_str_dup(&aq->query_params.body, &(ah_params.body)) < 0) {
			LM_ERR("Error allocating aq->query_params.body\n");
			goto error;
		}
	}

	aq->query_params.username = NULL;
	if (ah_params.username) {
		len = strlen(ah_params.username);
		aq->query_params.username = shm_malloc(len+1);
	
		if(aq->query_params.username == NULL) {
			LM_ERR("error in shm_malloc\n");
			goto error;
		}

		strncpy(aq->query_params.username, ah_params.username, len);
		aq->query_params.username[len] = '\0';
	}

	aq->query_params.password = NULL;
	if (ah_params.password) {
		len = strlen(ah_params.password);
		aq->query_params.password = shm_malloc(len+1);
	
		if(aq->query_params.password == NULL) {
			LM_ERR("error in shm_malloc\n");
			goto error;
		}

		strncpy(aq->query_params.password, ah_params.password, len);
		aq->query_params.password[len] = '\0';
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
	int len;
	p->headers.len = 0;
	p->headers.t = NULL;
	p->tls_verify_host = tls_verify_host;
	p->tls_verify_peer = tls_verify_peer;
	p->suspend_transaction = 1;
	p->timeout = http_timeout;
	p->method = AH_METH_DEFAULT;
	p->authmethod = default_authmethod;
	p->tcp_keepalive = tcp_keepalive;
	p->tcp_ka_idle = tcp_ka_idle;
	p->tcp_ka_interval = tcp_ka_interval;

	if (p->tls_client_cert) {
		shm_free(p->tls_client_cert);
		p->tls_client_cert = NULL;
	}
	if (tls_client_cert) {
		len = strlen(tls_client_cert);
		p->tls_client_cert = shm_malloc(len+1);

		if (p->tls_client_cert == NULL) {
			LM_ERR("Error allocating tls_client_cert\n");
			return;
		}

		strncpy(p->tls_client_cert, tls_client_cert, len);
		p->tls_client_cert[len] = '\0';
	}

	if (p->tls_client_key) {
		shm_free(p->tls_client_key);
		p->tls_client_key = NULL;
	}
	if (tls_client_key) {
		len = strlen(tls_client_key);
		p->tls_client_key = shm_malloc(len+1);

		if (p->tls_client_key == NULL) {
			LM_ERR("Error allocating tls_client_key\n");
			return;
		}

		strncpy(p->tls_client_key, tls_client_key, len);
		p->tls_client_key[len] = '\0';
	}

	if (p->tls_ca_path) {
		shm_free(p->tls_ca_path);
		p->tls_ca_path = NULL;
	}
	if (tls_ca_path) {
		len = strlen(tls_ca_path);
		p->tls_ca_path = shm_malloc(len+1);

		if (p->tls_ca_path == NULL) {
			LM_ERR("Error allocating tls_ca_path\n");
			return;
		}

		strncpy(p->tls_ca_path, tls_ca_path, len);
		p->tls_ca_path[len] = '\0';
	}

	if (p->body.s && p->body.len > 0) {
		shm_free(p->body.s);
		p->body.s = NULL;
		p->body.len = 0;
	}
	
	if (p->username) {
		shm_free(p->username);
		p->username = NULL;
	}
	
	if (p->password) {
		shm_free(p->password);
		p->password = NULL;
	}
}

int header_list_add(struct header_list *hl, str* hdr) {
	char *tmp;

	hl->len++;
	hl->t = shm_reallocxf(hl->t, hl->len * sizeof(char*));
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
