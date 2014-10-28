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

/* forward declaration for ucontact_t */
struct ucontact;
struct impurecord;

#define UL_CONTACT_INSERT      	(1<<0)
#define UL_CONTACT_UPDATE      	(1<<1)
#define UL_CONTACT_DELETE      	(1<<2)
#define UL_CONTACT_EXPIRE      	(1<<3)
#define UL_IMPU_REG_NC_DELETE	(1<<4)		/* reg, no contacts - deleted */
#define UL_IMPU_NR_DELETE		(1<<5)		/* Not registered - deleted */
#define UL_IMPU_UNREG_EXPIRED	(1<<6)		/* Unregistered time expired */
#define UL_IMPU_DELETE			(1<<7)		/* explicit impu delete - for example thru API */
#define UL_IMPU_INSERT		   	(1<<8)		/* new IMPU record has been added */
#define UL_IMPU_UPDATE		   	(1<<9)		/* IMPU record has been updated */
#define UL_IMPU_NEW_CONTACT		(1<<10)		/* a new contact has been inserted for this IMPU */
#define UL_IMPU_UPDATE_CONTACT		(1<<11)		/* a new contact has been inserted for this IMPU */
#define UL_IMPU_DELETE_CONTACT		(1<<12)		/* a new contact has been inserted for this IMPU */
#define UL_IMPU_EXPIRE_CONTACT		(1<<13)		/* a new contact has been inserted for this IMPU */
#define ULCB_MAX               	((1<<14)-1)

/*! \brief callback function prototype */
typedef void (ul_cb) (struct impurecord* r, struct ucontact *c, int type, void *param);
/*! \brief register callback function prototype */
typedef int (*register_ulcb_t)( struct impurecord* r, struct ucontact *c, int cb_types, ul_cb f, void *param);


struct ul_callback {
	int id;                      /*!< id of this callback - useless */
	int types;                   /*!< types of events that trigger the callback*/
	ul_cb* callback;             /*!< callback function */
	void *param;                 /*!< param to be passed to callback function */
	struct ul_callback* next;
};

    struct ulcb_head_list {
	struct ul_callback *first;
	int reg_types;
};

extern struct ulcb_head_list*  ulcb_list;

static inline int exists_ulcb_type(struct ulcb_head_list* list, int types) {
	if (list==NULL)
		return (ulcb_list->reg_types|types);
	else
		return (list->reg_types|types);
}

int init_ulcb_list(void);

void destroy_ulcb_list(void);

/*! \brief register a callback for several types of events */
int register_ulcb( struct impurecord* r, struct ucontact* c, int types, ul_cb f, void *param );

/*! \brief run all transaction callbacks for an event type */
static inline void run_ul_callbacks( struct ulcb_head_list* cb_list, int type , struct impurecord *r, struct ucontact *c)
{
	struct ul_callback *cbp;

	if (cb_list == NULL) { //must be for global list
		cb_list = ulcb_list;
	}

	for (cbp=cb_list->first; cbp; cbp=cbp->next)  {
		if(cbp->types&type) {
			LM_DBG("impurecord=%p, contact=%p, callback type %d/%d, id %d entered\n",
				r, c, type, cbp->types, cbp->id );
			cbp->callback( r, c, type, cbp->param );
		}
	}
}

#endif
