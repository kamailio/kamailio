/*
 * Copyright (C) 2006 Voice System SRL
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
 * \brief Timer related functions for the dialog module
 * \ingroup dialog
 * Module: \ref dialog
 */

#ifndef _DIALOG_DLG_TIMER_H_
#define _DIALOG_DLG_TIMER_H_


#include "../../locking.h"


/*! dialog timeout list */
typedef struct dlg_tl
{
	struct dlg_tl     *next;
	struct dlg_tl     *prev;
	volatile unsigned int  timeout; /*!< timeout in seconds */
} dlg_tl_t;


/*! dialog timer */
typedef struct dlg_timer
{
	struct dlg_tl   first; /*!< dialog timeout list */
	gen_lock_t      *lock; /*!< lock for the list */
} dlg_timer_t;


/*! dialog timer handler */
typedef void (*dlg_timer_handler)(struct dlg_tl *);


/*!
 * \brief Initialize the dialog timer handler
 * Initialize the dialog timer handler, allocate the lock and a global
 * timer in shared memory. The global timer handler will be set on success.
 * \param hdl dialog timer handler
 * \return 0 on success, -1 on failure
 */
int init_dlg_timer(dlg_timer_handler);


/*!
 * \brief Destroy global dialog timer
 */
void destroy_dlg_timer(void);


/*!
 * \brief Insert a dialog timer to the list
 * \param tl dialog timer list
 * \param interval timeout value in seconds
 * \return 0 on success, -1 when the input timer list is invalid
 */
int insert_dlg_timer(struct dlg_tl *tl, int interval);


/*!
 * \brief Remove a dialog timer from the list
 * \param tl dialog timer that should be removed
 * \return 1 when the input timer is empty, 0 when the timer was removed,
 * -1 when the input timer list is invalid
 */
int remove_dialog_timer(struct dlg_tl *tl);


/*!
 * \brief Update a dialog timer on the list
 * \param tl dialog timer
 * \param timeout new timeout value in seconds
 * \return 0 on success, -1 when the input list is invalid
 * \note the update is implemented as a remove, insert
 */
int update_dlg_timer(struct dlg_tl *tl, int timeout);


/*!
 * \brief Timer routine for expiration of dialogs
 * Timer handler for expiration of dialogs, runs the global timer handler on them.
 * \param ticks time for expiration checks
 * \param attr unused
 */
void dlg_timer_routine(unsigned int ticks , void * attr);

#endif
