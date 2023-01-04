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
 *

 *! \file
 *  \brief USRLOC - Userloc domain handling functions
 *  \ingroup usrloc
 *
 * - Module: \ref usrloc
 */

#include "udomain.h"
#include <string.h>
#include "../../core/hashes.h"
#include "../../core/parser/parse_methods.h"
#include "../../core/mem/shm_mem.h"
#include "../../core/dprint.h"
#include "../../lib/srdb1/db.h"
#include "../../core/socket_info.h"
#include "../../core/ut.h"
#include "../../core/counters.h"
#include "ims_usrloc_scscf_mod.h"            /* usrloc module parameters */
#include "usrloc.h"
#include "utime.h"
#include "usrloc.h"
#include "bin_utils.h"
#include "usrloc_db.h"
#include "contact_hslot.h"
#include "ul_scscf_stats.h"
#include "hslot_sp.h"
#include "dlist.h"

extern int unreg_validity;
extern int db_mode;
extern struct contact_list* contact_list;
extern struct ims_subscription_list* ims_subscription_list;
extern int subs_hash_size;

extern int contact_delete_delay;

extern char* cscf_realm;
extern int skip_cscf_realm;

/*!
 * \brief Create a new domain structure
 * \param  _n is pointer to str representing name of the domain, the string is
 * not copied, it should point to str structure stored in domain list
 * \param _s is hash table size
 * \param _d new created domain
 * \return 0 on success, -1 on failure
 */
int new_udomain(str* _n, int _s, udomain_t** _d) {
    int i;

    /* Must be always in shared memory, since
     * the cache is accessed from timer which
     * lives in a separate process
     */
    *_d = (udomain_t*) shm_malloc(sizeof (udomain_t));
    if (!(*_d)) {
        LM_ERR("new_udomain(): No memory left\n");
        goto error0;
    }
    memset(*_d, 0, sizeof (udomain_t));

    (*_d)->table = (hslot_t*) shm_malloc(sizeof (hslot_t) * _s);
    if (!(*_d)->table) {
        LM_ERR("no memory left 2\n");
        goto error1;
    }

    (*_d)->name = _n;

    for (i = 0; i < _s; i++) {
        init_slot(*_d, &((*_d)->table[i]), i);
    }

    (*_d)->size = _s;

    return 0;

error1:
    shm_free(*_d);
error0:
    return -1;
}

/*!
 * \brief Free all memory allocated for the domain
 * \param _d freed domain
 */
void free_udomain(udomain_t* _d) {
    int i;

    if (_d->table) {
        for (i = 0; i < _d->size; i++) {
            lock_ulslot(_d, i);
            deinit_slot(_d->table + i);
            unlock_ulslot(_d, i);
        }
        shm_free(_d->table);
    }
    shm_free(_d);
}

/*!
 * \brief Returns a static dummy impurecord for temporary usage
 * \param _d domain (needed for the name)
 * \param _aor address of record
 * \param _r new created urecord
 */
static inline void get_static_impurecord(udomain_t* _d, str* _aor, struct impurecord** _r) {
    static struct impurecord r;

    memset(&r, 0, sizeof (struct impurecord));
    r.public_identity = *_aor;
    r.domain = _d->name;
    *_r = &r;
}

/*!
 * \brief Debugging helper function
 */
void print_udomain(FILE* _f, udomain_t* _d) {
    int i;
    int max = 0, slot = 0, n = 0;
    struct impurecord* r;
    fprintf(_f, "---Domain---\n");
    fprintf(_f, "name : '%.*s'\n", _d->name->len, ZSW(_d->name->s));
    fprintf(_f, "size : %d\n", _d->size);
    fprintf(_f, "table: %p\n", _d->table);
    fprintf(_f, "\n");
    for (i = 0; i < _d->size; i++) {
        r = _d->table[i].first;
        n += _d->table[i].n;
        if (max < _d->table[i].n) {
            max = _d->table[i].n;
            slot = i;
        }
        while (r) {
            print_impurecord(_f, r);
            r = r->next;
        }
    }
    fprintf(_f, "\nMax slot: %d (%d/%d)\n", max, slot, n);
    fprintf(_f, "\n---/Domain---\n");
}

