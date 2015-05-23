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

#include "registrar_notify.h"

#include "reg_mod.h"
#include "../../lib/ims/ims_getters.h"
#include "regtime.h"
#include "usrloc_cb.h"


#include "../../parser/parse_from.h"
#include "../../parser/parse_content.h"
#include "../../parser/parse_uri.h"
#include <libxml/parser.h>

#include "../../lib/ims/useful_defs.h"

#define STATE_ACTIVE 1
#define STATE_TERMINATED 0
#define STATE_UNKNOWN -1

#define EVENT_UNKNOWN -1
#define EVENT_REGISTERED 0
#define EVENT_UNREGISTERED 1
#define EVENT_TERMINATED 2
#define EVENT_CREATED 3
#define EVENT_REFRESHED 4
#define EVENT_EXPIRED 5

/**
 * Initializes the reg notifications list.
 */
reg_notification_list *notification_list = 0; //< List of pending notifications

extern struct tm_binds tmb;

extern int notification_list_size_threshold;

extern int subscription_default_expires;
extern int subscription_min_expires;
extern int subscription_max_expires;

extern str scscf_name_str;

static str event_hdr = {"Event: reg\r\n", 12};
static str maxfwds_hdr = {"Max-Forwards: 70\r\n", 18};
static str subss_hdr1 = {"Subscription-State: ", 20};
static str subss_hdr2 = {"\r\n", 2};
static str ctype_hdr1 = {"Content-Type: ", 14};
static str ctype_hdr2 = {"\r\n", 2};

extern int ue_unsubscribe_on_dereg;
extern int subscription_expires_range;

extern char *domain;

/* \brief
 * Return randomized expires between expires-range% and expires.
 * RFC allows only value less or equal to the one provided by UAC.
 */

static inline int randomize_expires( int expires, int range )
{
	/* if no range is given just return expires */
	if(range == 0) return expires;

	int range_min = expires - (float)range/100 * expires;

	return range_min + (float)(rand()%100)/100 * ( expires - range_min );
}

int notify_init() {
    notification_list = shm_malloc(sizeof (reg_notification_list));
    if (!notification_list) {
	LM_ERR("No more SHM mem\n");
	return 0; 
    }
    memset(notification_list, 0, sizeof (reg_notification_list));
    notification_list->lock = lock_alloc();
    if (!notification_list->lock)  {
	LM_ERR("failed to create cdp event list lock\n");
	return 0;
    }
    if (lock_init(notification_list->lock)==0){
       lock_dealloc(notification_list->lock);
       notification_list->lock=0;
       LM_ERR("failed to initialize cdp event list lock\n");
       return 0;
    }
    notification_list->size = 0;
    sem_new(notification_list->empty, 0); //pre-locked - as we assume list is empty at start
    return 1;
}

/**
 * Destroys the reg notifications list.
 */
void notify_destroy() {
    reg_notification *n, *nn;
    lock_get(notification_list->lock);
    n = notification_list->head;
    while (n) {
        nn = n->next;
        free_notification(n);
        n = nn;
    }
    lock_destroy(notification_list->lock);
    lock_dealloc(notification_list->lock);
    shm_free(notification_list);
}

int can_publish_reg(struct sip_msg *msg, char *_t, char *str2) {

    int ret = CSCF_RETURN_FALSE;
    str presentity_uri = {0, 0};
    str event;
    str asserted_id;
    ucontact_t* c = 0;
    impurecord_t* r;
    int res;
    ims_public_identity *pi = 0;
    int i, j;

    LM_DBG("Checking if allowed to publish reg event\n");

    //check that this is a request
    if (msg->first_line.type != SIP_REQUEST) {
	LM_ERR("This message is not a request\n");
	goto error;
    }

    //check that this is a subscribe request
    if (msg->first_line.u.request.method.len != 7 ||
	    memcmp(msg->first_line.u.request.method.s, "PUBLISH", 7) != 0) {
	LM_ERR("This message is not a PUBLISH\n");
	goto error;
    }

    //check that this is a reg event - currently we only support reg event!
    event = cscf_get_event(msg);
    if (event.len != 3 || strncasecmp(event.s, "reg", 3) != 0) {
	LM_ERR("Accepting only <Event: reg>. Found: <%.*s>\n",
		event.len, event.s);
	goto done;
    }

    asserted_id = cscf_get_asserted_identity(msg, 0);
    if (!asserted_id.len) {
	LM_ERR("P-Asserted-Identity empty.\n");
	goto error;
    }
    LM_DBG("P-Asserted-Identity <%.*s>.\n", asserted_id.len, asserted_id.s);

    //get presentity URI
    presentity_uri = cscf_get_public_identity_from_requri(msg);

    LM_DBG("Looking for IMPU in usrloc <%.*s>\n", presentity_uri.len, presentity_uri.s);

    ul.lock_udomain((udomain_t*) _t, &presentity_uri);
    res = ul.get_impurecord((udomain_t*) _t, &presentity_uri, &r);

    if (res > 0) {
	LM_DBG("'%.*s' Not found in usrloc\n", presentity_uri.len, presentity_uri.s);
	ul.unlock_udomain((udomain_t*) _t, &presentity_uri);
	goto done;
    }

    LM_DBG("<%.*s> found in usrloc\n", presentity_uri.len, presentity_uri.s);

    //check if the asserted identity is in the same group as that presentity uri
    if (r->public_identity.len == asserted_id.len &&
	    strncasecmp(r->public_identity.s, asserted_id.s, asserted_id.len) == 0) {
	LM_DBG("Identity found as AOR <%.*s>\n",
		presentity_uri.len, presentity_uri.s);
	ul.unlock_udomain((udomain_t*) _t, &presentity_uri);
	ret = CSCF_RETURN_TRUE;
	goto done;
    }

    //check if asserted identity is in service profile
    lock_get(r->s->lock);
    if (r->s) {
	for (i = 0; i < r->s->service_profiles_cnt; i++)
	    for (j = 0; j < r->s->service_profiles[i].public_identities_cnt; j++) {
		pi = &(r->s->service_profiles[i].public_identities[j]);
		if (!pi->barring &&
			pi->public_identity.len == asserted_id.len &&
			strncasecmp(pi->public_identity.s, asserted_id.s, asserted_id.len) == 0) {
		    LM_DBG("Identity found in SP[%d][%d]\n",
			    i, j);
		    ret = CSCF_RETURN_TRUE;
		    ul.unlock_udomain((udomain_t*) _t, &presentity_uri);
		    lock_release(r->s->lock);
		    goto done;
		}
	    }
    }
    lock_release(r->s->lock);
    LM_DBG("Did not find p-asserted-identity <%.*s> in SP\n", asserted_id.len, asserted_id.s);

    //check if asserted is present in any of the path headers
    j=0;

    while (j<MAX_CONTACTS_PER_IMPU && (c=r->newcontacts[j])) {
	if (c->path.len) {
	    LM_DBG("Path: <%.*s>.\n",
		    c->path.len, c->path.s);
	    for (i = 0; i < c->path.len - (asserted_id.len - 4); i++){
		//we compare the asserted_id without "sip:" to the path 
		if (strncasecmp(c->path.s + i, asserted_id.s + 4, asserted_id.len - 4) == 0) {
		    LM_DBG("Identity found in Path <%.*s>\n",
			    c->path.len, c->path.s);
		    ret = CSCF_RETURN_TRUE;
		    ul.unlock_udomain((udomain_t*) _t, &presentity_uri);
		    goto done;
		}
	    }
	}
	j++;
    }
    LM_DBG("Did not find p-asserted-identity <%.*s> on Path\n", asserted_id.len, asserted_id.s);

    ul.unlock_udomain((udomain_t*) _t, &presentity_uri);
    LM_DBG("Publish forbidden\n");

done:
    if (presentity_uri.s) shm_free(presentity_uri.s); // shm_malloc in cscf_get_public_identity_from_requri
    return ret;
error:
    if (presentity_uri.s) shm_free(presentity_uri.s); // shm_malloc in cscf_get_public_identity_from_requri
    ret = CSCF_RETURN_ERROR;
    return ret;
}

