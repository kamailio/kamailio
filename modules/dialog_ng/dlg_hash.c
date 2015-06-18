/*!
 * \file
 * \brief Functions related to dialog creation and searching
 * \ingroup dialog
 * Module: \ref dialog
 */

#include <stdlib.h>
#include <string.h>

#include "../../dprint.h"
#include "../../ut.h"
#include "../../lib/kmi/mi.h"
#include "dlg_timer.h"
#include "dlg_var.h"
#include "dlg_hash.h"
#include "dlg_profile.h"
#include "dlg_handlers.h"
#include "dlg_db_handler.h"

#define MAX_LDG_LOCKS  2048
#define MIN_LDG_LOCKS  2

extern int dlg_db_mode;

/*! global dialog table */
struct dlg_table *d_table = 0;
/*! global dialog out table */
static int dlg_hash_size_out = 4096;


/*!
 * \brief Reference a dialog without locking
 * \param _dlg dialog
 * \param _cnt increment for the reference counter
 */
#define ref_dlg_unsafe(_dlg,_cnt)     \
	do { \
		(_dlg)->ref += (_cnt); \
		LM_DBG("ref dlg %p with %d -> %d\n", \
			(_dlg),(_cnt),(_dlg)->ref); \
	}while(0)


/*!
 * \brief Unreference a dialog without locking
 * \param _dlg dialog
 * \param _cnt decrement for the reference counter
 * \param _d_entry dialog entry
 */
#define unref_dlg_unsafe(_dlg,_cnt,_d_entry)   \
	do { \
		(_dlg)->ref -= (_cnt); \
		LM_DBG("unref dlg %p with %d -> %d\n",\
			(_dlg),(_cnt),(_dlg)->ref);\
		if ((_dlg)->ref<0) {\
			LM_CRIT("bogus ref %d with cnt %d for dlg %p [%u:%u] "\
				"with clid '%.*s' and tags '%.*s'\n",\
				(_dlg)->ref, _cnt, _dlg,\
				(_dlg)->h_entry, (_dlg)->h_id,\
				(_dlg)->callid.len, (_dlg)->callid.s,\
				(_dlg)->from_tag.len,\
				(_dlg)->from_tag.s);\
		}\
		if ((_dlg)->ref<=0) { \
			unlink_unsafe_dlg( _d_entry, _dlg);\
			LM_DBG("ref <=0 for dialog %p\n",_dlg);\
			destroy_dlg(_dlg);\
		}\
	}while(0)

/*!
 * \brief Initialize the global dialog table
 * \param size size of the table
 * \return 0 on success, -1 on failure
 */
int init_dlg_table(unsigned int size) {
    unsigned int n;
    unsigned int i;

    d_table = (struct dlg_table*) shm_malloc
            (sizeof (struct dlg_table) +size * sizeof (struct dlg_entry));
    if (d_table == 0) {
        LM_ERR("no more shm mem (1)\n");
        goto error0;
    }

    memset(d_table, 0, sizeof (struct dlg_table));
    d_table->size = size;
    d_table->entries = (struct dlg_entry*) (d_table + 1);

    n = (size < MAX_LDG_LOCKS) ? size : MAX_LDG_LOCKS;
    for (; n >= MIN_LDG_LOCKS; n--) {
        d_table->locks = lock_set_alloc(n);
        if (d_table->locks == 0)
            continue;
        if (lock_set_init(d_table->locks) == 0) {
            lock_set_dealloc(d_table->locks);
            d_table->locks = 0;
            continue;
        }
        d_table->locks_no = n;
        break;
    }

    if (d_table->locks == 0) {
        LM_ERR("unable to allocted at least %d locks for the hash table\n",
                MIN_LDG_LOCKS);
        goto error1;
    }

    for (i = 0; i < size; i++) {
        memset(&(d_table->entries[i]), 0, sizeof (struct dlg_entry));
        d_table->entries[i].next_id = rand() % (3*size);
        d_table->entries[i].lock_idx = i % d_table->locks_no;
    }

    return 0;
error1:
    shm_free(d_table);
error0:
    return -1;
}

/*!
 * \brief free all the memory in the entry_out structure
 * \param d_entry_out structure
 * \return void
 */
static void destroy_entry_out(struct dlg_entry_out *d_entry_out) {
    struct dlg_cell_out *dlg_out;
    struct dlg_cell_out *dlg_out_tmp;
    dlg_out = d_entry_out->first;

    LM_DBG("Destroy dialog entry out\n");
    while (dlg_out) {
        //clear all dlg_out memory space
        if (dlg_out->caller_cseq.s) {
            LM_DBG("content before freeing caller cseq is [%.*s]\n", dlg_out->caller_cseq.len, dlg_out->caller_cseq.s);
            shm_free(dlg_out->caller_cseq.s);
        }

        if (dlg_out->callee_cseq.s) {
            LM_DBG("content before freeing callee cseq is [%.*s]\n", dlg_out->callee_cseq.len, dlg_out->callee_cseq.s);
            shm_free(dlg_out->callee_cseq.s);
        }

        if (dlg_out->callee_contact.s) {
            LM_DBG("content before freeing callee contact is [%.*s]\n", dlg_out->callee_contact.len, dlg_out->callee_contact.s);
            shm_free(dlg_out->callee_contact.s);
        }

        if (dlg_out->callee_route_set.s) {
            shm_free(dlg_out->callee_route_set.s);
        }
        if (dlg_out->did.s) {
            shm_free(dlg_out->did.s);
        }

        dlg_out_tmp = dlg_out->next;
        shm_free(dlg_out);

        dlg_out = dlg_out_tmp;
    }
}

/*!
 * \brief Destroy a dialog, run callbacks and free memory
 * \param dlg destroyed dialog
 */
inline void destroy_dlg(struct dlg_cell *dlg) {
    int ret = 0;
    struct dlg_var *var;

    LM_DBG("destroying dialog %p\n", dlg);

    ret = remove_dialog_timer(&dlg->tl);
    if (ret < 0) {
        LM_CRIT("unable to unlink the timer on dlg %p [%u:%u] "
                "with clid '%.*s' and tags '%.*s'\n",
                dlg, dlg->h_entry, dlg->h_id,
                dlg->callid.len, dlg->callid.s,
                dlg->from_tag.len, dlg->from_tag.s);

    } else if (ret > 0) {
        LM_DBG("removed timer for dlg %p [%u:%u] "
                "with clid '%.*s' and tags '%.*s' \n",
                dlg, dlg->h_entry, dlg->h_id,
                dlg->callid.len, dlg->callid.s,
                dlg->from_tag.len, dlg->from_tag.s);

    }

    if (dlg_db_mode)
    	remove_dialog_in_from_db(dlg);

    LM_DBG("About to run dlg callback for destroy\n");
    run_dlg_callbacks(DLGCB_DESTROY, dlg, NULL, NULL, DLG_DIR_NONE, 0);
    LM_DBG("DONE: About to run dlg callback for destroy\n");
    if (dlg == get_current_dlg_pointer())
        reset_current_dlg_pointer();

    if (dlg->cbs.first)
        destroy_dlg_callbacks_list(dlg->cbs.first);

    if (dlg->profile_links)
        destroy_linkers(dlg->profile_links);


    if (dlg->from_tag.s)
        shm_free(dlg->from_tag.s);

    if (dlg->first_req_cseq.s)
        shm_free(dlg->first_req_cseq.s);

    if (dlg->toroute_name.s)
        shm_free(dlg->toroute_name.s);

    if (dlg->did.s)
        shm_free(dlg->did.s);

    if (dlg->caller_route_set.s)
        shm_free(dlg->caller_route_set.s);

    if (dlg->caller_contact.s)
        shm_free(dlg->caller_contact.s);

    while (dlg->vars) {

        var = dlg->vars;
        dlg->vars = dlg->vars->next;
        shm_free(var->key.s);
        shm_free(var->value.s);
        shm_free(var);
    }

    if (&(dlg->dlg_entry_out)) {
        lock_get(dlg->dlg_out_entries_lock);
        destroy_entry_out(&(dlg->dlg_entry_out));
        lock_release(dlg->dlg_out_entries_lock);
    }

    lock_destroy(dlg->dlg_out_entries_lock);
    lock_dealloc(dlg->dlg_out_entries_lock);

    shm_free(dlg);

    dlg = 0;

}

