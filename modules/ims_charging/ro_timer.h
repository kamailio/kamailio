/* 
 * File:   ro_timer.h
 * Author: Jason Penton
 *
 * Created on 06 April 2011, 1:39 PM
 */

#ifndef RO_TIMER_H
#define	RO_TIMER_H

#include "../../locking.h"
#include "../../timer.h"

extern struct interim_ccr *i_req;

/*! ro timeout list */
struct ro_tl {
    struct ro_tl *next;
    struct ro_tl *prev;
    volatile unsigned int timeout; /*!< timeout in seconds */
};

/*! ro_session timer */
struct ro_timer {
    struct ro_tl first; /*!< ro session timeout list */
    gen_lock_t *lock; /*!< lock for the list */
};

/*! ro_session timer handler */
typedef void (*ro_timer_handler)(struct ro_tl *);


/*!
 * \brief Initialize the ro_session timer handler
 * Initialize the ro_session timer handler, allocate the lock and a global
 * timer in shared memory. The global timer handler will be set on success.
 * \param hdl dialog timer handler
 * \return 0 on success, -1 on failure
 */
int init_ro_timer(ro_timer_handler);


/*!
 * \brief Destroy ro_session dialog timer
 */
void destroy_ro_timer(void);


/*!
 * \brief Insert a ro_session timer to the list
 * \param tl ro_session timer list
 * \param interval timeout value in seconds
 * \return 0 on success, -1 when the input timer list is invalid
 */
int insert_ro_timer(struct ro_tl *tl, int interval);


/*!
 * \brief Remove a ro_session timer from the list
 * \param tl ro_session timer that should be removed
 * \return 1 when the input timer is empty, 0 when the timer was removed,
 * -1 when the input timer list is invalid
 */
int remove_ro_timer(struct ro_tl *tl);


/*!
 * \brief Update a ro_session timer on the list
 * \param tl ro_session timer
 * \param timeout new timeout value in seconds
 * \return 0 on success, -1 when the input list is invalid
 * \note the update is implemented as a remove, insert
 */
int update_ro_timer(struct ro_tl *tl, int timeout);


/*!
 * \brief Timer routine for expiration of ro_session credit reservations
 * Timer handler for expiration of ro_session credit reservations, runs the global timer handler on them.
 * \param time for expiration checks on credit reservations
 * \param attr unused
 */
void ro_timer_routine(unsigned int ticks, void * attr);

/* this is the function called when a we need to request more funds/credit. We need to try and reserve more credit.
 * If we cant we need to put a new timer to kill the call at the appropriate time
 */
void ro_session_ontimeout(struct ro_tl *tl);

void resume_ro_session_ontimeout(struct interim_ccr *i_req);

#endif	/* RO_TIMER_H */

