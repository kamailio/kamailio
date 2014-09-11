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

#ifndef __CDP_API_PROCESS_H_
#define __CDP_API_PROCESS_H_

#include "peer.h"
#include "diameter.h"

/**
 * Request types for handler switching.
 */
enum handler_types {
	REQUEST_HANDLER=0, 	/**< the message received is a request */
	RESPONSE_HANDLER=1  /**< the message received is a response */
};

/**
 * Diameter message received handler list element.
 */ 
typedef struct handler_t{
	enum handler_types type;					/**< type of the handler */
	union {
		AAARequestHandler_f *requestHandler;	/**< request callback function */
		AAAResponseHandler_f *responseHandler;  /**< response callback function */
	} handler;									/**< union for handler callback function */
	void *param;								/**< transparent parameter to pass to callback */
	struct handler_t *next;				/**< next handler in the list */
	struct handler_t *prev;				/**< prev handler in the list */
} handler;
		
typedef struct handler_list_t{
	handler *head;				/**< first handler in the list */
	handler *tail;				/**< last handler in the list */
} handler_list;
		
int api_callback(peer *p,AAAMessage *msg,void* ptr);

#endif /*API_PROCESS_H_*/