int can_subscribe_to_reg(struct sip_msg *msg, char *_t, char *str2) {

    int ret = CSCF_RETURN_FALSE;
    str presentity_uri = {0, 0};
    str callid = {0, 0};
    str ftag = {0, 0};
    str ttag = {0, 0};
    str event;
    str asserted_id;
    ucontact_t* c = 0;
    impurecord_t* r;
    int res;

    ims_public_identity *pi = 0;
    int i, j;

    LM_DBG("Checking if allowed to subscribe to event\n");

    //check that this is a request
    if (msg->first_line.type != SIP_REQUEST) {
        LM_ERR("This message is not a request\n");
        goto error;
    }

    //check that this is a subscribe request
    if (msg->first_line.u.request.method.len != 9 ||
            memcmp(msg->first_line.u.request.method.s, "SUBSCRIBE", 9) != 0) {
        LM_ERR("This message is not a SUBSCRIBE\n");
        goto error;
    }

    //check that this is a reg event - currently we only support reg event!
    event = cscf_get_event(msg);
    if (event.len != 3 || strncasecmp(event.s, "reg", 3) != 0) {
        LM_ERR("Accepting only <Event: reg>. Found: <%.*s>\n",
                event.len, event.s);
        goto done;
    }
    
    //get callid, from and to tags to be able to identify dialog
    //callid
    callid = cscf_get_call_id(msg, 0);
    if (callid.len <= 0 || !callid.s) {
        LM_ERR("unable to get callid\n");
        goto error;
    }
    //ftag
    if (!cscf_get_from_tag(msg, &ftag)) {
        LM_ERR("Unable to get ftag\n");
        goto error;
    }
    
    //ttag
     if (!cscf_get_to_tag(msg, &ttag)) {
        LM_ERR("Unable to get ttag\n");
        goto error;
    }
    
    //get presentity URI
    //check if SUBSCRIBE is initial or SUBSEQUENT
    if (ttag.len == 0) {
        LM_DBG("Msg has no ttag - this is initial subscribe - get presentity URI from req URI\n");
	presentity_uri = cscf_get_public_identity_from_requri(msg);
    } else {
	presentity_uri = ul.get_presentity_from_subscriber_dialog(&callid, &ttag, &ftag);
	if (presentity_uri.len == 0) {
	    LM_ERR("Unable to get pres uri from subscriber dialog with callid <%.*s>, ttag <%.*s> and ftag <%.*s>\n", callid.len, callid.s, ttag.len, ttag.s, ftag.len, ftag.s);
	    goto done;
	}
    }


    asserted_id = cscf_get_asserted_identity(msg, 0);
    if (!asserted_id.len) {
        LM_ERR("P-Asserted-Identity empty.\n");
        goto error;
    }
    LM_DBG("P-Asserted-Identity <%.*s>.\n",
            asserted_id.len, asserted_id.s);

    LM_DBG("Looking for IMPU in usrloc <%.*s>\n", presentity_uri.len, presentity_uri.s);

    ul.lock_udomain((udomain_t*) _t, &presentity_uri);
    res = ul.get_impurecord((udomain_t*) _t, &presentity_uri, &r);
    
    if (res > 0) {
        LM_DBG("'%.*s' Not found in usrloc\n", presentity_uri.len, presentity_uri.s);
        ul.unlock_udomain((udomain_t*) _t, &presentity_uri);
        goto done;
    }
    
    LM_DBG("<%.*s> found in usrloc\n", presentity_uri.len, presentity_uri.s);

    //check if the asserted identity is in the same group as that presentity uri
    if (r->public_identity.len == asserted_id.len &&
            strncasecmp(r->public_identity.s, asserted_id.s, asserted_id.len) == 0) {
        LM_DBG("Identity found as AOR <%.*s>\n",
                presentity_uri.len, presentity_uri.s);
        ul.unlock_udomain((udomain_t*) _t, &presentity_uri);
        ret = CSCF_RETURN_TRUE;
        goto done;
    }

    //check if asserted identity is in service profile
    lock_get(r->s->lock);
    if (r->s) {
        for (i = 0; i < r->s->service_profiles_cnt; i++)
            for (j = 0; j < r->s->service_profiles[i].public_identities_cnt; j++) {
                pi = &(r->s->service_profiles[i].public_identities[j]);
                if (!pi->barring &&
                        pi->public_identity.len == asserted_id.len &&
                        strncasecmp(pi->public_identity.s, asserted_id.s, asserted_id.len) == 0) {
                    LM_DBG("Identity found in SP[%d][%d]\n",
                            i, j);
                    ret = CSCF_RETURN_TRUE;
                    ul.unlock_udomain((udomain_t*) _t, &presentity_uri);
                    lock_release(r->s->lock);
                    goto done;
                }
            }
    }
    lock_release(r->s->lock);
    LM_DBG("Did not find p-asserted-identity <%.*s> in SP\n", asserted_id.len, asserted_id.s);

    //check if asserted is present in any of the path headers
    j = 0;
    while (j < MAX_CONTACTS_PER_IMPU && (c = r->newcontacts[j])) {
	if (c->path.len) {
	    LM_DBG("Path: <%.*s>.\n",
		    c->path.len, c->path.s);
	    for (i = 0; i < c->path.len - (asserted_id.len - 4); i++) {
		//we compare the asserted_id without "sip:" to the path 
		if (strncasecmp(c->path.s + i, asserted_id.s + 4, asserted_id.len - 4) == 0) {
		    LM_DBG("Identity found in Path <%.*s>\n",
			    c->path.len, c->path.s);
		    ret = CSCF_RETURN_TRUE;
		    ul.unlock_udomain((udomain_t*) _t, &presentity_uri);
		    goto done;
		}
	    }
	}
	j++;
    }

    LM_DBG("Did not find p-asserted-identity <%.*s> on Path\n", asserted_id.len, asserted_id.s);
    
    ul.unlock_udomain((udomain_t*) _t, &presentity_uri);
    LM_DBG("Subscribe forbidden\n");

done:
	if (presentity_uri.s) shm_free(presentity_uri.s); // shm_malloc in cscf_get_public_identity_from_requri or get_presentity_from_subscriber_dialog
	return ret;
error:
    ret = CSCF_RETURN_ERROR;
	if (presentity_uri.s) shm_free(presentity_uri.s); // shm_malloc in cscf_get_public_identity_from_requri or get_presentity_from_subscriber_dialog
    return ret;
}

/*
 * called to deliver new event into notification process
 * return 0 on success. anything else failure
 */
int event_reg(udomain_t* _d, impurecord_t* r_passed, ucontact_t* c_passed, int event_type, str *presentity_uri, str *watcher_contact) {

    str content = {0, 0};
    impurecord_t* r;
    int num_impus;
    str* impu_list;
    int res = 0;
    udomain_t* udomain;

    get_act_time();
    
    LM_DBG("Sending Reg event notifies\n");
    LM_DBG("Switching on event type: %d", event_type);
    switch (event_type) {
        case IMS_REGISTRAR_NONE:
            return 0;
        case IMS_REGISTRAR_SUBSCRIBE:
            if (r_passed || c_passed || !presentity_uri || !watcher_contact || !_d) {
                LM_ERR("this is a subscribe called from cfg file: r_passed and c_passed should both be zero and presentity_uri, watcher_contact and _d should be valid for a subscribe");
                return 0;
            }
            LM_DBG("Event type is IMS REGISTRAR SUBSCRIBE about to get reginfo_full");
            //lets get IMPU list for presentity as well as register for callbacks (IFF its a new SUBSCRIBE)

            ul.lock_udomain(_d, presentity_uri);
            res = ul.get_impurecord(_d, presentity_uri, &r);
            if (res != 0) {
                LM_WARN("Strange, '%.*s' Not found in usrloc\n", presentity_uri->len, presentity_uri->s);
                ul.unlock_udomain(_d, presentity_uri);
                //no point in continuing
                return 1;
            }

            //get IMPU set from the presentity's subscription
            res = ul.get_impus_from_subscription_as_string(_d, r,
                    0/*all unbarred impus*/, &impu_list, &num_impus);
            if (res != 0) {
                LM_WARN("failed to get IMPUs from subscription\n");
                ul.unlock_udomain(_d, presentity_uri);
                if (impu_list) {
                    pkg_free(impu_list);
                }
                return 1;
            }
            ul.unlock_udomain((udomain_t*) _d, presentity_uri);

            content = generate_reginfo_full(_d, impu_list,
                    num_impus, 0, 0);

            if (impu_list) {
                pkg_free(impu_list);
            }

            LM_DBG("About to ceate notification");

            create_notifications(_d, r_passed, c_passed, presentity_uri, watcher_contact, content, event_type);
            if (content.s) pkg_free(content.s);
            //            if (send_now) notification_timer(0, 0);
            return 0;
            break;

            //richard: we only use reg unreg expired and refresh
        case IMS_REGISTRAR_CONTACT_UNREGISTERED:
        case IMS_REGISTRAR_CONTACT_REGISTERED:
        case IMS_REGISTRAR_CONTACT_REFRESHED:
        case IMS_REGISTRAR_CONTACT_EXPIRED:
            if (!r_passed || !c_passed || presentity_uri || watcher_contact || _d) {
                LM_ERR("this is a contact change passed from ul callback: r_passed and c_passed should both be valid and presentity_uri, watcher_contact and _d should be 0 for ul callback");
                return 0;
            }

	    //content = get_reginfo_partial(r_passed, c_passed, event_type);
	    
	    //this is a ulcallback so r_passed domain is already locked
	    res = ul.get_impus_from_subscription_as_string(_d, r_passed,
                    0/*all unbarred impus*/, &impu_list, &num_impus);
	    if (res != 0) {
                LM_WARN("failed to get IMPUs from subscription\n");
                if (impu_list) {
                    pkg_free(impu_list);
                }
                return 1;
            }
	    
	    //TODO this should be a configurable module param
	    if (ul.register_udomain(domain, &udomain) < 0) {
		LM_ERR("Unable to register usrloc domain....aborting\n");
		return 0;
	    }
//	    
	    content = generate_reginfo_full(udomain, impu_list,
                    num_impus, &r_passed->public_identity, 1);
//	    
	    if (impu_list) {
                pkg_free(impu_list);
            }

            LM_DBG("About to ceate notification");
	    
            create_notifications(_d, r_passed, c_passed, presentity_uri, watcher_contact, content, event_type);
            if (content.s) pkg_free(content.s);
            //                        if (send_now) notification_timer(0, 0);
            return 1;

        default:
            LM_ERR("ERR:event_reg: Unknown event %d\n", event_type);
            //            if (send_now) notification_timer(0, 0);
            return 1;
    }
}

