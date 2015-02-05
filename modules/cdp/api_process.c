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

#include "api_process.h"
#include "transaction.h"
#include "receiver.h"
#include "peerstatemachine.h"
#include "cdp_stats.h"

extern unsigned int* latency_threshold_p;	/**<max delay for Diameter call */
extern struct cdp_counters_h cdp_cnts_h;

handler_list *handlers = 0; /**< list of handlers */
gen_lock_t *handlers_lock;	/**< lock for list of handlers */

/**
 * This callback is added as an internal message listener and used to process
 * transaction requests.
 * - first it calls all the registered handlers for requests and responses
 * - then it calls the transaction handler
 * @param p - peer that this message came from
 * @param msg - the diameter message
 * @param ptr - not used anymore
 * @returns 1 always
 */
int api_callback(peer *p,AAAMessage *msg,void* ptr)
{
	cdp_trans_t *t;
	int auto_drop;
	handler *h;
	handler x;
	enum handler_types type;
	AAAMessage *rsp;
	if (is_req(msg)) type = REQUEST_HANDLER;
	else type=RESPONSE_HANDLER;

	lock_get(handlers_lock);
		for(h=handlers->head;h;h=h->next){
			if (h->type==type){
				x.handler = h->handler;
				if (h->type == REQUEST_HANDLER) {
					lock_release(handlers_lock);
					rsp = (x.handler.requestHandler)(msg,h->param);
					if (rsp) {
						//peer_send_msg(p,rsp);
						sm_process(p,Send_Message,rsp,0,0);
					}
					lock_get(handlers_lock);
				}
				else {
					lock_release(handlers_lock);
					(x.handler.responseHandler)(msg,h->param);
					lock_get(handlers_lock);
				}
			}
		}
	lock_release(handlers_lock);

	if (!is_req(msg)){
		/* take care of transactional callback if any */
		t = cdp_take_trans(msg);
		if (t){
			t->ans = msg;
            struct timeval stop;
            gettimeofday(&stop, NULL);
            long elapsed_usecs =  (stop.tv_sec - t->started.tv_sec)*1000000 + (stop.tv_usec - t->started.tv_usec);
            long elapsed_msecs = elapsed_usecs/1000;
            if (elapsed_msecs > *latency_threshold_p) {
            	LM_ERR("Received diameter response outside of threshold (%d) - %ld\n", *latency_threshold_p, elapsed_msecs);
            }
	    counter_inc(cdp_cnts_h.replies_received);
	    counter_add(cdp_cnts_h.replies_response_time, elapsed_msecs);
			auto_drop = t->auto_drop;
			if (t->cb){
				(t->cb)(0,*(t->ptr),msg, elapsed_msecs);
			}
			if (auto_drop) cdp_free_trans(t);
		}
	}
	return 1;
}


