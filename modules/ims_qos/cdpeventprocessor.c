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

#include <time.h>
#include "sem.h"
#include "../ims_usrloc_pcscf/usrloc.h"
#include "../dialog_ng/dlg_load.h"
#include "../cdp/session.h"
#include "mod.h"
#include "cdpeventprocessor.h"
#include "rx_str.h"
#include "ims_qos_stats.h"

cdp_cb_event_list_t *cdp_event_list = 0;
extern usrloc_api_t ul;
extern struct dlg_binds dlgb;
extern int cdp_event_latency;
extern int cdp_event_threshold;
extern int cdp_event_latency_loglevel;
extern int cdp_event_list_size_threshold;

extern struct ims_qos_counters_h ims_qos_cnts_h;

int init_cdp_cb_event_list() {
    cdp_event_list = shm_malloc(sizeof (cdp_cb_event_list_t));
    if (!cdp_event_list) {
        LM_ERR("No more SHM mem\n");
        return 0;
    }
    memset(cdp_event_list, 0, sizeof (cdp_cb_event_list_t));
    cdp_event_list->lock = lock_alloc();
    if (!cdp_event_list->lock) {
        LM_ERR("failed to create cdp event list lock\n");
        return 0;
    }
    cdp_event_list->lock = lock_init(cdp_event_list->lock);
    cdp_event_list->size = 0;

    sem_new(cdp_event_list->empty, 0); //pre-locked - as we assume list is empty at start

    return 1;
}

void destroy_cdp_cb_event_list() {
    cdp_cb_event_t *ev, *tmp;

    lock_get(cdp_event_list->lock);
    ev = cdp_event_list->head;
    while (ev) {
        tmp = ev->next;
        free_cdp_cb_event(ev);
        ev = tmp;
    }
    lock_destroy(cdp_event_list->lock);
    lock_dealloc(cdp_event_list->lock);
    shm_free(cdp_event_list);
}

cdp_cb_event_t* new_cdp_cb_event(int event, str *rx_session_id, rx_authsessiondata_t *session_data) {
    cdp_cb_event_t *new_event = shm_malloc(sizeof (cdp_cb_event_t));
    if (!new_event) {
        LM_ERR("no more shm mem\n");
        return NULL;
    }
    memset(new_event, 0, sizeof (cdp_cb_event_t));

    //we have to make a copy of the rx session id because it is not in shm mem
    if ((rx_session_id->len > 0) && rx_session_id->s) {
        LM_DBG("Creating new event for rx session [%.*s]\n", rx_session_id->len, rx_session_id->s);
        new_event->rx_session_id.s = (char*) shm_malloc(rx_session_id->len);
        if (!new_event->rx_session_id.s) {
            LM_ERR("no more shm memory\n");
            shm_free(new_event);
            return NULL;
        }
        memcpy(new_event->rx_session_id.s, rx_session_id->s, rx_session_id->len);
        new_event->rx_session_id.len = rx_session_id->len;
    }

    new_event->event = event;
    new_event->registered = time(NULL);
    new_event->session_data = session_data; //session_data is already in shm mem

    return new_event;
}
//add to tail

void push_cdp_cb_event(cdp_cb_event_t* event) {
    lock_get(cdp_event_list->lock);
    if (cdp_event_list->head == 0) { //empty list
        cdp_event_list->head = cdp_event_list->tail = event;
    } else {
        cdp_event_list->tail->next = event;
        cdp_event_list->tail = event;
    }
    cdp_event_list->size++;
    if(cdp_event_list_size_threshold > 0 && cdp_event_list->size > cdp_event_list_size_threshold) {
	    LM_WARN("cdp_event_list is size [%d] and has exceed cdp_event_list_size_threshold of [%d]", cdp_event_list->size, cdp_event_list_size_threshold);
    }
    sem_release(cdp_event_list->empty);
    lock_release(cdp_event_list->lock);
}

//pop from head

cdp_cb_event_t* pop_cdp_cb_event() {
    cdp_cb_event_t *ev;

    lock_get(cdp_event_list->lock);
    while (cdp_event_list->head == 0) {
        lock_release(cdp_event_list->lock);
        sem_get(cdp_event_list->empty);
        lock_get(cdp_event_list->lock);
    }

    ev = cdp_event_list->head;
    cdp_event_list->head = ev->next;

    if (ev == cdp_event_list->tail) { //list now empty
        cdp_event_list->tail = 0;
    }
    ev->next = 0; //make sure whoever gets this cant access our list
    cdp_event_list->size--;
    lock_release(cdp_event_list->lock);

    return ev;
}