/*!
 * \brief Destroy the global dialog table
 */
void destroy_dlg_table(void) {
    struct dlg_cell *dlg, *l_dlg;
    unsigned int i;

    if (d_table == 0)
        return;

    if (d_table->locks) {
        lock_set_destroy(d_table->locks);
        lock_set_dealloc(d_table->locks);
    }

    for (i = 0; i < d_table->size; i++) {
        dlg = d_table->entries[i].first;
        while (dlg) {
            l_dlg = dlg;
            dlg = dlg->next;
            destroy_dlg(l_dlg);
        }

    }

    shm_free(d_table);
    d_table = 0;

    return;
}

/*!
 * \brief Create a new dialog structure for a SIP dialog
 * \param callid dialog callid
 * \param from_uri dialog from uri
 * \param to_uri dialog to uri
 * \param from_tag dialog from tag
 * \param req_uri dialog r-uri
 * \return created dialog structure on success, NULL otherwise
 */
struct dlg_cell* build_new_dlg(str *callid, str *from_uri, str *from_tag, str *req_uri) {
    struct dlg_cell *dlg;
    int len;
    char *p;

    len = sizeof (struct dlg_cell) +callid->len + from_uri->len + req_uri->len;
    dlg = (struct dlg_cell*) shm_malloc(len);
    if (dlg == 0) {
        LM_ERR("no more shm mem (%d)\n", len);
        return 0;
    }

    memset(dlg, 0, len);

    dlg->dlg_out_entries_lock = lock_alloc();
    if (dlg->dlg_out_entries_lock == NULL) {
        LM_ERR("Cannot allocate lock for dlg out entries. Aborting...\n");
        shm_free(dlg);
        return 0;
    } else {
        if (lock_init(dlg->dlg_out_entries_lock) == NULL) {
            LM_ERR("Cannot init the lock for dlg out entries. Aborting...\n");
            lock_destroy(dlg->dlg_out_entries_lock);
            lock_dealloc(dlg->dlg_out_entries_lock);
            shm_free(dlg);
            return 0;
        }
    }

    dlg->state = DLG_STATE_UNCONFIRMED;

    dlg->h_entry = core_hash(callid, 0, d_table->size);
    LM_DBG("new dialog on hash %u\n", dlg->h_entry);

    p = (char*) (dlg + 1);

    dlg->callid.s = p;
    dlg->callid.len = callid->len;
    memcpy(p, callid->s, callid->len);
    p += callid->len;

    dlg->from_uri.s = p;
    dlg->from_uri.len = from_uri->len;
    memcpy(p, from_uri->s, from_uri->len);
    p += from_uri->len;

    dlg->req_uri.s = p;
    dlg->req_uri.len = req_uri->len;
    memcpy(p, req_uri->s, req_uri->len);
    p += req_uri->len;

    if (p != (((char*) dlg) + len)) {
        LM_CRIT("buffer overflow\n");
        shm_free(dlg);
        return 0;
    }

    return dlg;
}

/*!
 * \brief Set the leg information for an existing dialog
 * \param dlg dialog
 * \param tag from tag or to tag
 * \param rr record-routing information
 * \param contact caller or callee contact
 * \param cseq CSEQ of caller or callee
 * \param leg must be either DLG_CALLER_LEG, or DLG_CALLEE_LEG
 * \return 0 on success, -1 on failure
 */
int dlg_set_leg_info(struct dlg_cell *dlg, str* tag, str *rr, str *contact,
        str *cseq, struct socket_info *bind_addr, unsigned int leg) {

    if (!dlg) {
        return -1;
    }

    //create new dlg_out entry
    struct dlg_entry_out *d_entry_out = &(dlg->dlg_entry_out);
    struct dlg_cell_out *dlg_out;

    if (leg == DLG_CALLER_LEG) {
        if (tag->len > 0) {
            dlg->from_tag.s = (char*) shm_malloc(tag->len);
            if (!dlg->from_tag.s) {
                LM_ERR("no more shm_mem\n");
                return -1;
            }
            memcpy(dlg->from_tag.s, tag->s, tag->len);
            dlg->from_tag.len = tag->len;
        }
        if (contact->len > 0) {
            dlg->caller_contact.s = (char*) shm_malloc(contact->len);
            if (!dlg->caller_contact.s) {
                LM_ERR("no more shm_mem\n");
                return -1;
            }
            memcpy(dlg->caller_contact.s, contact->s, contact->len);
            dlg->caller_contact.len = contact->len;
        }
        if (rr->len > 0) {
            dlg->caller_route_set.s = (char*) shm_malloc(rr->len);
            if (!dlg->caller_route_set.s) {
                LM_ERR("no more shm_mem\n");
                return -1;
            }
            memcpy(dlg->caller_route_set.s, rr->s, rr->len);
            dlg->caller_route_set.len = rr->len;
        }
        if (cseq->len > 0) {
            dlg->first_req_cseq.s = (char*) shm_malloc(cseq->len);
            if (!dlg->first_req_cseq.s) {
                LM_ERR("no more shm_mem\n");
                return -1;
            }
            memcpy(dlg->first_req_cseq.s, cseq->s, cseq->len);
            dlg->first_req_cseq.len = cseq->len;
        }
    } else {
        /* this is the callee side so we need to find the dialog_out entry with the correct to_tag
           and assign caller and callee cseq, callee contact, callee route_set
         */
        if (d_entry_out) {
            lock_get(dlg->dlg_out_entries_lock);
            dlg_out = d_entry_out->first;
            while (dlg_out) {
                LM_DBG("Searching out dialog with to_tag '%.*s' (looking for %.*s\n", dlg_out->to_tag.len, dlg_out->to_tag.s, tag->len, tag->s);

                if (dlg_out->to_tag.len == tag->len && memcmp(dlg_out->to_tag.s, tag->s, dlg_out->to_tag.len) == 0) {
                    LM_DBG("Found dialog_out entry with correct to_tag - updating leg info\n");

                    if (contact->len > 0) {
                        dlg_out->callee_contact.s = (char*) shm_malloc(contact->len);
                        if (!dlg_out->callee_contact.s) {
                            LM_ERR("no more shm mem\n");
                            return -1; //if we're out of mem we dont really care about cleaning up - prob going to crash anyway
                        }
                        dlg_out->callee_contact.len = contact->len;
                        memcpy(dlg_out->callee_contact.s, contact->s, contact->len);
                    }
                    if (rr->len > 0) {
                        dlg_out->callee_route_set.s = (char*) shm_malloc(rr->len);
                        if (!dlg_out->callee_route_set.s) {
                            LM_ERR("no more shm mem\n");
                            return -1; //if we're out of mem we dont really care about cleaning up - prob going to crash anyway
                        }
                        dlg_out->callee_route_set.len = rr->len;
                        memcpy(dlg_out->callee_route_set.s, rr->s, rr->len);
                    }
                    if (cseq->len > 0) {
                        dlg_out->callee_cseq.s = (char*) shm_malloc(cseq->len);
                        dlg_out->caller_cseq.s = (char*) shm_malloc(cseq->len);
                        if (!dlg_out->callee_cseq.s || !dlg_out->caller_cseq.s) {
                            LM_ERR("no more shm mem\n");
                            return -1; //if we're out of mem we dont really care about cleaning up - prob going to crash anyway
                        }
                        dlg_out->caller_cseq.len = cseq->len;
                        memcpy(dlg_out->caller_cseq.s, cseq->s, cseq->len);
                        dlg_out->callee_cseq.len = cseq->len;
                        memcpy(dlg_out->callee_cseq.s, cseq->s, cseq->len);
                    }
                    dlg_out->callee_bind_addr = bind_addr;
                }
                dlg_out = dlg_out->next;
            }
            lock_release(dlg->dlg_out_entries_lock);
        } else {
            LM_ERR("This dialog has no dialog out entries\n");
            return -1;
        }
    }
    return 0;
}

