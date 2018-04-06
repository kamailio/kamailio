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

#include "impurecord.h"
#include <string.h>
#include "../../core/hashes.h"
#include "../../core/mem/shm_mem.h"
#include "../../core/dprint.h"
#include "../../core/ut.h"
#include "ims_usrloc_scscf_mod.h"
#include "usrloc.h"
#include "utime.h"
#include "ul_callback.h"
#include "usrloc.h"
#include "bin_utils.h"
#include "subscribe.h"
#include "usrloc_db.h"
#include "../../lib/ims/useful_defs.h"
#include "../../modules/ims_dialog/dlg_load.h"
#include "../../modules/ims_dialog/dlg_hash.h"
#include "contact_hslot.h"
#include "dlist.h"
#include "ul_scscf_stats.h"
#include "hslot_sp.h"

/*! contact matching mode */
int matching_mode = CONTACT_ONLY;
/*! retransmission detection interval in seconds */
int cseq_delay = 20;

extern int unreg_validity;
extern int maxcontact_behaviour;
extern int maxcontact;
extern int maxcontact_3gpp;
extern int db_mode;

extern int sub_dialog_hash_size;
extern int subs_hash_size;
extern shtable_t sub_dialog_table;
extern struct contact_list* contact_list;
extern struct ims_subscription_list* ims_subscription_list;

extern ims_dlg_api_t dlgb;

static ucontact_t* contacts_to_expire [MAX_CONTACTS_PER_IMPU]; //this is done to prevent fragmentation of memory...
static int num_contacts_to_expire;

/*!
 * \brief Create and initialize new record structure
 * \param _dom domain name
 * \param _aor address of record
 * \param _r pointer to the new record
 * \return 0 on success, negative on failure
 */
int new_impurecord(str* _dom, str* public_identity, str* private_identity, int reg_state, int barring, ims_subscription** s, str* ccf1, str* ccf2, str* ecf1, str* ecf2, impurecord_t** _r) {
    *_r = (impurecord_t*) shm_malloc(sizeof (impurecord_t));
    if (*_r == 0) {
        LM_ERR("no more shared memory\n");
        return -1;
    }
    memset(*_r, 0, sizeof (impurecord_t));

    //setup callback list
    (*_r)->cbs = (struct ulcb_head_list*) shm_malloc(
            sizeof (struct ulcb_head_list));
    if ((*_r)->cbs == 0) {
        LM_CRIT("no more shared mem\n");
        shm_free(*_r);
        goto error;
    }
    (*_r)->cbs->first = 0;
    (*_r)->cbs->reg_types = 0;
	(*_r)->linked_contacts.head = (*_r)->linked_contacts.tail = 0;
	(*_r)->linked_contacts.numcontacts = 0;
    (*_r)->public_identity.s = (char*) shm_malloc(public_identity->len);
    if ((*_r)->public_identity.s == 0) {
        LM_ERR("no more shared memory\n");
        shm_free(*_r);
        goto error;
    }
    memcpy((*_r)->public_identity.s, public_identity->s, public_identity->len);
    (*_r)->public_identity.len = public_identity->len;

    (*_r)->private_identity.s = (char*) shm_malloc(private_identity->len);
    if ((*_r)->private_identity.s == 0) {
        LM_ERR("no more shared memory\n");
        shm_free(*_r);
        goto error;
    }
    memcpy((*_r)->private_identity.s, private_identity->s, private_identity->len);
    (*_r)->private_identity.len = private_identity->len;

    (*_r)->domain = _dom;
    (*_r)->aorhash = core_hash(public_identity, 0, 0);
    (*_r)->reg_state = reg_state;
    if (barring >= 0) { //just in case we call this with no barring -1 will ignore
        (*_r)->barring = barring;
    }
    (*_r)->send_sar_on_delete = 1; /*defaults to 1 */
    if (ccf1 && ccf1->len > 0) STR_SHM_DUP((*_r)->ccf1, *ccf1, "CCF1");
    if (ccf2 && ccf2->len > 0) STR_SHM_DUP((*_r)->ccf2, *ccf2, "CCF2");
    if (ecf1 && ecf1->len > 0) STR_SHM_DUP((*_r)->ecf1, *ecf1, "ECF1");
    if (ecf2 && ecf2->len > 0) STR_SHM_DUP((*_r)->ecf2, *ecf2, "ECF2");
    /*assign ims subscription profile*/
    if (s && *s) {
        ref_subscription_unsafe(*s);
        (*_r)->s = *s;
    }

    return 0;

out_of_memory:
    LM_ERR("no more shared memory\n");
    *_r = 0;
    return -3;
error:
    LM_ERR("Failed to create new impurecord...\n");
    *_r = 0;
    return -2;
}

/*!
 * \brief Free all memory used by the given structure
 *
 * Free all memory used by the given structure.
 * The structure must be removed from all linked
 * lists first
 * \param _r freed record list
 */
void free_impurecord(impurecord_t* _r) {
    struct ul_callback *cbp, *cbp_tmp;
    struct _reg_subscriber* subscriber, *s_tmp;

    LM_DBG("free_impurecord\n");
    //free IMS specific extensions
    if (_r->ccf1.s)
        shm_free(_r->ccf1.s);
    if (_r->ccf2.s)
        shm_free(_r->ccf2.s);
    if (_r->ecf1.s)
        shm_free(_r->ecf1.s);
    if (_r->ecf2.s)
        shm_free(_r->ecf2.s);
    if (_r->s) {
        unref_subscription(_r->s);
    }

    /*remove REG subscriptions to this IMPU*/
    subscriber = _r->shead;
    while (subscriber) {
        s_tmp = subscriber->next;
        free_subscriber(subscriber);
        subscriber = s_tmp;
    }
    _r->shead = 0;

    if (_r->public_identity.s)
        shm_free(_r->public_identity.s);

    if (_r->private_identity.s)
        shm_free(_r->private_identity.s);

    //free callback list
    for (cbp = _r->cbs->first; cbp;) {
        cbp_tmp = cbp;
        cbp = cbp->next;
        if (cbp_tmp->param)
            shm_free(cbp_tmp->param);
        shm_free(cbp_tmp);
    }
    shm_free(_r->cbs);
    shm_free(_r);
}

/*!
 * \brief Print a record, useful for debugging
 * \param _f print output
 * \param _r printed record
 */