inline int time2str(time_t _v, char* _s, int* _l) {
    struct tm* t;
    int l;

    if ((!_s) || (!_l) || (*_l < 2)) {
        LM_ERR("Invalid parameter value\n");
        return -1;
    }

    *_s++ = '\'';

    /* Convert time_t structure to format accepted by the database */
    t = localtime(&_v);
    l = strftime(_s, *_l - 1, "%Y-%m-%d %H:%M:%S", t);

    if (l == 0) {
        LM_ERR("Error during time conversion\n");
        /* the value of _s is now unspecified */
        _s = NULL;
        _l = 0;
        return -1;
    }
    *_l = l;

    *(_s + l) = '\'';
    *_l = l + 2;
    return 0;
}

/*!
 * \brief Insert a new record into domain in memory
 * \param _d domain the record belongs to
 * \param _aor address of record
 * \param _r new created record
 * \return 0 on success, -1 on failure
 */
int mem_insert_impurecord(struct udomain* _d, str* public_identity, str* private_identity, int reg_state, int barring,
        ims_subscription** s, str* ccf1, str* ccf2, str* ecf1, str* ecf2,
        struct impurecord** _r) {
    int sl;

    if (new_impurecord(_d->name, public_identity, private_identity, reg_state, barring, s, ccf1, ccf2, ecf1,
            ecf2, _r) < 0) {
        LM_ERR("creating impurecord failed\n");
        return -1;
    }

    sl = ((*_r)->aorhash) & (_d->size - 1);
    slot_add(&_d->table[sl], *_r);
    counter_inc(ul_scscf_cnts_h.active_impus);

    LM_DBG("inserted new impurecord into memory [%.*s]\n", (*_r)->public_identity.len, (*_r)->public_identity.s);
    return 0;
}

/*!
 * \brief Remove a record from domain in memory
 * \param _d domain the record belongs to
 * \param _r deleted record
 */
void mem_delete_impurecord(udomain_t* _d, struct impurecord* _r) {
    LM_DBG("deleting impurecord from memory [%.*s]\n", _r->public_identity.len, _r->public_identity.s);
    slot_rem(_r->slot, _r);
    free_impurecord(_r);
    counter_add(ul_scscf_cnts_h.active_impus, -1);
}

static unsigned int expired_contacts_size = 0;
ucontact_t** expired_contacts = 0;


/*!
 * \brief Run timer handler for given domain
 * \param _d domain
 */
