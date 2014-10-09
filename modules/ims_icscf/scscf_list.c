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

#include "scscf_list.h"
#include "db.h"
#include "../../lib/ims/useful_defs.h"

#if defined (__OS_freebsd)
#include "sys/limits.h"
#define MAXINT INT_MAX
#endif

extern int scscf_entry_expiry; //time for scscf entries to remain the scscf_list

extern struct tm_binds tmb; //Structure with pointers to tm funcs

extern int use_preferred_scscf_uri;
extern str preferred_scscf_uri;

int i_hash_size;
i_hash_slot *i_hash_table = 0;

scscf_capabilities *SCSCF_Capabilities = 0;
int SCSCF_Capabilities_cnt = 0;

/**
 * Refreshes the capabilities list reading them from the db.
 * Drops the old cache and queries the db
 * \todo - IMPLEMENT A WAY TO PUSH AN EXTERNAL EVENT FOR THIS
 * \todo - SOLVE THE LOCKING PROBLEM - THIS IS A WRITER
 * @returns 1 on success, 0 on failure
 */
int I_get_capabilities() {
    int i, j, r;
    /* free the old cache */
    if (SCSCF_Capabilities != 0) {
        for (i = 0; i < SCSCF_Capabilities_cnt; i++) {
            if (SCSCF_Capabilities[i].capabilities)
                shm_free(SCSCF_Capabilities[i].capabilities);
        }
        shm_free(SCSCF_Capabilities);
    }

    SCSCF_Capabilities_cnt = ims_icscf_db_get_scscf(&SCSCF_Capabilities);

    r = ims_icscf_db_get_capabilities(&SCSCF_Capabilities, SCSCF_Capabilities_cnt);

    LM_DBG("DBG:------  S-CSCF Map with Capabilities  begin ------\n");
    if (SCSCF_Capabilities != 0) {
        for (i = 0; i < SCSCF_Capabilities_cnt; i++) {
            LM_DBG("DBG:S-CSCF [%d] <%.*s>\n", SCSCF_Capabilities[i].id_s_cscf, SCSCF_Capabilities[i].scscf_name.len, SCSCF_Capabilities[i].scscf_name.s);
            for (j = 0; j < SCSCF_Capabilities[i].cnt; j++)
                LM_DBG("DBG:       \t [%d]\n", SCSCF_Capabilities[i].capabilities[j]);
        }
    }
    LM_DBG("DBG:------  S-CSCF Map with Capabilities  end ------\n");

    return r;
}

/**
 * Adds the name to the list starting at root, ordered by score.
 * Returns the new root
 */
static inline scscf_entry* I_add_to_scscf_list(scscf_entry *root, str name, int score, int originating) {
    scscf_entry *x, *i;

    //duplicate?
    for (i = root; i; i = i->next)
        if (name.len == i->scscf_name.len &&
                strncasecmp(name.s, i->scscf_name.s, name.len) == 0)
            return root;

    x = new_scscf_entry(name, score, originating);
    if (!x) return root;

    if (!root) {
        return x;
    }
    if (root->score < x->score) {
        x->next = root;
        return x;
    }
    i = root;
    while (i->next && i->next->score > x->score)
        i = i->next;
    x->next = i->next;
    i->next = x;
    return root;
}

/**
 * Initialize the hash with S-CSCF lists
 */
int i_hash_table_init(int hash_size) {
    int i;

    i_hash_size = hash_size;
    i_hash_table = shm_malloc(sizeof (i_hash_slot) * i_hash_size);

    if (!i_hash_table) return 0;

    memset(i_hash_table, 0, sizeof (i_hash_slot) * i_hash_size);

    for (i = 0; i < i_hash_size; i++) {
        i_hash_table[i].lock = lock_alloc();
        if (!i_hash_table[i].lock) {
            LM_ERR("ERR:i_hash_table_init(): Error creating lock\n");
            return 0;
        }
        i_hash_table[i].lock = lock_init(i_hash_table[i].lock);
    }

    return 1;
}

/**
 * Frees memory for scscf list
 */