int process_contact(impurecord_t* presentity_impurecord, udomain_t * _d, int expires, str contact_uri, int contact_state) {
	
	int ret = CSCF_RETURN_TRUE;
	int i, j;
	ims_public_identity* pi = 0;
	struct ucontact* ucontact;
	str callid = {0, 0};
	str path = {0, 0};
	ims_subscription* subscription = 0;
	impurecord_t* implicit_impurecord = 0;

	//first get the subscription
	//then go through each implicit public identity (exclude the explicit identity)
	//get the IMPU rec for each implicit public identity
	//then get the contact for each implicit IMPU and delete if contact_state == STATE_TERMINATED
	//then get the contact for each explicit IMPU and delete if contact_state == STATE_TERMINATED
	
	subscription = presentity_impurecord->s;
	if (!subscription) {
	    LM_DBG("No subscriber info associated with <%.*s>, so no implicit IMPUs to process\n", presentity_impurecord->public_identity.len, presentity_impurecord->public_identity.s);
	    goto done;
	} 

	lock_get(subscription->lock);
	subscription->ref_count++;
	LM_DBG("subscription ref count after add is now %d\n", subscription->ref_count);
	lock_release(subscription->lock);

	//now update the implicit set
	for (i = 0; i < subscription->service_profiles_cnt; i++) {
	    for (j = 0; j < subscription->service_profiles[i].public_identities_cnt; j++) {
		pi = &(subscription->service_profiles[i].public_identities[j]);

		if (memcmp(presentity_impurecord->public_identity.s, pi->public_identity.s, presentity_impurecord->public_identity.len) == 0) { //we don't need to update the explicit IMPU
		    LM_DBG("Ignoring explicit identity <%.*s>, updating later.....\n", presentity_impurecord->public_identity.len, presentity_impurecord->public_identity.s);
		    goto next_implicit_impu;
		}
		ul.lock_udomain(_d, &pi->public_identity);
		if (ul.get_impurecord(_d, &pi->public_identity, &implicit_impurecord) != 0) {
		    LM_DBG("usrloc does not have imprecord for implicity IMPU, ignore\n");
		    goto next_implicit_impu;
		}
		if (ul.get_ucontact(implicit_impurecord, &contact_uri, &callid, &path, 0/*cseq*/,  &ucontact) != 0) { //contact does not exist
		    LM_DBG("This contact: <%.*s> is not in usrloc, ignore - NOTE: You need S-CSCF usrloc set to match_mode CONTACT_ONLY\n", contact_uri.len, contact_uri.s);
		    goto next_implicit_impu;
		} else {//contact exists
			if (contact_state == STATE_TERMINATED) {
				//delete contact
				LM_DBG("This contact <%.*s> is in state terminated and is in usrloc so removing it from usrloc\n", contact_uri.len, contact_uri.s);
				ul.lock_contact_slot(&contact_uri);
				if (ul.unlink_contact_from_impu(implicit_impurecord, ucontact, 1) != 0) {
				    LM_ERR("Failed to delete ucontact <%.*s> from implicit IMPU\n", contact_uri.len, contact_uri.s);
				    ul.unlock_contact_slot(&contact_uri);
				    ul.release_ucontact(ucontact);
				    goto next_implicit_impu;	//TODO: don't need to use goto here...
				}
				ul.unlock_contact_slot(&contact_uri);
			}else {//state is active
				LM_DBG("This contact: <%.*s> is not in state terminated and is in usrloc, ignore\n", contact_uri.len, contact_uri.s);
				ul.release_ucontact(ucontact);
				goto next_implicit_impu;
			}
			ul.release_ucontact(ucontact);
		}
next_implicit_impu:
		ul.unlock_udomain(_d, &pi->public_identity);
	    }
	}

	lock_get(subscription->lock);
	subscription->ref_count--;
	LM_DBG("subscription ref count after sub is now %d\n", subscription->ref_count);
	lock_release(subscription->lock);

	
	ul.lock_udomain(_d, &presentity_impurecord->public_identity);
	
	if (ul.get_ucontact(presentity_impurecord, &contact_uri, &callid, &path, 0/*cseq*/,  &ucontact) != 0) { //contact does not exist
	    LM_DBG("This contact: <%.*s> is not in usrloc, ignore - NOTE: You need S-CSCF usrloc set to match_mode CONTACT_ONLY\n", contact_uri.len, contact_uri.s);
	    goto done;
	} else {//contact exists
		if (contact_state == STATE_TERMINATED) {
			//delete contact
			LM_DBG("This contact <%.*s> is in state terminated and is in usrloc so removing it from usrloc\n", contact_uri.len, contact_uri.s);
			ul.lock_contact_slot(&contact_uri);
			if (ul.unlink_contact_from_impu(presentity_impurecord, ucontact, 1) != 0) {
			    LM_ERR("Failed to delete ucontact <%.*s>\n", contact_uri.len, contact_uri.s);
			    ret = CSCF_RETURN_FALSE;
    			    ul.unlock_contact_slot(&contact_uri);
			    ul.release_ucontact(ucontact);
			    goto done;
			}
			ul.unlock_contact_slot(&contact_uri);
		}else {//state is active
			LM_DBG("This contact: <%.*s> is not in state terminated and is in usrloc, ignore\n", contact_uri.len, contact_uri.s);
			ul.release_ucontact(ucontact);
			goto done;
		}
		ul.release_ucontact(ucontact);
	}
	
done:
	    ul.unlock_udomain(_d, &presentity_impurecord->public_identity);
	    return ret;
}


int reginfo_parse_state(char * s) {
	if (s == NULL) {
		return STATE_UNKNOWN;
	}
	switch (strlen(s)) {
		case 6:
			if (strncmp(s, "active", 6) ==  0) return STATE_ACTIVE;
			break;
		case 10:
			if (strncmp(s, "terminated", 10) ==  0) return STATE_TERMINATED;
			break;
		default:
			LM_ERR("Unknown State %s\n", s);
			return STATE_UNKNOWN;
	}
	LM_ERR("Unknown State %s\n", s);
	return STATE_UNKNOWN;
}

int reginfo_parse_event(char * s) {
	if (s == NULL) {
		return EVENT_UNKNOWN;
	}
	switch (strlen(s)) {
		case 7:
			if (strncmp(s, "created", 7) ==  0) return EVENT_CREATED;
			if (strncmp(s, "expired", 7) ==  0) return EVENT_EXPIRED;
			break;
		case 9:
			if (strncmp(s, "refreshed", 9) ==  0) return EVENT_CREATED;
			break;
		case 10:
			if (strncmp(s, "registered", 10) ==  0) return EVENT_REGISTERED;
			if (strncmp(s, "terminated", 10) ==  0) return EVENT_TERMINATED;
			break;
		case 12:
			if (strncmp(s, "unregistered", 12) ==  0) return EVENT_UNREGISTERED;
			break;
		default:
			LM_ERR("Unknown Event %s\n", s);
			return EVENT_UNKNOWN;
	}
	LM_ERR("Unknown Event %s\n", s);
	return EVENT_UNKNOWN;
}

xmlNodePtr xmlGetNodeByName(xmlNodePtr parent, const char *name) {
	xmlNodePtr cur = parent;
	xmlNodePtr match = NULL;
	while (cur) {
		if (xmlStrcasecmp(cur->name, (unsigned char*)name) == 0)
			return cur;
		match = xmlGetNodeByName(cur->children, name);
		if (match)
			return match;
		cur = cur->next;
	}
	return NULL;
}

char * xmlGetAttrContentByName(xmlNodePtr node, const char *name) {
	xmlAttrPtr attr = node->properties;
	while (attr) {
		if (xmlStrcasecmp(attr->name, (unsigned char*)name) == 0)
			return (char*)xmlNodeGetContent(attr->children);
		attr = attr->next;
	}
	return NULL;
}


int process_publish_body(struct sip_msg* msg, str publish_body, udomain_t * domain) {
	xmlDocPtr doc= NULL;
	xmlNodePtr doc_root = NULL, registrations = NULL, contacts = NULL, uris = NULL;
	str aor = {0, 0};
	str callid = {0, 0};
	str contact_uri = {0, 0};
	str received = {0,0};
	str path = {0,0};
	str user_agent = {0, 0};
	int reg_state, contact_state, event, expires, result, final_result = CSCF_RETURN_FALSE;
	char * expires_char,  * cseq_char;
	int cseq = 0;
	impurecord_t* presentity_impurecord;
	
	doc = xmlParseMemory(publish_body.s, publish_body.len);
	if(doc== NULL)  {
		LM_ERR("Error while parsing the xml body message, Body is:\n%.*s\n",
			publish_body.len, publish_body.s);
		return -1;
	}
	doc_root = xmlGetNodeByName(doc->children, "reginfo");
	if(doc_root == NULL) {
		LM_ERR("while extracting the reginfo node\n");
		goto error;
	}
	registrations = doc_root->children;
	while (registrations) {
		/* Only process registration sub-items */
		if (xmlStrcasecmp(registrations->name, BAD_CAST "registration") != 0)
			goto next_registration;
		reg_state = reginfo_parse_state(xmlGetAttrContentByName(registrations, "state"));
		aor.s = xmlGetAttrContentByName(registrations, "aor");
		if (aor.s == NULL) {
			LM_ERR("No AOR for this registration!\n");		
			goto next_registration;
		}
		aor.len = strlen(aor.s);
		LM_DBG("AOR %.*s has reg_state \"%d\"\n", aor.len, aor.s, reg_state);
		
		//TOD get IMPU record here
		ul.lock_udomain(domain, &aor);
		if (ul.get_impurecord(domain, &aor, &presentity_impurecord) != 0) {
		    LM_DBG("usrloc does not have imprecord for presentity being published too, ignore\n");
		    ul.unlock_udomain(domain, &aor);
		    goto next_registration;
		}
		ul.unlock_udomain(domain, &aor);

		LM_DBG("Received impurecord for presentity being published on [%.*s]\n", presentity_impurecord->public_identity.len, presentity_impurecord->public_identity.s);
		
		if (reg_state == STATE_TERMINATED) {
		    LM_DBG("This impurecord is in STATE_TERMINATED - TODO we should should delete all contacts");
		}
		else {
		    /* Now lets process the Contact's from this Registration: */
		    contacts = registrations->children;
		    while (contacts) {
			    if (xmlStrcasecmp(contacts->name, BAD_CAST "contact") != 0)
				    goto next_contact;
			    callid.s = xmlGetAttrContentByName(contacts, "callid");
			    if (callid.s == NULL) {
				    LM_DBG("No Call-ID for this contact!\n");		
				    callid.len = 0;
			    } else {
				    callid.len = strlen(callid.s);
				    LM_DBG("contact has callid <%.*s>\n", callid.len, callid.s);		
			    }	

			    received.s = xmlGetAttrContentByName(contacts, "received");
			    if (received.s == NULL) {
				    LM_DBG("No received for this contact!\n");
				    received.len = 0;
			    } else {
				    received.len = strlen(received.s);
				    LM_DBG("contact has received <%.*s>\n", received.len, received.s);
			    }

			    path.s = xmlGetAttrContentByName(contacts, "path");	
			    if (path.s == NULL) {
				    LM_DBG("No path for this contact!\n");
				    path.len = 0;
			    } else {
				    path.len = strlen(path.s);
				    LM_DBG("contact has path <%.*s>\n", path.len, path.s);
			    }

			    user_agent.s = xmlGetAttrContentByName(contacts, "user_agent");
			    if (user_agent.s == NULL) {
				    LM_DBG("No user_agent for this contact!\n");
				    user_agent.len = 0;
			    } else {
				    user_agent.len = strlen(user_agent.s);
				    LM_DBG("contact has user_agent <%.*s>\n", user_agent.len, user_agent.s);
			    }
			    event = reginfo_parse_event(xmlGetAttrContentByName(contacts, "event"));
			    if (event == EVENT_UNKNOWN) {
				    LM_ERR("No event for this contact - going to next contact!\n");		
				    goto next_contact;
			    }
			    expires_char = xmlGetAttrContentByName(contacts, "expires");
			    if (expires_char == NULL) {
				    LM_ERR("No expires for this contact - going to next contact!\n");		
				    goto next_contact;
			    }
			    expires = atoi(expires_char);
			    if (expires < 0) {
				    LM_ERR("No valid expires for this contact - going to next contact!\n");		
				    goto next_contact;
			    }

			    contact_state = reginfo_parse_state(xmlGetAttrContentByName(contacts, "state"));
			    if (contact_state == STATE_UNKNOWN) {
				LM_ERR("No state for this contact - going to next contact!\n");		
				goto next_contact;
			    } 

			    LM_DBG("Contact state %d: Event \"%d\", expires %d\n", contact_state, event, expires);

			    cseq_char = xmlGetAttrContentByName(contacts, "cseq");
			    if (cseq_char == NULL) {
				    LM_DBG("No cseq for this contact!\n");		
			    } else {
				    cseq = atoi(cseq_char);
				    if (cseq < 0) {
					    LM_DBG("No valid cseq for this contact!\n");		
				    }
			    }

			    /* Now lets process the URI's from this Contact: */
			    uris = contacts->children;
			    while (uris) {
				    if (xmlStrcasecmp(uris->name, BAD_CAST "uri") != 0)
					    goto next_uri;
				    contact_uri.s = (char*)xmlNodeGetContent(uris);	
				    if (contact_uri.s == NULL) {
					    LM_ERR("No URI for this contact - going to next registration!\n");		
					    goto next_registration;
				    }
				    contact_uri.len = strlen(contact_uri.s);
				    LM_DBG("Contact: %.*s\n",
					    contact_uri.len, contact_uri.s);

				    /* Add to Usrloc: */
				    result = process_contact(presentity_impurecord, domain, expires, contact_uri, contact_state);

				    /* Process the result */
				    if (final_result != CSCF_RETURN_TRUE) final_result = result;
    next_uri:
				    uris = uris->next;
			    }
    next_contact:
			    contacts = contacts->next;
		    }
		}
next_registration:
		registrations = registrations->next;
	}
error:
	/* Free the XML-Document */
    	if(doc) xmlFreeDoc(doc);
	return final_result;
}


