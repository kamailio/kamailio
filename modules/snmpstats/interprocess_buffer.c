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
 * \author jmagder
 *
 * This file implements the interprocess buffer, used for marshalling data
 * exchange from the usrloc module to the kamailioSIPRegUserTable,
 * kamailioSIPContactTable, and indirectly the kamailioSIPRegUserLookupTable.
 *
 * Details on why the interprocess buffer is needed can be found in the comments
 * at the top of interprocess_buffer.h
 * \ingroup snmpstats
 * - Module: \ref snmpstats
 */


#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>

#include "interprocess_buffer.h"
#include "snmpSIPContactTable.h"
#include "snmpSIPRegUserTable.h"
#include "hashTable.h"
#include "utilities.h"

#include "../usrloc/ul_callback.h"

/*!
 * The hash table:
 *
 *    1) maps all aor's to snmp's UserIndex for help in deleting SNMP Rows.
 *
 *    2) maps a given aor to a contact list. 
 */
hashSlot_t *hashTable = NULL;

/*! All interprocess communication is stored between these two declarations. */
interprocessBuffer_t *frontRegUserTableBuffer = NULL;
interprocessBuffer_t *endRegUserTableBuffer = NULL;

/*! This is to protect the potential racecondition in which a command is added to
 * the buffer while it is being consumed */
gen_lock_t           *interprocessCBLock = NULL;

/*!
 * This function takes an element of the interprocess buffer passed to it, and
 * handles populating the respective user and contact tables with its contained
 * data.  
 */
static void executeInterprocessBufferCmd(interprocessBuffer_t *currentBuffer);

/*!
 * Initialize shared memory used to buffer communication between the usrloc
 * module and the SNMPStats module.  (Specifically, the user and contact tables)
 */
int initInterprocessBuffers(void) 
{
	/* Initialize the shared memory that will be used to buffer messages
	 * over the usrloc module to RegUserTable callback. */
	frontRegUserTableBuffer =  shm_malloc(sizeof(interprocessBuffer_t));
	endRegUserTableBuffer   =  shm_malloc(sizeof(interprocessBuffer_t));

    if(frontRegUserTableBuffer == NULL || endRegUserTableBuffer == NULL)
    {
        LM_ERR("no more shared memory\n");
        return -1;
    }

	memset(frontRegUserTableBuffer, 0x00, sizeof(interprocessBuffer_t));
	memset(endRegUserTableBuffer,   0x00, sizeof(interprocessBuffer_t));

	/* Initialize a lock to the interprocess buffer.  The lock will be used
	 * to control race-conditions that would otherwise occur if an snmp
	 * command was received while the interprocess buffer was being consumed.
	 */
	interprocessCBLock = lock_alloc();
	if(interprocessCBLock==NULL)
	{
        LM_ERR("cannot allocate the lock\n");
        shm_free(frontRegUserTableBuffer);
        frontRegUserTableBuffer = NULL;
        shm_free(endRegUserTableBuffer);
        endRegUserTableBuffer = NULL;
        return -1;
	}
	lock_init(interprocessCBLock);

	hashTable = createHashTable(HASH_SIZE);
    if(hashTable == NULL)
    {
        LM_ERR("no more shared memory\n");
		lock_destroy(interprocessCBLock);
		lock_dealloc(interprocessCBLock);
        shm_free(frontRegUserTableBuffer);
        frontRegUserTableBuffer = NULL;
        shm_free(endRegUserTableBuffer);
        endRegUserTableBuffer = NULL;
        return -1;
    }

	return 1;
}

/*! USRLOC Callback Handler:
 *
 * This function should be registered to receive callbacks from the usrloc
 * module.  It can be called for any of the callbacks listed in ul_callback.h.
 * The callback type will be passed in 'type', and the contact the callback
 * applies to will be supplied in 'contactInfo.  This information will be copied
 * into the interprocess buffer.  The interprocess buffer will be consumed at a
 * later time, when consumeInterprocessBuffer() is called.  
 *
 * This callback is thread safe with respect to the consumeInterprocessBuffer()
 * function.  Specifically, the interprocess buffer should not be corrupted by
 * any race conditions between this function and the consumeInterprocessBuffer()
 * function.
 */