void mem_timer_udomain(udomain_t* _d, int istart, int istep) {
    struct impurecord* ptr, *t;
    struct ucontact* contact_ptr;
    unsigned int num_expired_contacts = 0;
    int i, n, temp;
    time_t now;
    int abort = 0;
    int slot;
	int ref_count_db;
    
    now = time(0);
    
    if (istart == 0) {
        int numcontacts = contact_list->size*2;     //assume we should be ok for each slot to have 2 collisions
        if (expired_contacts_size < numcontacts) {
            LM_DBG("Changing expired_contacts list size from %d to %d\n", expired_contacts_size, numcontacts);
            if (expired_contacts){
                pkg_free(expired_contacts);
            }
            expired_contacts = (ucontact_t**)pkg_malloc(numcontacts*sizeof(ucontact_t**));
            if (!expired_contacts) {
                LM_ERR("no more pkg mem trying to allocate [%lu] bytes\n", numcontacts*sizeof(ucontact_t**));
                return;
            }
            expired_contacts_size = numcontacts;
        }

        //go through contacts first
        n = contact_list->max_collisions;
        LM_DBG("*** mem_timer_udomain - checking contacts - START ***\n");
        for (i=0; i < contact_list->size; i++) {
            lock_contact_slot_i(i);
            contact_ptr = contact_list->slot[i].first;
            while (contact_ptr) {
                if (num_expired_contacts >= numcontacts) {
                    LM_WARN("we don't have enough space to expire all contacts in this pass - will continue in next pass\n");
                    abort = 1;
                    break;
                }
                LM_DBG("We have a [3gpp=%d] contact in the new contact list in slot %d = [%.*s] (%.*s) which expires in %lf seconds and has a ref count of %d (state: %s)\n", 
                        contact_ptr->is_3gpp, i, contact_ptr->aor.len, contact_ptr->aor.s, contact_ptr->c.len, contact_ptr->c.s, 
                        (double) contact_ptr->expires - now, contact_ptr->ref_count,
                        get_contact_state_as_string(contact_ptr->state));
                    //contacts are now deleted during impurecord processing
                if ((contact_ptr->expires-now) <= 0) {
                    if (contact_ptr->state == CONTACT_DELAYED_DELETE) {
                        if (contact_ptr->ref_count <= 0) {
                            LM_DBG("contact in state CONTACT_DELATED_DELETE is about to be deleted\n");
                            expired_contacts[num_expired_contacts] = contact_ptr;
                            num_expired_contacts++;
                        } else {
							/* we could fall here not because contact is still
							 referenced but also because we failed before to
							 get a lock to unref the contact, so we check if
							 contact is really referenced*/
							if (db_mode != NO_DB) {
								LM_DBG("contact in state CONTACT_DELAYED_DELETE still has a ref count of [%d] in memory. Check on DB \n", contact_ptr->ref_count);
								ref_count_db = db_check_if_contact_is_linked(contact_ptr);
								if (ref_count_db < 0) {
									LM_ERR("Unable to check if contact is unlinked\n");
								} else if (ref_count_db == 0) {
									LM_DBG("Contact has ref count [%d] but there's no link on the DB. Deleting contact\n", contact_ptr->ref_count);
									contact_ptr->ref_count = 0;
									expired_contacts[num_expired_contacts] = contact_ptr;
									num_expired_contacts++;
								} else {
									LM_DBG("Contact in state CONTACT_DELAYED_DELETE has ref count [%d] on DB\n", ref_count_db);
								}
							} else {
								LM_DBG("contact in state CONTACT_DELAYED_DELETE still has a ref count of [%d] in memory. Not doing anything for now \n", contact_ptr->ref_count);
							}
                        }
                    } else if (contact_ptr->state == CONTACT_EXPIRE_PENDING_NOTIFY) {
						LM_DBG("expired pending notify contact [%.*s](%.*s).... setting to CONTACT_NOTIFY_READY\n",
								contact_ptr->aor.len, contact_ptr->aor.s, contact_ptr->c.len, contact_ptr->c.s);
						contact_ptr->state = CONTACT_NOTIFY_READY;
						expired_contacts[num_expired_contacts] = contact_ptr;
						num_expired_contacts++;
					} else if (contact_ptr->state == CONTACT_NOTIFY_READY) {
						LM_DBG("expired notify ready contact [%.*s](%.*s).... marking for deletion\n",
								contact_ptr->aor.len, contact_ptr->aor.s, contact_ptr->c.len, contact_ptr->c.s);
						expired_contacts[num_expired_contacts] = contact_ptr;
						num_expired_contacts++;
					} else if (contact_ptr->state != CONTACT_DELETED) {
						LM_DBG("expiring contact [%.*s](%.*s).... setting to CONTACT_EXPIRE_PENDING_NOTIFY\n",
								contact_ptr->aor.len, contact_ptr->aor.s, contact_ptr->c.len, contact_ptr->c.s);
                        contact_ptr->state = CONTACT_EXPIRE_PENDING_NOTIFY;
                        ref_contact_unsafe(contact_ptr);
                        expired_contacts[num_expired_contacts] = contact_ptr;
                        num_expired_contacts++;
                    }
                }
                contact_ptr = contact_ptr->next;
            } 
            if (contact_list->slot[i].n > n) {
                n = contact_list->slot[i].n;
            }
            unlock_contact_slot_i(i);
            contact_list->max_collisions = n;
            if (abort == 1) {
                break;
            }
        }
        LM_DBG("*** mem_timer_udomain - checking contacts - FINISHED ***\n");
    }
       
    temp = 0;
    n = _d->max_collisions;

    LM_DBG("*** mem_timer_udomain - checking IMPUs - START ***\n");
    for (i = istart; i < _d->size; i+=istep) {
        lock_ulslot(_d, i);
        ptr = _d->table[i].first;
        temp = 0;
        while (ptr) {
            temp = 1;
#ifdef EXTRA_DEBUG
            LM_DBG("ULSLOT %d LOCKED\n", i);
#endif
            t = ptr;
            ptr = ptr->next;
            timer_impurecord(t);
        }
        if (temp) {
#ifdef EXTRA_DEBUG
            LM_DBG("ULSLOT %d UN-LOCKED\n", i);
#endif
        }
        if (_d->table[i].n > n)
            n = _d->table[i].n;
        
        unlock_ulslot(_d, i);
        _d->max_collisions = n;
    }
    LM_DBG("*** mem_timer_udomain - checking IMPUs - FINISHED ***\n");
    
    if (istart == 0) {
        n = ims_subscription_list->max_collisions;
        for (i = 0; i < ims_subscription_list->size; i++) {
            lock_subscription_slot(i);
            if (ims_subscription_list->slot[i].n > n) {
                n = ims_subscription_list->slot[i].n;
            }
            unlock_subscription_slot(i);
        }
        ims_subscription_list->max_collisions = n;

        /* now we delete the expired contacts.  (mark them for deletion */
        for (i=0; i<num_expired_contacts; i++) {
            slot = expired_contacts[i]->sl;
            lock_contact_slot_i(slot);
			if(expired_contacts[i]->state == CONTACT_EXPIRE_PENDING_NOTIFY) {
				LM_DBG("Contact state CONTACT_EXPIRE_PENDING_NOTIFY for contact [%.*s](%.*s)\n",
						expired_contacts[i]->aor.len, expired_contacts[i]->aor.s, expired_contacts[i]->c.len, expired_contacts[i]->c.s);
			} else {
				if (expired_contacts[i]->state != CONTACT_DELAYED_DELETE) {
					LM_DBG("Setting contact state from '%s' to CONTACT_DELETED for contact [%.*s](%.*s)\n",
							get_contact_state_as_string(expired_contacts[i]->state),
							expired_contacts[i]->aor.len, expired_contacts[i]->aor.s, expired_contacts[i]->c.len, expired_contacts[i]->c.s);
					expired_contacts[i]->state = CONTACT_DELETED;
					unref_contact_unsafe(expired_contacts[i]);
				} else {
					LM_DBG("deleting contact [%.*s](%.*s)\n",
							expired_contacts[i]->aor.len, expired_contacts[i]->aor.s, expired_contacts[i]->c.len, expired_contacts[i]->c.s);
					delete_scontact(expired_contacts[i]);
				}
			}
            unlock_contact_slot_i(slot);
        }
    }
    
}


