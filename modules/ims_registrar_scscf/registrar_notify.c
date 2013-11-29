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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 * 
 */

#include "registrar_notify.h"

#include "reg_mod.h"
#include "../../lib/ims/ims_getters.h"
#include "regtime.h"
#include "usrloc_cb.h"

#include "../../lib/ims/useful_defs.h"

/**
 * Initializes the reg notifications list.
 */
reg_notification_list *notification_list = 0; //< List of pending notifications

extern struct tm_binds tmb;

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

int notify_init() {
    notification_list = shm_malloc(sizeof (reg_notification_list));
    if (!notification_list) return 0;
    memset(notification_list, 0, sizeof (reg_notification_list));
    notification_list->lock = lock_alloc();
    if (!notification_list->lock) return 0;
    notification_list->lock = lock_init(notification_list->lock);
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

int can_subscribe_to_reg(struct sip_msg *msg, char *_t, char *str2) {

    int ret = CSCF_RETURN_FALSE;
    str presentity_uri = {0, 0};
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
        ret = CSCF_RETURN_FALSE;
        goto done;
    }

    //get the target/presentity URI from the request uri
    presentity_uri = cscf_get_public_identity_from_requri(msg);


    asserted_id = cscf_get_asserted_identity(msg);
    if (!asserted_id.len) {
        LM_ERR("P-Asserted-Identity empty.\n");
        ret = CSCF_RETURN_FALSE;
        goto done;
    }
    LM_DBG("P-Asserted-Identity <%.*s>.\n",
            asserted_id.len, asserted_id.s);

    LM_DBG("Looking for IMPU in usrloc <%.*s>\n", presentity_uri.len, presentity_uri.s);

    ul.lock_udomain((udomain_t*) _t, &presentity_uri);
    res = ul.get_impurecord((udomain_t*) _t, &presentity_uri, &r);
    
    if (res > 0) {
        LM_DBG("'%.*s' Not found in usrloc\n", presentity_uri.len, presentity_uri.s);
        ul.unlock_udomain((udomain_t*) _t, &presentity_uri);
        ret = CSCF_RETURN_FALSE;
        goto done;
    }

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

    //check if asserted is present in any of the path headers
    c = r->contacts;

    while (c) {
	if (c->path.len) {
            for (i = 0; i < c->path.len - asserted_id.len; i++)
                if (strncasecmp(c->path.s + i, asserted_id.s, asserted_id.len) == 0) {
                    LM_DBG("Identity found in Path <%.*s>\n",
                            c->path.len, c->path.s);
                    ret = CSCF_RETURN_TRUE;
                    ul.unlock_udomain((udomain_t*) _t, &presentity_uri);
                    goto done;
                }
        }
        c = c->next;
    }
    
    ul.unlock_udomain((udomain_t*) _t, &presentity_uri);

done:
    if (presentity_uri.s) shm_free(presentity_uri.s);
    return ret;
error:
    if (presentity_uri.s) shm_free(presentity_uri.s);
    ret = CSCF_RETURN_ERROR;
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
            int res = ul.get_impurecord(_d, presentity_uri, &r);
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
                    num_impus);

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

            content = get_reginfo_partial(r_passed, c_passed, event_type);
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

    int new_subscription = 1;
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
    //to tag - doesn't exist in request, must use tm to get it
    tmb.t_get_reply_totag(msg, &ttag);
    LM_DBG("Got to tag from sent response: %.*s", ttag.len, ttag.s);

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

    //get the presentity uri from the request uri
    presentity_uri = cscf_get_public_identity_from_requri(msg);

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

    if (expires > 0) {
        LM_DBG("expires is more than zero - SUBSCRIBE");
        event_type = IMS_REGISTRAR_SUBSCRIBE;

        if (expires < subscription_min_expires) expires = subscription_min_expires;
        if (expires > subscription_max_expires) expires = subscription_max_expires;

        get_act_time();
        expires_time = expires + act_time;

        subscriber_data.expires = expires_time;


        LM_DBG("Subscription expires time <%d> expiry length <%d>\n",
                expires_time, expires);

        LM_DBG("Received a new subscription (expires > 0), checking to see of impurecord for presentity exists\n");
        ul.lock_udomain(domain, &presentity_uri);
        res = ul.get_impurecord(domain, &presentity_uri, &presentity_impurecord);
        if (res != 0) {
            LM_DBG("usrloc does not have imprecord for presnetity being subscribed too, we should create one.... TODO\n");
            ul.unlock_udomain(domain, &presentity_uri);
            ret = CSCF_RETURN_FALSE;
            goto error;
        }

        LM_DBG("Received impurecord for presentity being subscribed to [%.*s]\n", presentity_impurecord->public_identity.len, presentity_impurecord->public_identity.s);

        res = ul.get_subscriber(presentity_impurecord, &presentity_uri, &watcher_contact, event_i, &reg_subscriber);
        if (res != 0) {
            LM_DBG("this must be a new subscriber, lets add it\n");
            res = ul.add_subscriber(presentity_impurecord, &watcher_impu, &watcher_contact, &subscriber_data, &reg_subscriber);
            if (res != 0) {
                LM_ERR("Failed to add new subscription\n");
                ul.unlock_udomain(domain, &presentity_uri);
                ret = CSCF_RETURN_FALSE;
                goto error;
            }
            //send full update on first registration

            new_subscription = 1;

        } else {

            LM_DBG("this must be a re subscribe, lets update it\n");
            res = ul.update_subscriber(presentity_impurecord, &watcher_impu, &watcher_contact, &expires_time, &reg_subscriber);
            if (res != 1) {
                LM_ERR("Failed to update subscription - expires is %d\n", expires_time);
                ul.unlock_udomain(domain, &presentity_uri);
                ret = CSCF_RETURN_FALSE;
                goto error;
            }
            new_subscription = 0;
        }

        ul.unlock_udomain(domain, &presentity_uri);

        ret = CSCF_RETURN_TRUE;
        LM_DBG("Sending 200 OK to subscribing user");
        subscribe_reply(msg, 200, MSG_REG_SUBSCRIBE_OK, &expires, &scscf_name_str);

        //only do reg event on new subscriptions
        if (new_subscription) {
            if (event_reg(domain, 0, 0, event_type, &presentity_uri, &watcher_contact) != 0) {
                LM_ERR("failed to send NOTIFYs for reg events\n");
                ret = CSCF_RETURN_BREAK;
                goto error;
            } else {
                LM_DBG("success sending NOTIFY\n");
            }
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
            ul.external_delete_subscriber(reg_subscriber, (udomain_t*) _t);
        }
        ret = CSCF_RETURN_TRUE;
        LM_DBG("Sending 200 OK to subscribing user");
        subscribe_reply(msg, 200, MSG_REG_UNSUBSCRIBE_OK, &expires, &scscf_name_str);
    }

    //free memory
    if (record_route.s) pkg_free(record_route.s);
    if (presentity_uri.s) shm_free(presentity_uri.s);
    return ret;
