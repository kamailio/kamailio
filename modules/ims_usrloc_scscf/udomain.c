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
#include "../../hashes.h"
#include "../../parser/parse_methods.h"
#include "../../mem/shm_mem.h"
#include "../../dprint.h"
#include "../../lib/srdb1/db.h"
#include "../../socket_info.h"
#include "../../ut.h"
#include "ul_mod.h"            /* usrloc module parameters */
#include "usrloc.h"
#include "utime.h"
#include "usrloc.h"
#include "bin_utils.h"
#include "usrloc_db.h"
#include "contact_hslot.h"

extern int unreg_validity;
extern int db_mode;
struct contact_list* contact_list;

#ifdef STATISTICS

static char *build_stat_name(str* domain, char *var_name) {
    int n;
    char *s;
    char *p;

    n = domain->len + 1 + strlen(var_name) + 1;
    s = (char*) shm_malloc(n);
    if (s == 0) {
	LM_ERR("no more shm mem\n");
	return 0;
    }
    memcpy(s, domain->s, domain->len);
    p = s + domain->len;
    *(p++) = '-';
    memcpy(p, var_name, strlen(var_name));
    p += strlen(var_name);
    *(p++) = 0;
    return s;
}
#endif

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
#ifdef STATISTICS
    char *name;
#endif

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

#ifdef STATISTICS
    /* register the statistics */
    if ((name = build_stat_name(_n, "users")) == 0 || register_stat("usrloc",
	    name, &(*_d)->users, STAT_NO_RESET | STAT_SHM_NAME) != 0) {
	LM_ERR("failed to add stat variable\n");
	goto error2;
    }
    if ((name = build_stat_name(_n, "contacts")) == 0 || register_stat("usrloc",
	    name, &(*_d)->contacts, STAT_NO_RESET | STAT_SHM_NAME) != 0) {
	LM_ERR("failed to add stat variable\n");
	goto error2;
    }
    if ((name = build_stat_name(_n, "expires")) == 0 || register_stat("usrloc",
	    name, &(*_d)->expires, STAT_SHM_NAME) != 0) {
	LM_ERR("failed to add stat variable\n");
	goto error2;
    }
#endif

    return 0;
#ifdef STATISTICS
error2:
    shm_free((*_d)->table);
#endif
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
    /*fprintf(_f, "lock : %d\n", _d->lock); -- can be a structure --andrei*/
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
int mem_insert_impurecord(struct udomain* _d, str* public_identity, int reg_state, int barring,
	ims_subscription** s, str* ccf1, str* ccf2, str* ecf1, str* ecf2,
	struct impurecord** _r) {
    int sl;

    if (new_impurecord(_d->name, public_identity, reg_state, barring, s, ccf1, ccf2, ecf1,
	    ecf2, _r) < 0) {
	LM_ERR("creating impurecord failed\n");
	return -1;
    }
    LM_DBG("Successfully parsed user data\n");

    sl = ((*_r)->aorhash) & (_d->size - 1);
    slot_add(&_d->table[sl], *_r);
    update_stat(_d->users, 1);

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
    update_stat(_d->users, -1);
}

/*!
 * \brief Run timer handler for given domain
 * \param _d domain
 */