/**
 * Modify the subscription based on publish
 * @param msg - the SIP PUBLISH message
 * @param str1 - not used
 * @param str2 - not used
 * @returns #CSCF_RETURN_TRUE if allowed, #CSCF_RETURN_FALSE if not, #CSCF_RETURN_ERROR on error
 */
int publish_reg(struct sip_msg *msg, char *_t, char *str2) {
    
    udomain_t* domain = (udomain_t*) _t;
    int expires = 0;
    int ret = CSCF_RETURN_FALSE;
    str body;
    
    LM_DBG("Publishing reg info\n");
    
    
    /* If not done yet, parse the whole message now: */
    if (parse_headers(msg, HDR_EOH_F, 0) == -1) {
	    LM_ERR("Error parsing headers\n");
	    return -1;
    }
    if (get_content_length(msg) == 0) {
	    LM_DBG("Content length = 0\n");
	    /* No Body? Then there is no published information available, which is ok. */
	    goto done;
    } else {
	    body.s=get_body(msg);
	    if (body.s== NULL) {
		    LM_ERR("cannot extract body from msg\n");
		    goto done;
	    }
	    body.len = get_content_length(msg);
    }

    LM_DBG("Body is %.*s\n", body.len, body.s);

    ret = process_publish_body(msg, body, (udomain_t*)domain);
    
done:
    //get expires
    expires = cscf_get_expires_hdr(msg, 0);
    if (expires == -1) expires = subscription_default_expires;
    
    if(ret==CSCF_RETURN_TRUE){
	LM_DBG("Sending 200 OK to publishing user");
	subscribe_reply(msg, 200, MSG_REG_PUBLISH_OK, &expires, &scscf_name_str);
    }
       
    return ret;
}

/**
 * Save this subscription.
 * @param msg - the SIP SUBSCRIBE message
 * @param str1 - not used
 * @param str2 - not used
 * @returns #CSCF_RETURN_TRUE if allowed, #CSCF_RETURN_FALSE if not, #CSCF_RETURN_ERROR on error
 */
int subscribe_to_reg(struct sip_msg *msg, char *_t, char *str2) {
    int ret = CSCF_RETURN_FALSE;
    int res;
    str presentity_uri = {0, 0};
    str event;
    int event_i = IMS_EVENT_NONE;
    int expires = 0, expires_time = 0;
    str watcher_impu;
    str watcher_contact;
    impurecord_t* presentity_impurecord;
    reg_subscriber *reg_subscriber;
    subscriber_data_t subscriber_data;

    int event_type = IMS_REGISTRAR_NONE;

    int rt = 0;

    str callid = {0, 0};
    str ftag = {0, 0};
    str ttag = {0, 0};
    str record_route = {0, 0};

    int remote_cseq = 0;
    int local_cseq = 0;

    udomain_t* domain = (udomain_t*) _t;

    LM_DBG("Saving SUBSCRIBE\n");

    //check we have a valid transaction, if not, create one TODO

    //check that this is a request
    if (msg->first_line.type != SIP_REQUEST) {
        LM_ERR("This message is not a request\n");
        ret = CSCF_RETURN_FALSE;
        goto error;
    }

    //check that this is a subscribe
    if (msg->first_line.u.request.method.len != 9 ||
            memcmp(msg->first_line.u.request.method.s, "SUBSCRIBE", 9) != 0) {
        LM_ERR("This message is not a SUBSCRIBE\n");
        ret = CSCF_RETURN_FALSE;
        goto error;
    }

    //check that this is a reg event  - we currently only support reg event
    event = cscf_get_event(msg);
    if (event.len != 3 || strncasecmp(event.s, "reg", 3) != 0) {
        LM_WARN("Accepting only <Event: reg>. Found: <%.*s>\n",
                event.len, event.s);
        ret = CSCF_RETURN_FALSE;
        goto error;
    }
    if (event.len == 0 && strncasecmp(event.s, "reg", 3) == 0)
        event_i = IMS_EVENT_REG;

    //get callid, from and to tags to be able to identify dialog
    //callid
    callid = cscf_get_call_id(msg, 0);
    if (callid.len <= 0 || !callid.s) {
        LM_ERR("unable to get callid\n");
        ret = CSCF_RETURN_FALSE;
        goto error;
    }
    //ftag
    if (!cscf_get_from_tag(msg, &ftag)) {
        LM_ERR("Unable to get ftag\n");
        ret = CSCF_RETURN_FALSE;
        goto error;
    }
    
    //ttag
     if (!cscf_get_to_tag(msg, &ttag)) {
        LM_ERR("Unable to get ttag\n");
        ret = CSCF_RETURN_FALSE;
        goto error;
    }
    
    //check if SUBSCRIBE is initial or SUBSEQUENT
    if (ttag.len == 0) {
        LM_DBG("Msg has no ttag - this is initial subscribe\n");
	//to tag - doesn't exist in initial request, must use tm to get it
	tmb.t_get_reply_totag(msg, &ttag);
	LM_DBG("Got to tag from sent response: [%.*s]", ttag.len, ttag.s);
	LM_DBG("This is initial subscribe - get presentity URI from req URI\n");
	presentity_uri = cscf_get_public_identity_from_requri(msg);
	
    } else {
	LM_DBG("Msg has ttag: [%.*s] - this is subsequent subscribe\n", ttag.len, ttag.s);
	//cscf_get_to_uri(msg, &presentity_uri);
	LM_DBG("This is subsequent subscribe - get presentity URI from stored subscriber dialog\n");
	//get the presentity uri from To Header
	//cscf_get_to_uri(msg, &presentity_uri);
	presentity_uri = ul.get_presentity_from_subscriber_dialog(&callid, &ttag, &ftag);
	if (presentity_uri.len == 0) {
	    LM_ERR("Unable to get pres uri from subscriber dialog with callid <%.*s>, ttag <%.*s> and ftag <%.*s>\n", callid.len, callid.s, ttag.len, ttag.s, ftag.len, ftag.s);
	    ret = CSCF_RETURN_FALSE;
	    goto error;
	}
    }

    //get cseq
    remote_cseq = cscf_get_cseq(msg, 0);
    local_cseq = remote_cseq + 1;

    //get sockinfo_str
    str sockinfo_str = msg->rcv.bind_address->sock_str;

    //get record route
    /*process record route and add it to a string*/
    if (msg->record_route != NULL) {
        rt = print_rr_body(msg->record_route, &record_route, 0, 0);
        if (rt != 0) {
            LM_ERR("Failed processing the record route [%d]\n", rt);
            record_route.s = NULL;
            record_route.len = 0;
            ret = CSCF_RETURN_FALSE;
            goto error;
        }
    }

    //get the presentity uri from To Header
    //cscf_get_to_uri(msg, &presentity_uri);
	
    //get the watcher uri from the to header
    cscf_get_from_uri(msg, &watcher_impu);

    if (!watcher_impu.len) {
        LM_ERR("Failed to get URI from To header.\n");
        ret = CSCF_RETURN_FALSE;
        goto error;
    }
    LM_DBG("To header URI (watcher URI) <%.*s>.\n",
            watcher_impu.len, watcher_impu.s);

    //get the watcher contact from contact header
    watcher_contact = cscf_get_contact(msg);
    if (!watcher_contact.len) {
        LM_ERR("ERR: Contact empty.\n");
        ret = CSCF_RETURN_FALSE;
        goto error;
    }
    LM_DBG("watcher Contact <%.*s>.\n",
            watcher_contact.len, watcher_contact.s);

    //get expires
    expires = cscf_get_expires_hdr(msg, 0);
    if (expires == -1) expires = subscription_default_expires;

    //build subscriber parcel for passing data around more easily
    subscriber_data.callid = &callid;
    subscriber_data.event = event_i;
    subscriber_data.ftag = &ftag;
    subscriber_data.ttag = &ttag;
    subscriber_data.record_route = &record_route;
    subscriber_data.sockinfo_str = &sockinfo_str;
    subscriber_data.local_cseq = local_cseq;
    subscriber_data.watcher_uri = &watcher_impu;
    subscriber_data.watcher_contact = &watcher_contact;
    subscriber_data.version = 1; /*default version starts at 1*/
    
    if (expires > 0) {
        LM_DBG("expires is more than zero - SUBSCRIBE");
        event_type = IMS_REGISTRAR_SUBSCRIBE;

        if (expires < subscription_min_expires) expires = subscription_min_expires;
        if (expires > subscription_max_expires) expires = subscription_max_expires;
	
	expires = randomize_expires(expires, subscription_expires_range);

        get_act_time();
        expires_time = expires + act_time;

        subscriber_data.expires = expires_time;


        LM_DBG("Subscription expires time <%d> expiry length <%d>\n",
                expires_time, expires);

        LM_DBG("Received a new subscription (expires > 0), checking to see of impurecord for presentity exists\n");
        ul.lock_udomain(domain, &presentity_uri);
        res = ul.get_impurecord(domain, &presentity_uri, &presentity_impurecord);
        if (res != 0) {
            LM_DBG("usrloc does not have imprecord for presentity being subscribed too, This a problem we shouldn't get here as offline users should have been assigned in config file\n");
            ul.unlock_udomain(domain, &presentity_uri);
            ret = CSCF_RETURN_FALSE;
            goto error;
        }

        LM_DBG("Received impurecord for presentity being subscribed to [%.*s]\n", presentity_impurecord->public_identity.len, presentity_impurecord->public_identity.s);
	
        res = ul.get_subscriber(presentity_impurecord, &presentity_uri, &watcher_contact, event_i, &reg_subscriber);
	if (res != 0) {
            LM_DBG("this must be a new subscriber, lets add it\n");
	    subscriber_data.presentity_uri = &presentity_impurecord->public_identity;
            res = ul.add_subscriber(presentity_impurecord, &subscriber_data, &reg_subscriber, 0 /*not a db_load*/);
            if (res != 0) {
                LM_ERR("Failed to add new subscription\n");
                ul.unlock_udomain(domain, &presentity_uri);
                ret = CSCF_RETURN_FALSE;
                goto error;
            }
        } else {

            if(memcmp(reg_subscriber->call_id.s, subscriber_data.callid->s, reg_subscriber->call_id.len) == 0 && 
		    memcmp(reg_subscriber->from_tag.s, subscriber_data.ftag->s, reg_subscriber->from_tag.len) == 0 && 
		    memcmp(reg_subscriber->to_tag.s, subscriber_data.ttag->s, reg_subscriber->to_tag.len) == 0) {
		LM_DBG("This has same callid, fromtag and totag - must be a re subscribe, lets update it\n");
		res = ul.update_subscriber(presentity_impurecord, &reg_subscriber, &expires_time, 0, 0);
		if (res != 1) {
		    LM_ERR("Failed to update subscription - expires is %d\n", expires_time);
		    ul.unlock_udomain(domain, &presentity_uri);
		    ret = CSCF_RETURN_FALSE;
		    goto error;
		}
	    } else {
		LM_ERR("Re-subscribe for same watcher_contact, presentity_uri, event but with different callid [%.*s], fromtag [%.*s] and totag [%.*s] for presentity [%.*s] and watcher contact [%.*s] - What happened?\n",
			subscriber_data.callid->len, subscriber_data.callid->s,
			subscriber_data.ftag->len, subscriber_data.ftag->s,
			subscriber_data.ttag->len, subscriber_data.ttag->s,
			presentity_impurecord->public_identity.len, presentity_impurecord->public_identity.s,
			subscriber_data.watcher_contact->len, subscriber_data.watcher_contact->s);
		LM_DBG("Removing old subscriber and adding new one\n");
		subscriber_data.presentity_uri = &presentity_impurecord->public_identity;
		ul.external_delete_subscriber(reg_subscriber, (udomain_t*) _t, 0 /*domain is already locked*/);
		res = ul.add_subscriber(presentity_impurecord, &subscriber_data, &reg_subscriber, 0 /*not a db_load*/);
		if (res != 0) {
		    LM_ERR("Failed to add new subscription\n");
		    ul.unlock_udomain(domain, &presentity_uri);
		    ret = CSCF_RETURN_FALSE;
		    goto error;
		}
	    }
        }

        ul.unlock_udomain(domain, &presentity_uri);

        ret = CSCF_RETURN_TRUE;
        LM_DBG("Sending 200 OK to subscribing user");
        subscribe_reply(msg, 200, MSG_REG_SUBSCRIBE_OK, &expires, &scscf_name_str);

        //do reg event every time you get a subscribe
	if (event_reg(domain, 0, 0, event_type, &presentity_uri, &watcher_contact) != 0) {
	    LM_ERR("failed adding notification for reg events\n");
	    ret = CSCF_RETURN_ERROR;
	    goto error;
	} else {
	    LM_DBG("success adding notification for reg events\n");
	}
    } else {
        event_type = IMS_REGISTRAR_UNSUBSCRIBE;
        LM_DBG("expires is zero or less - UNSUBSCRIBE");

        ul.lock_udomain(domain, &presentity_uri);
        res = ul.get_impurecord(domain, &presentity_uri, &presentity_impurecord);
        if (res != 0) {
            LM_DBG("usrloc does not have imprecord for presnetity being subscribed too, we should create one.... TODO\n");
            ul.unlock_udomain(domain, &presentity_uri);
            goto error;
        }
        LM_DBG("Received impurecord for presentity being unsubscribed to [%.*s]\n", presentity_impurecord->public_identity.len, presentity_impurecord->public_identity.s);
        //        //get the subscription if it exists
        LM_DBG("Getting subscription s from usrloc");

        res = ul.get_subscriber(presentity_impurecord, &presentity_uri, &watcher_contact, event_i, &reg_subscriber);
        if (res != 0) {
            LM_WARN("could not get subscriber\n");
            ret = CSCF_RETURN_FALSE;
            ul.unlock_udomain(domain, &presentity_uri);
            goto error;
        } else {
            LM_DBG("subscription s exists");
            LM_DBG("deleting subscriber from usrloc");
            ul.external_delete_subscriber(reg_subscriber, (udomain_t*) _t, 0 /*domain is already locked*/);
	    ul.unlock_udomain(domain, &presentity_uri);
        }
        ret = CSCF_RETURN_TRUE;
        LM_DBG("Sending 200 OK to subscribing user");
        subscribe_reply(msg, 200, MSG_REG_UNSUBSCRIBE_OK, &expires, &scscf_name_str);
    }

    //free memory
    if (presentity_uri.s) shm_free(presentity_uri.s); // shm_malloc in cscf_get_public_identity_from_requri or get_presentity_from_subscriber_dialog
    if (record_route.s) pkg_free(record_route.s);
    return ret;
error:
    //free memory
    if (presentity_uri.s) shm_free(presentity_uri.s); // shm_malloc in cscf_get_public_identity_from_requri or get_presentity_from_subscriber_dialog
    if (record_route.s) pkg_free(record_route.s);

    return ret;
}