error:
    //free memory
    if (record_route.s) pkg_free(record_route.s);
    if (presentity_uri.s) shm_free(presentity_uri.s);

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
            if ((watcher_contact->len == s->watcher_contact.len) && (strncasecmp(s->watcher_contact.s, watcher_contact->s, watcher_contact->len) == 0) &&
                    (presentity_uri->len == s->presentity_uri.len) && (strncasecmp(s->presentity_uri.s, presentity_uri->s, presentity_uri->len) == 0)) {
                LM_DBG("This is a fix to ensure that we only send full reg info XML to the UE that just subscribed");
                LM_DBG("about to make new notification!");
                n = new_notification(subscription_state, content_type, content,
                        s->version++, s);
                if (n) {
                    //LM_DBG("Notification exists - about to add it");
                    //add_notification(n);

                    //Richard just gonna send it - not bother queueing etc.
                    //TODO look at impact of this - sending straight away vs queueing and getting another process to send
                    LM_DBG("About to send notification");
                    send_notification(n);
                    LM_DBG("About to free notification");
                    free_notification(n);
                } else {
                    LM_DBG("Notification does not exist");
                }
            }
        } else {
            LM_DBG("about to make new notification!");
            n = new_notification(subscription_state, content_type, content,
                    s->version++, s);
            if (n) {
                //LM_DBG("Notification exists - about to add it");
                //add_notification(n);

                //Richard just gonna send it - not bother queueing etc.
                //TODO look at impact of this - sending straight away vs queueing and getting another process to send
                LM_DBG("About to send notification");
                send_notification(n);
                LM_DBG("About to free notification");
                free_notification(n);
            } else {
                LM_DBG("Notification does not exist");
            }
        }
        //}
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
str generate_reginfo_full(udomain_t* _t, str* impu_list, int num_impus) {
    str x = {0, 0};
    str buf, pad;
    char bufc[MAX_REGINFO_SIZE], padc[MAX_REGINFO_SIZE];
    impurecord_t *r;
    ucontact_t *c;
    int i, res;

    buf.s = bufc;
    buf.len = 0;
    pad.s = padc;
    pad.len = 0;

    LM_DBG("Getting reginfo_full");

    STR_APPEND(buf, xml_start);
    sprintf(pad.s, r_reginfo_s.s, "%d", r_full.len, r_full.s);
    pad.len = strlen(pad.s);
    STR_APPEND(buf, pad);

    for (i = 0; i < num_impus; i++) {
        ul.lock_udomain(_t, &impu_list[i]);
        LM_DBG("Scrolling through public identities, current one <%.*s>", impu_list[i].len, impu_list[i].s);
        res = ul.get_impurecord(_t, &(impu_list[i]), &r);
        if (res != 0) {
            LM_WARN("impu disappeared, ignoring it\n");
            continue;
        }
        LM_DBG("Retrieved IMPU record");

        if (r->reg_state == IMPU_REGISTERED) {
            LM_DBG("IMPU reg state is IMPU REGISTERED so putting in status active");
            sprintf(pad.s, registration_s.s, r->public_identity.len,
                    r->public_identity.s, r, r_active.len, r_active.s);
        } else {
            LM_DBG("IMPU reg state is not IMPU REGISTERED so putting in status terminated");
            sprintf(pad.s, registration_s.s, r->public_identity.len,
                    r->public_identity.s, r, r_terminated.len,
                    r_terminated.s);
        }
        pad.len = strlen(pad.s);
        STR_APPEND(buf, pad);
        c = r->contacts;
        LM_DBG("Scrolling through contact for this IMPU");
        while (c) {
            if (c->q != -1) {
                LM_DBG("q value not equal to -1");
                float q = (float) c->q / 1000;
                sprintf(pad.s, contact_s_q.s, c, r_active.len, r_active.s,
                        r_registered.len, r_registered.s, c->expires - act_time,
                        q);
            } else {
                LM_DBG("q value equal to -1");
                sprintf(pad.s, contact_s.s, c, r_active.len, r_active.s,
                        r_registered.len, r_registered.s,
                        c->expires - act_time);
            }
            pad.len = strlen(pad.s);
            STR_APPEND(buf, pad);
            STR_APPEND(buf, uri_s);

            LM_DBG("Appending contact address: <%.*s>", c->c.len, c->c.s);

            STR_APPEND(buf, (c->c));
            STR_APPEND(buf, uri_e);

            STR_APPEND(buf, contact_e);
            c = c->next;
        }
        STR_APPEND(buf, registration_e);

        ul.unlock_udomain(_t, &impu_list[i]);
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
    str buf, pad;
    char bufc[MAX_REGINFO_SIZE], padc[MAX_REGINFO_SIZE];
    int expires = -1;

    str state, event;

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
        if (r->contacts == c &&
                //richard we only use expired and unregistered
                (event_type == IMS_REGISTRAR_CONTACT_EXPIRED ||
                event_type == IMS_REGISTRAR_CONTACT_UNREGISTERED)
                )
            sprintf(pad.s, registration_s.s, r->public_identity.len, r->public_identity.s, r, r_terminated.len, r_terminated.s);
        else
            sprintf(pad.s, registration_s.s, r->public_identity.len, r->public_identity.s, r, r_active.len, r_active.s);
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
                sprintf(pad.s, contact_s_q.s, c, r_active.len, r_active.s, r_registered.len, r_registered.s, c->expires - act_time, q);
            } else
                sprintf(pad.s, contact_s.s, c, state.len, state.s, event.len, event.s, expires);
            pad.len = strlen(pad.s);
            STR_APPEND(buf, pad);
            STR_APPEND(buf, uri_s);
            STR_APPEND(buf, (c->c));
            STR_APPEND(buf, uri_e);

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
    LM_DBG("DBG:uac_request_cb: Type %d\n", type);
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
 * The Notification timer looks for unsent notifications and sends them.
 *  - because not all events should wait until the notifications for them are sent
 * @param ticks - the current time
 * @param param - pointer to the domain_list
 */
void notification_timer(unsigned int ticks, void* param) {

    LM_DBG("Running notification timer");

    reg_notification *n = 0;
    LM_DBG("Getting lock of notification list");
    lock_get(notification_list->lock);
    LM_DBG("Scrolling through list");
    while (notification_list->head) {
        n = notification_list->head;
        LM_DBG("Taking notification out of list with watcher uri <%.*s> and presentity uri <%.*s>", n->watcher_uri.len, n->watcher_uri.s, n->presentity_uri.len, n->presentity_uri.s);
        notification_list->head = n->next;
        if (n->next) n->next->prev = 0;
        else notification_list->tail = n->next;

        LM_DBG("Releasing lock");
        lock_release(notification_list->lock);

        LM_DBG("About to send notification");
        send_notification(n);
        LM_DBG("About to free notification");
        free_notification(n);
        LM_DBG("Getting lock of notification list again");
        lock_get(notification_list->lock);
    }
    LM_DBG("Releasing lock again");
    lock_release(notification_list->lock);
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
        str content_type, str content, int version, reg_subscriber * r) {

    reg_notification *n = 0;

    str buf;
    char bufc[MAX_REGINFO_SIZE];

    sprintf(bufc, content.s, version);
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
    lock_release(notification_list->lock);
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
