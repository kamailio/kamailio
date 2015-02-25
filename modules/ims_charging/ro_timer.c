/* 
 * File:   ro_timer.c
 * Author: Jason Penton
 *
 * Created on 06 April 2011, 1:37 PM
 */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "../../mem/shm_mem.h"
#include "../dialog_ng/dlg_load.h"
#include "ro_timer.h"
#include "ro_session_hash.h"
#include "ims_ro.h"
#include "ro_db_handler.h"
#include "mod.h"
#include "ims_charging_stats.h"
#include "../../counters.h"

extern int interim_request_credits;
extern int ro_timer_buffer;
extern int ro_db_mode;
extern struct dlg_binds dlgb;
extern struct ims_charging_counters_h ims_charging_cnts_h;

/*! global dialog timer */
struct ro_timer *roi_timer = 0;
/*! global dialog timer handler */
ro_timer_handler timer_hdl = 0;

/*!
 * \brief Initialize the ro_session timer handler
 * Initialize the ro_session timer handler, allocate the lock and a global
 * timer in shared memory. The global timer handler will be set on success.
 * \param hdl ro_session timer handler
 * \return 0 on success, -1 on failure
 */
int init_ro_timer(ro_timer_handler hdl) {
    roi_timer = (struct ro_timer*) shm_malloc(sizeof (struct ro_timer));
    if (roi_timer == 0) {
        LM_ERR("no more shm mem\n");
        return -1;
    }
    memset(roi_timer, 0, sizeof (struct ro_timer));

    roi_timer->first.next = roi_timer->first.prev = &(roi_timer->first);

    roi_timer->lock = lock_alloc();
    if (roi_timer->lock == 0) {
        LM_ERR("failed to alloc lock\n");
        goto error0;
    }

    if (lock_init(roi_timer->lock) == 0) {
        LM_ERR("failed to init lock\n");
        goto error1;
    }

    timer_hdl = hdl;
    return 0;
error1:
    lock_dealloc(roi_timer->lock);
error0:
    shm_free(roi_timer);
    roi_timer = 0;
    return -1;
}

/*!
 * \brief Destroy global ro_session timer
 */
void destroy_ro_timer(void) {
    if (roi_timer == 0)
        return;

    lock_destroy(roi_timer->lock);
    lock_dealloc(roi_timer->lock);

    shm_free(roi_timer);
    roi_timer = 0;
}

/*!
 * \brief Helper function for insert_ro_session_timer
 * \see insert_ro_session_timer
 * \param tl ro_session timer list
 */
static inline void insert_ro_timer_unsafe(struct ro_tl *tl) {
    struct ro_tl* ptr;

    /* insert in sorted order */
    for (ptr = roi_timer->first.prev; ptr != &roi_timer->first; ptr = ptr->prev) {
        if (ptr->timeout <= tl->timeout)
            break;
    }

    LM_DBG("inserting %p for %d\n", tl, tl->timeout);
    tl->prev = ptr;
    tl->next = ptr->next;
    tl->prev->next = tl;
    tl->next->prev = tl;
}

/*!
 * \brief Insert a ro_session timer to the list
 * \param tl ro_session timer list
 * \param interval timeout value in seconds
 * \return 0 on success, -1 when the input timer list is invalid
 */
int insert_ro_timer(struct ro_tl *tl, int interval) {
    lock_get(roi_timer->lock);

    LM_DBG("inserting timer for interval [%i]\n", interval);
    if (tl->next != 0 || tl->prev != 0) {
        lock_release(roi_timer->lock);
        LM_CRIT("Trying to insert a bogus ro tl=%p tl->next=%p tl->prev=%p\n",
                tl, tl->next, tl->prev);
        return -1;
    }
    tl->timeout = get_ticks() + interval;
    insert_ro_timer_unsafe(tl);


    LM_DBG("TIMER inserted");
    lock_release(roi_timer->lock);

    return 0;
}

/*!
 * \brief Helper function for remove_ro_session_timer
 * \param tl ro_session timer list
 * \see remove_ro_session_timer
 */
static inline void remove_ro_timer_unsafe(struct ro_tl *tl) {
    tl->prev->next = tl->next;
    tl->next->prev = tl->prev;
}