void free_scscf_list(scscf_list *sl) {
    scscf_entry *i;
    if (!sl) return;
    if (sl->call_id.s) shm_free(sl->call_id.s);
    while (sl->list) {
        i = sl->list->next;
        if (sl->list->scscf_name.s) shm_free(sl->list->scscf_name.s);
        shm_free(sl->list);
        sl->list = i;
    }
    shm_free(sl);
}

/**
 * Returns a list of S-CSCFs that we should try on, based on the
 * capabilities requested
 * \todo - order the list according to matched optionals - r
 * @param scscf_name - the first S-CSCF if specified
 * @param m - mandatory capabilities list
 * @param mcnt - mandatory capabilities list size
 * @param o - optional capabilities list
 * @param ocnt - optional capabilities list size
 * @param orig - indicates originating session case
 * @returns list of S-CSCFs, terminated with a str={0,0}
 */
scscf_entry* I_get_capab_ordered(str scscf_name, int *m, int mcnt, int *o, int ocnt, str *p, int pcnt, int orig) {
    scscf_entry *list = 0;
    int i, r;

    if (scscf_name.len) list = I_add_to_scscf_list(list, scscf_name, MAXINT, orig);

    for (i = 0; i < pcnt; i++)
        list = I_add_to_scscf_list(list, p[i], MAXINT - i, orig);

    for (i = 0; i < SCSCF_Capabilities_cnt; i++) {
        r = I_get_capab_match(SCSCF_Capabilities + i, m, mcnt, o, ocnt);
        if (r != -1) {
            list = I_add_to_scscf_list(list, SCSCF_Capabilities[i].scscf_name, r, orig);
            LM_DBG("DBG:I_get_capab_ordered: <%.*s> Added to the list, orig=%d\n",
                    SCSCF_Capabilities[i].scscf_name.len, SCSCF_Capabilities[i].scscf_name.s, orig);
        }
    }
    return list;
}

/**
 * Creates new scscf entry structure
 */
scscf_entry* new_scscf_entry(str name, int score, int orig) {
    scscf_entry *x = 0;
    x = shm_malloc(sizeof (scscf_entry));
    if (!x) {
        LM_ERR("ERR:new_scscf_entry: Error allocating %lx bytes\n",
                sizeof (scscf_entry));
        return 0;
    }
    /* duplicate always the scscf_name because of possible list reloads and scscf_name coming in LIA/UAA */
    if (orig) x->scscf_name.s = shm_malloc(name.len + 5);
    else x->scscf_name.s = shm_malloc(name.len);
    if (!x->scscf_name.s) {
        LM_ERR("ERR:new_scscf_entry: Error allocating %d bytes\n",
                orig ? name.len + 5 : name.len);
        shm_free(x);
        return 0;
    }
    memcpy(x->scscf_name.s, name.s, name.len);
    x->scscf_name.len = name.len;
    if (orig) {
        memcpy(x->scscf_name.s + name.len, ";orig", 5);
        x->scscf_name.len += 5;
    }

    LM_DBG("INFO:new_scscf_entry:  <%.*s>\n", x->scscf_name.len, x->scscf_name.s);

    x->score = score;

    x->start_time = time(0);

    x->next = 0;
    return x;
}

/**
 * Returns the matching rank of a S-CSCF
 * \todo - optimize the search as O(n^2) is hardly desireable
 * @param c - the capabilities of the S-CSCF
 * @param m - mandatory capabilities list requested
 * @param mcnt - mandatory capabilities list size
 * @param o - optional capabilities list
 * @param ocnt - optional capabilities list sizeint I_get_capab_match(ims_icscf_capabilities *c,int *m,int mcnt,int *o,int ocnt)
 * @returns - -1 if mandatory not satisfied, else count of matched optional capab
 */
int I_get_capab_match(scscf_capabilities *c, int *m, int mcnt, int *o, int ocnt) {
    int r = 0, i, j, t = 0;
    for (i = 0; i < mcnt; i++) {
        t = 0;
        for (j = 0; j < c->cnt; j++)
            if (m[i] == c->capabilities[j]) {
                t = 1;
                break;
            }
        if (!t) return -1;
    }
    for (i = 0; i < ocnt; i++) {
        for (j = 0; j < c->cnt; j++)
            if (o[i] == c->capabilities[j]) r++;
    }
    return r;
}

