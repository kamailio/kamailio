/*
 * Copyright (C) 2006 Voice System SRL
 * Copyright (C) 2011 Carsten Bock, carsten@ng-voice.com
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
 * History:
 * --------
 * 2006-04-14  initial version (bogdan)
 * 2006-11-28  Added num_100s and num_200s to dlg_cell, to aid in adding 
 *             statistics tracking of the number of early, and active dialogs.
 *             (Jeffrey Magder - SOMA Networks)
 * 2007-03-06  syncronized state machine added for dialog state. New tranzition
 *             design based on events; removed num_1xx and num_2xx (bogdan)
 * 2007-07-06  added flags, cseq, contact, route_set and bind_addr 
 *             to struct dlg_cell in order to store these information into db
 *             (ancuta)
 * 2008-04-17  added new dialog flag to avoid state tranzitions from DELETED to
 *             CONFIRMED_NA due delayed "200 OK" (bogdan)
 * 2010-09      Add dlg_out structure to cater for forked calls and early dialog termination (richard and jason)
 */

/*!
 * \file
 * \brief Functions and definitions related to dialog creation and searching
 * \ingroup dialog
 * Module: \ref dialog
 */

#ifndef _DIALOG_DLG_HASH_H_
#define _DIALOG_DLG_HASH_H_

#include "../../locking.h"
#include "../../lib/kmi/mi.h"
#include "dlg_timer.h"
#include "dlg_cb.h"
#include "../../modules/tm/tm_load.h"


/* states of a dialog */
#define DLG_STATE_UNCONFIRMED  1 /*!< unconfirmed dialog */
#define DLG_STATE_EARLY        2 /*!< early dialog */
#define DLG_STATE_CONFIRMED    4 /*!< confirmed dialog */
#define DLG_STATE_DELETED      5 /*!< deleted dialog */
#define DLG_STATE_CONCURRENTLY_CONFIRMED      6 /*!< confirmed concurrent dailogs */

/* events for dialog processing */
#define DLG_EVENT_TDEL         1 /*!< transaction was destroyed */
#define DLG_EVENT_RPL1xx       2 /*!< 1xx request */
#define DLG_EVENT_RPL2xx       3 /*!< 2xx request */ 
#define DLG_EVENT_RPL3xx       4 /*!< 3xx request */
#define DLG_EVENT_REQPRACK     5 /*!< PRACK request */
#define DLG_EVENT_REQACK       6 /*!< ACK request */
#define DLG_EVENT_REQBYE       7 /*!< BYE request */
#define DLG_EVENT_REQ          8 /*!< other requests */

/* dialog flags */
#define DLG_FLAG_NEW           (1<<0) /*!< new dialog */
#define DLG_FLAG_CHANGED       (1<<1) /*!< dialog was changed */
#define DLG_FLAG_HASBYE        (1<<2) /*!< bye was received */
#define DLG_FLAG_TOBYE         (1<<3) /*!< flag from dialog context */
#define DLG_FLAG_CALLERBYE     (1<<4) /*!< bye from caller */
#define DLG_FLAG_CALLEEBYE     (1<<5) /*!< bye from callee */
#define DLG_FLAG_LOCALDLG      (1<<6) /*!< local dialog, unused */
#define DLG_FLAG_CHANGED_VARS  (1<<7) /*!< dialog-variables changed */

/* dialog-variable flags (in addition to dialog-flags) */
#define DLG_FLAG_DEL           (1<<8) /*!< delete this var */
#define DLG_FLAG_INSERTED      (1<<9) /*!< DLG already written to DB - could have been put in by early media or confirmed */

#define DLG_CALLER_LEG         0 /*!< attribute that belongs to a caller leg */
#define DLG_CALLEE_LEG         1 /*!< attribute that belongs to a callee leg */

#define DLG_DIR_NONE           0 /*!< dialog has no direction */
#define DLG_DIR_DOWNSTREAM     1 /*!< dialog has downstream direction */
#define DLG_DIR_UPSTREAM       2 /*!< dialog has upstream direction */

/*! entries in the main dialog table */
struct dlg_entry_out {
    struct dlg_cell_out *first; /*!< dialog list */
    struct dlg_cell_out *last; /*!< optimisation, end of the dialog list */
    unsigned int count; /*! number of out entries in the linked list */
};