/*!
 * \brief Create a new dialog out structure for a SIP dialog
 * \param to_tag - dialog to_tag
 * \return created dlg_out structure on success, NULL otherwise
 */
struct dlg_cell_out* build_new_dlg_out(struct dlg_cell *dlg, str* to_uri, str* to_tag) {

    struct dlg_cell_out *dlg_out;
    int len;
    char *p;

    //len = sizeof (struct dlg_cell_out) +dlg->did.len + to_tag->len + to_uri->len;
    len = sizeof (struct dlg_cell_out) +to_tag->len + to_uri->len;

    dlg_out = (struct dlg_cell_out*) shm_malloc(len);
    if (dlg_out == 0) {
        LM_ERR("no more shm mem (%d)\n", len);
        return 0;
    }
    memset(dlg_out, 0, len);

    dlg_out->h_entry = core_hash(to_tag, 0, dlg_hash_size_out);
    LM_DBG("new dialog_out on hash %u\n", dlg_out->h_entry);

    p = (char*) (dlg_out + 1);

    //dlg_out->did.s = p;
    //dlg_out->did.len = dlg->did.len;
    //memcpy(p, dlg->did.s, dlg->did.len);
    //p += dlg->did.len;

    dlg_out->to_uri.s = p;
    dlg_out->to_uri.len = to_uri->len;
    memcpy(p, to_uri->s, to_uri->len);
    p += to_uri->len;

    dlg_out->to_tag.s = p;
    dlg_out->to_tag.len = to_tag->len;
    memcpy(p, to_tag->s, to_tag->len);
    p += to_tag->len;

    if (p != (((char*) dlg_out) + len)) {
        LM_CRIT("buffer overflow\n");
        shm_free(dlg_out);

        return 0;
    }

    //did might be updated (check update_did_dlg_out) if there is a concurrent call  -therefore this should not be done as single block of memory
    //so Richard editted this to not have did in the single block of memory
    if (dlg->did.len > 0) {
        dlg_out->did.s = (char*) shm_malloc(dlg->did.len);
        if (!dlg_out->did.s) {
            LM_ERR("no more shm_mem\n");
            return 0;
        }
        memcpy(dlg_out->did.s, dlg->did.s, dlg->did.len);
        dlg_out->did.len = dlg->did.len;
    }




    return dlg_out;
}

/*!
 * \brief Free the memory for a dlg_out cell
 * \param dlg_out structure
 * \return void
 */
void free_dlg_out_cell(struct dlg_cell_out *dlg_out) {
    if (dlg_out->callee_contact.s)
        shm_free(dlg_out->callee_contact.s);
    if (dlg_out->callee_cseq.s)
        shm_free(dlg_out->callee_cseq.s);
    if (dlg_out->callee_route_set.s)
        shm_free(dlg_out->callee_route_set.s);
    if (dlg_out->caller_cseq.s)
        shm_free(dlg_out->caller_cseq.s);

    //Richard removed this - it is free-ed two lines above!!??!
    //if (dlg_out->callee_route_set.s)
    //    shm_free(dlg_out->callee_route_set.s);

    //Richard added this as the did is now malloc-ed separately and not as a single concurrent block (as the totag etc. are)
    if (dlg_out->did.s)
        shm_free(dlg_out->did.s);


    shm_free(dlg_out);
}

/*!
 * \brief Remove dlg_out entry identified by to_tag from dlg structure
 * \param dlg structure
 * \param dlg_out to_tag
 * \return void
 */
void dlg_remove_dlg_out_tag(struct dlg_cell *dlg, str *to_tag) {

    lock_get(dlg->dlg_out_entries_lock);
    struct dlg_entry_out *d_entry_out = &(dlg->dlg_entry_out);
    struct dlg_cell_out *pdlg_out = d_entry_out->first;
    struct dlg_cell_out *tmpdlg = 0;

    int only = 0;

    while (pdlg_out) {
        if (pdlg_out->deleted) {
            LM_DBG("Found dlg_out to remove\n");
            if (pdlg_out->prev) {
                pdlg_out->prev->next = pdlg_out->next;
            } else {
                //assume that we are first
                if (pdlg_out->next) {
                    d_entry_out->first = pdlg_out->next;
                    pdlg_out->next->prev = 0;
                } else {
                    LM_ERR("dlg out entry has prev set to null and next set to null too\n");
                    only = 1;
                }
            }
            if (pdlg_out->next) {
                pdlg_out->next->prev = pdlg_out->prev;
            } else {
                //we are likely the last
                if (pdlg_out->prev) {
                    d_entry_out->last = pdlg_out->prev;
                } else {
                    LM_ERR("dlg out next is NULL and so is prev");
                    only = 1;
                }
            }
            tmpdlg = pdlg_out->next;
            free_dlg_out_cell(pdlg_out);
            pdlg_out = tmpdlg;

            if (only) {
                d_entry_out->last = 0;
                d_entry_out->first = 0;
            }

        } else {
            LM_DBG("Not deleting dlg_out as it is not set to deleted\n");
            pdlg_out = pdlg_out->next;
        }

    }
    lock_release(dlg->dlg_out_entries_lock);

}

/*!
 * \brief Remove all dlg_out entries from dlg structure expect that identified as dlg_do_not_remove
 * \param dlg_out cell - structure to not remove
 * \param mark_only - 1 then only mark for deletion, if 0 then delete
 * \return void
 */

void dlg_remove_dlg_out(struct dlg_cell_out *dlg_out_do_not_remove, struct dlg_cell *dlg, int only_mark) {

    lock_get(dlg->dlg_out_entries_lock);
    //get dlg_out_entry list from dlg
    struct dlg_entry_out *d_entry_out = &(dlg->dlg_entry_out);
    struct dlg_cell_out *dlg_out;

    //check if list is empty
    if ((d_entry_out->first == d_entry_out->last) && (d_entry_out->first == 0)) {
        LM_DBG("There are no dlg_out entries\n");
        lock_release(dlg->dlg_out_entries_lock);
        return;
    }

    dlg_out = d_entry_out->first;
    LM_DBG("Scanning dlg_entry_out list for dlg_out entry with did: [%s]", dlg->did.s);
    //run through the list and for each dlg_out_entry:
    while (dlg_out) {

        //check if it is the dlg_out that we don't want to remove (compare the to-tags)
        if (dlg_out->to_tag.len == dlg_out_do_not_remove->to_tag.len &&
                memcmp(dlg_out->to_tag.s, dlg_out_do_not_remove->to_tag.s, dlg_out->to_tag.len) == 0) {
            LM_DBG("This is the dlg_out not to be removed!\n");
        } else {
            //check if this the last entry in the entry_table
            if ((d_entry_out->first == d_entry_out->last)) {
                //we shouldnt ever get here
                LM_DBG("This is the last dlg_out_entry in the dlg_entries_out\n");
                //this is the last then set entry_out-> first and entry_out->last to zero
                dlg->dlg_entry_out.first = dlg->dlg_entry_out.last = 0;
            } else {
                if (!only_mark) {
                    LM_DBG("Deleteing dlg out structure\n");
                    if (dlg_out->prev) {
                        dlg_out->prev->next = dlg_out->next;
                    }

                    //make the next->previous dlg_out point to the previous of this dlg_out
                    //do not do this if this is the last entry in the list as the next struct is null
                    if (dlg_out->next) {
                        dlg_out->next->prev = dlg_out->prev;
                    }

                    free_dlg_out_cell(dlg_out);
                } else {
                    LM_DBG("Marking dlg_out structure for deletion - it should be deleted by tm callback instead to_tag: %.*s\n", dlg_out->to_tag.len, dlg_out->to_tag.s);
                    dlg_out->deleted = 1;
                }
            }
        }
        dlg_out = dlg_out->next;
    }
    lock_release(dlg->dlg_out_entries_lock);
}

