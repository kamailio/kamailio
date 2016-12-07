/*
 * pua module - presence user agent module
 *
 * Copyright (C) 2007 Voice Sistem S.R.L.
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
 */

#ifndef PUA_CBACK
#define PUA_CBACK

#include "../../parser/parse_fline.h"
#include "../pua/hash.h"

#define PUACB_MAX    		(1<<9)

/* callback function prototype */
typedef int (pua_cb)(ua_pres_t* hentity, struct sip_msg*);
/* register callback function prototype */
typedef int (*register_puacb_t)(int types, pua_cb f, void* param );


struct pua_callback {
	int id;                      /* id of this callback - useless */
	int types;                   /* types of events that trigger the callback*/
	pua_cb* callback;             /* callback function */
	void* param;
	struct pua_callback* next;
};

struct puacb_head_list {
	struct pua_callback *first;
	int reg_types;
};


extern struct puacb_head_list*  puacb_list;

int init_puacb_list(void);

void destroy_puacb_list(void);


/* register a callback for several types of events */
int register_puacb( int types, pua_cb f, void* param );

/* run all transaction callbacks for an event type */
static inline void run_pua_callbacks(ua_pres_t* hentity, struct sip_msg* msg)
{
	struct pua_callback *cbp;

	for (cbp= puacb_list->first; cbp; cbp=cbp->next)  {
		if(cbp->types & hentity->flag) 
		{	
			LM_DBG("found callback\n");
			cbp->callback(hentity, msg);
		}
	}
}

/* Q: should I call the registered callback functions when the modules refreshes a request? */
#endif