/*!
 * \brief Remove a ro_session timer from the list
 * \param tl ro_session timer that should be removed
 * \return 1 when the input timer is empty, 0 when the timer was removed,
 * -1 when the input timer list is invalid
 */
int remove_ro_timer(struct ro_tl *tl) {
    lock_get(roi_timer->lock);
	
    if (tl->prev == NULL && tl->timeout == 0) {
        lock_release(roi_timer->lock);
        return 1;
    }

    if (tl->prev == NULL || tl->next == NULL) {
        LM_CRIT("bogus tl=%p tl->prev=%p tl->next=%p\n",
                tl, tl->prev, tl->next);
        lock_release(roi_timer->lock);
        return -1;
    }
    LM_DBG("TIMER REMOVED");
    remove_ro_timer_unsafe(tl);
    tl->next = NULL;
    tl->prev = NULL;
    tl->timeout = 0;

    lock_release(roi_timer->lock);
    return 0;
}

/*!
 * \brief Update a ro_session timer on the list
 * \param tl dialog timer
 * \param timeout new timeout value in seconds
 * \return 0 on success, -1 when the input list is invalid
 * \note the update is implemented as a remove, insert
 */
int update_ro_timer(struct ro_tl *tl, int timeout) {
    lock_get(roi_timer->lock);

    if (tl->next) {
        if (tl->prev == 0) {
            lock_release(roi_timer->lock);
            return -1;
        }
        remove_ro_timer_unsafe(tl);
    }

    tl->timeout = get_ticks() + timeout;
    insert_ro_timer_unsafe(tl);

    lock_release(roi_timer->lock);
    return 0;
}

/*!
 * \brief Helper function for ro_timer_routine
 * \param time time for expiration check
 * \return list of expired credit reservations on sessions on success, 0 on failure
 */
static inline struct ro_tl* get_expired_ro_sessions(unsigned int time) {
    struct ro_tl *tl, *end, *ret;

    lock_get(roi_timer->lock);

    if (roi_timer->first.next == &(roi_timer->first) || roi_timer->first.next->timeout > time) {
        lock_release(roi_timer->lock);
        return 0;
    }

    end = &roi_timer->first;
    tl = roi_timer->first.next;
    LM_DBG("start with tl=%p tl->prev=%p tl->next=%p (%d) at %d and end with end=%p end->prev=%p end->next=%p\n", tl, tl->prev, tl->next, tl->timeout, time, end, end->prev, end->next);
    while (tl != end && tl->timeout <= time) {
        LM_DBG("getting tl=%p tl->prev=%p tl->next=%p with %d\n", tl, tl->prev, tl->next, tl->timeout);
        tl->prev = 0;
        tl->timeout = 0;
        tl = tl->next;
    }
    LM_DBG("end with tl=%p tl->prev=%p tl->next=%p and d_timer->first.next->prev=%p\n", tl, tl->prev, tl->next, roi_timer->first.next->prev);

    if (tl == end && roi_timer->first.next->prev) {
        ret = 0;
    } else {
        ret = roi_timer->first.next;
        tl->prev->next = 0;
        roi_timer->first.next = tl;
        tl->prev = &roi_timer->first;
    }

    lock_release(roi_timer->lock);

    return ret;
}

/*!
 * \brief Timer routine for expiration of credit reservations
 * Timer handler for expiration of credit reservations on a session, runs the global timer handler on them.
 * \param time for expiration checks
 * \param attr unused
 */
void ro_timer_routine(unsigned int ticks, void * attr) {

    struct ro_tl *tl, *ctl;
    LM_DBG("getting expired ro-sessions");

    tl = get_expired_ro_sessions(ticks);

    while (tl) {
        ctl = tl;
        tl = tl->next;
        ctl->next = NULL;
        LM_DBG("Ro Session Timer firing: tl=%p next=%p\n", ctl, tl);
        timer_hdl(ctl);
    }
}