/*!
 * \brief Update or set the CSEQ for an existing dialog
 * \param dlg dialog
 * \param leg must be either DLG_CALLER_LEG, or DLG_CALLEE_LEG
 * \param cseq CSEQ of caller or callee
 * \return 0 on success, -1 on failure
 */
int dlg_update_cseq(struct dlg_cell * dlg, unsigned int leg, str *cseq, str *to_tag) {
    LM_DBG("trying to update cseq with seq [%.*s]\n", cseq->len, cseq->s);


    //Runs through the dlg_oput entries finds the one that matches the to_tag and updates the callee or caller cseq accordingly
    struct dlg_entry_out *d_entry_out = &(dlg->dlg_entry_out);
    struct dlg_cell_out *dlg_out;
    dlg_out = d_entry_out->first;
    if (to_tag) {

        //compare the to_tag passed parameter to all the dlg_out to_tag entry of the dlg parameter  (There could be multiple)
        while (dlg_out) {

            if (dlg_out->to_tag.len == to_tag->len && memcmp(dlg_out->to_tag.s, to_tag->s, dlg_out->to_tag.len) == 0) {
                //this parameter matches we have found the dlg_out to update the cseq

                if (leg == DLG_CALLER_LEG) {
                    //update caller cseq
                    if (dlg_out->caller_cseq.s) {
                        if (dlg_out->caller_cseq.len < cseq->len) {
                            shm_free(dlg_out->caller_cseq.s);
                            dlg_out->caller_cseq.s = (char*) shm_malloc(cseq->len);
                            if (dlg_out->caller_cseq.s == NULL)
                                goto error;
                            dlg_out->caller_cseq.len = cseq->len;
                            memcpy(dlg_out->caller_cseq.s, cseq->s, cseq->len);
                        }
                    } else {
                        dlg_out->caller_cseq.s = (char*) shm_malloc(cseq->len);
                        if (dlg_out->caller_cseq.s == NULL)
                            goto error;

                        dlg_out->caller_cseq.len = cseq->len;
                        memcpy(dlg_out->caller_cseq.s, cseq->s, cseq->len);
                    }

                } else if (leg == DLG_CALLEE_LEG) {
                    //update callee cseq
                    if (dlg_out->callee_cseq.s) {
                        if (dlg_out->callee_cseq.len < cseq->len) {
                            shm_free(dlg_out->callee_cseq.s);
                            dlg_out->callee_cseq.s = (char*) shm_malloc(cseq->len);
                            if (dlg_out->callee_cseq.s == NULL)
                                goto error;

                            dlg_out->callee_cseq.len = cseq->len;
                            memcpy(dlg_out->callee_cseq.s, cseq->s, cseq->len);
                        }
                    } else {
                        dlg_out->callee_cseq.s = (char*) shm_malloc(cseq->len);
                        if (dlg_out->callee_cseq.s == NULL)
                            goto error;

                        dlg_out->callee_cseq.len = cseq->len;
                        memcpy(dlg_out->callee_cseq.s, cseq->s, cseq->len);
                    }

                }
            }
            dlg_out = dlg_out->next;
        }

    }
    return 0;
error:
    LM_ERR("not more shm mem\n");
    return -1;
}

/*!
 * \brief Update or set the CSEQ for an existing dialog
 * \param dlg dialog
 * \param leg must be either DLG_CALLER_LEG, or DLG_CALLEE_LEG
 * \param cseq CSEQ of caller or callee
 * \return 0 on success, -1 on failure
 */
int dlg_update_contact(struct dlg_cell * dlg, unsigned int leg, str *contact, str *to_tag) {
    LM_DBG("trying to update contact with contact [%.*s]\n", contact->len, contact->s);

    //Runs through the dlg_oput entries finds the one that matches the to_tag and updates the callee or caller contact accordingly
    struct dlg_entry_out *d_entry_out = &(dlg->dlg_entry_out);
    struct dlg_cell_out *dlg_out;
    dlg_out = d_entry_out->first;

    if (leg == DLG_CALLER_LEG) {
        //update caller contact
        if (dlg->caller_contact.s) {
            if (dlg->caller_contact.len < contact->len) {
                shm_free(dlg->caller_contact.s);
                dlg->caller_contact.s = (char*) shm_malloc(contact->len);
                if (dlg->caller_contact.s == NULL)
                    goto error;
                dlg->caller_contact.len = contact->len;
                memcpy(dlg->caller_contact.s, contact->s, contact->len);
            }
        } else {
            dlg->caller_contact.s = (char*) shm_malloc(contact->len);
            if (dlg->caller_contact.s == NULL)
                goto error;

            dlg->caller_contact.len = contact->len;
            memcpy(dlg->caller_contact.s, contact->s, contact->len);
        }
    }
    if (leg == DLG_CALLEE_LEG) {
        //update callee contact
        if (!to_tag) {
            LM_ERR("No to tag to identify dlg_out\n");
            return -1;
        }
        //compare the to_tag passed parameter to all the dlg_out to_tag entry of the dlg parameter  (There could be multiple)
        while (dlg_out) {

            if (dlg_out->to_tag.len == to_tag->len && memcmp(dlg_out->to_tag.s, to_tag->s, dlg_out->to_tag.len) == 0) {
                //this parameter matches we have found the dlg_out to update the callee contact

                //update callee contact
                if (dlg_out->callee_contact.s) {
                    if (dlg_out->callee_contact.len < contact->len) {
                        shm_free(dlg_out->callee_contact.s);
                        dlg_out->callee_contact.s = (char*) shm_malloc(contact->len);
                        if (dlg_out->callee_contact.s == NULL)
                            goto error;

                        dlg_out->callee_contact.len = contact->len;
                        memcpy(dlg_out->callee_contact.s, contact->s, contact->len);
                    }
                } else {
                    dlg_out->callee_contact.s = (char*) shm_malloc(contact->len);
                    if (dlg_out->callee_contact.s == NULL)
                        goto error;

                    dlg_out->callee_contact.len = contact->len;
                    memcpy(dlg_out->callee_contact.s, contact->s, contact->len);
                }
            }
            dlg_out = dlg_out->next;
        }
    }
    return 0;
error:
    LM_ERR("not more shm mem\n");
    return -1;
}

/*!
 * \brief Lookup a dialog in the global list
 *
 * Note that the caller is responsible for decrementing (or reusing)
 * the reference counter by one again iff a dialog has been found.
 * \param h_entry number of the hash table entry
 * \param h_id id of the hash table entry
 * \return dialog structure on success, NULL on failure
 */
struct dlg_cell * lookup_dlg(unsigned int h_entry, unsigned int h_id) {
    struct dlg_cell *dlg;
    struct dlg_entry *d_entry;

    if (h_entry >= d_table->size)
        goto not_found;

    d_entry = &(d_table->entries[h_entry]);

    dlg_lock(d_table, d_entry);

    for (dlg = d_entry->first; dlg; dlg = dlg->next) {
        if (dlg->h_id == h_id) {
            ref_dlg_unsafe(dlg, 1);
            dlg_unlock(d_table, d_entry);
            LM_DBG("dialog id=%u found on entry %u\n", h_id, h_entry);
            return dlg;
        }
    }

