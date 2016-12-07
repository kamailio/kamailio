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

#ifndef _UL_CALLBACKS_H
#define _UL_CALLBACKS_H

#include "../../dprint.h"

struct pcontact;

#define PCSCF_CONTACT_INSERT      (1<<0)
#define PCSCF_CONTACT_UPDATE      (1<<1)
#define PCSCF_CONTACT_DELETE      (1<<2)
#define PCSCF_CONTACT_EXPIRE      (1<<3)
#define PCSCF_MAX                 ((1<<4)-1)

typedef void (ul_cb) (struct pcontact *c, int type, void *param);		/*! \brief callback function prototype */
typedef int (*register_ulcb_t)(struct pcontact *c, int cb_types, ul_cb f, void *param);	/*! \brief register callback function prototype */

struct ul_callback {
	int types;                   /*!< types of events that trigger the callback*/
	ul_cb* callback;             /*!< callback function */
	void *param;                 /*!< param to be passed to callback function */
	//param_free_cb* callback_param_free;
	struct ul_callback* next;
};

struct ulcb_head_list {
	struct ul_callback *first;
	int reg_types;
};

extern struct ulcb_head_list*  ulcb_list;		/*!< this is the list for the INSERT callbacks*/

#define exists_ulcb_type(_types_) \
	( (ulcb_list->reg_types)|(_types_) )

int init_ulcb_list(void);
void destroy_ulcb_list(void);
void destroy_ul_callbacks_list(struct ul_callback* cb);
int register_ulcb( struct pcontact *c, int types, ul_cb f, void *param);
void run_ul_callbacks( int type , struct pcontact *c);
void run_ul_create_callbacks(struct pcontact *c);

#endif