/*! entries in the dialog list */
struct dlg_cell {
    volatile int ref; /*!< reference counter */
    struct dlg_cell *next; /*!< next entry in the list */
    struct dlg_cell *prev; /*!< previous entry in the list */
    unsigned int h_id; /*!< id of the hash table entry */
    unsigned int h_entry; /*!< number of hash entry */
    str did;
    str callid; /*!< callid from SIP message */
    str from_tag; /*!< from tags of caller*/
    str from_uri; /*!< from uri from SIP message */
    str first_req_cseq; /*!< CSEQ of caller*/
    str req_uri; /*!< r-uri from SIP message */
    str caller_contact; /*!< contact of caller*/
    str caller_route_set; /*!< route set of caller*/
    struct socket_info * caller_bind_addr; /*! binded address of caller*/
    unsigned int state; /*!< dialog state */
    unsigned int start_ts; /*!< start time  (absolute UNIX ts)*/
    unsigned int lifetime; /*!< dialog lifetime */
    unsigned int toroute; /*!< index of route that is executed on timeout */
    str toroute_name; /*!< name of route that is executed on timeout */
    unsigned int dflags; /*!< internal dialog flags */
    unsigned int sflags; /*!< script dialog flags */
    struct dlg_tl tl; /*!< dialog timer list */
    struct dlg_head_cbl cbs; /*!< dialog callbacks */
    struct dlg_profile_link *profile_links; /*!< dialog profiles */
    struct dlg_var *vars; /*!< dialog variables */
    struct dlg_entry_out dlg_entry_out; /*!< list of dialog_out entries */
    struct cell *transaction; /*!< ptr to associated transaction for this dialog TM module cell ptr */
    gen_lock_t *dlg_out_entries_lock; /*!< lock for dialog_out linked list */
    unsigned int from_rr_nb; /*!< information from record routing */
};

struct dlg_cell_out {
    struct dlg_cell_out *next; /*!< next entry in the list */
    struct dlg_cell_out *prev; /*!< previous entry in the list */
    unsigned int h_id; /*!< id of the hash table entry */
    unsigned int h_entry; /*!< number of hash entry */
    str did;
    str to_uri; /*!< to uri */
    str to_tag; /*!< to tags of callee*/
    str caller_cseq; /*!< CSEQ of caller*/
    str callee_cseq; /*!< CSEQ of callee*/
    str callee_contact; /*!< contact of callee*/
    str callee_route_set; /*!< route set of caller*/
    struct socket_info * callee_bind_addr; /*! binded address of caller*/
    unsigned int dflags; /*!< internal dialog flags */
    unsigned int deleted;
};

/*! entries in the main dialog table */
struct dlg_entry {
    struct dlg_cell *first; /*!< dialog list */
    struct dlg_cell *last; /*!< optimisation, end of the dialog list */
    unsigned int next_id; /*!< next id */
    unsigned int lock_idx; /*!< lock index */
};

/*! main dialog table */
struct dlg_table {
    unsigned int size; /*!< size of the dialog table */
    struct dlg_entry *entries; /*!< dialog hash table */
    unsigned int locks_no; /*!< number of locks */
    gen_lock_set_t *locks; /*!< lock table */
};


/*! global dialog table */
extern struct dlg_table *d_table;
/*! point to the current dialog */
extern struct dlg_cell *current_dlg_pointer;


/*!
 * \brief Set a dialog lock
 * \param _table dialog table
 * \param _entry locked entry
 */
#define dlg_lock(_table, _entry) \
		lock_set_get( (_table)->locks, (_entry)->lock_idx);


/*!
 * \brief Release a dialog lock
 * \param _table dialog table
 * \param _entry locked entry
 */
#define dlg_unlock(_table, _entry) \
		lock_set_release( (_table)->locks, (_entry)->lock_idx);

/*!
 * \brief Unlink a dialog from the list without locking
 * \see unref_dlg_unsafe
 * \param d_entry unlinked entry
 * \param dlg unlinked dialog
 */
static inline void unlink_unsafe_dlg(struct dlg_entry *d_entry, struct dlg_cell *dlg) {
    if (dlg->next)
        dlg->next->prev = dlg->prev;
    else
        d_entry->last = dlg->prev;
    if (dlg->prev)
        dlg->prev->next = dlg->next;
    else
        d_entry->first = dlg->next;

    dlg->next = dlg->prev = 0;

    return;
}