void print_impurecord(FILE* _f, impurecord_t* _r) {
    fprintf(_f, "...IMPU Record(%p)...\n", _r);
    fprintf(_f, "\tdomain : '%.*s'\n", _r->domain->len, ZSW(_r->domain->s));
    fprintf(_f, "\tpublic_identity    : '%.*s'\n", _r->public_identity.len, ZSW(_r->public_identity.s));
    fprintf(_f, "\taorhash: '%u'\n", (unsigned) _r->aorhash);
    fprintf(_f, "\tslot:    '%d'\n", _r->aorhash & (_r->slot->d->size - 1));
    fprintf(_f, "\tstate:  '%s (%d)'\n", get_impu_regstate_as_string(_r->reg_state), _r->reg_state);
    fprintf(_f, "\tbarring: '%d'\n", _r->barring);
    fprintf(_f, "\tccf1:    '%.*s'\n", _r->ccf1.len, _r->ccf1.s);
    fprintf(_f, "\tccf2:    '%.*s'\n", _r->ccf2.len, _r->ccf2.s);
    fprintf(_f, "\tecf1:    '%.*s'\n", _r->ecf1.len, _r->ecf1.s);
    fprintf(_f, "\tecf2:    '%.*s'\n", _r->ecf2.len, _r->ecf2.s);
    if (_r->s) {
        fprintf(_f, "\tIMS service profiles count (%d):   '%p' (refcount: %d)\n", _r->s->service_profiles_cnt, _r->s, _r->s->ref_count);
		fprintf(_f, "\tIMPI for subscription: [%.*s]\n", _r->s->private_identity.len, _r->s->private_identity.s);
    }

    int header = 0;
    reg_subscriber* subscriber = _r->shead;
    while (subscriber) {
        if (!header) {
            fprintf(_f, "\t...Subscriptions...\n");
            header = 1;
        }
        fprintf(_f, "\t\twatcher uri: <%.*s> and presentity uri: <%.*s>\n", subscriber->watcher_uri.len, subscriber->watcher_uri.s, subscriber->presentity_uri.len, subscriber->presentity_uri.s);
        fprintf(_f, "\t\tExpires: %ld\n", subscriber->expires);
        subscriber = subscriber->next;
    }

	impu_contact_t *impucontact = _r->linked_contacts.head;
	while (impucontact) {
		print_ucontact(_f, impucontact->contact);
		impucontact = impucontact->next;
	}

    fprintf(_f, ".../Record...\n\n\n\n");
}

/*!
 * \brief Add a new contact in memory
 *
 * Add a new contact in memory, contacts are ordered by:
 * 1) q value, 2) descending modification time
 * \param _r record this contact belongs to
 * \param _c contact
 * \param _ci contact information
 * \return pointer to new created contact on success, 0 on failure
 */
ucontact_t* mem_insert_scontact(impurecord_t* _r, str* _c, ucontact_info_t* _ci) {
    ucontact_t* c;
    int sl;

    if ((c = new_ucontact(_r->domain, &_r->public_identity, _c, _ci)) == 0) {
        LM_ERR("failed to create new contact\n");
        return 0;
    }
    counter_inc(ul_scscf_cnts_h.active_contacts);

    LM_DBG("Created new contact in memory with AOR: [%.*s] and hash [%d]\n", _c->len, _c->s, c->sl);

    sl = (c->sl);
    lock_contact_slot_i(sl);
    contact_slot_add(&contact_list->slot[sl], c);
    unlock_contact_slot_i(sl);

    return c;
}

/*!
 * \brief Remove the contact from lists in memory
 * \param _r record this contact belongs to
 * \param _c removed contact
 */
void mem_remove_ucontact(ucontact_t* _c) {
    LM_DBG("removing contact [%.*s] from slot %d\n", _c->c.len, _c->c.s, _c->sl);
    contact_slot_rem(&contact_list->slot[_c->sl], _c);
    counter_add(ul_scscf_cnts_h.active_contacts, -1);
}

/*!
 * \brief Remove contact in memory from the list and delete it
 * \param _r record this contact belongs to
 * \param _c deleted contact
 */
void mem_delete_ucontact(ucontact_t* _c) {

    struct contact_dialog_data *dialog_data;
    //tear down dialogs in dialog data list
    LM_DBG("Checking if dialog_data is there and needs to be torn down\n");
    if(_c->first_dialog_data == 0) {
        LM_DBG("first dialog is 0!\n");
    } else {
        LM_DBG("first dialog is not 0\n");
    }
    for (dialog_data = _c->first_dialog_data; dialog_data;) {
        LM_DBG("Going to tear down dialog with info h_entry [%d] h_id [%d]\n", dialog_data->h_entry, dialog_data->h_id);
        dlgb.lookup_terminate_dlg(dialog_data->h_entry, dialog_data->h_id, NULL);
        dialog_data = dialog_data->next;
    }

    mem_remove_ucontact(_c);
    free_ucontact(_c);
}

static str autocommit_off = str_init("SET AUTOCOMMIT=0");
static str fail_isolation_level = str_init("SET TRANSACTION ISOLATION LEVEL READ COMMITTED");
static str start_transaction = str_init("START TRANSACTION");
static str commit = str_init("COMMIT");
/* static str rollback = str_init("ROLLBACK"); */
static str autocommit_on = str_init("SET AUTOCOMMIT=1");

static inline void start_dbtransaction() {
    
    if (db_mode == NO_DB) 
        return;
    
    if (ul_dbf.raw_query(ul_dbh, &autocommit_off, NULL) < 0) {
        LM_ERR("could not "
                "set autocommit off!\n");
    }
    if (ul_dbf.raw_query(ul_dbh, &fail_isolation_level, NULL) < 0) {
        LM_ERR("could not "
                "set transaction isolation level!\n");
    }
    if (ul_dbf.raw_query(ul_dbh, &start_transaction, NULL) < 0) {
        LM_ERR("could not "
                "start transaction!\n");
    }
}
/*!
 * \brief Expires timer for NO_DB db_mode
 *
 * Expires timer for NO_DB db_mode, process all contacts from
 * the record, delete the expired ones from memory.
 * \param _r processed record
 */
