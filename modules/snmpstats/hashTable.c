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
 * \brief SNMP statistic module, hash table
 * Hash Stuff;
 * \author jmagder
 *
 * For an overview of its structures, please see hashTable.h
 *
 * Potential Performance Improvements: Pass the length of the aor strings around
 * everywhere, so we don't have to calculate it ourselves.
 * \ingroup snmpstats
 * - Module: \ref snmpstats
 */

#include <stdlib.h>
#include <string.h>

#include "hashTable.h"
#include "../../dprint.h"
#include "../../mem/mem.h"

#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>
#include "snmpSIPRegUserTable.h"


/*! Calculates and returns a hash index to a hash table.  The index is calculated
 * by summing up all the characters specified with theString, and using the
 * hashTableSize as the modulus.  */
int calculateHashSlot(char *theString, int hashTableSize) 
{
	char *currentCharacter = theString;
	int   runningTotal     = 0;

	while (*currentCharacter != '\0') {
		runningTotal += *currentCharacter;
		currentCharacter++;
	}

	return runningTotal % hashTableSize;
}

/*! Searches the hash table specified as theTable, of size 'size', for a record
 * indexed with 'aor'.  If a match is found, then an aorToIndextStruct_t
 * structure is returned. 
 *
 * This function is called to discover the map between Kamailio's "aor" 
 * (Address of Records) indexing scheme, and the SNMPStats modules integer
 * indexing scheme for its contact/user data. 
 *
 * Returns: the aorToIndexStruct_t mapping structure if a match was found, 
 *          or NULL otherwise.
 */
aorToIndexStruct_t *findHashRecord(hashSlot_t *theTable, char *aor, int size)
{
	int hashIndex = calculateHashSlot(aor, size);
	int aorStringLength = strlen(aor);

	aorToIndexStruct_t *currentRecord = theTable[hashIndex].first;

	while (currentRecord != NULL) {
		
		/* If the strings are the same length and the same in every
		 * other way, then return the given record. */
		if (currentRecord->aorLength == aorStringLength &&
		    memcmp(currentRecord->aor, aor, aorStringLength)==0) {
			return currentRecord;
		}

		currentRecord = currentRecord->next;
	}

	return NULL;
}


/*! Returns a chunk of memory large enough to store 'size' hashSlot's.  The
 * table will contain mappings between Kamailio's "aor" user/contact indexing
 * scheme, and SNMPStats integer indexing scheme */
hashSlot_t  *createHashTable(int size) 
{
	hashSlot_t *hashTable     = NULL;
	int         numberOfBytes = sizeof(hashSlot_t)*size;

	hashTable = pkg_malloc(numberOfBytes);

	if (!hashTable)
	{
		LM_ERR("no more pkg memory");
		return NULL;
	}

	memset(hashTable, 0, numberOfBytes);

	return hashTable;
}


/*! Inserts the record specified with 'theRecord' into our hash table. */
void insertHashRecord(hashSlot_t *theTable, aorToIndexStruct_t *theRecord, 
		int size) 
{
	int hashIndex = calculateHashSlot(theRecord->aor, size);

	/* Link up this record backward so that it points to whatever the last
	 * 'last element' was.  */
	theRecord->prev = theTable[hashIndex].last;

	/* This is the first record in the hash table, so assign the first and
	 * last pointers to this record. */
	if (theTable[hashIndex].last == NULL) {
		
		theTable[hashIndex].last  = theRecord;
		theTable[hashIndex].first = theRecord;

	} else {
		
		/* Make the element that was previously the last element point
		 * to this new record, as its next element. */
		theTable[hashIndex].last->next = theRecord;

		/* Reassign the 'final element' pointer to this new record. */
		theTable[hashIndex].last = theRecord;

	}
	
}

/*!
 * This function will search the provided hash table for an entry indexed by
 * 'aor'.  If an entry is found then: 
 *
 *   - Its numContacts counter will be decremented.
 *   - If its numContacts counter reaches zero, then the entry will be removed
 *     from the hash table.
 *
 */
void deleteUser(hashSlot_t *theTable, char *aor, int hashTableSize)
{
	int hashIndex = calculateHashSlot(aor, hashTableSize);
	int searchStringLength = strlen(aor);

	aorToIndexStruct_t *currentRecord  = theTable[hashIndex].first;

	while (currentRecord != NULL) {

		/* First make sure both strings are the same length.  If so,
		 * then compare all bytes.  If this succeeds, then we need to
		 * link up the previous and next element together. */
		if (currentRecord->aorLength == searchStringLength &&
		    memcmp(currentRecord->aor, aor, searchStringLength) == 0) {

			currentRecord->numContacts--;

			/* There are still contacts relying on this user, so
			 * don't delete anything. */
			if (currentRecord->numContacts > 0) 
			{
				return;
			}

			/* There are no more contacts relying on this user, so
			 * delete the row from the table. */
			deleteRegUserRow(currentRecord->userIndex);


			/* Maintenance of the hash table */

			if (currentRecord->prev == NULL) 
			{
					/* Edge Case: First element in list was just deleted, so set
					 * up the first element to point to the one after the one
					 * just deleted */
					theTable[hashIndex].first = currentRecord->next;
			}
			else
			{
					/* Not the first element, so hook up the previous node to
					 * the node after the one just deleted. */
					currentRecord->prev->next = currentRecord->next;
			}

			if (currentRecord->next == NULL)
			{
					/* Edge Case: The last element has been targetted for
					 * deletion.  So move the pointer to the node just before
					 * this one.  */
					theTable[hashIndex].last = currentRecord->prev;
			}
			else
			{
					/* Not the last element, so hook up next nodes previous
					 * element to this nodes previous.  */
					currentRecord->next->prev = currentRecord->prev;
			}

			pkg_free(currentRecord);

			/* We are done, so just return. */
			return;
		}

		/* Advance to the next records. */
		currentRecord = currentRecord->next;
	}

}


/*! Returns a aorToIndexStruct_t, holding the given 'userIndex' and 'aor'.  The
 * structure is used to map between the "aor" (Kamailio's way of indexing
 * users/contacts), and the SNMPStats user and contact integer indexes.  
 *
 * NOTE: that this record does not make a copy of aor, but instead points
 * directly to the parameter.  Therefore make sure that aor is not on the stack,
 * and is not going to disappear before this record is deleted. 
 */
aorToIndexStruct_t *createHashRecord(int userIndex, char *aor) 
{
	int aorLength =strlen(aor);

    aorToIndexStruct_t *theRecord = pkg_malloc(sizeof(aorToIndexStruct_t)+
            (aorLength+1)* sizeof(char));
	if (theRecord == NULL)
	{
		LM_ERR("failed to create a mapping record for %s", aor);
		return NULL;
	}

	memset(theRecord, 0, sizeof(aorToIndexStruct_t));

	theRecord->aor = (char*)theRecord + sizeof(aorToIndexStruct_t);
    memcpy(theRecord->aor, aor, aorLength );
	theRecord->aor[aorLength] = '\0';
    theRecord->aorLength = aorLength;
	theRecord->userIndex = userIndex;
	theRecord->numContacts = 1;

	return theRecord;
}




/*! Debugging function.  Prints off an entire hash slot. */
void printHashSlot(hashSlot_t *theTable, int index) 
{
	aorToIndexStruct_t *currentRecord = theTable[index].first;

	LM_ERR("dumping Hash Slot #%d\n", index);

	while (currentRecord != NULL) {
		LM_ERR( "\tString: %s - Index: %d\n", 
				currentRecord->aor, currentRecord->userIndex);
		currentRecord = currentRecord->next;
	}
}