/*!
 * \brief Get lock for a domain
 * \param _d domain
 * \param _aor adress of record, used as hash source for the lock slot
 */
void lock_udomain(udomain_t* _d, str* _aor) {
    unsigned int sl;
    sl = core_hash(_aor, 0, _d->size);
    lock_ulslot(_d, sl);
}

/*!
 * \brief Release lock for a domain
 * \param _d domain
 * \param _aor address of record, uses as hash source for the lock slot
 */
void unlock_udomain(udomain_t* _d, str* _aor) {
    unsigned int sl;
    sl = core_hash(_aor, 0, _d->size);
    unlock_ulslot(_d, sl);
}

/*!
 * \brief  Get lock for a slot
 * \param _d domain
 * \param i slot number
 */
void lock_ulslot(udomain_t* _d, int i) {
#ifdef EXTRA_DEBUG
    LM_DBG("LOCKING UDOMAIN SLOT [%d]\n", i);
#endif
    int mypid;
    mypid = my_pid();
    if (likely(atomic_get(&_d->table[i].locker_pid) != mypid)) {
        lock_get(_d->table[i].lock);
        atomic_set(&_d->table[i].locker_pid, mypid);
    } else {
        /* locked within the same process that executed us */
        _d->table[i].recursive_lock_level++;
    }
}

