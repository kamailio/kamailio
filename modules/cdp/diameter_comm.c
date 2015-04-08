/*
 * $Id$
 *
 * Copyright (C) 2012 Smile Communications, jason.penton@smilecoms.com
 * Copyright (C) 2012 Smile Communications, richard.good@smilecoms.com
 * 
 * The initial version of this code was written by Dragos Vingarzan
 * (dragos(dot)vingarzan(at)fokus(dot)fraunhofer(dot)de and the
 * Fruanhofer Institute. It was and still is maintained in a separate
 * branch of the original SER. We are therefore migrating it to
 * Kamailio/SR and look forward to maintaining it from here on out.
 * 2011/2012 Smile Communications, Pty. Ltd.
 * ported/maintained/improved by 
 * Jason Penton (jason(dot)penton(at)smilecoms.com and
 * Richard Good (richard(dot)good(at)smilecoms.com) as part of an 
 * effort to add full IMS support to Kamailio/SR using a new and
 * improved architecture
 * 
 * NB: Alot of this code was originally part of OpenIMSCore,
 * FhG Fokus. 
 * Copyright (C) 2004-2006 FhG Fokus
 * Thanks for great work! This is an effort to 
 * break apart the various CSCF functions into logically separate
 * components. We hope this will drive wider use. We also feel
 * that in this way the architecture is more complete and thereby easier
 * to manage in the Kamailio/SR environment
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

#include "diameter_api.h"
#include "peer.h"
#include "peermanager.h"
#include "receiver.h"
#include "transaction.h"
#include "api_process.h"
#include "routing.h"
#include "peerstatemachine.h"
#include "globals.h"

extern dp_config *config;				/**< Configuration for this diameter peer 	*/
extern unsigned int* latency_threshold_p;	/**<max delay for Diameter call */

				/* CALLBACKS */
extern handler_list *handlers; 		/**< list of handlers */
extern gen_lock_t *handlers_lock;	/**< lock for list of handlers */

/**
 * Add a handler function for incoming requests.
 * @param f - the callback function
 * @param param - generic parameter to be used when calling the callback functions
 * @returns 1 on success, 0 on failure
 */
int AAAAddRequestHandler(AAARequestHandler_f *f,void *param)
{
	handler *h = shm_malloc(sizeof(handler));
	if (!h) {
		LM_ERR("AAAAddRequestHandler: error allocating %ld bytes in shm\n",
			(long int)sizeof(handler));
		return 0;
	}
	h->type = REQUEST_HANDLER;
	h->handler.requestHandler = f;
	h->param = param;
	h->next = 0;
	lock_get(handlers_lock);
	h->prev = handlers->tail;
	if (handlers->tail) handlers->tail->next = h;
	handlers->tail = h;
	if (!handlers->head) handlers->head = h;
	lock_release(handlers_lock);
	return 1;
}

/**
 * Add a handler function for incoming responses.
 * @param f - the callback function
 * @param param - generic parameter to be used when calling the callback functions
 * @returns 1 on success, 0 on failure
 */
int AAAAddResponseHandler(AAAResponseHandler_f *f,void *param)
{
	handler *h = shm_malloc(sizeof(handler));
	if (!h) {
		LM_ERR("AAAAddResponseHandler: error allocating %ld bytes in shm\n",
			(long int)sizeof(handler));
		return 0;
	}
	h->type = RESPONSE_HANDLER;
	h->handler.responseHandler = f;
	h->param = param;
	h->next = 0;
	lock_get(handlers_lock);
	h->prev = handlers->tail;
	if (handlers->tail) handlers->tail->next = h;
	handlers->tail = h;
	if (!handlers->head) handlers->head = h;
	lock_release(handlers_lock);
	return 1;
}


				/* MESSAGE SENDING */

/**
 * Send a AAAMessage asynchronously.
 * When the response is received, the callback_f(callback_param,...) is called.
 * @param message - the request to be sent
 * @param peer_id - FQDN of the peer to send
 * @param callback_f - callback to be called on transactional response or transaction timeout
 * @param callback_param - generic parameter to call the transactional callback function with
 * @returns 1 on success, 0 on failure
 * \todo remove peer_id and add Realm routing
 */
