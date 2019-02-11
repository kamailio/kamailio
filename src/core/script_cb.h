/*
 * Copyright (C) 2001-2003 FhG Fokus
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
 * \brief Kamailio core :: Script callbacks
 * \ingroup core
 * Module: \ref core
 */

#ifndef _SCRIPT_CB_H_
#define _SCRIPT_CB_H_

#include "parser/msg_parser.h"

typedef int (cb_function)(struct sip_msg *msg, unsigned int flags, void *param);


#define PRE_SCRIPT_CB    (1<<30)
#define POST_SCRIPT_CB   (1<<31)

/* Pre- and post-script callback flags. Use these flags to register
 * for the callbacks, and to check the type of the callback from the
 * functions.
 * (Power of 2 so more callbacks can be registered at once.)
 */
enum script_cb_flag { REQUEST_CB=1, FAILURE_CB=2, ONREPLY_CB=4,
			BRANCH_CB=8, ONSEND_CB=16, ERROR_CB=32,
			LOCAL_CB=64, EVENT_CB=128, BRANCH_FAILURE_CB=256 };

/* Callback types used for executing the callbacks.
 * Keep in sync with script_cb_flag!!!
 */
enum script_cb_type { REQUEST_CB_TYPE=1, FAILURE_CB_TYPE, ONREPLY_CB_TYPE,
			BRANCH_CB_TYPE, ONSEND_CB_TYPE, ERROR_CB_TYPE,
			LOCAL_CB_TYPE, EVENT_CB_TYPE, BRANCH_FAILURE_CB_TYPE, MAX_CB_TYPE };

struct script_cb{
	cb_function *cbf;
	struct script_cb *next;
	void *param;
};

/* Register pre- or post-script callbacks.
 * Returns -1 on error, 0 on success
 */
int register_script_cb( cb_function f, unsigned int flags, void *param );

int init_script_cb(void);
void destroy_script_cb(void);

/* Execute pre-script callbacks of a given type.
 * Returns 0 on error, 1 on success
 */
int exec_pre_script_cb( struct sip_msg *msg, enum script_cb_type type);

/* Execute post-script callbacks of a given type.
 * Always returns 1, success.
 */
int exec_post_script_cb( struct sip_msg *msg, enum script_cb_type type);

#endif