str expires_hdr1 = {"Expires: ", 9};
str expires_hdr2 = {"\r\n", 2};
str contact_hdr1 = {"Contact: <", 10};
str contact_hdr2 = {">\r\n", 3};

/**
 * Replies to a SUBSCRIBE and also adds the need headers.
 * Path for example.
 * @param msg - the SIP SUBSCRIBE message
 * @param code - response code to send
 * @param text - response phrase to send
 * @param expires - expiration interval in seconds
 * @param contact - contact to add to reply
 * @returns the tmn.r_reply returned value value
 */
int subscribe_reply(struct sip_msg *msg, int code, char *text, int *expires, str * contact) {
    str hdr = {0, 0};

    if (expires) {
        hdr.len = expires_hdr1.len + 12 + expires_hdr1.len;
        hdr.s = pkg_malloc(hdr.len);
        if (!hdr.s) {
            LM_ERR("Error allocating %d bytes.\n",
                    hdr.len);
        } else {
            hdr.len = 0;
            STR_APPEND(hdr, expires_hdr1);
            sprintf(hdr.s + hdr.len, "%d", *expires);
            hdr.len += strlen(hdr.s + hdr.len);
            STR_APPEND(hdr, expires_hdr2);
            cscf_add_header_rpl(msg, &hdr);
            pkg_free(hdr.s);
        }
    }

    if (contact) {
        hdr.len = contact_hdr1.len + contact->len + contact_hdr2.len;
        hdr.s = pkg_malloc(hdr.len);
        if (!hdr.s) {
            LM_ERR("Error allocating %d bytes.\n",
                    hdr.len);
        } else {
            hdr.len = 0;
            STR_APPEND(hdr, contact_hdr1);
            STR_APPEND(hdr, *contact);
            STR_APPEND(hdr, contact_hdr2);
            cscf_add_header_rpl(msg, &hdr);
            pkg_free(hdr.s);
        }
    }

    return tmb.t_reply(msg, code, text);

}

/* function to convert contact aor to only have data after @ - ie strip user part */
int aor_to_contact(str* aor, str* contact) {
	char* p;
	int ret = 0;	//success

	contact->s = aor->s;
	contact->len = aor->len;
	if (memcmp(aor->s, "sip:", 4) == 0) {
		contact->s = aor->s + 4;
		contact->len-=4;
	}

	if ((p=memchr(contact->s, '@', contact->len))) {
		contact->len -= (p - contact->s + 1);
		contact->s = p+1;
	}

	if ((p=memchr(contact->s, ';', contact->len))) {
		contact->len = p - contact->s;
	}

	if ((p=memchr(contact->s, '>', contact->len))) {
		contact->len = p - contact->s;
	}

	return ret;
}

/*!
 * \brief Match a contact record to a contact string but only compare the ip port portion
 * \param ptr contact record
 * \param _c contact string
 * \return ptr on successfull match, 0 when they not match
 */
int contact_port_ip_match(str *c1, str *c2) {
    
    str ip_port1, ip_port2;
    aor_to_contact(c1, &ip_port1);//strip userpart from test contact
    aor_to_contact(c2, &ip_port2);//strip userpart from test contact
    LM_DBG("Matching contact using only port and ip - comparing [%.*s] and [%.*s]\n", ip_port1.len, ip_port1.s, ip_port2.len, ip_port2.s);
    if ((ip_port1.len == ip_port2.len) && !memcmp(ip_port1.s, ip_port2.s, ip_port1.len)) {
	return 1;
    }
    return 0;
}

static str subs_terminated = {"terminated", 10};
static str subs_active = {"active;expires=", 15};

/**
 * Creates notifications with the given content for all of the subscribers.
 * @param r - r_public* to which it refers
 * @param for_s - the r_subscriber*  to which it refers or NULL if for all
 * @param content - the body content
 * @param expires - the remaining subcription expiration time in seconds
 */
