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
#include "../../timer.h"
#include "../../atomic_ops.h"
#include "dlg_timer.h"
#include "dlg_cb.h"


/* states of a dialog */
#define DLG_STATE_UNCONFIRMED  1 /*!< unconfirmed dialog */
#define DLG_STATE_EARLY        2 /*!< early dialog */
#define DLG_STATE_CONFIRMED_NA 3 /*!< confirmed dialog without a ACK yet */
#define DLG_STATE_CONFIRMED    4 /*!< confirmed dialog */
#define DLG_STATE_DELETED      5 /*!< deleted dialog */

/* events for dialog processing */
#define DLG_EVENT_TDEL         1 /*!< transaction was destroyed */
#define DLG_EVENT_RPL1xx       2 /*!< 1xx request */
#define DLG_EVENT_RPL2xx       3 /*!< 2xx request */ 
#define DLG_EVENT_RPL3xx       4 /*!< 3xx request */
#define DLG_EVENT_REQPRACK     5 /*!< PRACK request */
#define DLG_EVENT_REQACK       6 /*!< ACK request */
#define DLG_EVENT_REQBYE       7 /*!< BYE request */
#define DLG_EVENT_REQ          8 /*!< other requests */

/* dialog internal flags only in memory */
#define DLG_FLAG_NEW           (1<<0) /*!< new dialog */
#define DLG_FLAG_CHANGED       (1<<1) /*!< dialog was changed */
#define DLG_FLAG_HASBYE        (1<<2) /*!< bye was received */
#define DLG_FLAG_CALLERBYE     (1<<4) /*!< bye from caller */
#define DLG_FLAG_CALLEEBYE     (1<<5) /*!< bye from callee */
#define DLG_FLAG_LOCALDLG      (1<<6) /*!< local dialog, unused */
#define DLG_FLAG_CHANGED_VARS  (1<<7) /*!< dialog-variables changed */

/* dialog-variable flags (in addition to dialog-flags) */
#define DLG_FLAG_DEL           (1<<8) /*!< delete this var */

#define DLG_FLAG_TM            (1<<9) /*!< dialog is set in transaction */
#define DLG_FLAG_EXPIRED       (1<<10)/*!< dialog is expired */

/* internal flags stored in db */
#define DLG_IFLAG_TIMEOUTBYE        (1<<0) /*!< send bye on time-out */
#define DLG_IFLAG_KA_SRC            (1<<1) /*!< send keep alive to src */
#define DLG_IFLAG_KA_DST            (1<<2) /*!< send keep alive to dst */
#define DLG_IFLAG_TIMER_NORESET     (1<<3) /*!< don't reset dialog timers on in-dialog messages reception */
#define DLG_IFLAG_CSEQ_DIFF         (1<<4) /*!< CSeq changed in dialog */

#define DLG_CALLER_LEG         0 /*!< attribute that belongs to a caller leg */
#define DLG_CALLEE_LEG         1 /*!< attribute that belongs to a callee leg */

#define DLG_DIR_NONE           0 /*!< dialog has no direction */
#define DLG_DIR_DOWNSTREAM     1 /*!< dialog has downstream direction */
#define DLG_DIR_UPSTREAM       2 /*!< dialog has upstream direction */

#define DLG_EVENTRT_START    0
#define DLG_EVENTRT_END      1
#define DLG_EVENTRT_FAILED   2
#define DLG_EVENTRT_MAX      3

/*! internal unique ide per dialog */
typedef struct dlg_iuid {
	unsigned int         h_id;		/*!< id in the hash table entry (seq nr in slot) */
	unsigned int         h_entry;	/*!< index of hash table entry (the slot number) */
} dlg_iuid_t;