void resume_ro_session_ontimeout(struct interim_ccr *i_req) {
	time_t now = time(0);
	time_t used_secs;
	struct ro_session_entry *ro_session_entry = NULL;
	int call_terminated = 0;

	if (!i_req) {
		LM_ERR("This is so wrong: i_req is NULL\n");
		return;
	}

	ro_session_entry = &(ro_session_table->entries[i_req->ro_session->h_entry]);
	ro_session_lock(ro_session_table, ro_session_entry);
	LM_DBG("credit=%d credit_valid_for=%d", i_req->new_credit, i_req->credit_valid_for);

	used_secs = now - i_req->ro_session->last_event_timestamp;

	/* check to make sure diameter server is giving us sane values */
	if (i_req->new_credit > i_req->credit_valid_for) {
		LM_WARN("That's weird, Diameter server gave us credit with a lower validity period :D. Setting reserved time to validity period instead \n");
		i_req->new_credit = i_req->credit_valid_for;
	}

	if (i_req->new_credit > 0) {
		//now insert the new timer
		i_req->ro_session->last_event_timestamp = time(0);
		i_req->ro_session->event_type = answered;
		i_req->ro_session->valid_for = i_req->credit_valid_for;

		int ret = 0;
		if (i_req->is_final_allocation) {
			LM_DBG("This is a final allocation and call will end in %i seconds\n", i_req->new_credit);
			i_req->ro_session->event_type = no_more_credit;
			ret = insert_ro_timer(&i_req->ro_session->ro_tl, i_req->new_credit);
		}
		else {
			int timer_timeout = i_req->new_credit;

			if (i_req->new_credit > ro_timer_buffer /*TIMEOUTBUFFER*/) {

				// We haven't finished using our 1st block of units, and we need to set the timer to
				// (new_credit - ro_timer_buffer[5 secs]) to ensure we get new credit before our previous
				// reservation is exhausted. This will only be done the first time, because the timer
				// will always be fired 5 seconds before we run out of time thanks to this operation

				if ((now - i_req->ro_session->start_time) /* call time */ < i_req->ro_session->reserved_secs)
					timer_timeout = i_req->new_credit - ro_timer_buffer;
				else
					timer_timeout = i_req->new_credit;
			}

			ret = insert_ro_timer(&i_req->ro_session->ro_tl, timer_timeout);

		}

		// update to the new block of units we got
		i_req->ro_session->reserved_secs = i_req->new_credit;

		if (ret != 0) {
			LM_CRIT("unable to insert timer for Ro Session [%.*s]\n",
					i_req->ro_session->ro_session_id.len, i_req->ro_session->ro_session_id.s);
		}
		else {
			ref_ro_session_unsafe(i_req->ro_session, 1);
		}
		
		if (ro_db_mode == DB_MODE_REALTIME) {
		    i_req->ro_session->flags |= RO_SESSION_FLAG_CHANGED;
		    if (update_ro_dbinfo_unsafe(i_req->ro_session) != 0) {
			LM_ERR("Failed to update Ro session in DB... continuing\n");
		    }
		}
	}
	else {
		/* just put the timer back in with however many seconds are left (if any!!! in which case we need to kill */
		/* also update the event type to no_more_credit to save on processing the next time we get here */
		i_req->ro_session->event_type = no_more_credit;
		int whatsleft = i_req->ro_session->reserved_secs - used_secs;
		if (whatsleft <= 0) {
			// TODO we need to handle this situation more precisely.
			// in case CCR times out, we get a call shutdown but the error message assumes it was due to a lack of credit.
			//
			LM_WARN("Immediately killing call due to no more credit *OR* no CCA received (timeout) after reservation request\n");

			//
			// we need to unlock the session or else we might get a deadlock on dlg_terminated() dialog callback.
			// Do not unref the session because it will be made inside the dlg_terminated() function.
			//

			//unref_ro_session_unsafe(i_req->ro_session, 1, ro_session_entry);
			ro_session_unlock(ro_session_table, ro_session_entry);

			dlgb.lookup_terminate_dlg(i_req->ro_session->dlg_h_entry, i_req->ro_session->dlg_h_id, NULL );
			call_terminated = 1;
		}
		else {
			LM_DBG("No more credit for user - letting call run out of money in [%i] seconds", whatsleft);
			int ret = insert_ro_timer(&i_req->ro_session->ro_tl, whatsleft);
			if (ret != 0) {
				LM_CRIT("unable to insert timer for Ro Session [%.*s]\n",
						i_req->ro_session->ro_session_id.len, i_req->ro_session->ro_session_id.s);
			}
			else {
				ref_ro_session_unsafe(i_req->ro_session, 1);
			}
		}
	}

	//
	// if call was forcefully terminated, the lock was released before dlgb.lookup_terminate_dlg() function call.
	//
	if (!call_terminated) {
		unref_ro_session_unsafe(i_req->ro_session, 1, ro_session_entry);//unref from the initial timer that fired this event.
		ro_session_unlock(ro_session_table, ro_session_entry);
	}

	shm_free(i_req);
	LM_DBG("Exiting async ccr interim nicely");
}