static inline void process_impurecord(impurecord_t* _r) {
    int flag, mustdeleteimpu = 1, n, k;
    unsigned int sl;
    ucontact_t* ptr;
    int hascontacts;
    udomain_t* _d;
    reg_subscriber *s, *next;
    subs_t* sub_dialog;
    int dbwork = 0;
	impu_contact_t *impu_contact;

    get_act_time();

    s = _r->shead;
    LM_DBG("Checking validity of IMPU: <%.*s> registration subscriptions\n", _r->public_identity.len, _r->public_identity.s);
    while (s) {
		next = s->next;
        if (!valid_subscriber(s, act_time)) {
            LM_DBG("DBG:registrar_timer: Subscriber with watcher_contact <%.*s> and presentity uri <%.*s> expired and removed.\n",
                    s->watcher_contact.len, s->watcher_contact.s, s->presentity_uri.len, s->presentity_uri.s);
            if (!dbwork) {
                start_dbtransaction();
                dbwork = 1;
            }
            delete_subscriber(_r, s);
        } else {
            LM_DBG("DBG:registrar_timer: Subscriber with watcher_contact <%.*s> and presentity uri <%.*s> is valid and expires in %d seconds.\n",
                    s->watcher_contact.len, s->watcher_contact.s, s->presentity_uri.len, s->presentity_uri.s,
                    (unsigned int) (s->expires - time(NULL)));
            sl = core_hash(&s->call_id, &s->to_tag, sub_dialog_hash_size);
            LM_DBG("Hash size: <%i>", sub_dialog_hash_size);
            LM_DBG("Searching sub dialog hash info with call_id: <%.*s> and ttag <%.*s> ftag <%.*s> and hash code <%i>", s->call_id.len, s->call_id.s, s->to_tag.len, s->to_tag.s, s->from_tag.len, s->from_tag.s, sl);
            /* search the record in hash table */
            lock_get(&sub_dialog_table[sl].lock);
            sub_dialog = pres_search_shtable(sub_dialog_table, s->call_id, s->to_tag, s->from_tag, sl);
            if (sub_dialog == NULL) {
                LM_ERR("DBG:registrar_timer: Subscription has no dialog record in hash table\n");
            } else {
                LM_DBG("DBG:registrar_timer: Subscription has dialog record in hash table with presentity uri <%.*s>\n", sub_dialog->pres_uri.len, sub_dialog->pres_uri.s);
            }
            lock_release(&sub_dialog_table[sl].lock);
            mustdeleteimpu = 0;
        }
        s = next;
    }

    LM_DBG("\tPublic Identity %.*s, Barred: [%d], State: [%s], contacts [%d], 3gppcontacts [%d]\n",
            _r->public_identity.len, _r->public_identity.s,
            _r->barring,
            get_impu_regstate_as_string(_r->reg_state),
			_r->linked_contacts.numcontacts,
			_r->linked_contacts.num3gppcontacts);
    flag = 0;
    hascontacts = 0;
    num_contacts_to_expire = 0;
	impu_contact = _r->linked_contacts.head;
	k=0;
	while (impu_contact) {
		ptr = impu_contact->contact;
		flag = 1;
		if (!VALID_CONTACT(ptr, act_time)) {
			if (ptr->state == CONTACT_DELETED) {
				LM_DBG("Contact: <%.*s> has been deleted - unlinking from IMPU\n", ptr->c.len, ptr->c.s);
				contacts_to_expire[num_contacts_to_expire] = ptr;
				num_contacts_to_expire++;
			} else if (ptr->state == CONTACT_EXPIRE_PENDING_NOTIFY) {
				LM_DBG("Contact: <%.*s> is in state CONTACT_EXPIRE_PENDING_NOTIFY....running callback\n", ptr->c.len, ptr->c.s);
				if (exists_ulcb_type(_r->cbs, UL_IMPU_DELETE_CONTACT)) {
					LM_DBG("Running callback UL_IMPU_DELETE_CONTACT for contact [%.*s] and impu [%.*s]\n", ptr->c.len, ptr->c.s, _r->public_identity.len, _r->public_identity.s);
					run_ul_callbacks(_r->cbs, UL_IMPU_DELETE_CONTACT, _r, ptr);
				}
				hascontacts = 1;    // we do this because the impu must only be deleted if in state deleted....
				mustdeleteimpu = 0;
			} else if (ptr->state == CONTACT_VALID) {
				LM_DBG("Contact: <%.*s> is in state valid but it has expired.... ignoring as the contact check will set the appropriate action/state\n", ptr->c.len, ptr->c.s);
				mustdeleteimpu = 0;
				hascontacts = 1;
			} else {
				LM_WARN("Bogus state for contact [%.*s] - state: %d... ignoring", ptr->c.len, ptr->c.s, ptr->state);
				mustdeleteimpu = 0;
				hascontacts = 1;
			}
		} else {
			LM_DBG("\t\tContact #%i - %.*s, Ref [%d] (expires in %ld seconds) (State: %d)\n", 
					k, ptr->c.len, ptr->c.s, ptr->ref_count, ptr->expires - act_time, ptr->state);
			mustdeleteimpu = 0;
			hascontacts = 1;
		}
		impu_contact = impu_contact->next;
		k++;
    }

    if (num_contacts_to_expire > 0) {
        LM_DBG("\tThere are %d contacts to expire/unlink\n", num_contacts_to_expire);
        for (n = 0; n < num_contacts_to_expire; n++) {
            ptr = contacts_to_expire[n];
            LM_DBG("\t\texpiring contact %i: [%.*s] in slot [%d]\n", n, contacts_to_expire[n]->c.len, contacts_to_expire[n]->c.s, contacts_to_expire[n]->sl);
            sl = ptr->sl;
            lock_contact_slot_i(sl);
            if (!dbwork) {
                start_dbtransaction();
                dbwork=1;
            }
            unlink_contact_from_impu(_r, ptr, 1, 0 /*implicit dereg of contact from IMPU*/);
            unlock_contact_slot_i(sl);
        }
    }

    if (!flag)
        LM_DBG("no contacts\n");

    if (mustdeleteimpu) {
        if (!dbwork) {
            start_dbtransaction();
            dbwork=1;
        }
        register_udomain("location", &_d);
        delete_impurecord(_d, &_r->public_identity, _r);
    } else {
        if (!hascontacts) {
            LM_DBG("This impu is not to be deleted but has no contacts - changing state to IMPU_UNREGISTERED\n");
			//run callback  here UL_IMPU_UNREG_NC for UL_IMPU_UNREG_NC
			if (_r->reg_state != IMPU_UNREGISTERED && exists_ulcb_type(_r->cbs, UL_IMPU_UNREG_NC)) {
				run_ul_callbacks(_r->cbs, UL_IMPU_UNREG_NC, _r, 0);
			}
			_r->reg_state = IMPU_UNREGISTERED;
        }
    }
    
    if (dbwork && db_mode != NO_DB) {
        if (ul_dbf.raw_query(ul_dbh, &commit, NULL) < 0) {
            LM_ERR("transaction commit "
                    "failed.\n");
        }
        if (ul_dbf.raw_query(ul_dbh, &autocommit_on, NULL) < 0) {
            LM_ERR("could not turn "
                    "transaction autocommit on.\n");
        }
    }
}

/*!
 * \brief Process impurecords (check contacts for expiry, etc (assume domain slot is locked)
 * @param _r impurecord to process
 */
void timer_impurecord(impurecord_t* _r) {
    process_impurecord(_r);
}

int get_contacts_count(impurecord_t* _r) {
	return _r->linked_contacts.numcontacts;
}

int get_contacts_3gpp_count(impurecord_t* _r) {
	return _r->linked_contacts.num3gppcontacts;
}

/*!
 * \brief Create and insert new contact into impurecord
 * \param _r record into the new contact should be inserted
 * \param _contact contact string
 * \param _ci contact information
 * \param _c new created contact
 * \return 0 on success, -1 on failure
 */