    dlg_unlock(d_table, d_entry);
not_found:
    LM_DBG("no dialog id=%u found on entry %u\n", h_id, h_entry);

    return 0;
}

/*!
 * \brief Helper function to get a dialog corresponding to a SIP message
 * \see get_dlg
 * \param callid callid
 * \param ftag from tag
 * \param ttag to tag
 * \param dir direction
 * \return dialog structure on success, NULL on failure
 */
static inline struct dlg_cell * internal_get_dlg(unsigned int h_entry,
        str *callid, str *ftag, str *ttag, unsigned int *dir) {
    struct dlg_cell *dlg;
    struct dlg_entry *d_entry;

    d_entry = &(d_table->entries[h_entry]);

    dlg_lock(d_table, d_entry);

    for (dlg = d_entry->first; dlg; dlg = dlg->next) {
        /* Check callid / fromtag / totag */
        if (match_dialog(dlg, callid, ftag, ttag, dir) == 1) {
            ref_dlg_unsafe(dlg, 1);
            dlg_unlock(d_table, d_entry);
            return dlg;
        }
    }

    dlg_unlock(d_table, d_entry);

    return 0;
}

/*!
 * \brief Get dialog that correspond to CallId, From Tag and To Tag
 *
 * Get dialog that correspond to CallId, From Tag and To Tag.
 * See RFC 3261, paragraph 4. Overview of Operation:
 * "The combination of the To tag, From tag, and Call-ID completely
 * defines a peer-to-peer SIP relationship between [two UAs] and is
 * referred to as a dialog."
 * Note that the caller is responsible for decrementing (or reusing)
 * the reference counter by one again iff a dialog has been found.
 * \param callid callid
 * \param ftag from tag
 * \param ttag to tag
 * \param dir direction
 * \return dialog structure on success, NULL on failure
 */
struct dlg_cell * get_dlg(str *callid, str *ftag, str *ttag, unsigned int *dir) {
    struct dlg_cell *dlg;

    if ((dlg = internal_get_dlg(core_hash(callid, 0,
            d_table->size), callid, ftag, ttag, dir)) == 0 &&
            (dlg = internal_get_dlg(core_hash(callid, ttag->len
            ? ttag : 0, d_table->size), callid, ftag, ttag, dir)) == 0) {
        LM_DBG("no dialog callid='%.*s' found\n", callid->len, callid->s);

        return 0;
    }
    return dlg;
}

/*!
 * \brief Link a dialog structure
 * \param dlg dialog
 * \param n extra increments for the reference counter
 */
void link_dlg_out(struct dlg_cell *dlg, struct dlg_cell_out *dlg_out, int n) {
    LM_DBG("Start: link_dlg_out\n");

    lock_get(dlg->dlg_out_entries_lock);
    struct dlg_entry_out *d_entry_out = &(dlg->dlg_entry_out);

    if ((d_entry_out->first == d_entry_out->last) && (d_entry_out->first == 0)) {
        //adding first out dialog
        LM_DBG("Adding first dlg_out structure\n");
        d_entry_out->first = dlg_out;
        d_entry_out->last = dlg_out;
    } else {
        LM_DBG("Adding new dlg_out structure\n");
        dlg_out->prev = d_entry_out->last;
        dlg_out->next = 0;

        d_entry_out->last->next = dlg_out;
        d_entry_out->last = dlg_out;
    }

    lock_release(dlg->dlg_out_entries_lock);

    LM_DBG("Done: link_dlg_out\n");
    return;
}

/*!
 * \brief Link a dialog structure
 * \param dlg dialog
 * \param n extra increments for the reference counter
 */
void link_dlg(struct dlg_cell *dlg, int n) {
    struct dlg_entry *d_entry;

    LM_DBG("Linking new dialog with h_entry: %u", dlg->h_entry);
    d_entry = &(d_table->entries[dlg->h_entry]);

    dlg_lock(d_table, d_entry);

    dlg->h_id = d_entry->next_id++;
    if (d_entry->first == 0) {
        d_entry->first = d_entry->last = dlg;
    } else {
        d_entry->last->next = dlg;
        dlg->prev = d_entry->last;
        d_entry->last = dlg;
    }

    ref_dlg_unsafe(dlg, 1 + n);

    dlg_unlock(d_table, d_entry);

    return;
}

/*!
 * \brief Refefence a dialog with locking
 * \see ref_dlg_unsafe
 * \param dlg dialog
 * \param cnt increment for the reference counter
 */
void ref_dlg(struct dlg_cell *dlg, unsigned int cnt) {

    struct dlg_entry *d_entry;

    d_entry = &(d_table->entries[dlg->h_entry]);

    dlg_lock(d_table, d_entry);
    ref_dlg_unsafe(dlg, cnt);
    dlg_unlock(d_table, d_entry);
}

/*!
 * \brief Unreference a dialog with locking
 * \see unref_dlg_unsafe
 * \param dlg dialog
 * \param cnt decrement for the reference counter
 */
void unref_dlg(struct dlg_cell *dlg, unsigned int cnt) {

    struct dlg_entry *d_entry;

    d_entry = &(d_table->entries[dlg->h_entry]);

    dlg_lock(d_table, d_entry);
    unref_dlg_unsafe(dlg, cnt, d_entry);
    dlg_unlock(d_table, d_entry);
}

/*!
 * Small logging helper functions for next_state_dlg.
 * \param event logged event
 * \param dlg dialog data
 * \see next_state_dlg
 */
static inline void log_next_state_dlg(const int event, const struct dlg_cell * dlg) {

    LM_CRIT("bogus event %d in state %d for dlg %p [%u:%u] with clid '%.*s' and tags "
            "'%.*s'\n", event, dlg->state, dlg, dlg->h_entry, dlg->h_id,
            dlg->callid.len, dlg->callid.s,
            dlg->from_tag.len, dlg->from_tag.s);
}

/*!
 * \brief Update a dialog state according a event and the old state
 *
 * This functions implement the main state machine that update a dialog
 * state according a processed event and the current state. If necessary
 * it will delete the processed dialog. The old and new state are also
 * saved for reference.
 * \param dlg updated dialog
 * \param event current event
 * \param old_state old dialog state
 * \param new_state new dialog state
 * \param unref set to 1 when the dialog was deleted, 0 otherwise
 */