/*!
 * \brief Destroy a dialog, run callbacks and free memory
 * \param dlg destroyed dialog
 */
inline void destroy_dlg(struct dlg_cell *dlg);


/*!
 * \brief Initialize the global dialog table
 * \param size size of the table
 * \return 0 on success, -1 on failure
 */
int init_dlg_table(unsigned int size);


/*!
 * \brief Destroy the global dialog table
 */
void destroy_dlg_table(void);


void free_dlg_out_cell(struct dlg_cell_out *dlg_out);


/*!
 * \brief Create a new dialog structure for a SIP dialog
 * \param callid dialog callid
 * \param from_uri dialog from uri
 * \param to_uri dialog to uri
 * \param from_tag dialog from tag
 * \param req_uri dialog r-uri
 * \return created dialog structure on success, NULL otherwise
 */
struct dlg_cell* build_new_dlg(str *callid, str *from_uri,
        str *from_tag, str *req_uri);


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
        str *cseq, struct socket_info *bind_addr, unsigned int leg);



/*!
 * \brief Update or set the CSEQ for an existing dialog
 * \param dlg dialog
 * \param leg must be either DLG_CALLER_LEG, or DLG_CALLEE_LEG
 * \param cseq CSEQ of caller or callee
 * \return 0 on success, -1 on failure
 */
int dlg_update_cseq(struct dlg_cell *dlg, unsigned int leg, str *cseq, str *to_tag);

/*!
 * \brief Update or set the contact for an existing dialog
 * \param dlg dialog
 * \param leg must be either DLG_CALLER_LEG, or DLG_CALLEE_LEG
 * \param contact CONTACT of caller or callee
 * \return 0 on success, -1 on failure
 */
int dlg_update_contact(struct dlg_cell * dlg, unsigned int leg, str *contact, str *to_tag);

/*!
 * \brief Set time-out route
 * \param dlg dialog
 * \param route name of route
 * \return 0 on success, -1 on failure
 */
int dlg_set_toroute(struct dlg_cell *dlg, str *route);


/*!
 * \brief Lookup a dialog in the global list
 *
 * Note that the caller is responsible for decrementing (or reusing)
 * the reference counter by one again iff a dialog has been found.
 * \param h_entry number of the hash table entry
 * \param h_id id of the hash table entry
 * \return dialog structure on success, NULL on failure
 */
struct dlg_cell* lookup_dlg(unsigned int h_entry, unsigned int h_id);


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
struct dlg_cell* get_dlg(str *callid, str *ftag, str *ttag, unsigned int *dir);


/*!
 * \brief Link a dialog structure
 * \param dlg dialog
 * \param n extra increments for the reference counter
 */
void link_dlg(struct dlg_cell *dlg, int n);


void link_dlg_out(struct dlg_cell *dlg, struct dlg_cell_out *dlg_out, int n);

/*!
 * \brief Unreference a dialog with locking
 * \see unref_dlg_unsafe
 * \param dlg dialog
 * \param cnt decrement for the reference counter
 */
void unref_dlg(struct dlg_cell *dlg, unsigned int cnt);


/*!
 * \brief Refefence a dialog with locking
 * \see ref_dlg_unsafe
 * \param dlg dialog
 * \param cnt increment for the reference counter
 */
void ref_dlg(struct dlg_cell *dlg, unsigned int cnt);


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
        int *old_state, int *new_state, int *unref, str *to_tag);


/*!
 * \brief Output all dialogs via the MI interface
 * \param cmd_tree MI root node
 * \param param unused
 * \return a mi node with the dialog information, or NULL on failure
 */
struct mi_root * mi_print_dlgs(struct mi_root *cmd, void *param);



/*!
 * \brief Print a dialog context via the MI interface
 * \param cmd_tree MI command tree
 * \param param unused
 * \return mi node with the dialog information, or NULL on failure
 */
struct mi_root * mi_print_dlgs_ctx(struct mi_root *cmd, void *param);

/*!
 * \brief Terminate selected dialogs via the MI interface
 * \param cmd_tree MI command tree
 * \param param unused
 * \return mi node with the dialog information, or NULL on failure
 */
struct mi_root * mi_terminate_dlg(struct mi_root *cmd_tree, void *param);

/*!
 * \brief Check if a dialog structure matches to a SIP message dialog
 * \param dlg dialog structure
 * \param callid SIP message Call-ID
 * \param ftag SIP message from tag
 * \param ttag SIP message to tag
 * \param dir direction of the message, if DLG_DIR_NONE it will set
 * \return 1 if dialog structure and message content matches, 0 otherwise
 */