/*main event process function*/
void cdp_cb_event_process() {
    cdp_cb_event_t *ev;
    udomain_t* domain;
    pcontact_t* pcontact;
    str release_reason = {"QoS released", 12}; /* TODO: This could be a module parameter */
    struct pcontact_info ci;
    memset(&ci, 0, sizeof (struct pcontact_info));

    for (;;) {
        ev = pop_cdp_cb_event();

        if (cdp_event_latency) { //track delays
            unsigned int diff = time(NULL) - ev->registered;
            if (diff > cdp_event_threshold) {
                switch (cdp_event_latency_loglevel) {
                    case 0:
                        LM_ERR("Took to long to pickup CDP callback event [%d] > [%d]\n", diff, cdp_event_threshold);
                        break;
                    case 1:
                        LM_WARN("Took to long to pickup CDP callback event [%d] > [%d]\n", diff, cdp_event_threshold);
                        break;
                    case 2:
                        LM_INFO("Took to long to pickup CDP callback event [%d] > [%d]\n", diff, cdp_event_threshold);
                        break;
                    case 3:
                        LM_DBG("Took to long to pickup CDP callback event [%d] > [%d]\n", diff, cdp_event_threshold);
                        break;
                    default:
                        LM_DBG("Unknown log level....printing as debug\n");
                        LM_DBG("Took to long to pickup CDP callback event [%d] > [%d]\n", diff, cdp_event_threshold);
                        break;
                }
            }
        }
        LM_DBG("processing event [%d]\n", ev->event);
        rx_authsessiondata_t *p_session_data = ev->session_data;
        str *rx_session_id = &ev->rx_session_id;

        switch (ev->event) {
            case AUTH_EV_SESSION_TIMEOUT:
            case AUTH_EV_SESSION_GRACE_TIMEOUT:
            case AUTH_EV_RECV_ASR:
                LM_DBG("Received notification of ASR from transport plane or CDP timeout for CDP session with Rx session ID: [%.*s] and associated contact [%.*s]"
                        " and domain [%.*s]\n",
                        rx_session_id->len, rx_session_id->s,
                        p_session_data->registration_aor.len, p_session_data->registration_aor.s,
                        p_session_data->domain.len, p_session_data->domain.s);


                if (p_session_data->subscribed_to_signaling_path_status) {
                    LM_DBG("This is a subscription to signalling bearer session");
                    //nothing to do here - just wait for AUTH_EV_SERVICE_TERMINATED event
                } else {
                    LM_DBG("This is a media bearer session session");
                    //this is a media bearer session that was terminated from the transport plane - we need to terminate the associated dialog
                    //so we set p_session_data->must_terminate_dialog to 1 and when we receive AUTH_EV_SERVICE_TERMINATED event we will terminate the dialog
                    p_session_data->must_terminate_dialog = 1;
                }
                break;

            case AUTH_EV_SERVICE_TERMINATED:
                LM_DBG("Received notification of CDP TERMINATE of CDP session with Rx session ID: [%.*s] and associated contact [%.*s]"
                        " and domain [%.*s]\n",
                        rx_session_id->len, rx_session_id->s,
                        p_session_data->registration_aor.len, p_session_data->registration_aor.s,
                        p_session_data->domain.len, p_session_data->domain.s);

                if (p_session_data->subscribed_to_signaling_path_status) {
                    LM_DBG("This is a subscription to signalling bearer session");
                    //instead of removing the contact from usrloc_pcscf we just change the state of the contact to TERMINATE_PENDING_NOTIFY
                    //pcscf_registrar sees this, sends a SIP PUBLISH and on SIP NOTIFY the contact is deleted
		    //note we only send SIP PUBLISH if the session has been successfully opened
		    
		    if(p_session_data->session_has_been_opened) {
			if (ul.register_udomain(p_session_data->domain.s, &domain)
				< 0) {
			    LM_DBG("Unable to register usrloc domain....aborting\n");
			    return;
			}
			ul.lock_udomain(domain, &p_session_data->registration_aor, &p_session_data->ip, p_session_data->recv_port);
			if (ul.get_pcontact(domain, &p_session_data->registration_aor, &p_session_data->ip, p_session_data->recv_port, &pcontact) != 0) {
			    LM_DBG("no contact found for terminated Rx reg session..... ignoring\n");
			} else {
			    LM_DBG("Updating contact [%.*s] after Rx reg session terminated, setting state to PCONTACT_DEREG_PENDING_PUBLISH\n", pcontact->aor.len, pcontact->aor.s);
			    ci.reg_state = PCONTACT_DEREG_PENDING_PUBLISH;
			    ci.num_service_routes = 0;
			    ul.update_pcontact(domain, &ci, pcontact);
			}
			ul.unlock_udomain(domain, &p_session_data->registration_aor, &p_session_data->ip, p_session_data->recv_port);
			counter_add(ims_qos_cnts_h.active_registration_rx_sessions, -1);
		    }
                } else {
                    LM_DBG("This is a media bearer session session");
		    
		    counter_add(ims_qos_cnts_h.active_media_rx_sessions, -1);
                    //we only terminate the dialog if this was triggered from the transport plane or timeout - i.e. if must_terminate_dialog is set
                    //if this was triggered from the signalling plane (i.e. someone hanging up) then we don'y need to terminate the dialog
                    if (p_session_data->must_terminate_dialog) {
                        LM_DBG("Terminating dialog with callid, ftag, ttag: [%.*s], [%.*s], [%.*s]\n",
                                p_session_data->callid.len, p_session_data->callid.s,
                                p_session_data->ftag.len, p_session_data->ftag.s,
                                p_session_data->ttag.len, p_session_data->ttag.s);
                        dlgb.terminate_dlg(&p_session_data->callid,
                                &p_session_data->ftag, &p_session_data->ttag, NULL,
                                &release_reason);
                    }
                }

                //free callback data
                if (p_session_data) {
		    free_callsessiondata(p_session_data);
                }
                break;
            default:
                break;
        }

        free_cdp_cb_event(ev);
    }
}

void free_cdp_cb_event(cdp_cb_event_t *ev) {
    if (ev) {
        LM_DBG("Freeing cdpb CB event structure\n");
        if (ev->rx_session_id.len > 0 && ev->rx_session_id.s) {
            LM_DBG("about to free string from cdp CB event [%.*s]\n", ev->rx_session_id.len, ev->rx_session_id.s);
            shm_free(ev->rx_session_id.s);
        }
        shm_free(ev);
    }
}