/*!
 * \brief Release lock for a slot
 * \param _d domain
 * \param i slot number
 */
void unlock_ulslot(udomain_t* _d, int i) {
#ifdef EXTRA_DEBUG
    LM_DBG("UN-LOCKING UDOMAIN SLOT [%d]\n", i);
#endif
    if (likely(_d->table[i].recursive_lock_level == 0)) {
        atomic_set(&_d->table[i].locker_pid, 0);
        lock_release(_d->table[i].lock);
    } else {
        /* recursive locked => decrease lock count */
        _d->table[i].recursive_lock_level--;
    }
}

void lock_contact_slot(str* contact_uri) {
    unsigned int sl;
    sl = core_hash(contact_uri, 0, contact_list->size);
    lock_contact_slot_i(sl);
}

void unlock_contact_slot(str* contact_uri) {
    unsigned int sl;
    sl = core_hash(contact_uri, 0, contact_list->size);

    unlock_contact_slot_i(sl);
}

void lock_contact_slot_i(int i) {
#ifdef EXTRA_DEBUG
    LM_DBG("LOCKING CONTACT SLOT [%d]\n", i);
#endif
#ifdef GEN_LOCK_T_PREFERED
    lock_get(contact_list->slot[i].lock);
#else
    lock_contacts_idx(contact_list->slot[i].lockidx);
#endif
}

void unlock_contact_slot_i(int i) {
#ifdef EXTRA_DEBUG
    LM_DBG("UN-LOCKING CONTACT SLOT [%d]\n", i);
#endif
#ifdef GEN_LOCK_T_PREFERED
    lock_release(contact_list->slot[i].lock);
#else
    release_contact_idx(contact_list->slot[i].lockidx);
#endif
}

void lock_subscription(ims_subscription* s) {
#ifdef EXTRA_DEBUG
    LM_DBG("LOCKING SUBSCRIPTION %p (Refcount: %d)\n", s->lock, s->ref_count);
    LM_DBG("(SUBSCRIPTION PRIVATE IDENTITY [%.*s])\n", s->private_identity.len, s->private_identity.s);
#endif
    lock_get(s->lock);
}

void unlock_subscription(ims_subscription* s) {
    if (s == 0)
        return;
#ifdef EXTRA_DEBUG
    LM_DBG("UN-LOCKING SUBSCRIPTION %p (Refcount: %d)\n", s->lock, s->ref_count);
    LM_DBG("(SUBSCRIPTION PRIVATE IDENTITY [%.*s])\n", s->private_identity.len, s->private_identity.s);
#endif
    lock_release(s->lock);
}

void lock_subscription_slot(int i) {
#ifdef EXTRA_DEBUG
    LM_DBG("LOCKING SUBSCRIPTION slot %d)\n", i);
#endif
    lock_get(ims_subscription_list->slot[i].lock);
}

void unlock_subscription_slot(int i) {
#ifdef EXTRA_DEBUG
    LM_DBG("UN-LOCKING SUBSCRIPTION slot %d\n", i);
#endif
    lock_release(ims_subscription_list->slot[i].lock);
}

