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

#include "subscribe.h"
#include "ul_mod.h"
#include "utime.h"
#include "udomain.h"

#include "../presence/subscribe.h"
#include "../presence/utils_func.h"
#include "../presence/hash.h"

#include "../../hashes.h"

#include "ul_mod.h"
#include "usrloc_db.h"


extern int sub_dialog_hash_size;
extern shtable_t sub_dialog_table;

extern int db_mode;

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

reg_subscriber* new_subscriber(subscriber_data_t* subscriber_data) {
    subs_t subs;
    reg_subscriber *s;

    int len;
    char *p;
    unsigned int hash_code = 0;
    
    memset(&subs, 0, sizeof(subs_t));
    
    len = sizeof (reg_subscriber) + subscriber_data->callid->len
            + subscriber_data->ftag->len + subscriber_data->ttag->len
            + subscriber_data->watcher_contact->len + subscriber_data->watcher_uri->len + subscriber_data->presentity_uri->len
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
    
    s->version = subscriber_data->version;

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
    s->watcher_uri.len = subscriber_data->watcher_uri->len;
    memcpy(p, subscriber_data->watcher_uri->s, subscriber_data->watcher_uri->len);
    p += subscriber_data->watcher_uri->len;

    s->watcher_contact.s = p;
    s->watcher_contact.len = subscriber_data->watcher_contact->len;
    memcpy(p, subscriber_data->watcher_contact->s, subscriber_data->watcher_contact->len);
    p += subscriber_data->watcher_contact->len;

    s->record_route.s = p;
    s->record_route.len = subscriber_data->record_route->len;
    memcpy(p, subscriber_data->record_route->s, subscriber_data->record_route->len);
    p += subscriber_data->record_route->len;

    s->sockinfo_str.s = p;
    s->sockinfo_str.len = subscriber_data->sockinfo_str->len;
    memcpy(p, subscriber_data->sockinfo_str->s, subscriber_data->sockinfo_str->len);
    p += subscriber_data->sockinfo_str->len;

    s->presentity_uri.s = p;
    s->presentity_uri.len = subscriber_data->presentity_uri->len;
    memcpy(p, subscriber_data->presentity_uri->s, subscriber_data->presentity_uri->len);
    p += subscriber_data->presentity_uri->len;

    if (p != (((char*) s) + len)) {
        LM_CRIT("buffer overflow\n");
        free_subscriber(s);
        return 0;
    }
    
    /*This lets us get presentity URI info for subsequent SUBSCRIBEs that don't have presentity URI as req URI*/
    get_act_time();
    
    subs.pres_uri = s->presentity_uri;
    subs.from_tag = s->from_tag;
    subs.to_tag = s->to_tag;
    subs.callid = s->call_id;
    subs.expires = s->expires - act_time;
    subs.contact = s->watcher_contact;
    
    hash_code = core_hash(&subs.callid, &subs.to_tag, sub_dialog_hash_size);
    
    LM_DBG("Adding sub dialog hash info with call_id: <%.*s> and ttag <%.*s> amd ftag <%.*s> and hash code <%d>", subs.callid.len, subs.callid.s, subs.to_tag.len, subs.to_tag.s, subs.from_tag.len,  subs.from_tag.s, hash_code);
    
    if (pres_insert_shtable(sub_dialog_table, hash_code, &subs))
    {
	LM_ERR("while adding new subscription\n");
	return 0;
    }

    return s;
}