void next_state_dlg(struct dlg_cell *dlg, int event,
        int *old_state, int *new_state, int *unref, str * to_tag) {
    struct dlg_entry *d_entry;
    


    d_entry = &(d_table->entries[dlg->h_entry]);

    *unref = 0;

    dlg_lock(d_table, d_entry);

    *old_state = dlg->state;

    struct dlg_entry_out *d_entry_out = &(dlg->dlg_entry_out);
    struct dlg_cell_out *dlg_out;
    dlg_out = d_entry_out->first;
    int found = -1;
    int delete = 1;

    switch (event) {
        case DLG_EVENT_TDEL:
            switch (dlg->state) {
                case DLG_STATE_UNCONFIRMED:
                case DLG_STATE_EARLY:
		    if (to_tag) {
                        LM_DBG("Going to check if there is another active branch - we only change state to DELETED if there are no other active branches\n");
                        while (dlg_out) {
                            if (dlg_out->to_tag.len != to_tag->len || memcmp(dlg_out->to_tag.s, to_tag->s, dlg_out->to_tag.len) != 0) {
				if(dlg_out->deleted != 1) {
				    LM_DBG("Found a dlg_out that is not for this event and is not in state deleted, therefore there is another active branch\n");
				    delete = 0;
                                    //we should delete this dlg_out tho...
                                    dlg_out->deleted=1;
				}
                            }
                            dlg_out = dlg_out->next;
                        }
                    } 
		    if(delete) {
			dlg->state = DLG_STATE_DELETED;
			unref_dlg_unsafe(dlg, 1, d_entry);
			*unref = 1;
		    }
                    break;
                case DLG_STATE_CONFIRMED:
                    unref_dlg_unsafe(dlg, 1, d_entry);
                    break;
                case DLG_STATE_DELETED:
                    *unref = 1;
                    break;
                default:
                    log_next_state_dlg(event, dlg);
            }
            break;
        case DLG_EVENT_RPL1xx:
            switch (dlg->state) {
                case DLG_STATE_UNCONFIRMED:
                case DLG_STATE_EARLY:
		    dlg->state = DLG_STATE_EARLY;
                    break;
                default:
                    log_next_state_dlg(event, dlg);
            }
            break;
        case DLG_EVENT_RPL3xx:
            switch (dlg->state) {
                case DLG_STATE_UNCONFIRMED:
                case DLG_STATE_EARLY:
		    if (to_tag) {
                        LM_DBG("Going to check if there is another active branch - we only change state to DELETED if there are no other active branches\n");
                        while (dlg_out) {
                            if (dlg_out->to_tag.len != to_tag->len || memcmp(dlg_out->to_tag.s, to_tag->s, dlg_out->to_tag.len) != 0) {
				if(dlg_out->deleted != 1) {
				    LM_DBG("Found a dlg_out that is not for this event and is not in state deleted, therefore there is another active branch\n");
				    delete = 0;
				}
                            }
                            dlg_out = dlg_out->next;
                        }
                    } 
		    if(delete) {
			dlg->state = DLG_STATE_DELETED;
			*unref = 1;
		    }
                    break;
                default:
                    log_next_state_dlg(event, dlg);
            }
            break;
        case DLG_EVENT_RPL2xx:
            switch (dlg->state) {
                case DLG_STATE_DELETED:
                    if (dlg->dflags & DLG_FLAG_HASBYE) {
                        LM_CRIT("bogus event %d in state %d (with BYE) "
                                "for dlg %p [%u:%u] with clid '%.*s' and tags '%.*s' \n",
                                event, dlg->state, dlg, dlg->h_entry, dlg->h_id,
                                dlg->callid.len, dlg->callid.s,
                                dlg->from_tag.len, dlg->from_tag.s);
                        break;
                    }
                    ref_dlg_unsafe(dlg, 1);
                case DLG_STATE_UNCONFIRMED:
                case DLG_STATE_EARLY:
                    dlg->state = DLG_STATE_CONFIRMED;
                    //TODO: check that the callbacks for confirmed are run
                    break;
                case DLG_STATE_CONFIRMED:
                    //check the to_tag passed parameter exists
                    if (to_tag) {
                        //compare the to_tag passed parameter to the dlg_out to_tag entry of the dlg parameter  (There should be only 1 dlg_out entry)

                        if (dlg_out->to_tag.len == to_tag->len && memcmp(dlg_out->to_tag.s, to_tag->s, dlg_out->to_tag.len) == 0) {
                            //this parameter matches the existing dlg_out and is therefore a retransmission, so break
                            break;
                        } else {

                            LM_ERR("It looks like this is a concurrently confirmed call!!\n");
                            LM_ERR("Error checking now so not putting into DLG_STATE_CONCURRENTLY_CONFIRMED\n");

                            LM_ERR("This is event DLG_EVENT_RPL2XX and the current dlg state is DLG_STATE_CONFIRMED\n");
                            LM_ERR("There should only be one dlg out here as the state is CONFIRMED but we are checking anyway!\n");
                            LM_ERR("To tag passed in the 2XX: [%.*s]", to_tag->len, to_tag->s);
                            LM_ERR("Now printing dlgouts totags - there should be only one!\n");
                            while (dlg_out) {
                                LM_ERR("dlg_out to_tag: [%.*s]\n", dlg_out->to_tag.len, dlg_out->to_tag.s);
                                dlg_out = dlg_out->next;
                            }

                            //The parameter does not match so this is a concurrently confirmed call
                            //dlg->state = DLG_STATE_CONCURRENTLY_CONFIRMED;
                        }
                    } else {
                        //to_tag parameter does not exist so break
                        break;
                    }
                case DLG_STATE_CONCURRENTLY_CONFIRMED:

                    //check the to_tag passed parameter exists

                    if (to_tag) {

                        //compare the to_tag passed parameter to all the dlg_out to_tag entry of the dlg parameter  (There could be multiple)
                        while (dlg_out) {

                            if (dlg_out->to_tag.len == to_tag->len && memcmp(dlg_out->to_tag.s, to_tag->s, dlg_out->to_tag.len) == 0) {
                                //this parameter matches the existing dlg_out and is therefore a retransmission
                                found = 1;
                            }
                            dlg_out = dlg_out->next;
                        }
                        if (found == -1) {
                            //The parameter does not match so this is another concurrently confirmed call (we would have breaked by now if it matched)
                            dlg->state = DLG_STATE_CONCURRENTLY_CONFIRMED;
                        }

                    } else {
                        //to_tag parameter does not exist so break
                        break;
                    }

                    break;
                default:
                    log_next_state_dlg(event, dlg);
            }
            break;
        case DLG_EVENT_REQACK:
            switch (dlg->state) {
                case DLG_STATE_CONFIRMED:
                    break;
                case DLG_STATE_DELETED:
                    break;
                default:
                    log_next_state_dlg(event, dlg);
            }
            break;
        case DLG_EVENT_REQBYE:
            switch (dlg->state) {
                case DLG_STATE_CONFIRMED:
                    dlg->dflags |= DLG_FLAG_HASBYE;
                    dlg->state = DLG_STATE_DELETED;
                    *unref = 1;
                    break;
                case DLG_STATE_EARLY:
                case DLG_STATE_DELETED:
                    break;
                default:
                    log_next_state_dlg(event, dlg);
            }
            break;
        case DLG_EVENT_REQPRACK:
            switch (dlg->state) {
                case DLG_STATE_EARLY:
                    //Richard added this - think it is necessary as can received PRACK in early state!
                    break;
                default:
                    log_next_state_dlg(event, dlg);
            }
            break;
        case DLG_EVENT_REQ:
            switch (dlg->state) {

                case DLG_STATE_EARLY:
                case DLG_STATE_CONFIRMED:
                    break;
                default:
                    log_next_state_dlg(event, dlg);
            }
            break;
        default:
            LM_CRIT("unknown event %d in state %d "
                    "for dlg %p [%u:%u] with clid '%.*s' and tags '%.*s'\n",
                    event, dlg->state, dlg, dlg->h_entry, dlg->h_id,
                    dlg->callid.len, dlg->callid.s,
                    dlg->from_tag.len, dlg->from_tag.s);
    }
    *new_state = dlg->state;

    dlg_unlock(d_table, d_entry);

    LM_DBG("dialog %p changed from state %d to "
            "state %d, due event %d\n", dlg, *old_state, *new_state, event);
}

/**
 *
 */
int dlg_set_toroute(struct dlg_cell *dlg, str * route) {
    if (dlg == NULL || route == NULL || route->len <= 0)
        return 0;
    if (dlg->toroute_name.s != NULL) {
        shm_free(dlg->toroute_name.s);
        dlg->toroute_name.s = NULL;
        dlg->toroute_name.len = 0;
    }
    dlg->toroute_name.s = (char*) shm_malloc((route->len + 1) * sizeof (char));
    if (dlg->toroute_name.s == NULL) {
        LM_ERR("no more shared memory\n");
        return -1;
    }
    memcpy(dlg->toroute_name.s, route->s, route->len);
    dlg->toroute_name.len = route->len;
    dlg->toroute_name.s[dlg->toroute_name.len] = '\0';
    dlg->toroute = route_lookup(&main_rt, dlg->toroute_name.s);

    return 0;
}

/*!
 * \brief Takes the did of the dialog and appends an "x" to it to make a different did for concurrent calls
 * \param dlg_cell - dlg_cell whose did we use
 * \param new_did - empty container for new_did
 * \return void
 */