int insert_scontact(impurecord_t* _r, str* _contact, ucontact_info_t* _ci, ucontact_t** _c) {
    //First check our constraints
    if (maxcontact > 0 && maxcontact_behaviour > 0) {
        int numcontacts = get_contacts_count(_r);
        if (numcontacts >= maxcontact) {
            switch (maxcontact_behaviour) {
                case 1://reject
                    LM_ERR("too many contacts already registered for IMPU <%.*s>\n", _r->public_identity.len, _r->public_identity.s);
                    return -1;
                case 2://overwrite oldest
                    LM_DBG("Too many contacts already registered, overwriting oldest for IMPU <%.*s>\n", _r->public_identity.len, _r->public_identity.s);
                    //we can just remove the first one seeing the contacts are ordered on insertion with newest last and oldest first
                    break;
                default://unknown
                    LM_ERR("unknown maxcontact behaviour..... ignoring\n");
                    break;
            }
        }
    }
	
	if (maxcontact_3gpp > 0 && maxcontact_behaviour > 0) {
        int num_3gpp_contacts = get_contacts_3gpp_count(_r);
        if (num_3gpp_contacts >= maxcontact_3gpp) {
            switch (maxcontact_behaviour) {
                case 1://reject
                    LM_ERR("too many 3GPP contacts already registered for IMPU <%.*s>\n", _r->public_identity.len, _r->public_identity.s);
                    return -1;
                case 2://overwrite oldest
                    LM_DBG("Too many 3GPP contacts already registered, overwriting oldest for IMPU <%.*s>\n", _r->public_identity.len, _r->public_identity.s);
                    //we can just remove the first 3GPP one seeing the contacts are ordered on insertion with newest last and oldest first
                    break;
                default://unknown
                    LM_ERR("unknown maxcontact behaviour..... ignoring\n");
                    break;
            }
        }
    }
	
	//at this stage we are safe to insert the new contact
    LM_DBG("INSERTing ucontact in usrloc module\n");
    if (((*_c) = mem_insert_scontact(_r, _contact, _ci)) == 0) {
        LM_ERR("failed to insert contact\n");
        return -1;
    }

    //    /*DB?*/
    if (db_mode == WRITE_THROUGH && db_insert_ucontact(_r, *_c) != 0) {
        LM_ERR("error inserting contact into db");
        return -1;
    }

    //make sure IMPU is linked to this contact
    link_contact_to_impu(_r, *_c, 1);

    release_scontact(*_c);

    if (exists_ulcb_type(NULL, UL_CONTACT_INSERT)) {
        run_ul_callbacks(NULL, UL_CONTACT_INSERT, _r, *_c);
    }
    if (exists_ulcb_type(_r->cbs, UL_IMPU_NEW_CONTACT)) {
        run_ul_callbacks(_r->cbs, UL_IMPU_NEW_CONTACT, _r, *_c);
    }

    return 0;
}

/*!
 * \brief Delete ucontact from impurecord
 * \param _r record where the contact belongs to
 * \param _c deleted contact
 * \return 0 on success, -1 on failure
 */
int delete_scontact(struct ucontact* _c) {
    int ret = 0;

    LM_DBG("Deleting contact: [%.*s]\n", _c->c.len, _c->c.s);
    /*DB?*/
    if (db_mode == WRITE_THROUGH && db_delete_ucontact(_c) != 0) {
        LM_ERR("error removing contact from DB [%.*s]... will still remove from memory\n", _c->c.len, _c->c.s);
    }
    mem_delete_ucontact(_c);

    return ret;
}

/* function to convert contact aor to only have data after @ - ie strip user part */
int aor_to_contact(str* aor, str* contact) {
    char* p;
    int ret = 0; //success

    contact->s = aor->s;
    contact->len = aor->len;
    if (memcmp(aor->s, "sip:", 4) == 0) {
        contact->s = aor->s + 4;
        contact->len -= 4;
    }

    if ((p = memchr(contact->s, '@', contact->len))) {
        contact->len -= (p - contact->s + 1);
        contact->s = p + 1;
    }

    if ((p = memchr(contact->s, ';', contact->len))) {
        contact->len = p - contact->s;
    }

    if ((p = memchr(contact->s, '>', contact->len))) {
        contact->len = p - contact->s;
    }

    return ret;
}

/*!
 * \brief Match a contact record to a contact string
 * \param ptr contact record
 * \param _c contact string
 * \return ptr on successfull match, 0 when they not match
 */
static inline struct ucontact* contact_match(unsigned int slot, str* _c) {
    ucontact_t* ptr = contact_list->slot[slot].first;

    while (ptr) {
        if ((ptr->state != CONTACT_DELAYED_DELETE) && (_c->len == ptr->c.len) && !memcmp(_c->s, ptr->c.s, _c->len)) {//check validity
            return ptr;
        }
        ptr = ptr->next;
    }
    return 0;
}

/*!
 * \brief Match a contact record to a contact string but only compare the ip port portion
 * \param ptr contact record
 * \param _c contact string
 * \return ptr on successfull match, 0 when they not match
 */
static inline struct ucontact* contact_port_ip_match(unsigned int slot, str* _c) {
    ucontact_t* ptr = contact_list->slot[slot].first;
    str string_ip_port, contact_ip_port;
    aor_to_contact(_c, &string_ip_port); //strip userpart from test contact

    while (ptr) {
        aor_to_contact(&ptr->c, &contact_ip_port); //strip userpart from contact
        if ((ptr->state != CONTACT_DELAYED_DELETE)
            && (string_ip_port.len == contact_ip_port.len) 
            && !memcmp(string_ip_port.s, contact_ip_port.s, string_ip_port.len)) {
            return ptr;
        }

        ptr = ptr->next;
    }
    return 0;
}

/*!
 * \brief Match a contact record to a contact string and callid
 * \param ptr contact record
 * \param _c contact string
 * \param _callid callid
 * \return ptr on successfull match, 0 when they not match
 */
static inline struct ucontact* contact_callid_match(unsigned int slot,
        str* _c, str *_callid) {
    ucontact_t* ptr = contact_list->slot[slot].first;

    while (ptr) {
        if ((ptr->state != CONTACT_DELAYED_DELETE) 
                && (_c->len == ptr->c.len) && (_callid->len == ptr->callid.len)
                && !memcmp(_c->s, ptr->c.s, _c->len)
                && !memcmp(_callid->s, ptr->callid.s, _callid->len)) {
            return ptr;
        }
        ptr = ptr->next;
    }
    return 0;
}