void create_notifications(udomain_t* _t, impurecord_t* r_passed, ucontact_t* c_passed, str *presentity_uri, str *watcher_contact, str content, int event_type) {

    reg_notification *n;
    reg_subscriber *s;
    impurecord_t* r;
    int local_cseq = 0;
    int version = 0;

    str subscription_state = {"active;expires=10000000000", 26},
    content_type = {"application/reginfo+xml", 23};

    int domain_locked = -1;

    get_act_time();

    int res;

    LM_DBG("Creating notification");

    if (r_passed && c_passed && !presentity_uri && !watcher_contact) {
        LM_DBG("r_passed and c_passed are valid and presentity uri and watcher_contact is 0 - this must be a ul callback no need to lock domain");
        r = r_passed;

    } else {
        LM_DBG("This must be a cfg file subscribe need to lock domain and get impurecord");
        ul.lock_udomain(_t, presentity_uri);
        res = ul.get_impurecord(_t, presentity_uri, &r);
        if (res != 0) {
            LM_WARN("No IMPU... ignoring\n");
            ul.unlock_udomain(_t, presentity_uri);
            return;
        }
        domain_locked = 1;
    }

    s = r->shead;
    while (s) {
        LM_DBG("Scrolling through reg subscribers for this IMPU");

        if (s->expires > act_time) {
            LM_DBG("Expires is greater than current time!");
            subscription_state.s = pkg_malloc(32);
            subscription_state.len = 0;
            if (subscription_state.s) {

                sprintf(subscription_state.s, "%.*s%ld", subs_active.len,
                        subs_active.s, s->expires - act_time);
                subscription_state.len = strlen(subscription_state.s);
            }

            LM_DBG("Subscription state: [%.*s]", subscription_state.len, subscription_state.s);

        } else {
            STR_PKG_DUP(subscription_state, subs_terminated, "pkg subs state");
            LM_DBG("Expires is past than current time!");
            LM_DBG("Subscription state: [%.*s]", subscription_state.len, subscription_state.s);
        }

	//This is a fix to ensure that when a user subscribes a full reg info is only sent to that UE
        if (event_type == IMS_REGISTRAR_SUBSCRIBE) {
	    if (contact_port_ip_match(watcher_contact, &s->watcher_contact) &&
                    (presentity_uri->len == s->presentity_uri.len) && (memcmp(s->presentity_uri.s, presentity_uri->s, presentity_uri->len) == 0)) {
                LM_DBG("This is a fix to ensure that we only send full reg info XML to the UE that just subscribed");
                LM_DBG("about to make new notification!");
		
		LM_DBG("we always increment the local cseq and version before we send a new notification\n");
		
		local_cseq = s->local_cseq + 1;
		version = s->version + 1;
		ul.update_subscriber(r, &s, 0, &local_cseq, &version);
		
                n = new_notification(subscription_state, content_type, content, s);
                if (n) {
                    LM_DBG("Notification exists - about to add it");
                    add_notification(n);
                } else {
                    LM_DBG("Notification does not exist");
                }
            }
        } else {
	    
	    if(event_type == IMS_REGISTRAR_CONTACT_UNREGISTERED && !ue_unsubscribe_on_dereg && 
		    (contact_port_ip_match(&c_passed->c, &s->watcher_contact) &&
	                (r_passed->public_identity.len == s->presentity_uri.len) && (memcmp(s->presentity_uri.s, r_passed->public_identity.s, r_passed->public_identity.len) == 0))){
		//if this is UNREGISTER and the UEs do not unsubscribe to dereg and this is a UE subscribing to its own reg event
		//then we do not send notifications
		LM_DBG("This is a UNREGISTER event for a UE that subscribed to its own state that does not unsubscribe to dereg - therefore no notification");
	    }
	    else{
		LM_DBG("about to make new notification!");
		
		LM_DBG("we always increment the local cseq and version before we send a new notification\n");
		
		local_cseq = s->local_cseq + 1;
		version = s->version + 1;
		ul.update_subscriber(r, &s, 0, &local_cseq, &version);
		
		n = new_notification(subscription_state, content_type, content, s);
		if (n) {
		    LM_DBG("Notification exists - about to add it");
		    add_notification(n);

		} else {
		    LM_DBG("Notification does not exist");
		}
	    }
	}
        s = s->next;

        if (subscription_state.s) {
            pkg_free(subscription_state.s);
        }
    }

    if (domain_locked == 1) {
        ul.unlock_udomain(_t, presentity_uri);
    }

    return;
out_of_memory:
    return;
}

/*We currently only support certain unknown params to be sent in NOTIFY bodies
 This prevents having compatability issues with UEs including non-standard params in contact header
 Supported params:
 */
static str param_q = {"q", 1};
static str param_video = {"video", 5};
static str param_expires = {"expires", 7};
static str param_sip_instance = {"+sip.instance", 13};
static str param_3gpp_smsip = {"+g.3gpp.smsip", 13};
static str param_3gpp_icsi_ref = {"+g.3gpp.icsi-ref", 16};
int inline supported_param(str *param_name) {
    
    if(strncasecmp(param_name->s, param_q.s, param_name->len) == 0) {
	return 0;
    } else if (strncasecmp(param_name->s, param_video.s, param_name->len) == 0) {
	return 0;
    } else if (strncasecmp(param_name->s, param_expires.s, param_name->len) == 0) {
	return 0;
    } else if (strncasecmp(param_name->s, param_sip_instance.s, param_name->len) == 0) {
	return 0;
    } else if (strncasecmp(param_name->s, param_3gpp_smsip.s, param_name->len) == 0) {
	return 0;
    } else if (strncasecmp(param_name->s, param_3gpp_icsi_ref.s, param_name->len) == 0) {
	return 0;
    } else {
	return -1;
    }
}

/** Maximum reginfo XML size */
#define MAX_REGINFO_SIZE 16384

static str xml_start = {"<?xml version=\"1.0\"?>\n", 22};

static str r_full = {"full", 4};
static str r_partial = {"partial", 7};
static str r_reginfo_s = {"<reginfo xmlns=\"urn:ietf:params:xml:ns:reginfo\" version=\"%s\" state=\"%.*s\">\n", 74};
static str r_reginfo_e = {"</reginfo>\n", 11};

static str r_active = {"active", 6};
static str r_terminated = {"terminated", 10};
static str registration_s = {"\t<registration aor=\"%.*s\" id=\"%p\" state=\"%.*s\">\n", 48};
static str registration_e = {"\t</registration>\n", 17};

//richard: we only use reg unreg refrsh and expire
static str r_registered = {"registered", 10};
static str r_refreshed = {"refreshed", 9};
static str r_expired = {"expired", 7};
static str r_unregistered = {"unregistered", 12};
static str contact_s = {"\t\t<contact id=\"%p\" state=\"%.*s\" event=\"%.*s\" expires=\"%d\">\n", 59};
static str contact_s_q = {"\t\t<contact id=\"%p\" state=\"%.*s\" event=\"%.*s\" expires=\"%d\" q=\"%.3f\">\n", 69};
static str contact_s_params_with_body = {"\t\t<unknown-param name=\"%.*s\">\"%.*s\"</unknown-param>\n", 1};
/**NOTIFY XML needs < to be replaced by &lt; and > to be replaced by &gt;*/
/*For params that need to be fixed we pass in str removing first and last character and replace them with &lt; and &gt;**/
static str contact_s_params_with_body_fix = {"\t\t<unknown-param name=\"%.*s\">\"&lt;%.*s&gt;\"</unknown-param>\n", 1};
static str contact_s_params_no_body = {"\t\t<unknown-param name=\"%.*s\"></unknown-param>\n", 1};
static str contact_e = {"\t\t</contact>\n", 13};

static str uri_s = {"\t\t\t<uri>", 8};
static str uri_e = {"</uri>\n", 7};

/**
 * Creates the full reginfo XML.
 * @param pv - the r_public to create for
 * @param event_type - event type
 * @param subsExpires - subscription expiration
 * @returns the str with the XML content
 * if its a new subscription we do things like subscribe to updates on IMPU, etc
 */
str generate_reginfo_full(udomain_t* _t, str* impu_list, int num_impus, str *primary_impu, int primary_locked) {
    str x = {0, 0};
    str buf, pad;
    char bufc[MAX_REGINFO_SIZE], padc[MAX_REGINFO_SIZE];
    impurecord_t *r;
    int i, j, res;
    ucontact_t* ptr;
    param_t *param;

    buf.s = bufc;
    buf.len = 0;
    pad.s = padc;
    pad.len = 0;
    
    int domain_locked = 1;
    int terminate_impu = 1;
    
    int expires;

    LM_DBG("Getting reginfo_full");

    STR_APPEND(buf, xml_start);
    sprintf(pad.s, r_reginfo_s.s, "%d", r_full.len, r_full.s);
    pad.len = strlen(pad.s);
    STR_APPEND(buf, pad);

    for (i = 0; i < num_impus; i++) {
	LM_DBG("Scrolling through public identities, current one <%.*s>", impu_list[i].len, impu_list[i].s);
	if(primary_locked && strncasecmp(impu_list[i].s, primary_impu->s, impu_list[i].len) == 0) {
	    LM_DBG("Don't need to lock this impu [%.*s]  as its a ulcallback so already locked\n", impu_list[i].len, impu_list[i].s);
	    domain_locked = 0;
	} else {
	    LM_DBG("Need to lock this impu\n");
	    ul.lock_udomain(_t, &impu_list[i]);
	    domain_locked = 1;
	}
	
	res = ul.get_impurecord(_t, &(impu_list[i]), &r);
	if (res != 0) {
	    LM_WARN("impu disappeared, ignoring it\n");
	    if(domain_locked) {
		ul.unlock_udomain(_t, &impu_list[i]);
	    }
	    continue;
	}
	
        LM_DBG("Retrieved IMPU record");
	
	j=0;
	terminate_impu = 1;
	while (j<MAX_CONTACTS_PER_IMPU && (ptr=r->newcontacts[j])) {
	    if (((ptr->expires - act_time) > 0)) {
		LM_DBG("IMPU <%.*s> has another active contact <%.*s> so will set its state to active\n",
			r->public_identity.len, r->public_identity.s, ptr->c.len, ptr->c.s);
		terminate_impu = 0;
		break;
	    }
	    j++;
	}
	if(terminate_impu) {
	    LM_DBG("IMPU reg state has no active contacts so putting in status terminated");
            sprintf(pad.s, registration_s.s, r->public_identity.len,
                    r->public_identity.s, r, r_terminated.len,
                    r_terminated.s);
	} else {
	    LM_DBG("IMPU has active contacts so putting in status active");
            sprintf(pad.s, registration_s.s, r->public_identity.len,
                    r->public_identity.s, r, r_active.len, r_active.s);
	}

        pad.len = strlen(pad.s);
        STR_APPEND(buf, pad);
        
	j=0;
	LM_DBG("Scrolling through contact for this IMPU");
	while (j < MAX_CONTACTS_PER_IMPU && (ptr = r->newcontacts[j])) {
	    if (ptr->q != -1) {
		LM_DBG("q value not equal to -1");
		float q = (float) ptr->q / 1000;
		expires = ptr->expires - act_time;
		if(expires < 0) {
		    LM_WARN("Contact expires is negative - setting to 0\n");
		    expires = 0;
		}
		if(expires == 0) {
		    sprintf(pad.s, contact_s_q.s, ptr, r_terminated.len, r_terminated.s,
			r_expired.len, r_expired.s, expires,
			q);
		} else {
		    sprintf(pad.s, contact_s_q.s, ptr, r_active.len, r_active.s,
			r_registered.len, r_registered.s, expires,
			q);
		}
		
	    } else {
		LM_DBG("q value equal to -1");
		expires = ptr->expires - act_time;
		if(expires < 0) {
		    LM_WARN("Contact expires is negative - setting to 0\n");
		    expires = 0;
		}
		if(expires == 0) {
		    sprintf(pad.s, contact_s_q.s, ptr, r_terminated.len, r_terminated.s,
			r_expired.len, r_expired.s, expires);
		} else {
		    sprintf(pad.s, contact_s_q.s, ptr, r_active.len, r_active.s,
			r_registered.len, r_registered.s, expires);
		}
	    }
	    pad.len = strlen(pad.s);
	    STR_APPEND(buf, pad);
	    STR_APPEND(buf, uri_s);
	    
	    LM_DBG("Appending contact address: <%.*s>", ptr->c.len, ptr->c.s);

	    STR_APPEND(buf, (ptr->c));
	    STR_APPEND(buf, uri_e);
	    
	    param = ptr->params;
	    while (param) {
			if (supported_param(&param->name) != 0) { 
				param = param->next;
				continue;
			}
		
		if(param->body.len > 0) {
		    LM_DBG("This contact has params name: [%.*s] body [%.*s]\n", param->name.len, param->name.s, param->body.len, param->body.s);
		    if (param->body.s[0] == '<' && param->body.s[param->body.len -1] == '>') {
			LM_DBG("This param body starts with '<' and ends with '>' we will clean these for the NOTIFY XML with &lt; and &gt;\n");
			sprintf(pad.s, contact_s_params_with_body_fix.s, param->name.len, param->name.s, param->body.len - 2, param->body.s + 1);
		    } else {
			sprintf(pad.s, contact_s_params_with_body.s, param->name.len, param->name.s, param->body.len, param->body.s);
		    }
		    
		    pad.len = strlen(pad.s);
		    STR_APPEND(buf, pad);
		} else {
		    LM_DBG("This contact has params name: [%.*s] \n", param->name.len, param->name.s);
		    sprintf(pad.s, contact_s_params_no_body.s, param->name.len, param->name.s);
		    pad.len = strlen(pad.s);
		    STR_APPEND(buf, pad);
		}
		param = param->next;
	    }
	    STR_APPEND(buf, contact_e);
	    j++;
	}
	
        STR_APPEND(buf, registration_e);

	if(domain_locked) {
	    ul.unlock_udomain(_t, &impu_list[i]);
	}
    }

    STR_APPEND(buf, r_reginfo_e);

    x.s = pkg_malloc(buf.len + 1);
    if (x.s) {
        x.len = buf.len;
        memcpy(x.s, buf.s, buf.len);
        x.s[x.len] = 0;
    }

    LM_DBG("Returned full reg-info: [%.*s]", x.len, x.s);

    return x;
}