/*! entries in the dialog list */
typedef struct dlg_cell
{
	volatile int         ref;		/*!< reference counter */
	struct dlg_cell      *next;		/*!< next entry in the list */
	struct dlg_cell      *prev;		/*!< previous entry in the list */
	unsigned int         h_id;		/*!< id in the hash table entry (seq nr in slot) */
	unsigned int         h_entry;	/*!< index of hash table entry (the slot number) */
	unsigned int         state;		/*!< dialog state */
	unsigned int         lifetime;		/*!< dialog lifetime */
	unsigned int         init_ts;		/*!< init (creation) time (absolute UNIX ts)*/
	unsigned int         start_ts;		/*!< start time  (absolute UNIX ts)*/
	unsigned int         dflags;		/*!< internal dialog memory flags */
	unsigned int         iflags;		/*!< internal dialog persistent flags */
	unsigned int         sflags;		/*!< script dialog persistent flags */
	unsigned int         toroute;		/*!< index of route that is executed on timeout */
	str                  toroute_name;	/*!< name of route that is executed on timeout */
	unsigned int         from_rr_nb;	/*!< information from record routing */
	struct dlg_tl        tl;			/*!< dialog timer list */
	str                  callid;		/*!< callid from SIP message */
	str                  from_uri;		/*!< from uri from SIP message */
	str                  to_uri;		/*!< to uri from SIP message */
	str                  req_uri;		/*!< r-uri from SIP message */
	str                  tag[2];		/*!< from tags of caller and to tag of callee */
	str                  cseq[2];		/*!< CSEQ of caller and callee */
	str                  route_set[2];	/*!< route set of caller and callee */
	str                  contact[2];	/*!< contact of caller and callee */
	struct socket_info * bind_addr[2];	/*! binded address of caller and callee */
	struct dlg_head_cbl  cbs;		/*!< dialog callbacks */
	struct dlg_profile_link *profile_links; /*!< dialog profiles */
	struct dlg_var       *vars;		/*!< dialog variables */
} dlg_cell_t;


/*! entries in the main dialog table */
typedef struct dlg_entry
{
	struct dlg_cell    *first;	/*!< dialog list */
	struct dlg_cell    *last;	/*!< optimisation, end of the dialog list */
	unsigned int       next_id;	/*!< next id */
	gen_lock_t lock;     /* mutex to access items in the slot */
	atomic_t locker_pid; /* pid of the process that holds the lock */
	int rec_lock_level;  /* recursive lock count */
} dlg_entry_t;


/*! main dialog table */
typedef struct dlg_table
{
	unsigned int       size;	/*!< size of the dialog table */
	struct dlg_entry   *entries;	/*!< dialog hash table */
} dlg_table_t;


typedef struct dlg_ka {
	dlg_iuid_t iuid;
	ticks_t katime;
	unsigned iflags;
	struct dlg_ka *next;
} dlg_ka_t;

/*! global dialog table */
extern dlg_table_t *d_table;


/*!
 * \brief Set a dialog lock (re-entrant)
 * \param _table dialog table
 * \param _entry locked entry
 */
#define dlg_lock(_table, _entry) \
		do { \
			int mypid; \
			mypid = my_pid(); \
			if (likely(atomic_get( &(_entry)->locker_pid) != mypid)) { \
				lock_get( &(_entry)->lock); \
				atomic_set( &(_entry)->locker_pid, mypid); \
			} else { \
				/* locked within the same process that executed us */ \
				(_entry)->rec_lock_level++; \
			} \
		} while(0)


/*!
 * \brief Release a dialog lock
 * \param _table dialog table
 * \param _entry locked entry
 */
#define dlg_unlock(_table, _entry) \
		do { \
			if (likely((_entry)->rec_lock_level == 0)) { \
				atomic_set( &(_entry)->locker_pid, 0); \
				lock_release( &(_entry)->lock); \
			} else  { \
				/* recursive locked => decrease lock count */ \
				(_entry)->rec_lock_level--; \
			} \
		} while(0)

/*!
 * \brief Unlink a dialog from the list without locking
 * \see unref_dlg_unsafe
 * \param d_entry unlinked entry
 * \param dlg unlinked dialog
 */