void handleContactCallbacks(ucontact_t *contactInfo, int type, void *param) 
{
	char *addressOfRecord;
	char *contact;

	interprocessBuffer_t *currentBufferElement;

	currentBufferElement = shm_malloc(sizeof(interprocessBuffer_t));

	if (currentBufferElement == NULL) 
	{
		goto error;
	}

	/* We need to maintain our own copies of the AOR and contact address to
	 * prevent the corruption of our internal data structures.  
	 *
	 * If we do not maintain our own copies, then the AOR and contact adress
	 * pointed to could be removed and reallocated to another thread before
	 * we get a chance to consume our interprocess buffer.  */
	convertStrToCharString(contactInfo->aor,  &addressOfRecord);
	convertStrToCharString(&(contactInfo->c), &contact);

	currentBufferElement->stringName    = addressOfRecord;
	currentBufferElement->stringContact = contact;
	currentBufferElement->contactInfo   = contactInfo;
	currentBufferElement->callbackType  = type;
	currentBufferElement->next          = NULL;


	/* A lock is necessary to prevent a race condition.  Specifically, it
	 * could happen that we find the front of the buffer to be non-null,
	 * are scheduled out, the entire buffer (or part of it) is consumed and
	 * freed, and then we assign our list to deallocated memory. */
	lock_get(interprocessCBLock);

	/* This is the first element to be added. */
	if (frontRegUserTableBuffer->next == NULL) {
		frontRegUserTableBuffer->next     = currentBufferElement;
	} else {
		endRegUserTableBuffer->next->next = currentBufferElement;
	}
	
	endRegUserTableBuffer->next   = currentBufferElement;

	lock_release(interprocessCBLock);
	
	return;

error:
	LM_ERR("Not enough shared memory for  kamailioSIPRegUserTable insert."
			" (%s)\n", contactInfo->c.s);
}


/*! Interprocess Buffer consumption Function.  This function will iterate over
 * every element of the interprocess buffer, and add or remove the specified
 * contacts and users.  Whether the contacts are added or removed is dependent
 * on if the original element was added as a result of a UL_CONTACT_INSERT or
 * UL_CONTACT_EXPIRE callback.
 *
 * The function will free any memory occupied by the interprocess buffer.
 *
 * \note This function is believed to be thread safe.  Specifically, it protects
 *       corruption of the interprocess buffer through the interprocessCBLock.
 *       This ensures no corruption of the buffer by race conditions.  The lock
 *       has been designed to be occupied for as short a period as possible, so 
 *       as to prevent long waits.  Specifically, once we start consumption of 
 *       the list, other processes are free to continue even before we are done.
 *       This is made possible by simply changing the head of the interprocess
 *       buffer, and then releasing the lock.  
 */
void consumeInterprocessBuffer(void) 
{
	interprocessBuffer_t *previousBuffer;
	interprocessBuffer_t *currentBuffer;
	
	/* There is nothing to consume, so just exit. */
	if (frontRegUserTableBuffer->next == NULL) 
	{
		return;
	}

	/* We are going to consume the entire buffer, but we don't want the
	 * buffer to change midway through.  So assign the front of the buffer
	 * to NULL so that any other callbacks from the usrloc module will be
	 * appended to a new list.  We need to be careful to get a lock first
	 * though, to avoid race conditions. */
	lock_get(interprocessCBLock);

	currentBuffer = frontRegUserTableBuffer->next;
	
	frontRegUserTableBuffer->next = NULL;
	endRegUserTableBuffer->next   = NULL;

	lock_release(interprocessCBLock);

	while (currentBuffer != NULL) {

		executeInterprocessBufferCmd(currentBuffer);

		/* We need to assign the current buffer to a temporary place
		 * before we move onto the next buffer.  Otherwise the memory
		 * could be modified between freeing it and moving onto the next
		 * buffer element. */
		previousBuffer = currentBuffer;
		currentBuffer = currentBuffer->next;
		shm_free(previousBuffer->stringName);
		shm_free(previousBuffer->stringContact);
		shm_free(previousBuffer);

	}

}


