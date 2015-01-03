/*
 * SNMPStats Module 
 * Copyright (C) 2006 SOMA Networks, INC.
 * Written by: Jeffrey Magder (jmagder@somanetworks.com)
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * Kamailio is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * Kamailio is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301
 * USA
 *
 */

/*!
 * \file
 * \brief SNMP statistic module, interprocess buffer
 *
 * The SNMPStats module exposes user information through kamailioSIPRegUserTable,
 * kamailioSIPContactTable, and kamailioSIPRegUserLookupTable.  These tables are
 * populated through callback mechanisms from the usrloc module.  Unfortunately
 * the NetSNMP table population code is very slow when dealing with large
 * amounts of data.  Because we don't want to experience a performance hit when
 * registering users, we make use of the interprocess buffer.  Specifically,
 * instead of adding/removing users/contacts from the SNMP tables directly, the
 * callbacks add an add/delete command to the interprocessBuffer.
 *
 * When an snmp request is recieved by the SNMPStats sub-process, it will
 * consume this interprocess buffer, adding and deleting users.  When it is
 * finished, it can service the SNMP request.
 *
 * This doesn't remove the NetSNMP inefficiency of course, but it does move it
 * to a non-critical path.  Such an approach allows SNMP support with almost no
 * overhead to the rest of the server.
 * \ingroup snmpstats
 * - Module: \ref snmpstats
 */


#ifndef _SNMPSTATS_USER_UTILITIES_
#define _SNMPSTATS_USER_UTILITIES_

#include "../../str.h"
#include "../../locking.h"

#include "snmpstats_globals.h"
#include "hashTable.h"

#include "../usrloc/ucontact.h"

/* Represents an element of the interprocess buffer. */
typedef struct interprocessBuffer 
{
	char  *stringName;
	char  *stringContact;
	int   callbackType;
	struct interprocessBuffer *next;

	ucontact_t *contactInfo;

} interprocessBuffer_t;

/* Both of these will be used to reference in the interprocess buffer */
extern interprocessBuffer_t *frontRegUserTableBuffer;
extern interprocessBuffer_t *endRegUserTableBuffer;

/* A request to consume the interprocess buffer could occur at the same time
 * there is a request to add to the interprocess buffer. (Or vice-versa).  This
 * lock is used to prevent these race conditions. */
extern gen_lock_t           *interprocessCBLock;
extern hashSlot_t           *hashTable;

/*
 * Initialize shared memory used to buffer communication between the usrloc
 * module and the SNMPStats module.  (Specifically, the user and contact tables)
 */
int  initInterprocessBuffers(void);

/* USRLOC Callback Handler:
 *
 * This function should be registered to receive callbacks from the usrloc
 * module.  It can be called for any of the callbacks listed in ul_Callback.h.
 * The callback type will be passed in 'type', and the contact the callback
 * applies to will be supplied in 'contactInfo.  This information will be copied
 * into the interprocess buffer.  The interprocess buffer will beconsumed at a
 * later time, when consumeInterprocessBuffer() is called.  
 *
 * This callback is thread safe with respect to the consumeInterprocessBuffer()
 * function.  Specifically, the interprocess buffer should not be corrupted by
 * any race conditions between this function and the consumeInterprocessBuffer()
 * function.
 */
void handleContactCallbacks(ucontact_t *contactInfo, int type, void *param);


/* Interprocess Buffer consumption Function.  This function will iterate over
 * every element of the interprocess buffer, and add or remove the specified
 * contacts and users.  Whether the contacts are added or removed is dependent
 * on if the original element was added as a result of a UL_CONTACT_INSERT or
 * UL_CONTACT_EXPIRE callback.
 *
 * The function will free any memory occupied by the interprocess buffer.
 *
 * Note: This function is believed to be thread safe.  Specifically, it protects
 *       corruption of the interprocess buffer through the interprocessCBLock.
 *       This ensures no corruption of the buffer by race conditions.  The lock
 *       has been designed to be occupied for as short a period as possible, so 
 *       as to prevent long waits.  Specifically, once we start consumption of 
 *       the list, other processes are free to continue even before we are done.
 *       This is made possible by simply changing the head of the interprocess
 *       buffer, and then releasing the lock.  
 */
void consumeInterprocessBuffer(void);

void freeInterprocessBuffer(void);

#endif