int add_scscf_list(str call_id, scscf_entry *sl) {
    scscf_list *l;
    unsigned int hash = get_call_id_hash(call_id, i_hash_size);

    l = new_scscf_list(call_id, sl);
    if (!l) return 0;

    i_lock(hash);
    l->prev = 0;
    l->next = i_hash_table[hash].head;
    if (l->next) l->next->prev = l;
    i_hash_table[hash].head = l;
    if (!i_hash_table[hash].tail) i_hash_table[hash].tail = l;
    i_unlock(hash);

    return 1;
}

/**
 * Computes the hash for a string.
 */
inline unsigned int get_call_id_hash(str callid, int hash_size) {
#define h_inc h+=v^(v>>3)
    char* p;
    register unsigned v;
    register unsigned h;

    h = 0;
    for (p = callid.s; p <= (callid.s + callid.len - 4); p += 4) {
        v = (*p << 24)+(p[1] << 16)+(p[2] << 8) + p[3];
        h_inc;
    }
    v = 0;
    for (; p < (callid.s + callid.len); p++) {
        v <<= 8;
        v += *p;
    }
    h_inc;

    h = ((h)+(h >> 11))+((h >> 13)+(h >> 23));
    return (h) % hash_size;
#undef h_inc
}

scscf_list* new_scscf_list(str call_id, scscf_entry *sl) {
    scscf_list *l;

    l = shm_malloc(sizeof (scscf_list));
    if (!l) {
        LM_ERR("ERR:new_scscf_list(): Unable to alloc %lx bytes\n",
                sizeof (scscf_list));
        goto error;
    }
    memset(l, 0, sizeof (scscf_list));

    STR_SHM_DUP(l->call_id, call_id, "shm");
    l->list = sl;

    return l;
error:
    out_of_memory :
    if (l) {
        shm_free(l);
    }
    return 0;
}

/**
 * Locks the required part of hash with S-CSCF lists
 */
inline void i_lock(unsigned int hash) {

    lock_get(i_hash_table[(hash)].lock);

}

/**
 * UnLocks the required part of hash with S-CSCF lists
 */
inline void i_unlock(unsigned int hash) {
    lock_release(i_hash_table[(hash)].lock);

}

static str route_hdr_s = {"Route: <", 8};
static str route_hdr_e = {">\r\n", 3};

int I_scscf_select(struct sip_msg* msg, char* str1, char* str2) {
    str call_id, scscf_name = {0, 0};
    struct sip_msg *req;
    int result;
    str hdr = {0, 0};

    call_id = cscf_get_call_id(msg, 0);
    LM_DBG("I_scscf_select() for call-id <%.*s>\n", call_id.len, call_id.s);
    if (!call_id.len)
        return CSCF_RETURN_FALSE;

    scscf_name = take_scscf_entry(call_id);

    if (!scscf_name.len) {
        LM_DBG("no scscf entry for callid [%.*s]\n", call_id.len, call_id.s);
        return CSCF_RETURN_FALSE;
    }

    if (msg->first_line.u.request.method.len == 8 &&
            strncasecmp(msg->first_line.u.request.method.s, "REGISTER", 8) == 0) {
        /* REGISTER fwding */
        if (str1 && str1[0] == '0') {
            /* first time */
            if (rewrite_uri(msg, &(scscf_name)) < 0) {
                LM_ERR("I_UAR_forward: Unable to Rewrite URI\n");
                result = CSCF_RETURN_FALSE;
            } else
                result = CSCF_RETURN_TRUE;
        } else {
            /* subsequent */
            req = msg;
            append_branch(req, &scscf_name, 0, 0, Q_UNSPECIFIED, 0, 0, 0, 0, 0, 0);
            result = CSCF_RETURN_TRUE;
        }
    } else {
        /* Another request */
        result = CSCF_RETURN_TRUE;

        hdr.len = route_hdr_s.len + scscf_name.len + route_hdr_e.len;
        hdr.s = pkg_malloc(hdr.len);
        if (!hdr.s) {
            LM_ERR("ERR:Mw_REQUEST_forward: Error allocating %d bytes\n",
                    hdr.len);
            result = CSCF_RETURN_TRUE;
        }
        hdr.len = 0;
        STR_APPEND(hdr, route_hdr_s);
        STR_APPEND(hdr, scscf_name);
        STR_APPEND(hdr, route_hdr_e);

        if (!cscf_add_header_first(msg, &hdr, HDR_ROUTE_T)) {
            pkg_free(hdr.s);
            result = CSCF_RETURN_TRUE;
        }

        if (msg->dst_uri.s) pkg_free(msg->dst_uri.s);
        STR_PKG_DUP(msg->dst_uri, scscf_name, "pkg");
    }

    return result;
out_of_memory:
    if (scscf_name.s) shm_free(scscf_name.s);
    return CSCF_RETURN_ERROR;
}