/*!
+ * \brief Match a contact record to a contact string and path
+ * \param ptr contact record
+ * \param _c contact string
+ * \param _path path
+ * \return ptr on successfull match, 0 when they not match
+ */
static inline struct ucontact* contact_path_match(unsigned int slot, str* _c, str *_path) {
    ucontact_t* ptr = contact_list->slot[slot].first;
    /* if no path is preset (in REGISTER request) or use_path is not configured
       in registrar module, default to contact_match() */
    if (_path == NULL) return contact_match(slot, _c);

    while (ptr) {
        if ((ptr->state != CONTACT_DELAYED_DELETE)
                && (_c->len == ptr->c.len) && (_path->len == ptr->path.len)
                && !memcmp(_c->s, ptr->c.s, _c->len)
                && !memcmp(_path->s, ptr->path.s, _path->len)
                && VALID_CONTACT(ptr, act_time)
                ) {
            return ptr;
        }

        ptr = ptr->next;
    }
    return 0;
}

/*!
 * \brief Get pointer to ucontact with given contact
 * \param _r record where to search the contacts
 * \param _c contact string
 * \param _callid callid
 * \param _path path
 * \param _cseq CSEQ number
 * \param _co found contact
 * \return 0 - found, 1 - not found, -1 - invalid found,
 * -2 - found, but to be skipped (same cseq) - don't forget to release_ucontact so dec. the ref counter
 */
int get_scontact(str* _c, str* _callid, str* _path, int _cseq, struct ucontact** _co) {
    unsigned int sl;
    ucontact_t* ptr;
    int with_callid = 0;
    ptr = 0;
    *_co = 0;

    sl = core_hash(_c, 0, contact_list->size);
    LM_DBG("looking for contact [%.*s] in slot %d\n", _c->len, _c->s, sl);
    get_act_time();

    lock_contact_slot_i(sl);

    switch (matching_mode) {
        case CONTACT_ONLY:
            ptr = contact_match(sl, _c);
            break;
        case CONTACT_CALLID:
            ptr = contact_callid_match(sl, _c, _callid);
            with_callid = 1;
            break;
        case CONTACT_PATH:
            ptr = contact_path_match(sl, _c, _path);
            break;
        case CONTACT_PORT_IP_ONLY:
            ptr = contact_port_ip_match(sl, _c);
            break;
        default:
            LM_CRIT("unknown matching_mode %d\n", matching_mode);
            unlock_contact_slot_i(sl);
            return -1;
    }

    if (ptr) {
        LM_DBG("have partially found a contact\n");
        /* found -> check callid and cseq */
        if (!with_callid || (_callid && ptr->callid.len == _callid->len
                && memcmp(_callid->s, ptr->callid.s, _callid->len) == 0)) {
            if (_cseq < ptr->cseq) {
                LM_DBG("cseq less than expected\n");
            }

        }
        LM_DBG("contact found p=[%p], aor:[%.*s] and contact:[%.*s], state [%d]\n", ptr, ptr->aor.len, ptr->aor.s, ptr->c.len, ptr->c.s, ptr->state);
        ref_contact_unsafe(ptr);
        *_co = ptr;
        unlock_contact_slot_i(sl); /*TODO: we probably need to ref count here..... */
        return 0;
    }
    unlock_contact_slot_i(sl);

    return 1;
}

/**
 * Deallocates memory used by a subscription.
 * \note Must be called with the lock got to avoid races
 * @param s - the ims_subscription to free
 */
void free_ims_subscription_data(ims_subscription *s) {
    int i, j, k;
    if (!s) return;
    /*	lock_get(s->lock); - must be called with the lock got */
    for (i = 0; i < s->service_profiles_cnt; i++) {
        for (j = 0; j < s->service_profiles[i].public_identities_cnt; j++) {
            if (s->service_profiles[i].public_identities[j].public_identity.s)
                shm_free(s->service_profiles[i].public_identities[j].public_identity.s);
            if (s->service_profiles[i].public_identities[j].wildcarded_psi.s)
                shm_free(s->service_profiles[i].public_identities[j].wildcarded_psi.s);

        }
        if (s->service_profiles[i].public_identities)
            shm_free(s->service_profiles[i].public_identities);

        for (j = 0; j < s->service_profiles[i].filter_criteria_cnt; j++) {
            if (s->service_profiles[i].filter_criteria[j].trigger_point) {
                for (k = 0; k < s->service_profiles[i].filter_criteria[j].trigger_point->spt_cnt; k++) {
                    switch (s->service_profiles[i].filter_criteria[j].trigger_point->spt[k].type) {
                        case IFC_REQUEST_URI:
                            if (s->service_profiles[i].filter_criteria[j].trigger_point->spt[k].request_uri.s)
                                shm_free(s->service_profiles[i].filter_criteria[j].trigger_point->spt[k].request_uri.s);
                            break;
                        case IFC_METHOD:
                            if (s->service_profiles[i].filter_criteria[j].trigger_point->spt[k].method.s)
                                shm_free(s->service_profiles[i].filter_criteria[j].trigger_point->spt[k].method.s);
                            break;
                        case IFC_SIP_HEADER:
                            if (s->service_profiles[i].filter_criteria[j].trigger_point->spt[k].sip_header.header.s)
                                shm_free(s->service_profiles[i].filter_criteria[j].trigger_point->spt[k].sip_header.header.s);
                            if (s->service_profiles[i].filter_criteria[j].trigger_point->spt[k].sip_header.content.s)
                                shm_free(s->service_profiles[i].filter_criteria[j].trigger_point->spt[k].sip_header.content.s);
                            break;
                        case IFC_SESSION_CASE:
                            break;
                        case IFC_SESSION_DESC:
                            if (s->service_profiles[i].filter_criteria[j].trigger_point->spt[k].session_desc.line.s)
                                shm_free(s->service_profiles[i].filter_criteria[j].trigger_point->spt[k].session_desc.line.s);
                            if (s->service_profiles[i].filter_criteria[j].trigger_point->spt[k].session_desc.content.s)
                                shm_free(s->service_profiles[i].filter_criteria[j].trigger_point->spt[k].session_desc.content.s);
                            break;

                    }
                }
                if (s->service_profiles[i].filter_criteria[j].trigger_point->spt)
                    shm_free(s->service_profiles[i].filter_criteria[j].trigger_point->spt);
                shm_free(s->service_profiles[i].filter_criteria[j].trigger_point);
            }
            if (s->service_profiles[i].filter_criteria[j].application_server.server_name.s)
                shm_free(s->service_profiles[i].filter_criteria[j].application_server.server_name.s);
            if (s->service_profiles[i].filter_criteria[j].application_server.service_info.s)
                shm_free(s->service_profiles[i].filter_criteria[j].application_server.service_info.s);
            if (s->service_profiles[i].filter_criteria[j].profile_part_indicator)
                shm_free(s->service_profiles[i].filter_criteria[j].profile_part_indicator);
        }
        if (s->service_profiles[i].filter_criteria)
            shm_free(s->service_profiles[i].filter_criteria);

        if (s->service_profiles[i].cn_service_auth)
            shm_free(s->service_profiles[i].cn_service_auth);

        if (s->service_profiles[i].shared_ifc_set)
            shm_free(s->service_profiles[i].shared_ifc_set);
    }
    if (s->service_profiles) shm_free(s->service_profiles);
    if (s->private_identity.s) shm_free(s->private_identity.s);
    // ul.unlock_subscription(s);
#ifdef EXTRA_DEBUG
    LM_DBG("SUBSCRIPTION LOCK %p destroyed\n", s->lock);
#endif
    lock_destroy(s->lock);
    lock_dealloc(s->lock);

    shm_free(s);

}