/* Used for subsequent SUBSCRIBE messages to get presentity URI from dialog struct*/
/* NB: free returned result str when done from shm */
str get_presentity_from_subscriber_dialog(str *callid, str *to_tag, str *from_tag) {
    subs_t* s;
    unsigned int hash_code = 0;
    str pres_uri = {0,0};
    
    hash_code = core_hash(callid, to_tag, sub_dialog_hash_size);
    
    /* search the record in hash table */
    lock_get(&sub_dialog_table[hash_code].lock);

    LM_DBG("Searching sub dialog hash info with call_id: <%.*s> and ttag <%.*s> and ftag <%.*s> and hash code <%d>", callid->len, callid->s, to_tag->len, to_tag->s, from_tag->len, from_tag->s, hash_code);
    
    s= pres_search_shtable(sub_dialog_table, *callid,
		    *to_tag, *from_tag, hash_code);
    if(s== NULL)
    {
	    LM_DBG("Subscriber dialog record not found in hash table\n");
	    lock_release(&sub_dialog_table[hash_code].lock);
	    return pres_uri;
}

    //make copy of pres_uri
    pres_uri.s = (char*) shm_malloc(s->pres_uri.len);
    if (pres_uri.s==0) {
	LM_ERR("no more shm mem\n");
	return pres_uri;
    }
    memcpy(pres_uri.s, s->pres_uri.s, s->pres_uri.len);
    pres_uri.len = s->pres_uri.len;
    
    lock_release(&sub_dialog_table[hash_code].lock);
    
    LM_DBG("Found subscriber dialog record in hash table with pres_uri: [%.*s]", pres_uri.len, pres_uri.s);
    return pres_uri;
}

/*db_load:  if this is a db_load then we don't write to db - as it will be an unecessary rewrite*/
int add_subscriber(impurecord_t* urec,
        subscriber_data_t* subscriber_data, reg_subscriber** _reg_subscriber, int db_load) {

    reg_subscriber *s;
    LM_DBG("Adding reg subscription to IMPU record");

    if (!urec) {
        LM_ERR("no presentity impu record provided\n");
        return 0;
    }
    
    s = new_subscriber(subscriber_data);
    

    if (!s) return -1;

    LM_DBG("Adding new subscription to IMPU record list");
    s->next = 0;
    s->prev = urec->stail;
    if (urec->stail) urec->stail->next = s;
    urec->stail = s;
    if (!urec->shead) urec->shead = s;

    *_reg_subscriber = s;
    
    /*DB?*/
    if(!db_load && db_mode == WRITE_THROUGH) {
	if(db_insert_subscriber(urec, s) != 0) {
	    LM_ERR("Failed to insert subscriber into DB subscriber [%.*s] to IMPU [%.*s]...continuing but db will be out of sync!\n", 
		    s->presentity_uri.len, s->presentity_uri.s, urec->public_identity.len, urec->public_identity.s);
	    goto done;
	}
	
	if(db_link_subscriber_to_impu(urec, s) !=0) {
	    LM_ERR("Failed to update DB linking subscriber [%.*s] to IMPU [%.*s]...continuing but db will be out of sync!\n", 
		    s->presentity_uri.len, s->presentity_uri.s, urec->public_identity.len, urec->public_identity.s);
	}
    }
    
done:
    return 0;
}


int update_subscriber(impurecord_t* urec, reg_subscriber** _reg_subscriber, int *expires, int *local_cseq, int *version) {

    subs_t subs;
    unsigned int hash_code = 0;
    reg_subscriber *rs = *_reg_subscriber;
    if (expires) {
        rs->expires = *expires;
    } else {
        LM_DBG("No expires so will not update subscriber expires.\n");
    }
    if (local_cseq) {
        rs->local_cseq = *local_cseq;
    } else {
        LM_DBG("No local cseq so will not update subscriber local cseq.\n");
    }
    if (version) {
        rs->version = *version;
    } else {
        LM_DBG("No version so will not update subscriber version.\n");
    }
    
    /*This lets us get presentity URI info for subsequent SUBSCRIBEs that don't have presentity URI as req URI*/
    get_act_time();
    
    subs.pres_uri = rs->presentity_uri;
    subs.from_tag = rs->from_tag;
    subs.to_tag = rs->to_tag;
    subs.callid = rs->call_id;
    subs.expires = rs->expires - act_time;
    subs.contact = rs->watcher_contact;
    
    hash_code = core_hash(&subs.callid, &subs.to_tag, sub_dialog_hash_size);
    
    LM_DBG("Updating sub dialog hash info with call_id: <%.*s> and ttag <%.*s> amd ftag <%.*s> and hash code <%d>", subs.callid.len, subs.callid.s, subs.to_tag.len, subs.to_tag.s, subs.from_tag.len,  subs.from_tag.s, hash_code);
    
    if (pres_update_shtable(sub_dialog_table, hash_code, &subs, REMOTE_TYPE))
    {
	LM_ERR("while updating new subscription\n");
	return 0;
    }
        
    /*DB?*/
    if (db_mode == WRITE_THROUGH && db_insert_subscriber(urec, rs) != 0) {
	    LM_ERR("Failed to insert subscriber into DB subscriber [%.*s] to IMPU [%.*s]...continuing but db will be out of sync!\n", 
		    rs->presentity_uri.len, rs->presentity_uri.s, urec->public_identity.len, urec->public_identity.s);
    }

    return 1;
}