/*!
 * \brief Create and insert a new impurecord assumes domain is locked
 * @param _d domain to insert the new record
 * @param public_identity IMPU of new record
 * @param private_identity IMPI of new record
 * @param reg_state state to insert in
 * @param barring is impu barred or not
 * @param s associated subscription data
 * @param ccf1  
 * @param ccf2
 * @param ecf1
 * @param ecf2
 * @param _r pointer to returned IMPU record
 * @return 0 on success with _r populated
 */
int insert_impurecord(struct udomain* _d, str* public_identity, str* private_identity, int reg_state, int barring,
        ims_subscription** s, str* ccf1, str* ccf2, str* ecf1, str* ecf2,
        struct impurecord** _r) {

    if (s == 0 || (*s) == 0) {
        LM_WARN("Can't insert an impurecord without it being associated to a subscription\n");
        goto error;
    }

    if (!private_identity || !private_identity->len || !private_identity->s) {
        LM_WARN("Can't insert an impurecord without it being associated to a subscription (private_identity\n");
        goto error;
    }

    /* check to see if we already have this subscription information in memory*/
    if (mem_insert_impurecord(_d, public_identity, private_identity, reg_state, barring, s, ccf1, ccf2, ecf1, ecf2, _r) < 0) {
        LM_ERR("inserting record failed\n");
        goto error;
    }

    /*DB?*/
    if (db_mode == WRITE_THROUGH && db_insert_impurecord(_d, public_identity, reg_state, barring, s, ccf1, ccf2, ecf1, ecf2, _r) != 0) {
        LM_ERR("error inserting contact into db\n");
        goto error;
    }

    return 0;

error:
    return -1;
}

/*!
 * \brief Obtain an impurecord pointer if the impurecord exists in domain. You should call this function with a lock on the domain
 * \param _d domain to search the record
 * \param _aor address of record
 * \param _r new created record
 * \return 0 if a record was found, 1 if nothing could be found (assumes caller has lock on domain)
 */
int get_impurecord_unsafe(udomain_t* _d, str* public_identity, struct impurecord** _r) {
    unsigned int sl, i, aorhash;
    impurecord_t* r;

    /* search in cache */
    aorhash = core_hash(public_identity, 0, 0);
    sl = aorhash & (_d->size - 1);
    r = _d->table[sl].first;

    for (i = 0; i < _d->table[sl].n; i++) {
        if ((r->aorhash == aorhash) && (r->public_identity.len == public_identity->len)
                && !memcmp(r->public_identity.s, public_identity->s, public_identity->len)) {
            *_r = r;
            return 0;
        }

        r = r->next;
    }
    return 1; /* Nothing found */
}

/*!
 * \brief Obtain an impurecord pointer if the impurecord exists in domain. domain must be locked before calling
 * \param _d domain to search the record
 * \param public_identity address of record
 * \param _r returned record - null if not found
 * \return 0 if a record was found, 1 if nothing could be found
 */
int get_impurecord(udomain_t* _d, str* public_identity, struct impurecord** _r) {
    unsigned int ret;

    ret = get_impurecord_unsafe(_d, public_identity, _r);

    return ret;
}

/*!
 * \brief release the lock on the impurecord - effectively the domain slot
 * \param _d domain
 * \param _r impurecord to release (unlock)
 */
void release_impurecord(udomain_t* _d, struct impurecord* _r) {
    unlock_udomain(_d, &_r->public_identity);
}

/*!
 * \brief Delete an impurecord from domain
 * \param _d domain where the record should be deleted
 * \param _aor address of record - used only if _r in next param is null
 * \param _r deleted record to delete - if null will use the aor to search (assumed that domain is locked).
 * \return 0 on success, -1 if the record could not be deleted
 */