void mem_timer_udomain(udomain_t* _d) {
    struct impurecord* ptr, *t;
    struct ucontact* contact_ptr, *tmp_contact_ptr;
    int i;

    //go through contacts first
    LM_DBG("*** mem_timer_udomain - checking contacts - START ***\n");
    
    for (i = 0; i < contact_list->size; i++) {
#ifdef EXTRA_DEBUG
	LM_DBG("looking for contacts in slot %d\n", i);
#endif
	lock_contact_slot_i(i);
	contact_ptr = contact_list->slot[i].first;
	while (contact_ptr) {
	    LM_DBG("We have a contact in the new contact list in slot %d = [%.*s] (%.*s) which expires in %lf seconds and has a ref count of %d\n", i, contact_ptr->aor.len, contact_ptr->aor.s, contact_ptr->c.len, contact_ptr->c.s, (double) contact_ptr->expires - time(NULL), contact_ptr->ref_count);
	    if (contact_ptr->ref_count <= 0) {
		LM_DBG("Deleting contact [%.*s]\n", contact_ptr->c.len, contact_ptr->c.s);
		tmp_contact_ptr = contact_ptr->next;
		delete_ucontact(contact_ptr);
		contact_ptr = tmp_contact_ptr;
	    } else {
		contact_ptr = contact_ptr->next;
	    }
	}
	unlock_contact_slot_i(i);
    }
    
    LM_DBG("*** mem_timer_udomain - checking contacts - FINISHED ***\n");

    int temp = 0;

    LM_DBG("*** mem_timer_udomain - checking IMPUs - START ***\n");
    for (i = 0; i < _d->size; i++) {
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

//	    			if (t->reg_state == IMPU_NOT_REGISTERED && t->shead == 0) {
//	    				//remove it - housekeeping - not sure why its still here...?
//	    				if (exists_ulcb_type(t->cbs, UL_IMPU_NR_DELETE))
//	    					run_ul_callbacks(t->cbs, UL_IMPU_NR_DELETE, t, NULL);
//	    					    
//	    				LM_DBG("about to delete impurecord\n");
//	    				delete_impurecord(_d, &t->public_identity, t);
//	    			} //else if (t->reg_state == IMPU_UNREGISTERED) {//Remove IMPU record if it is in state IMPU_UNREGISTERED and has expired
	    //			    
	    //				if (time_now >= t->expires) {//check here and only remove if no subscribes - if there is a subscribe then bump the validity by unreg_validity
	    //				    if(t->shead != 0){
	    //					LM_DBG("This impurecord still has subscriptions - extending the expiry");
	    //					t->expires = time(NULL) + unreg_validity;
	    //				    } else {
	    //					if (exists_ulcb_type(t->cbs, UL_IMPU_UNREG_EXPIRED))
	    //						run_ul_callbacks(t->cbs, UL_IMPU_UNREG_EXPIRED, t, NULL);
	    //					LM_DBG("about to delete impurecord\n");
	    //					delete_impurecord(_d, &t->public_identity, t);
	    //				    }
	    //				}
	    //			//} else if (t->reg_state != IMPU_UNREGISTERED && t->contacts == 0) { /* Remove the entire record if it is empty IFF it is not an UNREGISTERED RECORD */
	    //			} else if (t->reg_state != IMPU_UNREGISTERED && t->num_contacts == 0 && t->shead == 0) { /* Remove the entire record if it is empty IFF it is not an UNREGISTERED RECORD */
	    //																								/* TS 23.228 5.3.2.1 (release 11) */
	    //				//need a way of distinguishing between deletes that need a SAR (expired) and deletes that do not need a SAR (explicit de reg)
	    //				//we only want to send one SAR for each implicit IMPU set
	    //				//make sure all IMPU's associated with this set are de-registered before calling the callbacks
	    //				int first=1;
	    //				int this_is_first = 0;
	    //
	    //				lock_get(t->s->lock);
	    //				for (k = 0; k < t->s->service_profiles_cnt; k++){
	    //					for (j = 0;j < t->s->service_profiles[k].public_identities_cnt;j++) {
	    //						impu = &(t->s->service_profiles[k].public_identities[j]);
	    //
	    //						sl = core_hash(&impu->public_identity, 0, _d->size);
	    //						if (sl != i)
	    //							lock_udomain(_d, &impu->public_identity);
	    //
	    //						if (first) {
	    //							first = 0; //dont do anything - we will leave this impu to be processed as normal
	    //							if (!strncmp(impu->public_identity.s, t->public_identity.s, t->public_identity.len)) {
	    //								//we are the first in the implicit set
	    //								this_is_first = 1;
	    //							}
	    //						} else {
	    //							//set all other implicits to not registered
	    //							if (update_impurecord(_d, &impu->public_identity, IMPU_NOT_REGISTERED,
	    //														-1/*barring*/, -1 /*do not change send sar on delete */, 0/*is_primary*/, NULL, NULL, NULL, NULL, NULL, &temp_impu) != 0) {
	    //								LM_ERR("Unable to update impurecord for <%.*s>\n", impu->public_identity.len, impu->public_identity.s);
	    //							}
	    //						}
	    //						if (sl != i)
	    //							unlock_udomain(_d, &impu->public_identity);
	    //					}
	    //				}
	    //				lock_release(t->s->lock);
	    //
	    //				if (this_is_first) {
	    //					//now run a normal callback on our
	    //					if (exists_ulcb_type(t->cbs, UL_IMPU_REG_NC_DELETE))
	    //						run_ul_callbacks(t->cbs, UL_IMPU_REG_NC_DELETE, t, NULL);
	    //					LM_DBG("about to delete impurecord\n");
	    //						delete_impurecord(_d, &t->public_identity, t);
	    //				}
	    //			}
	}
	if (temp) {
#ifdef EXTRA_DEBUG
	    LM_DBG("ULSLOT %d UN-LOCKED\n", i);
#endif
	}
	unlock_ulslot(_d, i);
    }
    LM_DBG("*** mem_timer_udomain - checking IMPUs - FINISHED ***\n");
}