AAAReturnCode AAASendMessage(
		AAAMessage *message,
		AAATransactionCallback_f *callback_f,
		void *callback_param)
{
        cdp_session_t* cdp_session;
	peer *p;
        cdp_session = cdp_get_session(message->sessionId->data);
        
	p = get_routing_peer(cdp_session, message);
        if (cdp_session) {
            AAASessionsUnlock(cdp_session->hash);
        }
	if (!p) {
		LM_ERR("AAASendMessage(): Can't find a suitable connected peer in the routing table.\n");
		goto error;
	}
	LM_DBG("Found diameter peer [%.*s] from routing table\n", p->fqdn.len, p->fqdn.s);
	if (p->state!=I_Open && p->state!=R_Open){
		LM_ERR("AAASendMessage(): Peer not connected to %.*s\n",p->fqdn.len,p->fqdn.s);
		goto error;
	}
	/* only add transaction following when required */
	if (callback_f){
		if (is_req(message))
			cdp_add_trans(message,callback_f,callback_param,config->transaction_timeout,1);
		else
			LM_ERR("AAASendMessage(): can't add transaction callback for answer.\n");
	}

//	if (!peer_send_msg(p,message))
	if (!sm_process(p,Send_Message,message,0,0))
		goto error;

	return 1;
error:
	AAAFreeMessage(&message);
	return 0;
}

/**
 * Send a AAAMessage asynchronously.
 * When the response is received, the callback_f(callback_param,...) is called.
 * @param message - the request to be sent
 * @param peer_id - FQDN of the peer to send
 * @param callback_f - callback to be called on transactional response or transaction timeout
 * @param callback_param - generic parameter to call the transactional callback function with
 * @returns 1 on success, 0 on failure
 * \todo remove peer_id and add Realm routing
 */
AAAReturnCode AAASendMessageToPeer(
		AAAMessage *message,
		str *peer_id,
		AAATransactionCallback_f *callback_f,
		void *callback_param)
{
	peer *p;
	p = get_peer_by_fqdn(peer_id);
	if (!p) {
		LM_ERR("AAASendMessageToPeer(): Peer unknown %.*s\n",peer_id->len,peer_id->s);
		goto error;
	}
	if (p->state!=I_Open && p->state!=R_Open){
		LM_ERR("AAASendMessageToPeer(): Peer not connected to %.*s\n",peer_id->len,peer_id->s);
		goto error;
	}
	/* only add transaction following when required */
	if (callback_f){
		if (is_req(message))
			cdp_add_trans(message,callback_f,callback_param,config->transaction_timeout,1);
		else
			LM_ERR("AAASendMessageToPeer(): can't add transaction callback for answer.\n");
	}

	p->last_selected = time(NULL);
	if (!sm_process(p,Send_Message,message,0,0))
		goto error;

	return 1;
error:
	AAAFreeMessage(&message);
	return 0;
}


/**
 * Generic callback used by AAASendRecvMessage() to block until a transactional response
 * is received.
 * The AAASendRecvMessage() is basically a AAASendMessage() that has a callback
 * (this function) that blocks until a transactional response or timeout is received and
 * then it returns that.
 *
 * @param is_timeout - if this is a time-out or response event
 * @param param - generic parameter to call the transactional callback function with
 * @param ans - the answer for the callback
 */
void sendrecv_cb(int is_timeout,void *param,AAAMessage *ans, long elapsed_msecs)
{
	if (sem_release((gen_sem_t*)param)<0)
		LM_ERR("sendrecv_cb(): Failed to unlock a transactional sendrecv! > %s\n",strerror(errno));
}

/**
 * Send a AAAMessage synchronously.
 * This blocks until a response is received or a transactional time-out happens.
 * @param message - the request to be sent
 * @param peer_id - FQDN of the peer to send
 * @returns 1 on success, 0 on failure
 * \todo remove peer_id and add Realm routing
 * \todo replace the busy-waiting lock in here with one that does not consume CPU
 */