int delete_impurecord(udomain_t* _d, str* _aor, struct impurecord* _r) {
    LM_DBG("Deleting IMPURECORD [%.*s]\n", _r->public_identity.len, _r->public_identity.s);

    if (_r == 0) {
        LM_DBG("no impurecord passed in - let's search\n");
        if (get_impurecord(_d, _aor, &_r) != 0) {
            return 0;
        }
    }

    if (exists_ulcb_type(_r->cbs, UL_IMPU_DELETE)) {
        run_ul_callbacks(_r->cbs, UL_IMPU_DELETE, _r, 0);
    }

    /*DB?*/
    if (db_mode == WRITE_THROUGH
            && db_delete_impurecord(_d, _r) != 0) {
        LM_ERR("error deleting IMPU record from db...continuing to remove from memory\n");
    }

    mem_delete_impurecord(_d, _r);

    return 0;
}

/*
 * get all IMPUs as string from a subscription related to an impurecord. apply filter for barring (assumed to be called with lock on impurec)
 * you should have some for of lock on the subscription (ie a reference)
 * barring-1 get all barred
 * barring-0 get all unbarred
 * barring-(-1) get all records
 * NB. Remember to free the block of memory pointed to by impus (pkg_malloc)
 */
int get_impus_from_subscription_as_string(udomain_t* _d, impurecord_t* impu_rec, int barring, str** impus, int* num_impus, int is_shm) {
    int i, j, count;
    *num_impus = 0;
    *impus = 0;
    ims_public_identity* impi;
    int bytes_needed = 0;
    int len = 0;
    char* p = NULL;

    LM_DBG("getting IMPU subscription set\n");

    if (!impu_rec) {
        LM_ERR("no impu record provided\n");
        return 1;
    }

    if (!impu_rec->s) {
        LM_DBG("no subscription associated with impu\n");
        return 0;
    }

    lock_subscription(impu_rec->s);
    for (i = 0; i < impu_rec->s->service_profiles_cnt; i++) {
        for (j = 0; j < impu_rec->s->service_profiles[i].public_identities_cnt; j++) {
            impi = &(impu_rec->s->service_profiles[i].public_identities[j]);

            if (skip_cscf_realm && cscf_realm) {
                p = strstr(impi->public_identity.s, cscf_realm);
            } else {
                p = NULL;
            }

            if (p) {
                LM_DBG("Skip Record %.*s (%i)\n", impi->public_identity.len,
              impi->public_identity.s, impi->public_identity.len);
            } else {
                LM_DBG("Got Record %.*s (%i)\n", impi->public_identity.len,
                impi->public_identity.s, impi->public_identity.len);

                if (barring < 0) {
                    //get all records
                    bytes_needed += impi->public_identity.len;
                    (*num_impus)++;
                } else {
                    if (impi->barring == barring) {
                        //add the record to the list
                        bytes_needed += impi->public_identity.len;
                        (*num_impus)++;
                    }
                }
            }
        }
    }
    LM_DBG("num of records returned is %d and we need %d bytes\n", *num_impus, bytes_needed);

    len = (sizeof (str)*(*num_impus)) + bytes_needed;
    if (is_shm)
        *impus = (str*) shm_malloc(len); 
    else 
        *impus = (str*) pkg_malloc(len); //TODO: rather put this on the stack... dont' fragment pkg....
    
    if (*impus == 0) {
        if (is_shm)
            LM_ERR("no more shm_mem\n");
        else 
            LM_ERR("no more pkg_mem\n");
        return 1;
    }
    char* ptr = (char*) (*impus + *num_impus);

    //now populate the data
    count = 0;
    for (i = 0; i < impu_rec->s->service_profiles_cnt; i++) {
        for (j = 0; j < impu_rec->s->service_profiles[i].public_identities_cnt; j++) {
            impi = &(impu_rec->s->service_profiles[i].public_identities[j]);

            if (skip_cscf_realm && cscf_realm) {
                p = strstr(impi->public_identity.s, cscf_realm);
            } else {
                p = NULL;
            }

            if (p == NULL) {
                if (barring < 0) {
                    //get all records
                    (*impus)[count].s = ptr;
                    memcpy(ptr, impi->public_identity.s, impi->public_identity.len);
                    (*impus)[count].len = impi->public_identity.len;
                    ptr += impi->public_identity.len;
                    count++;
                } else {
                    if (impi->barring == barring) {
                        //add the record to the list
                        (*impus)[count].s = ptr;
                        memcpy(ptr, impi->public_identity.s, impi->public_identity.len);
                        (*impus)[count].len = impi->public_identity.len;
                        ptr += impi->public_identity.len;
                        count++;
                    }
                }
            }
        }
    }

    if (ptr != ((char*) *impus + len)) {
        LM_CRIT("buffer overflow\n");
        return 1;
    }

    unlock_subscription(impu_rec->s);

    return 0;
}