/**
 * Creates the partial reginfo XML.
 * @param pv - the r_public to create for
 * @param pc - the r_contatct to create for
 * @param event_type - event type
 * @param subsExpires - subscription expiration
 * @returns the str with the XML content
 */

str get_reginfo_partial(impurecord_t *r, ucontact_t *c, int event_type) {
    str x = {0, 0};
    int i;
    str buf, pad;
    char bufc[MAX_REGINFO_SIZE], padc[MAX_REGINFO_SIZE];
    int expires = -1;
    int terminate_impu = 1;
    ucontact_t *c_tmp;
    str state, event;
    param_t *param;
    
    buf.s = bufc;
    buf.len = 0;
    pad.s = padc;
    pad.len = 0;

    STR_APPEND(buf, xml_start);
    sprintf(pad.s, r_reginfo_s.s, "%d", r_partial.len, r_partial.s);
    pad.len = strlen(pad.s);
    STR_APPEND(buf, pad);


    if (r) {
        expires = c->expires - act_time;
	if(expires < 0) {
	    LM_WARN("Contact expires is negative - setting to 0\n");
	    expires = 0;
	}
	
        if (//richard we only use expired and unregistered
                (event_type == IMS_REGISTRAR_CONTACT_EXPIRED ||
                event_type == IMS_REGISTRAR_CONTACT_UNREGISTERED)
                ){
	    //check if impu record has any other active contacts - if not then set this to terminated - if so then keep this active
	    //check if asserted is present in any of the path headers
	    
	    
	    i=0;
	    while (i<MAX_CONTACTS_PER_IMPU && (c_tmp=r->newcontacts[i])) {
		if ((strncasecmp(c_tmp->c.s, c->c.s, c_tmp->c.len) != 0) && ((c_tmp->expires - act_time) > 0)) {
		    LM_DBG("IMPU <%.*s> has another active contact <%.*s> so will set its state to active\n",
			    r->public_identity.len, r->public_identity.s, c_tmp->c.len, c_tmp->c.s);
		    terminate_impu = 0;
		    break;
		}
		i++;
	    }
	    if(terminate_impu)
		sprintf(pad.s, registration_s.s, r->public_identity.len, r->public_identity.s, r, r_terminated.len, r_terminated.s);
	    else
		sprintf(pad.s, registration_s.s, r->public_identity.len, r->public_identity.s, r, r_active.len, r_active.s);
	}
        else{
	    sprintf(pad.s, registration_s.s, r->public_identity.len, r->public_identity.s, r, r_active.len, r_active.s);
	}

        pad.len = strlen(pad.s);
        STR_APPEND(buf, pad);
        if (c) {
            switch (event_type) {

                    //richard we only use registered and refreshed and expired and unregistered
                case IMS_REGISTRAR_CONTACT_REGISTERED:
                    state = r_active;
                    event = r_registered;
                    break;
                case IMS_REGISTRAR_CONTACT_REFRESHED:
                    state = r_active;
                    event = r_refreshed;
                    break;
                case IMS_REGISTRAR_CONTACT_EXPIRED:
                    state = r_terminated;
                    event = r_expired;
                    expires = 0;
                    break;
                case IMS_REGISTRAR_CONTACT_UNREGISTERED:
                    state = r_terminated;
                    event = r_unregistered;
                    expires = 0;
                    break;
                default:
                    state = r_active;
                    event = r_registered;
            }
            if (c->q != -1) {
                float q = (float) c->q / 1000;
		sprintf(pad.s, contact_s_q.s, c, state.len, state.s, event.len, event.s, expires, q);
            } else
                sprintf(pad.s, contact_s.s, c, state.len, state.s, event.len, event.s, expires);
            pad.len = strlen(pad.s);
            STR_APPEND(buf, pad);
            STR_APPEND(buf, uri_s);
            STR_APPEND(buf, (c->c));
            STR_APPEND(buf, uri_e);
	    
	    param = c->params;
	    while (param && supported_param(&param->name) == 0) {
		
		if(param->body.len > 0) {
		    LM_DBG("This contact has params name: [%.*s] body [%.*s]\n", param->name.len, param->name.s, param->body.len, param->body.s);
		    if (param->body.s[0] == '<' && param->body.s[param->body.len -1] == '>') {
			LM_DBG("This param body starts with '<' and ends with '>' we will clean these for the NOTIFY XML with &lt; and &gt;\n");
			sprintf(pad.s, contact_s_params_with_body_fix.s, param->name.len, param->name.s, param->body.len - 2, param->body.s + 1);
		    } else {
			sprintf(pad.s, contact_s_params_with_body.s, param->name.len, param->name.s, param->body.len, param->body.s);
		    }
		    
		    pad.len = strlen(pad.s);
		    STR_APPEND(buf, pad);
		} else {
		    LM_DBG("This contact has params name: [%.*s] \n", param->name.len, param->name.s);
		    sprintf(pad.s, contact_s_params_no_body.s, param->name.len, param->name.s);
		    pad.len = strlen(pad.s);
		    STR_APPEND(buf, pad);
		}
		param = param->next;
	    }
	    
            STR_APPEND(buf, contact_e);
            STR_APPEND(buf, registration_e);
        }
    }

    STR_APPEND(buf, r_reginfo_e);


    x.s = pkg_malloc(buf.len + 1);
    if (x.s) {
        x.len = buf.len;
        memcpy(x.s, buf.s, buf.len);
        x.s[x.len] = 0;
    }
    return x;
}

/**
 * Callback for the UAC response to NOTIFY
 */
void uac_request_cb(struct cell *t, int type, struct tmcb_params * ps) {
    LM_DBG("received NOTIFY reply type [%d] and code [%d]\n", type, ps->code);
}

static int free_tm_dlg(dlg_t * td) {
    if (td) {
        if (td->route_set)
            free_rr(&td->route_set);
        pkg_free(td);
    }
    return 0;
}

/**
 * Creates a NOTIFY message and sends it
 * @param n - the r_notification to create the NOTIFY after
 */


void send_notification(reg_notification * n) {
    str h = {0, 0};

    uac_req_t uac_r;
    dlg_t* td = NULL;

    str method = {"NOTIFY", 6};

    LM_DBG("DBG:send_notification: NOTIFY about <%.*s>\n", n->watcher_uri.len, n->watcher_uri.s);

    h.len = 0;
    h.len += contact_hdr1.len + scscf_name_str.len + contact_hdr2.len;
    if (n->subscription_state.len) h.len += subss_hdr1.len + subss_hdr2.len + n->subscription_state.len;
    h.len += event_hdr.len;
    h.len += maxfwds_hdr.len;
    if (n->content_type.len) h.len += ctype_hdr1.len + ctype_hdr2.len + n->content_type.len;
    h.s = pkg_malloc(h.len);
    if (!h.s) {
        LM_ERR("ERR:send_notification: Error allocating %d bytes\n", h.len);
        h.len = 0;
    }


    //Add SCSCF name as contact address
    h.len = 0;
    STR_APPEND(h, contact_hdr1);
    STR_APPEND(h, scscf_name_str);
    STR_APPEND(h, contact_hdr2);

    STR_APPEND(h, event_hdr);
    STR_APPEND(h, maxfwds_hdr);
    if (n->subscription_state.len) {
        STR_APPEND(h, subss_hdr1);
        STR_APPEND(h, n->subscription_state);
        STR_APPEND(h, subss_hdr2);
    }
    if (n->content_type.len) {
        STR_APPEND(h, ctype_hdr1);
        STR_APPEND(h, n->content_type);
        STR_APPEND(h, ctype_hdr2);
    }

    /* construct the dlg_t structure */
    td = build_dlg_t_from_notification(n);
    if (td == NULL) {
        LM_ERR("while building dlg_t structure\n");
        free_tm_dlg(td);
        return;
    }


    if (n->content.len) {

        LM_DBG("Notification content exists - about to send notification with subscription state: [%.*s] content_type: [%.*s] content: [%.*s] : presentity_uri: [%.*s] watcher_uri: [%.*s]",
                n->subscription_state.len, n->subscription_state.s, n->content_type.len, n->content_type.s, n->content.len, n->content.s,
                n->presentity_uri.len, n->presentity_uri.s, n->watcher_uri.len, n->watcher_uri.s);

        set_uac_req(&uac_r, &method, &h, &n->content, td, TMCB_LOCAL_COMPLETED,
                uac_request_cb, 0);
        tmb.t_request_within(&uac_r);
    } else {
        LM_DBG("o notification content - about to send notification with subscription state: [%.*s] presentity_uri: [%.*s] watcher_uri: [%.*s]",
                n->subscription_state.len, n->subscription_state.s, n->presentity_uri.len, n->presentity_uri.s,
                n->watcher_uri.len, n->watcher_uri.s);


        set_uac_req(&uac_r, &method, &h, 0, td, TMCB_LOCAL_COMPLETED,
                uac_request_cb, 0);
        tmb.t_request_within(&uac_r);
    }
    if (h.s) pkg_free(h.s);
    free_tm_dlg(td);

}