void create_concurrent_did(struct dlg_cell *dlg, str * new_did) {
    int len  = dlg->did.len + 1 + 1;
    new_did->s = shm_malloc(len);
    if (new_did->s == 0) {
        LM_ERR("no more shm mem (%d)\n", len);
        return;
    }
    memset(new_did->s, 0, len);
    memcpy(new_did->s, dlg->did.s, dlg->did.len);
    new_did->s[dlg->did.len] = 'x';
    new_did->len = dlg->did.len + 1;
}

/*!
 * \brief Update the did of the dlg_out structure
 * \param dlg_cell_out - structure to update
 * \param new_did - new did to use
 * \return 1 success, 0 failure
 */
int update_dlg_out_did(struct dlg_cell_out *dlg_out, str * new_did) {
    //update the did of the dlg_out
    if (dlg_out->did.s) {
        if (dlg_out->did.len < new_did->len) {
            shm_free(dlg_out->did.s);
            dlg_out->did.s = (char*) shm_malloc(new_did->len);
            if (dlg_out->did.s == NULL)
                goto error;
        }
    } else {
        dlg_out->did.s = (char*) shm_malloc(new_did->len);
        if (dlg_out->did.s == NULL)
            goto error;
    }
    memcpy(dlg_out->did.s, new_did->s, new_did->len);
    dlg_out->did.len = new_did->len;

    return 0;
error:
    LM_ERR("not more shm mem\n");
    return -1;
}

/*!
 * \brief Update the did of the dlg structure
 * \param dlg_cell - structure to update
 * \param new_did - new did to use
 * \return 1 success, 0 failure
 */
int update_dlg_did(struct dlg_cell *dlg, str * new_did) {
    //update the did of the dlg_out
    if (dlg->did.s) {
        if (dlg->did.len < new_did->len) {
            shm_free(dlg->did.s);
            dlg->did.s = (char*) shm_malloc(new_did->len);
            if (dlg->did.s == NULL)
                goto error;
        }
    } else {
        dlg->did.s = (char*) shm_malloc(new_did->len);
        if (dlg->did.s == NULL)
            goto error;
    }
    memcpy(dlg->did.s, new_did->s, new_did->len);
    dlg->did.len = new_did->len;

    return 0;
error:
    LM_ERR("not more shm mem\n");
    return -1;
}



/**************************** MI functions ******************************/

/*!
 * \brief Helper method that output a dialog via the MI interface
 * \see mi_print_dlg
 * \param rpl MI node that should be filled
 * \param dlg printed dialog
 * \param with_context if 1 then the dialog context will be also printed
 * \return 0 on success, -1 on failure
 */
static inline int internal_mi_print_dlg_out(struct mi_node *rpl,
        struct dlg_cell_out * dlg_out) {
    struct mi_node* node = NULL;
    struct mi_node* node1 = NULL;
    struct mi_attr* attr = NULL;

    node = add_mi_node_child(rpl, 0, "dialog_out", 10, 0, 0);
    if (node == 0)
        goto error;

    attr = addf_mi_attr(node, 0, "hash", 4, "%u:%u",
            dlg_out->h_entry, dlg_out->h_id);
    if (attr == 0)
        goto error;

    node1 = add_mi_node_child(node, MI_DUP_VALUE, "to_tag", 6,
            dlg_out->to_tag.s, dlg_out->to_tag.len);
    if (node1 == 0)
        goto error;

    node1 = add_mi_node_child(node, MI_DUP_VALUE, "did", 3,
            dlg_out->did.s, dlg_out->did.len);
    if (node1 == 0)
        goto error;


    node1 = add_mi_node_child(node, MI_DUP_VALUE, "callee_contact", 14,
            dlg_out->callee_contact.s,
            dlg_out->callee_contact.len);
    if (node1 == 0)
        goto error;

    node1 = add_mi_node_child(node, MI_DUP_VALUE, "caller_cseq", 11,
            dlg_out->caller_cseq.s,
            dlg_out->caller_cseq.len);
    if (node1 == 0)
        goto error;

    node1 = add_mi_node_child(node, MI_DUP_VALUE, "callee_cseq", 11,
            dlg_out->callee_cseq.s,
            dlg_out->callee_cseq.len);
    if (node1 == 0)
        goto error;


    node1 = add_mi_node_child(node, MI_DUP_VALUE, "callee_route_set", 16,
            dlg_out->callee_route_set.s,
            dlg_out->callee_route_set.len);
    if (node1 == 0)
        goto error;

    if (dlg_out->callee_bind_addr) {
        node1 = add_mi_node_child(node, 0,
                "callee_bind_addr", 16,
                dlg_out->callee_bind_addr->sock_str.s,
                dlg_out->callee_bind_addr->sock_str.len);
    } else {
        node1 = add_mi_node_child(node, 0,
                "callee_bind_addr", 16, 0, 0);
    }

    return 0;

error:
    LM_ERR("failed to add node\n");

    return -1;
}

/*!
 * \brief Helper method that output a dialog via the MI interface
 * \see mi_print_dlg
 * \param rpl MI node that should be filled
 * \param dlg printed dialog
 * \param with_context if 1 then the dialog context will be also printed
 * \return 0 on success, -1 on failure
 */
static inline int internal_mi_print_dlg(struct mi_node *rpl,
        struct dlg_cell *dlg, int with_context) {
    struct mi_node* node = NULL;
    struct mi_node* node1 = NULL;
    struct mi_attr* attr = NULL;
    int len;
    char* p;

    node = add_mi_node_child(rpl, 0, "dialog", 6, 0, 0);
    if (node == 0)
        goto error;

    attr = addf_mi_attr(node, 0, "hash", 4, "%u:%u",
            dlg->h_entry, dlg->h_id);
    if (attr == 0)
        goto error;

    p = int2str((unsigned long) dlg->state, &len);
    node1 = add_mi_node_child(node, MI_DUP_VALUE, "state", 5, p, len);
    if (node1 == 0)
        goto error;

    p = int2str((unsigned long) dlg->ref, &len);
    node1 = add_mi_node_child(node, MI_DUP_VALUE, "ref_count", 9, p, len);
    if (node1 == 0)
        goto error;

    p = int2str((unsigned long) dlg->start_ts, &len);
    node1 = add_mi_node_child(node, MI_DUP_VALUE, "timestart", 9, p, len);
    if (node1 == 0)
        goto error;

    p = int2str((unsigned long) dlg->tl.timeout, &len);
    node1 = add_mi_node_child(node, MI_DUP_VALUE, "timeout", 7, p, len);
    if (node1 == 0)
        goto error;

    node1 = add_mi_node_child(node, MI_DUP_VALUE, "callid", 6,
            dlg->callid.s, dlg->callid.len);
    if (node1 == 0)
        goto error;

    node1 = add_mi_node_child(node, MI_DUP_VALUE, "from_uri", 8,
            dlg->from_uri.s, dlg->from_uri.len);
    if (node1 == 0)
        goto error;

    node1 = add_mi_node_child(node, MI_DUP_VALUE, "from_tag", 8,
            dlg->from_tag.s, dlg->from_tag.len);
    if (node1 == 0)
        goto error;

    node1 = add_mi_node_child(node, MI_DUP_VALUE, "did", 3,
            dlg->did.s, dlg->did.len);
    if (node1 == 0)
        goto error;

    node1 = add_mi_node_child(node, MI_DUP_VALUE, "caller_contact", 14,
            dlg->caller_contact.s,
            dlg->caller_contact.len);
    if (node1 == 0)
        goto error;

    node1 = add_mi_node_child(node, MI_DUP_VALUE, "first_req_cseq", 14,
            dlg->first_req_cseq.s,
            dlg->first_req_cseq.len);
    if (node1 == 0)
        goto error;

    node1 = add_mi_node_child(node, MI_DUP_VALUE, "caller_route_set", 16,
            dlg->caller_route_set.s,
            dlg->caller_route_set.len);
    if (node1 == 0)
        goto error;

    if (dlg->caller_bind_addr) {
        node1 = add_mi_node_child(node, 0,
                "caller_bind_addr", 16,
                dlg->caller_bind_addr->sock_str.s,
                dlg->caller_bind_addr->sock_str.len);
    } else {
        node1 = add_mi_node_child(node, 0,
                "caller_bind_addr", 16, 0, 0);
    }

    if (with_context) {
        node1 = add_mi_node_child(node, 0, "context", 7, 0, 0);
        if (node1 == 0)
            goto error;
        run_dlg_callbacks(DLGCB_MI_CONTEXT,
                dlg,
                NULL,
                NULL,
                DLG_DIR_NONE,
                (void *) node1);
    }

    struct dlg_cell_out *dlg_out;
    struct dlg_entry_out *d_entry_out = &(dlg->dlg_entry_out);
    dlg_out = d_entry_out->first;

    while (dlg_out) {

        if (internal_mi_print_dlg_out(rpl, dlg_out) != 0)
            goto error;

        dlg_out = dlg_out->next;
    }

    return 0;

error:
    LM_ERR("failed to add node\n");

    return -1;
}