/* this is the function called when a we need to request more funds/credit. We need to try and reserve more credit.
 * If we cant we need to put a new timer to kill the call at the appropriate time
 */
void ro_session_ontimeout(struct ro_tl *tl) {
	time_t now, used_secs, call_time;

	LM_DBG("We have a fired timer [p=%p] and tl=[%i].\n", tl, tl->timeout);

	/* find the session id for this timer*/
	struct ro_session* ro_session = ((struct ro_session*) ((char *) (tl) - (unsigned long) (&((struct ro_session*) 0)->ro_tl)));

	if (!ro_session) {
		LM_ERR("Can't find a session. This is bad");
		return;
	}

	LM_DBG("event-type=%d", ro_session->event_type);
	
//	if (!ro_session->active) {
//		LM_ALERT("Looks like this session was terminated while requesting more units");
//		goto exit;
//		return;
//	}
	
	switch (ro_session->event_type) {
	case answered:
		now = time(0);
		used_secs = now - ro_session->last_event_timestamp;
		call_time = now - ro_session->start_time;

		counter_add(ims_charging_cnts_h.billed_secs, used_secs);

		if (ro_session->callid.s != NULL
				&& ro_session->dlg_h_entry	>= 0
				&& ro_session->dlg_h_id > 0
				&& ro_session->ro_session_id.s != NULL)
		{
			LM_DBG("Found a session to re-apply for timing [%.*s] and user is [%.*s]\n",
					ro_session->ro_session_id.len,
					ro_session->ro_session_id.s,
					ro_session->asserted_identity.len,
					ro_session->asserted_identity.s);

			LM_DBG("Call session has been active for %i seconds. The last reserved secs was [%i] and the last event was [%i seconds] ago",
					(unsigned int) call_time,
					(unsigned int) ro_session->reserved_secs,
					(unsigned int) used_secs);

			LM_DBG("Call session [p=%p]: we will now make a request for another [%i] of credit with a usage of [%i] seconds from the last bundle.\n",
					ro_session,
					interim_request_credits/* new reservation request amount */,
					(unsigned int) used_secs/* charged seconds from previous reservation */);

			// Apply for more credit.
			//
			// The function call will return immediately and we will receive the reply asynchronously via a callback
			send_ccr_interim(ro_session, (unsigned int) used_secs, interim_request_credits);
			return;
		}
		else {
			LM_ERR("Hmmm, the session we have either doesn't have all the data or something else has gone wrong.\n");
			/* put the timer back so the call will be killed according to previous timeout. */
			ro_session->event_type = unknown_error;
			int ret = insert_ro_timer(&ro_session->ro_tl,
					ro_session->reserved_secs - used_secs);
			if (ret != 0) {
				LM_CRIT("unable to insert timer for Ro Session [%.*s]\n", 
					ro_session->ro_session_id.len, ro_session->ro_session_id.s); 
			}
			else {
				ref_ro_session_unsafe(ro_session, 1);
			}
			LM_ERR("Immediately killing call due to unknown error\n");
		}

		break;
	default:
		LM_ERR("Diameter call session - event [%d]\n", ro_session->event_type);

		if (ro_session->event_type == no_more_credit)
			LM_INFO("Call/session must be ended - no more funds.\n");
		else if (ro_session->event_type == unknown_error)
			LM_ERR("last event caused an error. We will now tear down this session.\n");
	}


	counter_inc(ims_charging_cnts_h.killed_calls);

	//unref_ro_session_unsafe(ro_session, 1, ro_session_entry); //unref from the initial timer that fired this event.
//	ro_session_unlock(ro_session_table, ro_session_entry);

	dlgb.lookup_terminate_dlg(ro_session->dlg_h_entry, ro_session->dlg_h_id, NULL);
	return;
}