static inline void unlink_unsafe_dlg(dlg_entry_t *d_entry, dlg_cell_t *dlg)
{
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
void destroy_dlg(dlg_cell_t *dlg);


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


/*!
 * \brief Create a new dialog structure for a SIP dialog
 * \param callid dialog callid
 * \param from_uri dialog from uri
 * \param to_uri dialog to uri
 * \param from_tag dialog from tag
 * \param req_uri dialog r-uri
 * \return created dialog structure on success, NULL otherwise
 */
dlg_cell_t* build_new_dlg(str *callid, str *from_uri,
		str *to_uri, str *from_tag, str *req_uri);


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
int dlg_set_leg_info(dlg_cell_t *dlg, str* tag, str *rr, str *contact,
		str *cseq, unsigned int leg);


/*!
 * \brief Update or set the CSEQ for an existing dialog
 * \param dlg dialog
 * \param leg must be either DLG_CALLER_LEG, or DLG_CALLEE_LEG
 * \param cseq CSEQ of caller or callee
 * \return 0 on success, -1 on failure
 */
int dlg_update_cseq(dlg_cell_t *dlg, unsigned int leg, str *cseq);

/*!
 * \brief Set time-out route
 * \param dlg dialog
 * \param route name of route
 * \return 0 on success, -1 on failure
 */
int dlg_set_toroute(dlg_cell_t *dlg, str *route);


/*!
 * \brief Lookup a dialog in the global list
 *
 * Note that the caller is responsible for decrementing (or reusing)
 * the reference counter by one again if a dialog has been found.
 * \param h_entry number of the hash table entry
 * \param h_id id of the hash table entry
 * \return dialog structure on success, NULL on failure
 */
dlg_cell_t* dlg_lookup(unsigned int h_entry, unsigned int h_id);

/*!
 * \brief Search and return dialog in the global list by iuid
 *
 * Note that the caller is responsible for decrementing (or reusing)
 * the reference counter by one again if a dialog has been found.
 * \param diuid internal unique id per dialog
 * \return dialog structure on success, NULL on failure
 */
dlg_cell_t* dlg_get_by_iuid(dlg_iuid_t *diuid);


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
dlg_cell_t* get_dlg(str *callid, str *ftag, str *ttag, unsigned int *dir);


/*!
 * \brief Search dialog that corresponds to CallId, From Tag and To Tag
 *
 * Get dialog that correspond to CallId, From Tag and To Tag.
 * See RFC 3261, paragraph 4. Overview of Operation:
 * "The combination of the To tag, From tag, and Call-ID completely
 * defines a peer-to-peer SIP relationship between [two UAs] and is
 * referred to as a dialog."
 * Note that the caller is responsible for decrementing (or reusing)
 * the reference counter by one again if a dialog has been found.
 * If the dialog is not found, the hash slot is left locked, to allow
 * linking the structure of a new dialog.
 * \param callid callid
 * \param ftag from tag
 * \param ttag to tag
 * \param dir direction
 * \return dialog structure on success, NULL on failure (and slot locked)
 */
dlg_cell_t* search_dlg(str *callid, str *ftag, str *ttag, unsigned int *dir);


/*!
 * \brief Lock hash table slot by call-id
 * \param callid call-id value
 */
void dlg_hash_lock(str *callid);


/*!
 * \brief Release hash table slot by call-id
 * \param callid call-id value
 */
void dlg_hash_release(str *callid);


/*!
 * \brief Link a dialog structure
 * \param dlg dialog
 * \param n extra increments for the reference counter
 * \param mode link in safe mode (0 - lock slot; 1 - don't)
 */
void link_dlg(struct dlg_cell *dlg, int n, int mode);


/*!
 * \brief Unreference a dialog with locking
 * \see unref_dlg_unsafe
 * \param dlg dialog
 * \param cnt decrement for the reference counter
 */
void dlg_unref(dlg_cell_t *dlg, unsigned int cnt);


/*!
 * \brief Refefence a dialog with locking
 * \see ref_dlg_unsafe
 * \param dlg dialog
 * \param cnt increment for the reference counter
 */
void dlg_ref(dlg_cell_t *dlg, unsigned int cnt);


/*!
 * \brief Release a dialog from ref counter by 1
 * \see dlg_unref
 * \param dlg dialog
 */
void dlg_release(dlg_cell_t *dlg);

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
void next_state_dlg(dlg_cell_t *dlg, int event,
		int *old_state, int *new_state, int *unref);


/*!
 * \brief Output all dialogs via the MI interface
 * \param cmd_tree MI root node
 * \param param unused
 * \return a mi node with the dialog information, or NULL on failure
 */
struct mi_root * mi_print_dlgs(struct mi_root *cmd, void *param );


/*!
 * \brief Print a dialog context via the MI interface
 * \param cmd_tree MI command tree
 * \param param unused
 * \return mi node with the dialog information, or NULL on failure
 */
struct mi_root * mi_print_dlgs_ctx(struct mi_root *cmd, void *param );

/*!
 * \brief Terminate selected dialogs via the MI interface
 * \param cmd_tree MI command tree
 * \param param unused
 * \return mi node with the dialog information, or NULL on failure
 */
struct mi_root * mi_terminate_dlgs(struct mi_root *cmd_tree, void *param );

/*!
 * \brief Check if a dialog structure matches to a SIP message dialog
 * \param dlg dialog structure
 * \param callid SIP message Call-ID
 * \param ftag SIP message from tag
 * \param ttag SIP message to tag
 * \param dir direction of the message, if DLG_DIR_NONE it will set
 * \return 1 if dialog structure and message content matches, 0 otherwise
 */
static inline int match_dialog(dlg_cell_t *dlg, str *callid,
							   str *ftag, str *ttag, unsigned int *dir) {
	if (dlg->tag[DLG_CALLEE_LEG].len == 0) {
        // dialog to tag is undetermined ATM.
		if (*dir==DLG_DIR_DOWNSTREAM) {
			if (dlg->callid.len == callid->len &&
				dlg->tag[DLG_CALLER_LEG].len == ftag->len &&
				strncmp(dlg->callid.s, callid->s, callid->len)==0 &&
				strncmp(dlg->tag[DLG_CALLER_LEG].s, ftag->s, ftag->len)==0) {
				return 1;
			}
		} else if (*dir==DLG_DIR_UPSTREAM) {
			if (dlg->callid.len == callid->len &&
				dlg->tag[DLG_CALLER_LEG].len == ttag->len &&
				strncmp(dlg->callid.s, callid->s, callid->len)==0 &&
				strncmp(dlg->tag[DLG_CALLER_LEG].s, ttag->s, ttag->len)==0) {
				return 1;
			}
		} else {
			if (dlg->callid.len != callid->len)
				return 0;

			if (dlg->tag[DLG_CALLER_LEG].len == ttag->len &&
				strncmp(dlg->tag[DLG_CALLER_LEG].s, ttag->s, ttag->len)==0 &&
				strncmp(dlg->callid.s, callid->s, callid->len)==0) {

				*dir = DLG_DIR_UPSTREAM;
				return 1;
			} else if (dlg->tag[DLG_CALLER_LEG].len == ftag->len &&
					   strncmp(dlg->tag[DLG_CALLER_LEG].s, ftag->s, ftag->len)==0 &&
					   strncmp(dlg->callid.s, callid->s, callid->len)==0) {

				*dir = DLG_DIR_DOWNSTREAM;
				return 1;
			}
		}
	} else {
		if (*dir==DLG_DIR_DOWNSTREAM) {
			if (dlg->callid.len == callid->len &&
				dlg->tag[DLG_CALLER_LEG].len == ftag->len &&
				dlg->tag[DLG_CALLEE_LEG].len == ttag->len &&
				strncmp(dlg->callid.s, callid->s, callid->len)==0 &&
				strncmp(dlg->tag[DLG_CALLER_LEG].s, ftag->s, ftag->len)==0 &&
				strncmp(dlg->tag[DLG_CALLEE_LEG].s, ttag->s, ttag->len)==0) {
				return 1;
			}
		} else if (*dir==DLG_DIR_UPSTREAM) {
			if (dlg->callid.len == callid->len &&
				dlg->tag[DLG_CALLEE_LEG].len == ftag->len &&
				dlg->tag[DLG_CALLER_LEG].len == ttag->len &&
				strncmp(dlg->callid.s, callid->s, callid->len)==0 &&
				strncmp(dlg->tag[DLG_CALLEE_LEG].s, ftag->s, ftag->len)==0 &&
				strncmp(dlg->tag[DLG_CALLER_LEG].s, ttag->s, ttag->len)==0) {
				return 1;
			}
		} else {
			if (dlg->callid.len != callid->len)
				return 0;

			if (dlg->tag[DLG_CALLEE_LEG].len == ftag->len &&
				dlg->tag[DLG_CALLER_LEG].len == ttag->len &&
				strncmp(dlg->tag[DLG_CALLEE_LEG].s, ftag->s, ftag->len)==0 &&
				strncmp(dlg->tag[DLG_CALLER_LEG].s, ttag->s, ttag->len)==0 &&
				strncmp(dlg->callid.s, callid->s, callid->len)==0) {

				*dir = DLG_DIR_UPSTREAM;
				return 1;
			} else if (dlg->tag[DLG_CALLER_LEG].len == ftag->len &&
					   dlg->tag[DLG_CALLEE_LEG].len == ttag->len &&
					   strncmp(dlg->tag[DLG_CALLER_LEG].s, ftag->s, ftag->len)==0 &&
					   strncmp(dlg->tag[DLG_CALLEE_LEG].s, ttag->s, ttag->len)==0 &&
					   strncmp(dlg->callid.s, callid->s, callid->len)==0) {

				*dir = DLG_DIR_DOWNSTREAM;
				return 1;
			}
			/* if no ACK yet, might be a lookup of dlg from a TM callback that
			 * runs on 200ok but with initial INVITE that has no to-tag */
			if(ttag->len==0 && dlg->state==DLG_STATE_CONFIRMED_NA
					&& dlg->tag[DLG_CALLER_LEG].len == ftag->len &&
					   strncmp(dlg->tag[DLG_CALLER_LEG].s, ftag->s, ftag->len)==0 &&
					   strncmp(dlg->callid.s, callid->s, callid->len)==0) {

				*dir = DLG_DIR_DOWNSTREAM;
				return 1;
			}
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
static inline int match_downstream_dialog(dlg_cell_t *dlg, str *callid, str *ftag)
{
	if(dlg==NULL || callid==NULL)
		return 0;
	if (ftag==NULL) {
		if (dlg->callid.len!=callid->len ||
			strncmp(dlg->callid.s,callid->s,callid->len)!=0)
			return 0;
	} else {
		if (dlg->callid.len!=callid->len ||
			dlg->tag[DLG_CALLER_LEG].len!=ftag->len  ||
			strncmp(dlg->callid.s,callid->s,callid->len)!=0 ||
			strncmp(dlg->tag[DLG_CALLER_LEG].s,ftag->s,ftag->len)!=0)
			return 0;
	}
	return 1;
}

/*!
 *
 */
void dlg_run_event_route(dlg_cell_t *dlg, sip_msg_t *msg, int ostate, int nstate);


int dlg_ka_add(dlg_cell_t *dlg);

int dlg_ka_run(ticks_t ti);

int dlg_clean_run(ticks_t ti);

/*!
 * \brief Update dialog lifetime - for internal callers.
 */

int update_dlg_timeout(dlg_cell_t *, int);

/*!
 * \brief Output a dialog via the MI interface
 * \param rpl MI node that should be filled
 * \param dlg printed dialog
 * \param with_context if 1 then the dialog context will be also printed
 * \return 0 on success, -1 on failure
 */
int mi_print_dlg(struct mi_node *rpl, dlg_cell_t *dlg, int with_context);

#endif