/*!
 * \brief Get lock for a domain
 * \param _d domain
 * \param _aor adress of record, used as hash source for the lock slot
 */
void lock_udomain(udomain_t* _d, str* _aor) {
    unsigned int sl;
    sl = core_hash(_aor, 0, _d->size);
#ifdef EXTRA_DEBUG
    LM_DBG("LOCKING UDOMAIN SLOT [%d]\n", sl);
#endif

#ifdef GEN_LOCK_T_PREFERED
    lock_get(_d->table[sl].lock);
#else
    ul_lock_idx(_d->table[sl].lockidx);
#endif
}

/*!
 * \brief Release lock for a domain
 * \param _d domain
 * \param _aor address of record, uses as hash source for the lock slot
 */
void unlock_udomain(udomain_t* _d, str* _aor) {
    unsigned int sl;
    sl = core_hash(_aor, 0, _d->size);
#ifdef EXTRA_DEBUG
    LM_DBG("UN-LOCKING UDOMAIN SLOT [%d]\n", sl);
#endif
#ifdef GEN_LOCK_T_PREFERED
    lock_release(_d->table[sl].lock);
#else
    ul_release_idx(_d->table[sl].lockidx);
#endif
}

/*!
 * \brief  Get lock for a slot
 * \param _d domain
 * \param i slot number
 */
void lock_ulslot(udomain_t* _d, int i) {
#ifdef GEN_LOCK_T_PREFERED
    lock_get(_d->table[i].lock);
#else
    ul_lock_idx(_d->table[i].lockidx);
#endif
}

/*!
 * \brief Release lock for a slot
 * \param _d domain
 * \param i slot number
 */