/**
 * @brief Get a subscription from the subscription list based on the IMPI
 *  NB - does not return with a lock on the subscription but does increment ref count
 * @param impu string of impu to search for
 * @param s ims_subscription to be returned if found
 * @param leave_slot_locked if no subscription is found return with the slot locked (in case we want to add) 
 * @return 0 on success
 */
int get_subscription(str* impi_s, ims_subscription** s, int leave_slot_locked) {
    int subscription_hash, sl;
    ims_subscription* ptr;

    subscription_hash = core_hash(impi_s, 0, 0);
    sl = subscription_hash & (subs_hash_size - 1);
    lock_subscription_slot(sl);
    ptr = ims_subscription_list->slot[sl].first;
    while (ptr) {
        if ((impi_s->len == ptr->private_identity.len) && (memcmp(impi_s->s, ptr->private_identity.s, impi_s->len) == 0)) {
            LM_DBG("found an existing subscription for IMPI [%.*s]\n", impi_s->len, impi_s->s);
            (*s) = ptr;
            lock_subscription(ptr);
            ref_subscription_unsafe(ptr);
            unlock_subscription(ptr);
            unlock_subscription_slot(sl);
            return 0;
        }
        ptr = ptr->next;
    }
    if (!leave_slot_locked)
        unlock_subscription_slot(sl);
    return 1;
}

void add_subscription_unsafe(ims_subscription* s) {
    int sl;
    sl = core_hash(&s->private_identity, 0, subs_hash_size);
    subs_slot_add(&ims_subscription_list->slot[sl], s);
    s->sl = sl;

}

void add_subscription(ims_subscription* s) {
    int sl;
    sl = core_hash(&s->private_identity, 0, subs_hash_size);
    lock_subscription_slot(sl);
    add_subscription_unsafe(s);
    unlock_subscription_slot(sl);
}

void delete_subscription(ims_subscription* s) {
    LM_DBG("Deleting subscription %p [%.*s]\n", s, s->private_identity.len, s->private_identity.s);
    free_ims_subscription_data(s);
}

void release_subscription(ims_subscription* s) {
    LM_DBG("Releasing subscription %p [%.*s]\n", s, s->private_identity.len, s->private_identity.s);
    unref_subscription(s);
}

/**
 * @brief update/add subscription
 * @param s
 * @return 
 */
int update_subscription(ims_subscription* s) {
    return 0;
}

void ref_contact_unsafe(ucontact_t* c) {
    LM_DBG("incrementing ref count on contact [%.*s], was %d\n", c->c.len, c->c.s, c->ref_count);
    c->ref_count++;
}

/**
 * @brief unref contact - assume a lock on the slot is held prior to calling this
 * @param c
 */
void unref_contact_unsafe(ucontact_t* c) {
    LM_DBG("decrementing ref count on contact [%.*s], was %d\n", c->c.len, c->c.s, c->ref_count);
    c->ref_count--;
    if (c->ref_count <= 0) {
        LM_DBG("contact [%.*s] no longer referenced.... deleting\n", c->c.len, c->c.s);
        if (c->ref_count < 0) {
            LM_WARN("reference dropped below zero... this should not happen\n");
        }
	c->state = CONTACT_DELAYED_DELETE;
        c->expires = time(NULL) + contact_delete_delay;
//        delete_scontact(c);
    }
}