/**
 * Creates a notification based on the given parameters
 * @param req_uri - the Request-URI for the NOTIFY
 * @param uri - uri to send to
 * @param subscription_state - the Subscription-State header value
 * @param event - the event
 * @param content_type - content type
 * @param content - content
 * @param dialog - dialog to send on
 * @returns the r_notification or NULL on error
 */
reg_notification * new_notification(str subscription_state,
        str content_type, str content, reg_subscriber * r) {

    reg_notification *n = 0;

    str buf;
    char bufc[MAX_REGINFO_SIZE];

    if (content.len > MAX_REGINFO_SIZE) {
        LM_ERR("content size (%d) exceeds MAX_REGINFO_SIZE (%d)!\n", content.len, MAX_REGINFO_SIZE);
        return 0;
    }

    sprintf(bufc, content.s, r->version);
    buf.s = bufc;
    buf.len = strlen(bufc);


    int len;
    char *p;

    len = sizeof (reg_notification) + r->call_id.len + r->from_tag.len + r->to_tag.len + r->watcher_uri.len + r->watcher_contact.len +
            r->record_route.len + r->sockinfo_str.len + r->presentity_uri.len + subscription_state.len + content_type.len + buf.len;

    LM_DBG("Creating new notification");

    n = (reg_notification*) shm_malloc(len);
    if (n == 0) {
        LM_ERR("no more shm mem (%d)\n", len);
        return 0;
    }
    memset(n, 0, len);

    p = (char*) (n + 1);
    
    n->local_cseq = r->local_cseq;

    n->call_id.s = p;
    n->call_id.len = r->call_id.len;
    memcpy(p, r->call_id.s, r->call_id.len);
    p += r->call_id.len;
    LM_DBG("call id: [%.*s]", n->call_id.len, n->call_id.s);

    n->from_tag.s = p;
    n->from_tag.len = r->from_tag.len;
    memcpy(p, r->from_tag.s, r->from_tag.len);
    p += r->from_tag.len;
    LM_DBG("from tag: [%.*s]", n->from_tag.len, n->from_tag.s);

    n->to_tag.s = p;
    n->to_tag.len = r->to_tag.len;
    memcpy(p, r->to_tag.s, r->to_tag.len);
    p += r->to_tag.len;
    LM_DBG("to tag: [%.*s]", n->to_tag.len, n->to_tag.s);

    n->watcher_uri.s = p;
    n->watcher_uri.len = r->watcher_uri.len;
    memcpy(p, r->watcher_uri.s, r->watcher_uri.len);
    p += r->watcher_uri.len;
    LM_DBG("watcher_uri: [%.*s]", n->watcher_uri.len, n->watcher_uri.s);

    n->watcher_contact.s = p;
    n->watcher_contact.len = r->watcher_contact.len;
    memcpy(p, r->watcher_contact.s, r->watcher_contact.len);
    p += r->watcher_contact.len;
    LM_DBG("watcher_contact: [%.*s]", n->watcher_contact.len, n->watcher_contact.s);

    n->record_route.s = p;
    n->record_route.len = r->record_route.len;
    memcpy(p, r->record_route.s, r->record_route.len);
    p += r->record_route.len;
    LM_DBG("record_route: [%.*s]", n->record_route.len, n->record_route.s);

    n->sockinfo_str.s = p;
    n->sockinfo_str.len = r->sockinfo_str.len;
    memcpy(p, r->sockinfo_str.s, r->sockinfo_str.len);
    p += r->sockinfo_str.len;
    LM_DBG("sockinfo_str: [%.*s]", n->sockinfo_str.len, n->sockinfo_str.s);

    n->presentity_uri.s = p;
    n->presentity_uri.len = r->presentity_uri.len;
    memcpy(p, r->presentity_uri.s, r->presentity_uri.len);
    p += r->presentity_uri.len;
    LM_DBG("presentity_uri: [%.*s]", n->presentity_uri.len, n->presentity_uri.s);

    n->subscription_state.s = p;
    n->subscription_state.len = subscription_state.len;
    memcpy(p, subscription_state.s, subscription_state.len);
    p += subscription_state.len;
    LM_DBG("Notification subscription state: [%.*s]", n->subscription_state.len, n->subscription_state.s);

    n->content_type.s = p;
    n->content_type.len = content_type.len;
    memcpy(p, content_type.s, content_type.len);
    p += content_type.len;
    LM_DBG("Notification content type: [%.*s]", n->content_type.len, n->content_type.s);

    n->content.s = p;
    n->content.len = buf.len;
    memcpy(p, buf.s, buf.len);
    p += buf.len;
    LM_DBG("Notification content: [%.*s]", n->content.len, n->content.s);

    if (p != (((char*) n) + len)) {
        LM_CRIT("buffer overflow\n");
        free_notification(n);
        return 0;
    }

    return n;
}

/**
 * Adds a notification to the list of notifications at the end (FIFO).
 * @param n - the notification to be added
 */
void add_notification(reg_notification * n) {

    LM_DBG("Adding notification");
    if (!n) {
        LM_DBG("Notification does not exist");
        return;
    } else {
        LM_DBG("Notification exists");
    }
    LM_DBG("Adding to notification list");
    lock_get(notification_list->lock);
    n->next = 0;
    n->prev = notification_list->tail;
    if (notification_list->tail) notification_list->tail->next = n;
    notification_list->tail = n;
    if (!notification_list->head) notification_list->head = n;
    notification_list->size++;
    if(notification_list_size_threshold > 0 && notification_list->size > notification_list_size_threshold) {
	    LM_WARN("notification_list is size [%d] and has exceed notification_list_size_threshold of [%d]", notification_list->size, notification_list_size_threshold);
    }
    
    sem_release(notification_list->empty);
    lock_release(notification_list->lock);
}

/**
* Pop a notification to the list of notifications from the top
*/
reg_notification* get_notification() {
    reg_notification * n;

    lock_get(notification_list->lock);
    while (notification_list->head == 0) {
        lock_release(notification_list->lock);
        sem_get(notification_list->empty);
        lock_get(notification_list->lock);
    }

    n = notification_list->head;
    notification_list->head = n->next;

    if (n == notification_list->tail) { //list now empty
        notification_list->tail = 0;
    }
    n->next = 0; //make sure whoever gets this cant access our list
    notification_list->size--;
    lock_release(notification_list->lock);

    return n;
}

/**
 * This is the main event process for notifications to be sent
 */
void notification_event_process() {

    reg_notification *n = 0;
    
    LM_DBG("Running notification_event_process");
    
    for (;;) {
        n = get_notification();
	LM_DBG("About to send notification");
        send_notification(n);
        LM_DBG("About to free notification");
        free_notification(n);
    }
}


/**
 * Frees up space taken by a notification
 * @param n - the notification to be freed
 */
void free_notification(reg_notification * n) {
    if (n) {
        shm_free(n);
    }
}

dlg_t * build_dlg_t_from_notification(reg_notification * n) {
    dlg_t* td = NULL;
    td = (dlg_t*) pkg_malloc(sizeof (dlg_t));
    if (td == NULL) {
        LM_ERR("Error ran out of package memory");
    }
    memset(td, 0, sizeof (dlg_t));

    LM_DBG("Building dlg_t structure");

    td->loc_seq.value = n->local_cseq;
    LM_DBG("local cseq %d", n->local_cseq);
    td->loc_seq.is_set = 1;

    td->id.call_id = n->call_id;
    LM_DBG("call id %.*s", n->call_id.len, n->call_id.s);

    td->id.rem_tag = n->from_tag;
    LM_DBG("ftag %.*s", n->from_tag.len, n->from_tag.s);


    td->id.loc_tag = n->to_tag;
    LM_DBG("ttag %.*s", n->to_tag.len, n->to_tag.s);


    td->loc_uri = n->presentity_uri;
    LM_DBG("loc uri %.*s", n->presentity_uri.len, n->presentity_uri.s);

    td->rem_target = n->watcher_contact;
    LM_DBG("rem target %.*s", n->watcher_contact.len, n->watcher_contact.s);

    td->rem_uri = n->watcher_uri;
    LM_DBG("rem uri %.*s", n->watcher_uri.len, n->watcher_uri.s);

    if (n->record_route.s && n->record_route.len) {
        if (parse_rr_body(n->record_route.s, n->record_route.len,
                &td->route_set) < 0) {
            LM_ERR("in function parse_rr_body\n");
            goto error;
        }
    }
    td->state = DLG_CONFIRMED;

    if (n->sockinfo_str.len) {
        int port, proto;
        str host;
        char* tmp;
        if ((tmp = as_asciiz(&n->sockinfo_str)) == NULL) {
            LM_ERR("no pkg memory left\n");
            goto error;
        }
        if (parse_phostport(tmp, &host.s,
                &host.len, &port, &proto)) {
            LM_ERR("bad sockinfo string\n");
            pkg_free(tmp);
            goto error;
        }
        pkg_free(tmp);
        td->send_sock = grep_sock_info(
                &host, (unsigned short) port, (unsigned short) proto);
    }

    return td;

error:
    free_tm_dlg(td);
    return NULL;
}