void unlock_ulslot(udomain_t* _d, int i) {
#ifdef GEN_LOCK_T_PREFERED
    lock_release(_d->table[i].lock);
#else
    ul_release_idx(_d->table[i].lockidx);
#endif
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

/*!
 * \brief Create and insert a new record
 * \param _d domain to insert the new record
 * \param _aor address of the record
 * \param _r new created record
 * \return return 0 on success, -1 on failure
 */
int insert_impurecord(struct udomain* _d, str* public_identity, int reg_state, int barring,
	ims_subscription** s, str* ccf1, str* ccf2, str* ecf1, str* ecf2,
	struct impurecord** _r) {

    //	ims_subscription* s = 0;
    //	/*check we can parse XML user data*/
    //	if (xml_data->s && xml_data->len > 0) {
    //		s = parse_user_data(*xml_data);
    //		if (!s) {
    //			LM_ERR("Unable to parse XML user data from SAA\n");
    //			goto error;
    //		}
    //	}
    if (mem_insert_impurecord(_d, public_identity, reg_state, barring, s, ccf1, ccf2, ecf1, ecf2, _r) < 0) {
	LM_ERR("inserting record failed\n");
	goto error;
    }

    /*DB?*/
    if (db_mode == WRITE_THROUGH && db_insert_impurecord(_d, public_identity, reg_state, barring, s, ccf1, ccf2, ecf1, ecf2, _r) != 0) {
	LM_ERR("error inserting contact into db");
	goto error;
    }

    return 0;

error:
    //    if (s) {
    //    	free_ims_subscription_data(s);
    //    }
    return -1;
}

/*!
 * \brief Obtain a impurecord pointer if the impurecord exists in domain
 * \param _d domain to search the record
 * \param _aor address of record
 * \param _r new created record
 * \return 0 if a record was found, 1 if nothing could be found
 */
int get_impurecord(udomain_t* _d, str* public_identity, struct impurecord** _r) {
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
 * \brief Delete a impurecord from domain
 * \param _d domain where the record should be deleted
 * \param _aor address of record
 * \param _r deleted record
 * \return 0 on success, -1 if the record could not be deleted
 */
int delete_impurecord(udomain_t* _d, str* _aor, struct impurecord* _r) {
    //    struct ucontact* c;//, *t;

    LM_DBG("Deleting IMPURECORD [%.*s]\n", _r->public_identity.len, _r->public_identity.s);

    if (_r == 0) {
	if (get_impurecord(_d, _aor, &_r) > 0) {
	    return 0;
	}
    }

    //TODO: need to unref the contacts in the contact list (not delete them), the timer should delete all contacts that are unreffed
    //    c = _r->contacts;
    //    while (c) {
    //	t = c;
    //	c = c->next;
    //	if (delete_ucontact(_r, t) < 0) {
    //	    LM_ERR("deleting contact failed [%.*s]\n", c->aor.len, c->aor.s);
    //	    return -1;
    //	}
    //    }

    if (exists_ulcb_type(_r->cbs, UL_IMPU_DELETE)) {
	run_ul_callbacks(_r->cbs, UL_IMPU_DELETE, _r, 0);
    }

    /*DB?*/
    if (db_mode == WRITE_THROUGH
	    && db_delete_impurecord(_d, _r) != 0) {
	LM_ERR("error deleting IMPU record from db");
	return 0;
    }

    mem_delete_impurecord(_d, _r);
    return 0;
}

/*
 * get all IMPUs as string from a subscription related to an impurecord. apply filter for barring (assumed to be called with lock on impurec)
 * barring-1 get all barred
 * barring-0 get all unbarred
 * barring-(-1) get all records
 * NB. Remember to free the block of memory pointed to by impus (pkg_malloc)
 */
int get_impus_from_subscription_as_string(udomain_t* _d, impurecord_t* impu_rec, int barring, str** impus, int* num_impus) {
    int i, j, count;
    *num_impus = 0;
    *impus = 0;
    ims_public_identity* impi;
    int bytes_needed = 0;
    int len = 0;

    LM_DBG("getting IMPU subscription set\n");

    if (!impu_rec) {
	LM_ERR("no impu record provided\n");
	return 1;
    }

    if (!impu_rec->s) {
	LM_DBG("no subscription associated with impu\n");
	return 0;
    }

    lock_get(impu_rec->s->lock);
    for (i = 0; i < impu_rec->s->service_profiles_cnt; i++) {
	for (j = 0; j < impu_rec->s->service_profiles[i].public_identities_cnt; j++) {
	    impi = &(impu_rec->s->service_profiles[i].public_identities[j]);
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
    LM_DBG("num of records returned is %d and we need %d bytes\n", *num_impus, bytes_needed);

    len = (sizeof (str)*(*num_impus)) + bytes_needed;
    *impus = (str*) pkg_malloc(len);
    if (*impus == 0) {
	LM_ERR("no more pkg_mem\n");
	return 0;
    }
    char* ptr = (char*) (*impus + *num_impus);

    //now populate the data
    count = 0;
    for (i = 0; i < impu_rec->s->service_profiles_cnt; i++) {
	for (j = 0; j < impu_rec->s->service_profiles[i].public_identities_cnt; j++) {
	    impi = &(impu_rec->s->service_profiles[i].public_identities[j]);
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

    if (ptr != ((char*) *impus + len)) {
	LM_CRIT("buffer overflow\n");
	return 1;
    }

    lock_release(impu_rec->s->lock);

    return 0;
}

