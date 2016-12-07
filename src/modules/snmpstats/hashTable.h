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
 *
 * This file describes several structure.  In general, it was necessary to map
 * between Kamailio's "aor" (Address of Record) and string indexing mechanisms,
 * and the SNMPStats modules integer indexing scheme for users and contacts.
 * While it would have been a more natural fit to use string indexes in the
 * SNMPStats module, SNMP limitations precluded this.  
 *
 * aorToIndexStruct: maps an aor to:
 *  - a userIndex, to uniquely identify each RegUserTable SNMP row
 *  - a contactList, containing all contacts for the user specified by
 *    userIndex.
 *
 * The aorToIndexStruct also contains a numContacts counter. Each time a new
 * contact is associated with the aor (user), the counter is incremented.  Each
 * time a contact is dissasociated (due to an expiration), the counter is
 * decremented.  When the counter reaches zero the structure will be deleted.
 *
 *  contactToIndexStruct: maps a contact name to:
 *   - a contactIndex, used to uniquely identify each ContactTable SNMP row.
 * \ingroup snmpstats
 * - Module: \ref snmpstats
 */


#ifndef HASHSLOT_H
#define HASHSLOT_H

/*!
 * Used to map between a 'contact' name (Kamailio's index) and a contact index.
 * (SNMPStats Index) 
 */
typedef struct contactToIndexStruct 
{
	char *contactName;

	int contactIndex;

	struct contactToIndexStruct *next;

} contactToIndexStruct_t;


/*!
 * Used to map between an 'aor' (Kamailio index) and a user index. (SNMPStats
 * index).  Since each user can have multiple contacts, the structure also has a
 * 'contactIndex', and a reference to the contactToIndexStruct list. 
 */
typedef struct aorToIndexStruct 
{
	/* Pointer to the actual address record in the given SNMP row. */
	char *aor;
	int  aorLength;

	/* Points to the user index, which is used to uniquely identify each
	 * SNMP row in a table. */
	int userIndex;

	/* Each contact needs a unique index, for each user.  This value should
	 * be incremented each time a contact is added.  This way, we can know
	 * what index to use for the next addition to the contactList.  */
	int contactIndex;

	/* Pointer to the contact list. */
	contactToIndexStruct_t *contactList;

	struct aorToIndexStruct *prev;

	/* The structure is part of a hash table, so this element is needed so
	 * that we can point to the next element in the colission slot. */
	struct aorToIndexStruct *next;

	/* This counter will be incremented when a new contact is associated
	 * with this user record, and will be decremented each time an
	 * associated contact is removed.  When the count reaches 0, it is safe
	 * to remove this record. */
	int numContacts;

} aorToIndexStruct_t;


typedef struct hashSlot 
{
	/*! Number of elements in this list. */
	int numberOfElements;

	/*! First element in the list. */
	struct aorToIndexStruct* first; 

	/*! Last element in the list.  This is here for optimization purposes.
	 * It stands to reason that things added later will need to be deleted
	 * later.  So they should be added to the end of the list.  This way,
	 * things that are to be deleted sooner will be at the front of the
	 * list. */
	struct aorToIndexStruct* last; 

} hashSlot_t;

/*******************************************************************
* More detailed function definitions can be found in hashTable.c   */


/*! Returns a aorToIndexStruct_t, holding the given 'userIndex' and 'aor'.  The
 * structure is used to map between the "aor" (Kamailio's way of indexing
 * users/contacts), and the SNMPStats user and contact integer indexes.  
 *
 * \note This record does not make a copy of aor, but instead points
 * directly to the parameter.  Therefore make sure that aor is not on the stack,
 * and is not going to disappear before this record is deleted. 
 */
aorToIndexStruct_t *createHashRecord(int userIndex, char *aor);


/*! Returns a chunk of memory large enough to store 'size' hashSlot's.  The
 * table will contain mappings between Kamailio's "aor" user/contact indexing
 * scheme, and SNMPStats integer indexing scheme */
hashSlot_t  *createHashTable(int size);


/*! Calculates and returns a hash index to a hash table.  The index is calculated
 * by summing up all the characters specified with theString, and using the
 * hashTableSize as the modulus.  */
int calculateHashSlot(char *theString, int hashTableSize);


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
aorToIndexStruct_t *findHashRecord(hashSlot_t *theTable, char *aor, int size);


/*! Inserts theRecord into an appropriate place in theTable, when size is given. */
void insertHashRecord(hashSlot_t *theTable, aorToIndexStruct_t *theRecord, int size); 


/*! Debugging function.  Prints off an entire hash slot. */
void printHashSlot(hashSlot_t *theTable, int index);


/*! If a record is found with string aor in theTable, it is deleted and its
 * SNMPStats user integer index is returned. */
int deleteHashRecord(hashSlot_t *theTable, char *aor, int hashTableSize); 

/*!
 * This function will search the provided hash table for an entry indexed by
 * 'aor'.  If an entry is found then: 
 *
 *   - Its numContacts counter will be decremented.
 *   - If its numContacts counter reaches zero, then the entry will be removed
 *     from the hash table.
 *
 */
void deleteUser(hashSlot_t *theTable, char *aor, int hashTableSize);

#endif
