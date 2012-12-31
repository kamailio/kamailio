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

#include "subscribe.h"
#include "ul_mod.h"
#include "utime.h"
#include "udomain.h"



int get_subscriber(impurecord_t* urec, str *presentity_uri, str *watcher_contact, int event, reg_subscriber** r_subscriber) {
    
    reg_subscriber* s = NULL;

    if (!watcher_contact || !presentity_uri) {
        LM_DBG("no valid presentity_uri/watcher contact pair");
        return 0;
    }

    if (!urec) {
        LM_WARN("No impurecord passed.... ignoring");
        return 1;
    }

    LM_DBG("Getting existing subscription to reg if it exists for watcher contact <%.*s> and presentity uri <%.*s>", watcher_contact->len, watcher_contact->s,
            presentity_uri->len, presentity_uri->s);

    s = urec->shead;
    while (s) {
        LM_DBG("Scrolling through subscription to reg events in IMPU record list");
        if (s->event == event &&
                (s->watcher_contact.len == watcher_contact->len)
                && (strncasecmp(s->watcher_contact.s, watcher_contact->s, watcher_contact->len) == 0)
                && (strncasecmp(s->presentity_uri.s, presentity_uri->s, presentity_uri->len) == 0)) {
            LM_DBG("Found subscription for watcher contact  <%.*s> and presentity_uri <%.*s>", watcher_contact->len, watcher_contact->s, presentity_uri->len, presentity_uri->s);
            *r_subscriber = s;
            return 0;
        }
        s = s->next;
    }
    LM_DBG("Did not find subscription for watcher contact  <%.*s> and presentity_uri <%.*s>", watcher_contact->len, watcher_contact->s, presentity_uri->len, presentity_uri->s);
    
    return 1;
}

reg_subscriber* new_subscriber(str* presentity_uri, str* watcher_uri, str* watcher_contact, subscriber_data_t* subscriber_data) {
    reg_subscriber *s;

    int len;
    char *p;

    len = sizeof (reg_subscriber) + subscriber_data->callid->len
            + subscriber_data->ftag->len + subscriber_data->ttag->len
            + watcher_contact->len + watcher_uri->len + presentity_uri->len
            + subscriber_data->record_route->len + subscriber_data->sockinfo_str->len;

    LM_DBG("Creating new subscription to reg");

    s = (reg_subscriber*) shm_malloc(len);
    if (s == 0) {
        LM_ERR("no more shm mem (%d)\n", len);
        return 0;
    }
    memset(s, 0, len);

    s->local_cseq = subscriber_data->local_cseq;

    s->event = subscriber_data->event;

    s->expires = subscriber_data->expires;

    p = (char*) (s + 1);

    s->call_id.s = p;
    s->call_id.len = subscriber_data->callid->len;
    memcpy(p, subscriber_data->callid->s, subscriber_data->callid->len);
    p += subscriber_data->callid->len;

    s->to_tag.s = p;
    s->to_tag.len = subscriber_data->ttag->len;
    memcpy(p, subscriber_data->ttag->s, subscriber_data->ttag->len);
    p += subscriber_data->ttag->len;

    s->from_tag.s = p;
    s->from_tag.len = subscriber_data->ftag->len;
    memcpy(p, subscriber_data->ftag->s, subscriber_data->ftag->len);
    p += subscriber_data->ftag->len;

    s->watcher_uri.s = p;
    s->watcher_uri.len = watcher_uri->len;
    memcpy(p, watcher_uri->s, watcher_uri->len);
    p += watcher_uri->len;

    s->watcher_contact.s = p;
    s->watcher_contact.len = watcher_contact->len;
    memcpy(p, watcher_contact->s, watcher_contact->len);
    p += watcher_contact->len;

    s->record_route.s = p;
    s->record_route.len = subscriber_data->record_route->len;
    memcpy(p, subscriber_data->record_route->s, subscriber_data->record_route->len);
    p += subscriber_data->record_route->len;

    s->sockinfo_str.s = p;
    s->sockinfo_str.len = subscriber_data->sockinfo_str->len;
    memcpy(p, subscriber_data->sockinfo_str->s, subscriber_data->sockinfo_str->len);
    p += subscriber_data->sockinfo_str->len;

    s->presentity_uri.s = p;
    s->presentity_uri.len = presentity_uri->len;
    memcpy(p, presentity_uri->s, presentity_uri->len);
    p += presentity_uri->len;

    if (p != (((char*) s) + len)) {
        LM_CRIT("buffer overflow\n");
        free_subscriber(s);
        return 0;
    }

    return s;
}

int add_subscriber(impurecord_t* urec,
        str *watcher_uri, str *watcher_contact,
        subscriber_data_t* subscriber_data, reg_subscriber** _reg_subscriber) {

    LM_DBG("Adding reg subscription to IMPU record");

    if (!urec) {
        LM_ERR("no presentity impu record provided\n");
        return 0;
    }
    reg_subscriber *s = new_subscriber(&urec->public_identity, watcher_uri, watcher_contact, subscriber_data);

    if (!s) return 1;

    LM_DBG("Adding new subscription to IMPU record list");
    s->next = 0;
    s->prev = urec->stail;
    if (urec->stail) urec->stail->next = s;
    urec->stail = s;
    if (!urec->shead) urec->shead = s;

    *_reg_subscriber = s;
    return 0;
}


int update_subscriber(impurecord_t* urec,
        str *watcher_uri, str *watcher_contact,
        int *expires, reg_subscriber** _reg_subscriber) {


    reg_subscriber *rs = *_reg_subscriber;
    if (expires) {
        rs->expires = *expires;
        return 1;
    } else {
        LM_ERR("Failed to update subscriber as expires is expires is null");
        return 0;
    }
}

void external_delete_subscriber(reg_subscriber *s, udomain_t* _t) {
    LM_DBG("Deleting subscriber");
    impurecord_t* urec;

    LM_DBG("Updating reg subscription in IMPU record");

    lock_udomain(_t, &s->presentity_uri);
    int res = get_impurecord(_t, &s->presentity_uri, &urec);
    if (res != 0) {
        unlock_udomain(_t, &s->presentity_uri);
        return;
    }

    if (urec->shead == s) urec->shead = s->next;
    else s->prev->next = s->next;
    if (urec->stail == s) urec->stail = s->prev;
    else s->next->prev = s->prev;
    LM_DBG("About to free subscriber memory");
    free_subscriber(s);

    unlock_udomain(_t, &s->presentity_uri);

}

void delete_subscriber(impurecord_t* urec, reg_subscriber *s) {
    LM_DBG("Deleting subscriber");
    if (urec->shead == s) urec->shead = s->next;
    else s->prev->next = s->next;
    if (urec->stail == s) urec->stail = s->prev;
    else s->next->prev = s->prev;
    LM_DBG("About to free subscriber memory");
    free_subscriber(s);
}

void free_subscriber(reg_subscriber *s) {
    LM_DBG("Freeing subscriber memory");
    if (s) {
        shm_free(s);
    }


}

int valid_subscriber(reg_subscriber *s) {
    return (s->expires > act_time);
}