/*!
 * \brief Output a dialog via the MI interface
 * \param rpl MI node that should be filled
 * \param dlg printed dialog
 * \param with_context if 1 then the dialog context will be also printed
 * \return 0 on success, -1 on failure
 */
int mi_print_dlg(struct mi_node *rpl, struct dlg_cell *dlg, int with_context) {

    return internal_mi_print_dlg(rpl, dlg, with_context);
}

/*!
 * \brief Helper function that output all dialogs via the MI interface
 * \see mi_print_dlgs
 * \param rpl MI node that should be filled
 * \param with_context if 1 then the dialog context will be also printed
 * \return 0 on success, -1 on failure
 */
static int internal_mi_print_dlgs(struct mi_node *rpl, int with_context) {
    struct dlg_cell *dlg;
    unsigned int i;

    LM_DBG("printing %i dialogs\n", d_table->size);

    for (i = 0; i < d_table->size; i++) {
        dlg_lock(d_table, &(d_table->entries[i]));

        for (dlg = d_table->entries[i].first; dlg; dlg = dlg->next) {
            if (internal_mi_print_dlg(rpl, dlg, with_context) != 0)
                goto error;
        }
        dlg_unlock(d_table, &(d_table->entries[i]));
    }
    return 0;

error:
    dlg_unlock(d_table, &(d_table->entries[i]));
    LM_ERR("failed to print dialog\n");

    return -1;
}

static inline struct mi_root * process_mi_params(struct mi_root *cmd_tree, struct dlg_cell **dlg_p) {
    struct mi_node* node;
    struct dlg_entry *d_entry;
    struct dlg_cell *dlg;
    str *callid;
    str *from_tag;
    unsigned int h_entry;

    node = cmd_tree->node.kids;
    if (node == NULL) {
        /* no parameters at all */
        *dlg_p = NULL;
        return NULL;
    }

    /* we have params -> get callid and fromtag */
    callid = &node->value;
    LM_DBG("callid='%.*s'\n", callid->len, callid->s);

    node = node->next;
    if (!node || !node->value.s || !node->value.len) {
        from_tag = NULL;
    } else {
        from_tag = &node->value;
        LM_DBG("from_tag='%.*s'\n", from_tag->len, from_tag->s);
        if (node->next != NULL)
            return init_mi_tree(400, MI_SSTR(MI_MISSING_PARM));
    }

    h_entry = core_hash(callid, 0, d_table->size);

    d_entry = &(d_table->entries[h_entry]);
    dlg_lock(d_table, d_entry);

    for (dlg = d_entry->first; dlg; dlg = dlg->next) {
        if (match_downstream_dialog(dlg, callid, from_tag) == 1) {
            if (dlg->state == DLG_STATE_DELETED) {
                *dlg_p = NULL;
                break;
            } else {
                *dlg_p = dlg;
                dlg_unlock(d_table, d_entry);
                return 0;
            }
        }
    }
    dlg_unlock(d_table, d_entry);

    return init_mi_tree(404, MI_SSTR("Nu such dialog"));
}

/*!
 * \brief Output all dialogs via the MI interface
 * \param cmd_tree MI command tree
 * \param param unused
 * \return mi node with the dialog information, or NULL on failure
 */

struct mi_root * mi_print_dlgs(struct mi_root *cmd_tree, void *param) {
    struct mi_root* rpl_tree = NULL;
    struct mi_node* rpl = NULL;
    struct dlg_cell* dlg = NULL;

    rpl_tree = process_mi_params(cmd_tree, &dlg);
    if (rpl_tree)
        //param error
        return rpl_tree;

    rpl_tree = init_mi_tree(200, MI_SSTR(MI_OK));
    if (rpl_tree == 0)
        return 0;
    rpl = &rpl_tree->node;

    if (dlg == NULL) {
        if (internal_mi_print_dlgs(rpl, 0) != 0)
            goto error;
    } else {
        if (internal_mi_print_dlg(rpl, dlg, 0) != 0)
            goto error;
    }

    return rpl_tree;
error:
    free_mi_tree(rpl_tree);

    return NULL;
}

/*!
 * \brief Print a dialog context via the MI interface
 * \param cmd_tree MI command tree
 * \param param unused
 * \return mi node with the dialog information, or NULL on failure
 */
struct mi_root * mi_print_dlgs_ctx(struct mi_root *cmd_tree, void *param) {
    struct mi_root* rpl_tree = NULL;
    struct mi_node* rpl = NULL;
    struct dlg_cell* dlg = NULL;

    rpl_tree = process_mi_params(cmd_tree, &dlg);
    if (rpl_tree)
        /* param error */
        return rpl_tree;

    rpl_tree = init_mi_tree(200, MI_SSTR(MI_OK));
    if (rpl_tree == 0)
        return 0;
    rpl = &rpl_tree->node;

    if (dlg == NULL) {
        if (internal_mi_print_dlgs(rpl, 1) != 0)
            goto error;
    } else {
        if (internal_mi_print_dlg(rpl, dlg, 1) != 0)
            goto error;
    }

    return rpl_tree;
error:
    free_mi_tree(rpl_tree);

    return NULL;
}

/*!
 * \brief decrement dialog ref counter by 1
 * \see dlg_unref
 * \param dlg dialog
 */
void dlg_release(struct dlg_cell *dlg)
{
	if(dlg==NULL)
		return;
	unref_dlg(dlg, 1);
}

time_t api_get_dlg_expires(str *callid, str *ftag, str *ttag) {
    struct dlg_cell *dlg;
    time_t expires = 0;
    time_t start;

    if (!callid || !ftag || !ttag) {
        LM_ERR("Missing callid, from tag or to tag\n");
        return 0;
    }

    unsigned int direction = DLG_DIR_NONE;
    dlg = get_dlg(callid, ftag, ttag, &direction);
    if (!dlg) return 0;

    if (dlg->state != DLG_STATE_CONFIRMED || !dlg->start_ts) {
        /* Dialog not started yet so lets assume start time is now.*/
        start = time(0);
    } else {
        start = dlg->start_ts;
    }

    expires = start + dlg->lifetime;
    unref_dlg(dlg, 1);

    return expires;
}

char* state_to_char(unsigned int state) {
	switch (state) {
	case DLG_STATE_UNCONFIRMED:
		return "Unconfirmed";
	case DLG_STATE_EARLY:
		return "Early";
	case DLG_STATE_CONFIRMED:
		return "Confirmed";
	case DLG_STATE_DELETED:
		return "Deleted";
	default:
		return "Unknown";
	}
}