/**
 * Takes on S-CSCF name for the respective Call-ID from the respective name list.
 * Don't free the result.s - it is freed later!
 * @param call_id - the id of the call
 * @returns the shm_malloced S-CSCF name if found or empty string if list is empty or does not exists
 */
str take_scscf_entry(str call_id) {
    str scscf = {0, 0};
    scscf_list *l = 0;
    scscf_entry *scscf_entry = 0;
    unsigned int hash = get_call_id_hash(call_id, i_hash_size);

    LM_DBG("Getting scscf entry from list\n");
    
    i_lock(hash);
    l = i_hash_table[hash].head;
    
    //if use_preferred_scscf_uri then check the table for the preferred scscf set
    if(use_preferred_scscf_uri) {
	LM_DBG("use_preferred_scscf_uri is set so will check for preferred_scscf_uri first [%.*s]\n", preferred_scscf_uri.len, preferred_scscf_uri.s);
	while (l) {
	    if (l->call_id.len == call_id.len &&
		    strncasecmp(l->call_id.s, call_id.s, call_id.len) == 0) {
		scscf_entry = l->list;
		while (scscf_entry) {
		    LM_DBG("scscf_entry [%.*s]\n", scscf_entry->scscf_name.len, scscf_entry->scscf_name.s);
		    if (strncasecmp(scscf_entry->scscf_name.s, preferred_scscf_uri.s, preferred_scscf_uri.len) == 0) {
			LM_DBG("scscf_entry matches\n");
			scscf = scscf_entry->scscf_name;
			break;
		    }
		    scscf_entry = scscf_entry->next;
		}
		
		break;
	    }
	    l = l->next;
	}
    }
    
    // if scscf has not yet been set then find the first scscf that matches
    if(scscf.len <= 0 ) {
	LM_DBG("scscf has not been set so we just look for first match\n");
	while (l) {
	    if (l->call_id.len == call_id.len &&
		    strncasecmp(l->call_id.s, call_id.s, call_id.len) == 0) {
		if (l->list) {
		    LM_DBG("scscf_entry [%.*s]\n", l->list->scscf_name.len, l->list->scscf_name.s);
		    scscf = l->list->scscf_name;
		}
		break;
	    }
	    l = l->next;
	}
    }
    i_unlock(hash);
    return scscf;
}

int I_scscf_drop(struct sip_msg* msg, char* str1, char* str2) {
    str call_id;
    //print_scscf_list(L_DBG);
    call_id = cscf_get_call_id(msg, 0);
    LM_DBG("DBG:I_scscf_drop(): <%.*s>\n", call_id.len, call_id.s);
    if (!call_id.len)
        return CSCF_RETURN_FALSE;

    del_scscf_list(call_id);
    return CSCF_RETURN_TRUE;
}

void del_scscf_list(str call_id) {
    scscf_list *l = 0;
    unsigned int hash = get_call_id_hash(call_id, i_hash_size);

    i_lock(hash);
    l = i_hash_table[hash].head;
    while (l) {
        if (l->call_id.len == call_id.len &&
                strncasecmp(l->call_id.s, call_id.s, call_id.len) == 0) {
            if (l->prev) l->prev->next = l->next;
            else i_hash_table[hash].head = l->next;
            if (l->next) l->next->prev = l->prev;
            else i_hash_table[hash].tail = l->prev;
            i_unlock(hash);
            free_scscf_list(l);
            return;
        }
        l = l->next;
    }
    i_unlock(hash);
}

