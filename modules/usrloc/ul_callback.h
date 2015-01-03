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

/*! \file
 *  \brief USRLOC - Module callbacks
 *  \ingroup usrloc
 */

#ifndef _UL_CALLBACKS_H
#define _UL_CALLBACKS_H

#include "../../dprint.h"

/* forward declaration for ucontact_t */
struct ucontact;

#define UL_CONTACT_INSERT      (1<<0)
#define UL_CONTACT_UPDATE      (1<<1)
#define UL_CONTACT_DELETE      (1<<2)
#define UL_CONTACT_EXPIRE      (1<<3)
#define ULCB_MAX               ((1<<4)-1)

/*! \brief callback function prototype */
typedef void (ul_cb) (struct ucontact *c, int type, void *param);
/*! \brief register callback function prototype */
typedef int (*register_ulcb_t)( int cb_types, ul_cb f, void *param);


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


#define exists_ulcb_type(_types_) \
	( (ulcb_list->reg_types)&(_types_) )


int init_ulcb_list(void);

void destroy_ulcb_list(void);


/*! \brief register a callback for several types of events */
int register_ulcb( int types, ul_cb f, void *param );

/*! \brief run all transaction callbacks for an event type */
static inline void run_ul_callbacks( int type , struct ucontact *c)
{
	struct ul_callback *cbp;

	for (cbp=ulcb_list->first; cbp; cbp=cbp->next)  {
		if(cbp->types&type) {
			LM_DBG("contact=%p, callback type %d/%d, id %d entered\n",
				c, type, cbp->types, cbp->id );
			cbp->callback( c, type, cbp->param );
		}
	}
}



#endif
