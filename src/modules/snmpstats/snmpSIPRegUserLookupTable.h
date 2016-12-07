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
 * This file implements the kamailioSIPRegUserLookupTable.  For a full
 * description of the table, please see the KAMAILIO-SIP-SERVER-MIB.
 *
 * This file consists of many more functions than the other header files.  
 * This is because this table is writable, bringing a lot of SNMP overhead.
 *
 * Most of the contents are auto-generated (aside from white space and comment
 * changes), and can be ignored.  The functions that have been modified are:
 *
 * 1) kamailioSIPRegUserLookupTable_extract_index() 
 *
 * 2) kamailioSIPRegUserLookupTable_can_[activate|deactivate|delete]()
 *
 * 3) kamailioSIPRegUserLookupTable_set_reserve1()
 *
 * 4) kamailioSIPRegUserLookupTable_set_action()
 *
 * Full details can be found in kamailioSIPRegUserLookupTable.c.  You can safely
 * ignore the other functions.  
 */

#ifndef KAMAILIOSIPREGUSERLOOKUPTABLE_H
#define KAMAILIOSIPREGUSERLOOKUPTABLE_H

#ifdef __cplusplus
extern "C" {
#endif

    
#include <net-snmp/net-snmp-config.h>
#include <net-snmp/library/container.h>
#include <net-snmp/agent/table_array.h>

#include "../../config.h"


/* This structure represnts a row in the table */
typedef struct kamailioSIPRegUserLookupTable_context_s 
{
	netsnmp_index index; 

	/** UNSIGNED32 = ASN_UNSIGNED */
	unsigned long  kamailioSIPRegUserLookupIndex;

	/** SnmpAdminString = ASN_OCTET_STR */
	unsigned char *kamailioSIPRegUserLookupURI;
	long           kamailioSIPRegUserLookupURI_len;

	/** UNSIGNED32 = ASN_UNSIGNED */
	unsigned long  kamailioSIPRegUserIndex;

	/** RowStatus = ASN_INTEGER */
	long kamailioSIPRegUserLookupRowStatus;

	void * data;

} kamailioSIPRegUserLookupTable_context;

/*
 * Initializes the kamailioSIPRegUserLookupTable table.  This step is easier
 * than in the other tables because there is no table population.  All table
 * population takes place during run time. 
 */
void init_kamailioSIPRegUserLookupTable(void);


/*
 * Initialize the kamailioSIPRegUserLookupTable table by defining how it is
 * structured. 
 *
 * This function is mostly auto-generated.
 */
void initialize_table_kamailioSIPRegUserLookupTable(void);

/* 
 * This function was auto-generated and didn't need modifications from its
 * auto-generation.  It is called to handle an SNMP GET request. 
 */
int kamailioSIPRegUserLookupTable_get_value(netsnmp_request_info *, 
		netsnmp_index *, netsnmp_table_request_info *);

const kamailioSIPRegUserLookupTable_context * 
	kamailioSIPRegUserLookupTable_get_by_idx(netsnmp_index *);

const kamailioSIPRegUserLookupTable_context * 
	kamailioSIPRegUserLookupTable_get_by_idx_rs(
			netsnmp_index *, 
			int row_status);

/* oid declarations */
extern oid    kamailioSIPRegUserLookupTable_oid[];
extern size_t kamailioSIPRegUserLookupTable_oid_len;


#define kamailioSIPRegUserLookupTable_TABLE_OID KAMAILIO_OID,3,1,2,1,5,9


/* column number definitions for table kamailioSIPRegUserLookupTable */
#define COLUMN_KAMAILIOSIPREGUSERLOOKUPINDEX     1
#define COLUMN_KAMAILIOSIPREGUSERLOOKUPURI       2
#define COLUMN_KAMAILIOSIPREGUSERINDEX           3
#define COLUMN_KAMAILIOSIPREGUSERLOOKUPROWSTATUS 4

#define kamailioSIPRegUserLookupTable_COL_MIN 2
#define kamailioSIPRegUserLookupTable_COL_MAX 4


/* Handles index extraction for row creation */
int kamailioSIPRegUserLookupTable_extract_index( 
		kamailioSIPRegUserLookupTable_context *ctx, netsnmp_index *hdr);

/* Handle RESERVE1 and RESERVE2 phases of an SNMP SET */
void kamailioSIPRegUserLookupTable_set_reserve1(netsnmp_request_group *);
void kamailioSIPRegUserLookupTable_set_reserve2(netsnmp_request_group *);

/* Handle the SET and ACTION phases of an SNMP SET */
void kamailioSIPRegUserLookupTable_set_action(netsnmp_request_group *);
void kamailioSIPRegUserLookupTable_set_commit(netsnmp_request_group *);

/* Handle Resource cleanup if the ACTION or RESERVE1/RESERVE2 phases of an
 * SNMPSET fail */
void kamailioSIPRegUserLookupTable_set_free(netsnmp_request_group *);
void kamailioSIPRegUserLookupTable_set_undo(netsnmp_request_group *);

kamailioSIPRegUserLookupTable_context * 
	kamailioSIPRegUserLookupTable_duplicate_row(
			kamailioSIPRegUserLookupTable_context*);

netsnmp_index * kamailioSIPRegUserLookupTable_delete_row(
		kamailioSIPRegUserLookupTable_context*);

/* Used to check if there is a reason why a row can't be activated 
 * (There is no reason in our implementation)
 */
int kamailioSIPRegUserLookupTable_can_activate(
		kamailioSIPRegUserLookupTable_context *undo_ctx,
		kamailioSIPRegUserLookupTable_context *row_ctx,
		netsnmp_request_group * rg);

/* Used to check if there is a reason why a row can't be deactivated 
 * (There is no reason in our implementation)
 */
int kamailioSIPRegUserLookupTable_can_deactivate(
		kamailioSIPRegUserLookupTable_context *undo_ctx,
		kamailioSIPRegUserLookupTable_context *row_ctx,
		netsnmp_request_group * rg);

/* Used to check if there is a reason why a row can't be deleted
 * (There is no reason in our implementation)
 */
int kamailioSIPRegUserLookupTable_can_delete(
		kamailioSIPRegUserLookupTable_context *undo_ctx,
		kamailioSIPRegUserLookupTable_context *row_ctx,
		netsnmp_request_group * rg);
	
/* Basic structural setups of the new row */
kamailioSIPRegUserLookupTable_context * kamailioSIPRegUserLookupTable_create_row(
		netsnmp_index*);


#ifdef __cplusplus
}
#endif

#endif /** KAMAILIOSIPREGUSERLOOKUPTABLE_H */