void external_delete_subscriber(reg_subscriber *s, udomain_t* _t, int lock_domain) {
    LM_DBG("Deleting subscriber");
    impurecord_t* urec;
   
    LM_DBG("Updating reg subscription in IMPU record");

    if(lock_domain) lock_udomain(_t, &s->presentity_uri);
    int res = get_impurecord(_t, &s->presentity_uri, &urec);
    if (res != 0) {
        if(lock_domain) unlock_udomain(_t, &s->presentity_uri);
        return;
    }

    delete_subscriber(urec, s);

    if(lock_domain) unlock_udomain(_t, &s->presentity_uri);

}

void delete_subscriber(impurecord_t* urec, reg_subscriber *s) {
    LM_DBG("Deleting subscriber [%.*s] from IMPU: [%.*s]", s->watcher_uri.len, s->watcher_uri.s, urec->public_identity.len, urec->public_identity.s);
    
    if (db_mode == WRITE_THROUGH && db_unlink_subscriber_from_impu(urec, s) !=0) {
	    LM_ERR("Failed to delete DB linking subscriber [%.*s] to IMPU [%.*s]...continuing but db will be out of sync!\n", 
		    s->presentity_uri.len, s->presentity_uri.s, urec->public_identity.len, urec->public_identity.s);
	    
    }    
    if (db_mode == WRITE_THROUGH && db_delete_subscriber(urec, s) != 0) {
	    LM_ERR("error removing subscriber from DB [%.*s]... will still remove from memory\n", s->presentity_uri.len, s->presentity_uri.s);
    }
    
    if (urec->shead == s) urec->shead = s->next;
    else s->prev->next = s->next;
    if (urec->stail == s) urec->stail = s->prev;
    else s->next->prev = s->prev;
    LM_DBG("About to free subscriber memory");
    free_subscriber(s);
}

void free_subscriber(reg_subscriber *s) {
    
    unsigned int hash_code=0;
    subs_t subs;
    
    LM_DBG("Freeing subscriber memory");
    
    memset(&subs, 0, sizeof(subs_t));
    
    subs.pres_uri = s->presentity_uri;
    subs.from_tag = s->from_tag;
    subs.to_tag = s->to_tag;
    subs.callid = s->call_id;
    
    /* delete from cache table */
    hash_code= core_hash(&s->call_id, &s->to_tag, sub_dialog_hash_size);
    
    LM_DBG("Removing sub dialog hash info with call_id: <%.*s> and ttag <%.*s> and ftag <%.*s> and hash code <%d>", s->call_id.len, s->call_id.s, s->to_tag.len, s->to_tag.s, s->from_tag.len, s->from_tag.s, hash_code);
    if(pres_delete_shtable(sub_dialog_table,hash_code, &subs)< 0)
    {
	    LM_ERR("record not found in hash table\n");
    }
    
    if (s) {
        shm_free(s);
    }


}

int valid_subscriber(reg_subscriber *s, time_t time) {
    return (s->expires > time);
}