static inline int match_dialog(struct dlg_cell *dlg, str *callid,
        str *ftag, str *ttag, unsigned int *dir) {

    struct dlg_entry_out *d_entry_out = &(dlg->dlg_entry_out);
    struct dlg_cell_out *dlg_out;

    if (d_entry_out->first == 0) {
        //there are no dialog out entries yet
        if (*dir == DLG_DIR_DOWNSTREAM) {
            if (dlg->callid.len == callid->len &&
                    dlg->from_tag.len == ftag->len &&
                    strncmp(dlg->callid.s, callid->s, callid->len) == 0 &&
                    strncmp(dlg->from_tag.s, ftag->s, ftag->len) == 0) {
                return 1;
            }
        } else if (*dir == DLG_DIR_UPSTREAM) {
            if (dlg->callid.len == callid->len &&
                    dlg->from_tag.len == ttag->len &&
                    strncmp(dlg->callid.s, callid->s, callid->len) == 0 &&
                    strncmp(dlg->from_tag.s, ttag->s, ttag->len) == 0) {
                return 1;
            }
        } else {

            if (dlg->callid.len != callid->len) {
                return 0;
            }
            if (dlg->from_tag.len == ttag->len &&
                    strncmp(dlg->from_tag.s, ttag->s, ttag->len) == 0 &&
                    strncmp(dlg->callid.s, callid->s, callid->len) == 0) {

                *dir = DLG_DIR_UPSTREAM;
                return 1;
            } else if (dlg->from_tag.len == ftag->len &&
                    strncmp(dlg->from_tag.s, ftag->s, ftag->len) == 0 &&
                    strncmp(dlg->callid.s, callid->s, callid->len) == 0) {

                *dir = DLG_DIR_DOWNSTREAM;
                return 1;
            }
            LM_DBG("No match found\n");
        }
    } else {

        //there is a dialog out entry
        if (*dir == DLG_DIR_DOWNSTREAM) {
            if (dlg->callid.len == callid->len &&
                    dlg->from_tag.len == ftag->len &&
                    strncmp(dlg->callid.s, callid->s, callid->len) == 0 &&
                    strncmp(dlg->from_tag.s, ftag->s, ftag->len) == 0) {
                //now need to scroll thought d_out_entries to see if to_tag matches!
                dlg_out = d_entry_out->first;
                while (dlg_out) {
                    if (dlg_out->to_tag.len == ttag->len &&
                            memcmp(dlg_out->to_tag.s, ttag->s, dlg_out->to_tag.len) == 0) {
                        return 1;
                    }
                    dlg_out = dlg_out->next;
                }
            }
        } else if (*dir == DLG_DIR_UPSTREAM) {
            if (dlg->callid.len == callid->len &&
                    dlg->from_tag.len == ttag->len &&
                    strncmp(dlg->callid.s, callid->s, callid->len) == 0 &&
                    strncmp(dlg->from_tag.s, ttag->s, ttag->len) == 0) {
                dlg_out = d_entry_out->first;
                while (dlg_out) {
                    if (dlg_out->to_tag.len == ftag->len &&
                            memcmp(dlg_out->to_tag.s, ftag->s, dlg_out->to_tag.len) == 0) {
                        return 1;
                    }
                    dlg_out = dlg_out->next;
                }
            }
        } else {
            if (dlg->callid.len != callid->len) {
            	LM_DBG("no match cid: %d %d", dlg->callid.len, callid->len);
                return 0;
            }

            LM_DBG("p: %p ft[%.*s] tt [%.*s]", d_entry_out->first,
            			dlg->from_tag.len, dlg->from_tag.s,
            			ttag->len, ttag->s);
            if (dlg->from_tag.len == ttag->len &&
                    strncmp(dlg->from_tag.s, ttag->s, ttag->len) == 0 &&
                    strncmp(dlg->callid.s, callid->s, callid->len) == 0) {
                //now need to scroll thought d_out_entries to see if to_tag matches!
                dlg_out = d_entry_out->first;
                while (dlg_out) {
                	LM_DBG("dout: tt[%.*s]",
                			dlg_out->to_tag.len, dlg_out->to_tag.s);
                    if (dlg_out->to_tag.len == ftag->len &&
                            memcmp(dlg_out->to_tag.s, ftag->s, dlg_out->to_tag.len) == 0) {
                        *dir = DLG_DIR_UPSTREAM;
                        return 1;
                    }
                    dlg_out = dlg_out->next;
                }
            } else if (dlg->from_tag.len == ftag->len &&
                    strncmp(dlg->from_tag.s, ftag->s, ftag->len) == 0 &&
                    strncmp(dlg->callid.s, callid->s, callid->len) == 0) {
                //now need to scroll thought d_out_entries to see if to_tag matches!
                dlg_out = d_entry_out->first;
                while (dlg_out) {
                	LM_DBG("dout: tt[%.*s]",
                	                			dlg_out->to_tag.len, dlg_out->to_tag.s);
                    if (dlg_out->to_tag.len == ttag->len &&
                            memcmp(dlg_out->to_tag.s, ttag->s, dlg_out->to_tag.len) == 0) {
                        *dir = DLG_DIR_DOWNSTREAM;
                        return 1;
                    }
                    dlg_out = dlg_out->next;
                }
            }
            else
            	LM_DBG("no match tags: ");
        }
    }
    return 0;
}