AAAMessage* AAASendRecvMessage(AAAMessage *message)
{
	peer *p;
	gen_sem_t *sem=0;
	cdp_trans_t *t;
	AAAMessage *ans;
        struct timeval start, stop;
        long elapsed_usecs=0, elapsed_millis=0;
        cdp_session_t* cdp_session;

        gettimeofday(&start, NULL);
        cdp_session = cdp_get_session(message->sessionId->data);
	p = get_routing_peer(cdp_session, message);
        if (cdp_session) {
            AAASessionsUnlock(cdp_session->hash);
        }
	if (!p) {
		LM_ERR("AAASendRecvMessage(): Can't find a suitable connected peer in the routing table.\n");
		goto error;
	}
	if (p->state!=I_Open && p->state!=R_Open){
		LM_ERR("AAASendRecvMessage(): Peer not connected to %.*s\n",p->fqdn.len,p->fqdn.s);
		goto error;
	}


	if (is_req(message)){
		sem_new(sem,0);
		t = cdp_add_trans(message,sendrecv_cb,(void*)sem,config->transaction_timeout,0);

		if (!sm_process(p,Send_Message,message,0,0)){
			sem_free(sem);
			goto error;
		}

		/* block until callback is executed */
		while(sem_get(sem)<0){
			if (shutdownx&&(*shutdownx)) goto error;
			LM_WARN("AAASendRecvMessage(): interrupted by signal or something > %s\n",strerror(errno));
		}
		sem_free(sem);
		gettimeofday(&stop, NULL);
        elapsed_usecs = (stop.tv_sec - start.tv_sec)*1000000 + (stop.tv_usec - start.tv_usec);
        elapsed_millis = elapsed_usecs/1000;
		if (elapsed_millis > *latency_threshold_p) {
			LM_ERR("CDP response to Send_Message took too long (>%dms) - [%ldms]\n", *latency_threshold_p, elapsed_millis);
		}
		ans = t->ans;
		cdp_free_trans(t);
		return ans;
	} else {
		LM_ERR("AAASendRecvMessage(): can't add wait for answer to answer.\n");
		goto error;
	}


error:
out_of_memory:
	AAAFreeMessage(&message);
	return 0;
}

/**
 * Send a AAAMessage synchronously.
 * This blocks until a response is received or a transactional time-out happens.
 * @param message - the request to be sent
 * @param peer_id - FQDN of the peer to send
 * @returns 1 on success, 0 on failure
 * \todo remove peer_id and add Realm routing
 * \todo replace the busy-waiting lock in here with one that does not consume CPU
 */
AAAMessage* AAASendRecvMessageToPeer(AAAMessage *message, str *peer_id)
{
	peer *p;
	gen_sem_t *sem;
	cdp_trans_t *t;
	AAAMessage *ans;
	struct timeval start, stop;
    long elapsed_usecs=0, elapsed_millis=0;

    gettimeofday(&start, NULL);

	p = get_peer_by_fqdn(peer_id);
	if (!p) {
		LM_ERR("AAASendRecvMessageToPeer(): Peer unknown %.*s\n",peer_id->len,peer_id->s);
		goto error;
	}
	if (p->state!=I_Open && p->state!=R_Open){
		LM_ERR("AAASendRecvMessageToPeer(): Peer not connected to %.*s\n",peer_id->len,peer_id->s);
		goto error;
	}

	if (is_req(message)){
		sem_new(sem,0);
		t = cdp_add_trans(message,sendrecv_cb,(void*)sem,config->transaction_timeout,0);

//		if (!peer_send_msg(p,message)) {
		if (!sm_process(p,Send_Message,message,0,0)){
			sem_free(sem);
			goto error;
		}
		/* block until callback is executed */
		while(sem_get(sem)<0){
			if (shutdownx&&(*shutdownx)) goto error;
			LM_WARN("AAASendRecvMessageToPeer(): interrupted by signal or something > %s\n",strerror(errno));
		}

		gettimeofday(&stop, NULL);
		elapsed_usecs = (stop.tv_sec - start.tv_sec) * 1000000
				+ (stop.tv_usec - start.tv_usec);
		elapsed_millis = elapsed_usecs / 1000;
		if (elapsed_millis > *latency_threshold_p) {
			LM_ERR("CDP response to Send_Message took too long (>%dms) - [%ldms]\n", *latency_threshold_p, elapsed_millis);
		}
		sem_free(sem);

		ans = t->ans;
		cdp_free_trans(t);
		return ans;
	} else {
		LM_ERR("AAASendRecvMessageToPeer(): can't add wait for answer to answer.\n");
		goto error;
	}


error:
out_of_memory:
	AAAFreeMessage(&message);
	return 0;
}