/*!
 * This function takes an element of the interprocess buffer passed to it, and
 * handles populating the respective user and contact tables with its contained
 * data.  
 */
static void executeInterprocessBufferCmd(interprocessBuffer_t *currentBuffer) 
{
	int delContactIndex;

	aorToIndexStruct_t *currentUser;

	if (currentBuffer->callbackType == UL_CONTACT_INSERT) 
	{
		/* Add the user if the user doesn't exist, or increment its 
		 * contact index otherwise. */
		updateUser(currentBuffer->stringName);
	}
	else if (currentBuffer->callbackType != UL_CONTACT_EXPIRE)
	{
		/* Currently we only support UL_CONTACT_INSERT and
		 * UL_CONTACT_EXPIRE.  If we receive another callback type, this
		 * is a bug. */
		LM_ERR("found a command on the interprocess buffer that"
				" was not an INSERT or EXPIRE");
		return;
	}

	currentUser =
		findHashRecord(hashTable, currentBuffer->stringName, HASH_SIZE);


	/* This should never happen.  This is more of a sanity check. */
	if (currentUser == NULL) {
		LM_ERR("Received a request for contact: %s for user: %s who doesn't "
				"exists\n", currentBuffer->stringName, 
				currentBuffer->stringContact);
		return;
	} 

	/* This buffer element specified that we need to add a contact.  So lets
	 * add them */
	if (currentBuffer->callbackType == UL_CONTACT_INSERT) {

		/* Increment the contact index, which will be used to generate
		 * our new row.  */  
		currentUser->contactIndex++;

		/* We should do this after we create the row in the snmptable.
		 * Its easier to delete the SNMP Row than the contact record. */
		if(!insertContactRecord(&(currentUser->contactList), 
			currentUser->contactIndex, 
				currentBuffer->stringContact)) {

			LM_ERR("kamailioSIPRegUserTable was unable to allocate memory for "
					"adding contact: %s to user %s.\n",
					currentBuffer->stringName, currentBuffer->stringContact);

			/* We didn't use the index, so decrement it so we can
			 * use it next time around. */
			currentUser->contactIndex--;
			
			return;
		}
	
		if (!createContactRow(currentUser->userIndex, 
					currentUser->contactIndex,
					currentBuffer->stringContact, 
					currentBuffer->contactInfo)) {
		
			deleteContactRecord(&(currentUser->contactList), 
					currentBuffer->stringContact);

		}

	}
	else {

		delContactIndex = 
			deleteContactRecord(&(currentUser->contactList), 
					currentBuffer->stringContact);

		/* This should never happen.  But its probably wise to check and
		 * to print out debug messages in case there is a hidden bug.  */
		if(delContactIndex == 0) {
			
			LM_ERR("Received a request to delete contact: %s for user: %s"
				"  who doesn't exist\n", currentBuffer->stringName,
				currentBuffer->stringContact);
			return;

		}		

		deleteContactRow(currentUser->userIndex, delContactIndex);

		deleteUser(hashTable, currentBuffer->stringName, HASH_SIZE);
	}
}

void freeInterprocessBuffer(void)
{
    interprocessBuffer_t *currentBuffer, *previousBuffer;

	if (frontRegUserTableBuffer==NULL
			|| frontRegUserTableBuffer->next == NULL
			|| endRegUserTableBuffer==NULL) {
        LM_DBG("Nothing to clean\n");
		return;
	}

	currentBuffer = frontRegUserTableBuffer->next;
	
	frontRegUserTableBuffer->next = NULL;
	endRegUserTableBuffer->next   = NULL;


	while (currentBuffer != NULL) {

        previousBuffer = currentBuffer;
        currentBuffer = currentBuffer->next;
        shm_free(previousBuffer->stringName);
        shm_free(previousBuffer->stringContact);
        shm_free(previousBuffer);

	}

    if(frontRegUserTableBuffer)
        shm_free(frontRegUserTableBuffer);

    if(endRegUserTableBuffer)
        shm_free(endRegUserTableBuffer);

}
