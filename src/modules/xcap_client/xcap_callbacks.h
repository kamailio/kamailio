/*
 * xcap_client module - Kamailio xcap client module
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
 *	
 */

#ifndef XCAP_CBACK
#define XCAP_CBACK

#include "../../str.h"

#define PRES_RULES         1<<1
#define RESOURCE_LIST      1<<2
#define RLS_SERVICE        1<<3
#define PIDF_MANIPULATION  1<<4
#define XCAP_CAPS          1<<5
#define USER_PROFILE       1<<6
#define PRES_CONTENT       1<<7
#define SEARCH             1<<8
#define DIRECTORY          1<<9

/* callback function prototype */
typedef int (xcap_cb)(int doc_type, str xid, char* doc);

/* register callback function prototype */
typedef int (*register_xcapcb_t)(int types, xcap_cb f);


typedef struct xcap_callback {
	int types;                   /* types of events that trigger the callback*/
	xcap_cb* callback;            /* callback function */
	struct xcap_callback* next;
}xcap_callback_t;

/* destroy registered callback list */
void destroy_xcapcb_list(void);

/* register a callback for several types of events */
int register_xcapcb( int types, xcap_cb f);

/* run all transaction callbacks for an composed type */
void run_xcap_update_cb(int type, str xid, char* stream);

#endif