/*!
 * \brief Check if a downstream dialog structure matches a SIP message dialog
 * \param dlg dialog structure
 * \param callid SIP message callid
 * \param ftag SIP message from tag
 * \return 1 if dialog structure matches the SIP dialog, 0 otherwise
 */
static inline int match_downstream_dialog(struct dlg_cell *dlg, str *callid, str *ftag) {
    if (dlg == NULL || callid == NULL)
        return 0;
    if (ftag == NULL) {
        if (dlg->callid.len != callid->len ||
                strncmp(dlg->callid.s, callid->s, callid->len) != 0)
            return 0;
    } else {
        if (dlg->callid.len != callid->len ||
                dlg->from_tag.len != ftag->len ||
                strncmp(dlg->callid.s, callid->s, callid->len) != 0 ||
                strncmp(dlg->from_tag.s, ftag->s, ftag->len) != 0)
            return 0;
    }
    return 1;
}


/*!
 * \brief Output a dialog via the MI interface
 * \param rpl MI node that should be filled
 * \param dlg printed dialog
 * \param with_context if 1 then the dialog context will be also printed
 * \return 0 on success, -1 on failure
 */
int mi_print_dlg(struct mi_node *rpl, struct dlg_cell *dlg, int with_context);



/*!
 * \brief Create a new dialog out structure for a SIP dialog
 * \param to_tag - dialog to_tag
 * \return created dlg_out structure on success, NULL otherwise
 */

struct dlg_cell_out* build_new_dlg_out(struct dlg_cell *dlg, str *to_uri, str* to_tag);

/*!
 * \brief Remove all dlg_out entries from dlg structure expect that identified as dlg_do_not_remove
 * \param dlg_out cell - struture to not remove
 * \param mark_only - 1 then only mark for delection, if 0 then delete
 * \return void
 */

void dlg_remove_dlg_out(struct dlg_cell_out *dlg_out_do_not_remove, struct dlg_cell *dlg, int only_mark);

/*!
 * \brief Remove dlg_out entry identified by to_tag from dlg structure
 * \param dlg structure
 * \param dlg_out to_tag
 * \return void
 */

void dlg_remove_dlg_out_tag(struct dlg_cell *dlg, str *to_tag);

/*!
 * \brief Takes the did of the dialog and appends an "x" to it to make a different did for concurrent calls
 * \param dlg_cell - dlg_cell whose did we use
 * \param new_did - empty container for new_did
 * \return void
 */

void create_concurrent_did(struct dlg_cell *dlg, str *new_did);

/*!
 * \brief Update the did of the dlg_out structure
 * \param dlg_cell_out - structure to update
 * \param new_did - new did to use
 * \return 1 success, 0 failure
 */

int update_dlg_out_did(struct dlg_cell_out *dlg_out, str *new_did);

/*!
 * \brief Update the did of the dlg structure
 * \param dlg_cell - structure to update
 * \param new_did - new did to use
 * \return 1 success, 0 failure
 */

int update_dlg_did(struct dlg_cell *dlg, str *new_did);

time_t api_get_dlg_expires(str *callid, str *ftag, str *ttag);

void dlg_release(struct dlg_cell *dlg);

char* state_to_char(unsigned int state);

#endif