int compare_subscription(ims_subscription* new, ims_subscription* orig) {
    int i, j, k, l;
    LM_DBG("Comparing subscription for IMPI [%.*s]\n", orig->private_identity.len, orig->private_identity.s);
    for (i = 0; i < orig->service_profiles_cnt; i++) {
        for (j = 0; j < orig->service_profiles[i].public_identities_cnt; j++) {
            for (k = 0; k < new->service_profiles_cnt; k++) {
                for (l = 0; l < new->service_profiles[k].public_identities_cnt; l++) {
                    LM_DBG("new %.*s (%i) vs. orig %.*s (%i)\n",
                            new->service_profiles[k].public_identities[l].public_identity.len,
                            new->service_profiles[k].public_identities[l].public_identity.s,
                            new->service_profiles[k].public_identities[l].public_identity.len,
                            orig->service_profiles[i].public_identities[j].public_identity.len,
                            orig->service_profiles[i].public_identities[j].public_identity.s,
                            orig->service_profiles[i].public_identities[j].public_identity.len);

                    if (orig->service_profiles[i].public_identities[j].public_identity.len ==
                            new->service_profiles[k].public_identities[l].public_identity.len) {
                        if (memcmp(orig->service_profiles[i].public_identities[j].public_identity.s,
                                new->service_profiles[k].public_identities[l].public_identity.s,
                                new->service_profiles[k].public_identities[l].public_identity.len) == 0)
                            return 1;
                    }

                }
            }
        }
    }

    return 0;
}

/**
 * @brief update an existing impurecord. if one doesn't exist it will be created. assumes the domain is locked
 * @param _d
 * @param public_identity only used if impu_rec is null
 * @param impu_rec if passed in we use this as the record and we assume caller has already done locking on the domain...
 * @param reg_state
 * @param send_sar_on_delete
 * @param barring
 * @param is_primary
 * @param s
 * @param ccf1
 * @param ccf2
 * @param ecf1
 * @param ecf2
 * @param _r
 * @return 0 on success (domain will remain locked)
 */
int update_impurecord(struct udomain* _d, str* public_identity, impurecord_t* impu_rec, int reg_state, int send_sar_on_delete, int barring, int is_primary, ims_subscription** s, str* ccf1, str* ccf2, str* ecf1, str* ecf2, struct impurecord** _r) {
    int res;
    struct ims_subscription_s* subscription, *subs_ptr = 0;
    int leave_slot_locked = 1;
    int subscription_locked = 0;
    str private_identity = {0, 0};
    str* impu_str = public_identity;

    //make usre we have IMPU or enough data to find it...
    if (!impu_rec && (!public_identity || !public_identity->len || !public_identity->s)) {
        LM_WARN("can't call update_impurecord with no details of IMPU..n");
        return -1;
    }

    /* before we get started let's check if we already have subscription data for this impi */
    if (s && *s) {
        subs_ptr = (*s);
        res = get_subscription(&(*s)->private_identity, &subscription, leave_slot_locked); //leave slot locked in case we need to add.... don't want racing adds
        if (res != 0) {
            LM_DBG("No subscription yet for [%.*s]... adding\n", (*s)->private_identity.len, (*s)->private_identity.s);
            ref_subscription_unsafe(subs_ptr); //we reference coz we are using it - will be unreferenced later.
            add_subscription_unsafe(subs_ptr);
            unlock_subscription_slot(subs_ptr->sl);
        } else {
            //TODO: we may want to do a deep comparison of the subscription and update....
            if (compare_subscription(subs_ptr, subscription) != 0) {
                subs_ptr = subscription;
            } else {
                // Treat it as a new Subscription - it's not the same as the previous one
                ref_subscription_unsafe(subs_ptr); //we reference coz we are using it - will be unreferenced later.
                add_subscription_unsafe(subs_ptr);
                unlock_subscription_slot(subs_ptr->sl);
            }
        }
        lock_subscription(subs_ptr);
        subscription_locked = 1;
        private_identity = (*s)->private_identity;
    }

    if (impu_rec) {
        LM_DBG("We already have impurecord....\n");
        (*_r) = impu_rec;
        impu_str = &(*_r)->public_identity;
    } else {
        res = get_impurecord(_d, impu_str, _r); //return with lock on the domain
        if (res != 0) {
            if (reg_state != IMPU_NOT_REGISTERED && s) {
                LM_DBG("No existing impu record for <%.*s>.... creating new one\n", impu_str->len, impu_str->s);
                res = insert_impurecord(_d, impu_str, &private_identity, reg_state, barring, &subs_ptr, ccf1, ccf2, ecf1, ecf2, _r);
                if (res != 0) {
                    LM_ERR("Unable to insert new IMPU for <%.*s>\n", impu_str->len, impu_str->s);
                    //                    unlock_udomain(_d, impu_str);
                    goto error;
                } else {
                    //for the first time we create an IMPU we must set the primary record (we don't worry about it on updates - ignored)
                    (*_r)->is_primary = is_primary; //TODO = this should prob move to insert_impurecord fn
                    if (reg_state == IMPU_UNREGISTERED) {
                        //update unreg expiry so the unreg record is not stored 'forever'
                        (*_r)->expires = time(NULL) + unreg_validity;
                    }
                    run_ul_callbacks(NULL, UL_IMPU_INSERT, *_r, NULL);
                    if (subscription_locked) {
                        unref_subscription_unsafe(subs_ptr);
                        unlock_subscription(subs_ptr);
                    }
                    //                    unlock_udomain(_d, impu_str);
                    return 0;
                }
            } else {
                LM_DBG("no IMPU found to update and data not valid to create new one - not a problem record was probably removed as it has no contacts\n");
                if (subscription_locked) {
                    unref_subscription_unsafe(subs_ptr);
                    unlock_subscription(subs_ptr);
                }
                return 0;
            }
        }
    }

    //if we get here, we have a record to update
    LM_DBG("updating IMPU record with public identity for <%.*s>\n", impu_str->len, impu_str->s);
    (*_r)->reg_state = reg_state;
    if (reg_state == IMPU_UNREGISTERED) {
        //update unreg expiry so the unreg record is not stored 'forever'
        (*_r)->expires = time(NULL) + unreg_validity;
    }
    if (barring >= 0) (*_r)->barring = barring;

    if (send_sar_on_delete >= 0) (*_r)->send_sar_on_delete = send_sar_on_delete;

    if (ccf1) {
        if ((*_r)->ccf1.s)
            shm_free((*_r)->ccf1.s);
        STR_SHM_DUP((*_r)->ccf1, *ccf1, "SHM CCF1");
    }
    if (ccf2) {
        if ((*_r)->ccf2.s)
            shm_free((*_r)->ccf2.s);
        STR_SHM_DUP((*_r)->ccf2, *ccf2, "SHM CCF2");
    }
    if (ecf1) {
        if ((*_r)->ecf1.s)
            shm_free((*_r)->ecf1.s);
        STR_SHM_DUP((*_r)->ecf1, *ecf1, "SHM ECF1");
    }
    if (ecf2) {
        if ((*_r)->ecf2.s)
            shm_free((*_r)->ecf2.s);
        STR_SHM_DUP((*_r)->ecf2, *ecf2, "SHM ECF2");
    }

    if (subs_ptr) {
        LM_DBG("IMS subscription passed into update_impurecord\n");
        if ((*_r)->s != subs_ptr) {
            LM_DBG("new subscription for IMPU... swapping - TODO need to unref the old one...and then ref the new one\n");
            unref_subscription((*_r)->s); //different subscription which we don't have lock on yet.
            ref_subscription_unsafe(subs_ptr);
            (*_r)->s = subs_ptr;
        } else {
            LM_DBG("new subscription is the same as the old one....not doing anything");
            //check that the service profile and associated impus are in the subscription, if not, add...
            /* if (compare_subscription(subs_ptr, *s) != 0) {
                unref_subscription((*_r)->s); //different subscription which we don't have lock on yet.
                ref_subscription_unsafe(subs_ptr);
                (*_r)->s = subs_ptr;
            }   */
        }
    }

    run_ul_callbacks((*_r)->cbs, UL_IMPU_UPDATE, *_r, NULL);

    if (db_mode == WRITE_THROUGH && db_insert_impurecord(_d, &(*_r)->public_identity, (*_r)->reg_state, (*_r)->barring, &(*_r)->s, &(*_r)->ccf1, &(*_r)->ccf2, &(*_r)->ecf1, &(*_r)->ecf2, _r) != 0) {
        LM_ERR("error inserting IMPU [%.*s] into db... continuing", (*_r)->public_identity.len, (*_r)->public_identity.s);
    }

    if (subscription_locked) {
        unref_subscription_unsafe(subs_ptr);
        unlock_subscription(subs_ptr);
    }

    return 0;

out_of_memory:

    error :
    if (subscription_locked) {
        unref_subscription_unsafe(subs_ptr);
        unlock_subscription(subs_ptr);
    }

    return -1;
}

