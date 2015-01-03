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
 * This file defines the prototypes that implement the kamailioSIPRegUserTable.
 * For a full description of the table, please see the KAMAILIO-SIP-SERVER-MIB.
 *
 * Understanding this code will be much simpler with the following information:
 *
 * 1) All rows are indexed by an integer user index.  This is different from the
 *    usrloc module, which indexes by strings.  This less natural indexing
 *    scheme was required due to SNMP String index limitations.  (for example,
 *    SNMP has maximum index lengths.)
 *
 * 2) We need a quick way of mapping usrloc indices to our integer indices.  For
 *    this reason a string indexed Hash Table was created, with each entry mapping
 *    to an integer user index. 
 *
 *    This hash table is used by the kamailioSIPContactTable (the hash table also
 *    maps a user to its contacts), as well as the kamailioSIPRegUserLookupTable.
 *    The hash table is also used for quick lookups when a user expires. (i.e, it
 *    gives us a more direct reference, instead of having to search the whole
 *    table).
 *
 * 3) We are informed about new/expired users via a callback mechanism from the
 *    usrloc module.  Because of NetSNMP inefficiencies, we had to abstract this
 *    process.  Specifically:
 *
 *    - It can take a long time for the NetSNMP code base to populate a table with
 *      a large number of records. 
 *
 *    - We rely on callbacks for updated user information. 
 *
 *    Clearly, using the SNMPStats module in this situation could lead to some
 *    big performance loses if we don't find another way to deal with this.  The
 *    solution was to use an interprocess communications buffer.  
 *
 *    Instead of adding the record directly to the table, the callback functions
 *    now adds either an add/delete command to the interprocessBuffer.  When an
 *    snmp request is recieved by the SNMPStats sub-process, it will consume
 *    this interprocess buffer, adding and deleting users.  When it is finished,
 *    it can service the SNMP request.  
 *
 *    This doesn't remove the NetSNMP inefficiency, but instead moves it to a
 *    non-critical path.  Such an approach allows SNMP support with almost no
 *    overhead to the rest of Kamailio.
 */

#ifndef KAMAILIOSIPREGUSERTABLE_H
#define KAMAILIOSIPREGUSERTABLE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <net-snmp/net-snmp-config.h>
#include <net-snmp/library/container.h>
#include <net-snmp/agent/table_array.h>

#include "../../config.h"

/* Defines what each SNMP Row is made of. */
typedef struct kamailioSIPRegUserTable_context_s 
{
	netsnmp_index index; 

	unsigned long kamailioSIPUserIndex;

	/* There are potentially a lot of these of varying sizes, so lets
	 * allocate only the amount of memory we need when the row is
	 * created. */
	unsigned char *kamailioSIPUserUri;

	long kamailioSIPUserUri_len;

	unsigned long kamailioSIPUserAuthenticationFailures;

	void * data;

} kamailioSIPRegUserTable_context;

/*******************************/
/*    Customized Prototypes    */
/*******************************/

/* If the usrloc module is loaded, this function will grab hooks into its
 * callback registration function, and add handleContactCallbacks() as the
 * callback for UL_CONTACT_INSERT and UL_CONTACT_EXPIRE. 
 *
 * Returns 1 on success, and zero otherwise */
int registerForUSRLOCCallbacks(void);

/*
 * Creates a row and inserts it.  
 *
 * Returns: The rows userIndex on success, and 0 otherwise. 
 */
int  createRegUserRow(char *stringToRegister);


/* Removes an SNMP row indexed by userIndex, and frees the string and index it
 * pointed to. */
void  deleteRegUserRow(int userIndex);

/* Creates an 'aor to userindex' record from stringName and userIndex, and pushes
 * them onto the hash table. */
void  pushUserIntoHashTable(int userIndex, char *stringName);

/*
 * Adds or updates a user:
 *
 *   - If a user with the name userName exists, its 'number of contacts' count
 *     will be incremented.  
 *   - If the user doesn't exist, the user will be added to the table, and its
 *     number of contacts' count set to 1. 
 */
void updateUser(char *userName);

/*******************************/
/* Normal Function Prototypes  */
/*******************************/

/* Initializes the kamailioSIPRegUserTable module.  */
void  init_kamailioSIPRegUserTable(void);

/*
 * Initialize the kamailioSIPRegUserTable table by defining its contents and how
 * it's structured
 */
void  initialize_table_kamailioSIPRegUserTable(void);

const kamailioSIPRegUserTable_context * kamailioSIPRegUserTable_get_by_idx(
		netsnmp_index *);

const kamailioSIPRegUserTable_context * kamailioSIPRegUserTable_get_by_idx_rs(
		netsnmp_index *, int row_status);

/* Handles SNMP GET requests. */
int   kamailioSIPRegUserTable_get_value(
		netsnmp_request_info *, 
		netsnmp_index *, 
		netsnmp_table_request_info *);

/* OID Declarations. */
extern oid kamailioSIPRegUserTable_oid[];
extern size_t kamailioSIPRegUserTable_oid_len;

#define kamailioSIPRegUserTable_TABLE_OID KAMAILIO_OID,3,1,2,1,5,6
    
/* Column Definitions */
#define COLUMN_KAMAILIOSIPUSERINDEX                  1
#define COLUMN_KAMAILIOSIPUSERURI                    2
#define COLUMN_KAMAILIOSIPUSERAUTHENTICATIONFAILURES 3

#define kamailioSIPRegUserTable_COL_MIN 2
#define kamailioSIPRegUserTable_COL_MAX 3

#ifdef __cplusplus
}
#endif

#endif /** KAMAILIOSIPREGUSERTABLE_H */