void print_scscf_list(int log_level) {
    scscf_list *l;
    int i;
    scscf_entry *sl;
    LM_DBG("INF:----------  S-CSCF Lists begin --------------\n");
    for (i = 0; i < i_hash_size; i++) {
        i_lock(i);
        l = i_hash_table[i].head;
        while (l) {
            LM_DBG("INF:[%4d] Call-ID: <%.*s> \n", i,
                    l->call_id.len, l->call_id.s);
            sl = l->list;
            while (sl) {
                LM_DBG("INF:         Score:[%4d] S-CSCF: <%.*s> \n",
                        sl->score,
                        sl->scscf_name.len, sl->scscf_name.s);
                sl = sl->next;
            }
            l = l->next;
        }
        i_unlock(i);
    }
    LM_DBG("INF:----------  S-CSCF Lists end   --------------\n");

}

/**
 * Transactional SIP response - tries to create a transaction if none found.
 * @param msg - message to reply to
 * @param code - the Status-code for the response
 * @param text - the Reason-Phrase for the response
 * @returns the tmb.t_repy() result
 */
int cscf_reply_transactional(struct sip_msg *msg, int code, char *text) {
    unsigned int hash, label;
    if (tmb.t_get_trans_ident(msg, &hash, &label) < 0) {

        LM_DBG("INF:cscf_reply_transactional: Failed to get SIP transaction - creating new one\n");
        if (tmb.t_newtran(msg) < 0)
            LM_DBG("INF:cscf_reply_transactional: Failed creating SIP transaction\n");
    }
    return tmb.t_reply(msg, code, text);
}

int cscf_reply_transactional_async(struct cell* t, struct sip_msg *msg, int code, char *text) {
    return tmb.t_reply_trans(t, msg, code, text);
}

/**
 * Timeout routine called every x seconds and determines if scscf_list entries should be expired
 * @param msg - message to reply to
 * @param code - the Status-code for the response
 * @param text - the Reason-Phrase for the response
 * @returns the tmb.t_repy() result
 */

void ims_icscf_timer_routine() {
    //run through scscf_list and decide if they should be removed!
    scscf_list *l, *tmp;
    int i;
    scscf_entry *sl;

    int delete_list = -1;

    LM_DBG("INF: ims_icscf timer routine");
    //run through all entries and remove the whole list if one entry has expired
    for (i = 0; i < i_hash_size; i++) {
        i_lock(i);
        l = i_hash_table[i].head;
        while (l) {

            LM_DBG("INF:[%4d] Call-ID: <%.*s> \n", i,
                    l->call_id.len, l->call_id.s);

            sl = l->list;
            while (sl) {

                LM_DBG("INF: Score:[%4d] Start_time [%ld] S-CSCF: <%.*s> \n",
                        sl->score,
                        sl->start_time,
                        sl->scscf_name.len, sl->scscf_name.s);
                time_t now = time(0);
                time_t time_elapsed = now - sl->start_time;
                if (time_elapsed > scscf_entry_expiry) {

                    LM_DBG("Scscf entry expired: Time now %ld Start time %ld - elapsed %ld\n", now, sl->start_time, time_elapsed);
                    delete_list = 1; //if any of the entries in this list have expired remove the whole list!

                }
                sl = sl->next;
            }

            if (delete_list == 1) {
                //if any of the entries in this list have expired remove the whole list!
                //remove the list for call_id
                tmp = l->next;
                if (l->prev) l->prev->next = l->next;
                else i_hash_table[i].head = l->next;
                if (l->next) l->next->prev = l->prev;
                else i_hash_table[i].tail = l->prev;
                free_scscf_list(l);
                l = tmp;
                delete_list = -1;
            } else {
                l = l->next;
            }
        }
        i_unlock(i);
    }
}