int add_impucontact_to_list(impurecord_t* impu, impu_contact_t *impucontact) {
	if (impu->linked_contacts.head == 0) {
		impucontact->prev = 0;
		impucontact->next = 0;
		impu->linked_contacts.head = impu->linked_contacts.tail = impucontact;
	} else {
		impucontact->prev = impu->linked_contacts.tail;
		impu->linked_contacts.tail->next = impucontact;
		impucontact->next = 0;
		impu->linked_contacts.tail = impucontact;
	}
	
	impu->linked_contacts.numcontacts++;
	if (impucontact->contact->is_3gpp)
		impu->linked_contacts.num3gppcontacts++;
	
	return 0;
}

int remove_impucontact_from_list(impurecord_t* impu, impu_contact_t *impucontact) {
	ucontact_t* contact = impucontact->contact;

	if (contact == impu->linked_contacts.head->contact) {
		LM_DBG("deleting head\n");
		impu->linked_contacts.head = impu->linked_contacts.head->next;
	} else if (contact == impu->linked_contacts.tail->contact) {
		LM_DBG("deleting tail\n");
		impu->linked_contacts.tail = impu->linked_contacts.tail->prev;
	} else {
		LM_DBG("deleting mid list\n");
		impucontact->prev->next = impucontact->next;
		impucontact->prev = impucontact->next->prev;
	}
	
	impu->linked_contacts.numcontacts--;
	if (impucontact->contact->is_3gpp)
		impu->linked_contacts.num3gppcontacts--;
	
	shm_free(impucontact);
	
	return 0;
}


/* link contact to impu 
    must be called with lock on domain (IMPU) as well as lock on contact_slot 
 */
int link_contact_to_impu(impurecord_t* impu, ucontact_t* contact, int write_to_db) {
	impu_contact_t *impu_contact_ptr;
    int locked;
	int space_made = 0;

	impu_contact_ptr = impu->linked_contacts.head;
	while (impu_contact_ptr) {
		if (impu_contact_ptr->contact == contact) {
			LM_DBG("contact [%p] => [%.*s] already linked to impu [%.*s]\n", contact, contact->c.len, contact->c.s, impu->public_identity.len, impu->public_identity.s);
            return 0;
		}
		impu_contact_ptr = impu_contact_ptr->next;
	}
	
    if (contact->is_3gpp && (maxcontact_behaviour > 0) && (maxcontact_3gpp > 0) && (maxcontact_3gpp < (impu->linked_contacts.num3gppcontacts + 1))) {
		LM_DBG("Need to overwrite oldest (first) 3GPP contact\n");
		
		impu_contact_ptr = impu->linked_contacts.head;
		while (impu_contact_ptr) {
				if (impu_contact_ptr->contact->is_3gpp) {
						LM_DBG("Found first 3GPP contact");
						break;
				}
				impu_contact_ptr = impu_contact_ptr->next;
		}
		
		if (impu_contact_ptr) {
			if (impu_contact_ptr == impu->linked_contacts.head) {
					impu->linked_contacts.head = impu->linked_contacts.tail = impu->linked_contacts.head->next;
			} else if (impu_contact_ptr == impu->linked_contacts.tail) {
					impu_contact_ptr->prev->next = impu_contact_ptr->next;
			} else {
					impu_contact_ptr->prev->next = impu_contact_ptr->next;
					impu_contact_ptr->next->prev = impu_contact_ptr->prev;
			}

			if (write_to_db && db_mode == WRITE_THROUGH && db_unlink_contact_from_impu(impu, impu_contact_ptr->contact) != 0) {
				LM_ERR("Failed to un-link DB contact [%.*s] from IMPU [%.*s]...continuing but db will be out of sync!\n", impu_contact_ptr->contact->c.len, impu_contact_ptr->contact->c.s, impu->public_identity.len, impu->public_identity.s);
			}

			locked = lock_try(impu_contact_ptr->contact->lock);
			if (locked == 0) {
	//                found_contact->state = CONTACT_DELAYED_DELETE;
					unref_contact_unsafe(impu_contact_ptr->contact); //we don't unref because we don't have the lock on this particular contacts contact slot and we can't take it coz of deadlock. - so let
					//a housekeeper thread do it
					locked = 1;
			} else {
							LM_ERR("Could not get lock to remove link from of contact from impu....");
							//TODO: we either need to wait and retry or we need to get another process to do this for us.... right now we will leak a contact.
			}
			if (locked == 1) {
					lock_release(impu_contact_ptr->contact->lock);
			}
			impu->linked_contacts.numcontacts--;
			impu->linked_contacts.num3gppcontacts--;
			shm_free(impu_contact_ptr);
			space_made = 1;
		} else {
			LM_WARN("strange can't find 3gpp contact to remove\n");
		}
	}
	
	if (!space_made && (maxcontact_behaviour > 0) && (maxcontact > 0) && (maxcontact < (impu->linked_contacts.numcontacts + 1))) {
        LM_DBG("Need to overwrite oldest contact\n");
		impu_contact_ptr = impu->linked_contacts.head;

		//we will remove the contact at the head of the list
		if (impu->linked_contacts.head == impu->linked_contacts.tail)
			impu->linked_contacts.head = impu->linked_contacts.tail = impu->linked_contacts.head->next;
		else 
			impu->linked_contacts.head = impu->linked_contacts.head->next;
			
		if (write_to_db && db_mode == WRITE_THROUGH && db_unlink_contact_from_impu(impu, impu_contact_ptr->contact) != 0) {
            LM_ERR("Failed to un-link DB contact [%.*s] from IMPU [%.*s]...continuing but db will be out of sync!\n", impu_contact_ptr->contact->c.len, impu_contact_ptr->contact->c.s, impu->public_identity.len, impu->public_identity.s);
        }
		
        locked = lock_try(impu_contact_ptr->contact->lock);
        if (locked == 0) {
//                found_contact->state = CONTACT_DELAYED_DELETE;
				unref_contact_unsafe(impu_contact_ptr->contact); //we don't unref because we don't have the lock on this particular contacts contact slot and we can't take it coz of deadlock. - so let
                //a housekeeper thread do it
                locked = 1;
        } else {
                        LM_ERR("Could not get lock to remove link from of contact from impu....");
                        //TODO: we either need to wait and retry or we need to get another process to do this for us.... right now we will leak a contact.
        }
        if (locked == 1) {
                lock_release(impu_contact_ptr->contact->lock);
        }
		impu->linked_contacts.numcontacts--;
		if (impu_contact_ptr->contact->is_3gpp)
				impu->linked_contacts.num3gppcontacts--;
		shm_free(impu_contact_ptr);
    } 
	
	//at this point we know we have space to add the contact;
	impu_contact_ptr = (impu_contact_t*)shm_malloc(sizeof(struct impu_contact));
	impu_contact_ptr->contact = contact;
	add_impucontact_to_list(impu, impu_contact_ptr);
	
	ref_contact_unsafe(contact);
	LM_DBG("number of contacts for IMPU [%.*s] is %d\n", impu->public_identity.len, impu->public_identity.s, impu->linked_contacts.numcontacts);
	if (write_to_db && db_mode == WRITE_THROUGH && db_link_contact_to_impu(impu, contact) != 0) {
		LM_ERR("Failed to update DB linking contact [%.*s] to IMPU [%.*s]...continuing but db will be out of sync!\n", contact->c.len, contact->c.s, impu->public_identity.len, impu->public_identity.s);
	};
	
    return 0;
}

int unlink_contact_from_impu(impurecord_t* impu, ucontact_t* contact, int write_to_db, int is_explicit) {
    impu_contact_t *impucontact;
	int locked = 0;

    LM_DBG("asked to unlink contact [%p] => [%.*s] from impu [%.*s]\n", contact, contact->c.len, contact->c.s, impu->public_identity.len, impu->public_identity.s);

	impucontact = impu->linked_contacts.head;
	
	while (impucontact) {
		if ((contact == impucontact->contact)) {
			remove_impucontact_from_list(impu, impucontact);
			if (write_to_db && db_mode == WRITE_THROUGH && (db_unlink_contact_from_impu(impu, contact) != 0)) {
				LM_ERR("Failed to un-link DB contact [%.*s] from IMPU [%.*s]...continuing but db will be out of sync!\n", contact->c.len, contact->c.s, impu->public_identity.len, impu->public_identity.s);
			}

			locked = lock_try(contact->lock);
			if (locked == 0) {
				//                found_contact->state = CONTACT_DELAYED_DELETE;
				unref_contact_unsafe(contact); //we don't unref because we don't have the lock on this particular contacts contact slot and we can't take it coz of deadlock. - so let
				//a housekeeper thread do it
				locked = 1;
			} else {
				LM_ERR("Could not get lock to remove link from of contact from impu....");
				//TODO: we either need to wait and retry or we need to get another process to do this for us.... right now we will leak a contact.
			}
			if (locked == 1) {
				lock_release(contact->lock);
			}
			LM_DBG("unlinking contact [%p] => [%.*s] from impu [%.*s]\n", 
				contact,
				contact->c.len, contact->c.s, impu->public_identity.len, impu->public_identity.s);
			break;
		}
		impucontact = impucontact->next;
	}

    return 0;
}

void ref_subscription_unsafe(ims_subscription* s) {
    LM_DBG("Reffing subscription [%.*s] - was [%d]\n", s->private_identity.len, s->private_identity.s, s->ref_count);
    s->ref_count++;
}

/**
 * @brief unref a subscription - assume slot and subscription locked!
 * @param s
 */
void unref_subscription_unsafe(ims_subscription* s) {
    int sl;

    LM_DBG("un-reffing subscription [%.*s] - was [%d]\n", s->private_identity.len, s->private_identity.s, s->ref_count);
    s->ref_count--;
    if (s->ref_count == 0) {
        if (s->sl >= 0) { //-1 as sl means the subscription was never added to the list
            sl = s->sl;
            subs_slot_rem(&ims_subscription_list->slot[sl], s);
        }
        delete_subscription(s);
        s = 0;
    }
    
}

void ref_subscription(ims_subscription* s) {
    lock_subscription(s);
    ref_subscription_unsafe(s);
    unlock_subscription(s);
}

/**
 * @brief unref subscription safely - assume no lock on subscription or subscription slot
 * @param s
 */
void unref_subscription(ims_subscription* s) {
    int ref;
    lock_subscription(s);
    ref = s->ref_count;
    unref_subscription_unsafe(s);
    if (ref > 1)
        unlock_subscription(s);
}
