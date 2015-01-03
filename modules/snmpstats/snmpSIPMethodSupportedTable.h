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
 * This file defines the prototypes used to define the
 * kamailioSIPMethodSupportedTable.  For full details, please see the
 * KAMAILIO-SIP-COMMON-MIB.
 */

#ifndef KAMAILIOSIPMETHODSUPPORTEDTABLE_H
#define KAMAILIOSIPMETHODSUPPORTEDTABLE_H

#ifdef __cplusplus
extern "C" {
#endif

    
#include <net-snmp/net-snmp-config.h>
#include <net-snmp/library/container.h>
#include <net-snmp/agent/table_array.h>

#include "../../config.h"

/* 
 * This strucutre represents a single row in the SNMP table, and is mostly
 * auto-generated. 
 */
typedef struct kamailioSIPMethodSupportedTable_context_s {

	netsnmp_index index;

	/** KamailioSIPMethodIdentifier = ASN_UNSIGNED */
	unsigned long kamailioSIPMethodSupportedIndex;

	/** SnmpAdminString = ASN_OCTET_STR */
	unsigned char *kamailioSIPMethodName;
	
	long kamailioSIPMethodName_len;

	void * data;

} kamailioSIPMethodSupportedTable_context;


/* Initializes the kamailioSIPMethodSupportedTable, and populates the tables 
 * contents */
void init_kamailioSIPMethodSupportedTable(void);

/* Defines kamailioSIPMethodSupportedTable's structure and callback mechanisms */
void initialize_table_kamailioSIPMethodSupportedTable(void);


/* 
 * This routine is called to process get requests for elements of the table.
 *
 * The function is pretty much left as is from the auto-generated code. 
 */
int kamailioSIPMethodSupportedTable_get_value(netsnmp_request_info *, 
		netsnmp_index *, netsnmp_table_request_info *);

const kamailioSIPMethodSupportedTable_context * 
	kamailioSIPMethodSupportedTable_get_by_idx(netsnmp_index *);

const kamailioSIPMethodSupportedTable_context * 
	kamailioSIPMethodSupportedTable_get_by_idx_rs(netsnmp_index *,
			int row_status);

/*
 * oid declarations
 */
extern oid    kamailioSIPMethodSupportedTable_oid[];
extern size_t kamailioSIPMethodSupportedTable_oid_len;

#define kamailioSIPMethodSupportedTable_TABLE_OID KAMAILIO_OID,3,1,1,1,1,7
    
/*
 * column number definitions for table kamailioSIPMethodSupportedTable
 */
#define COLUMN_KAMAILIOSIPMETHODSUPPORTEDINDEX  1
#define COLUMN_KAMAILIOSIPMETHODNAME            2

#define kamailioSIPMethodSupportedTable_COL_MIN 2
#define kamailioSIPMethodSupportedTable_COL_MAX 2


#ifdef __cplusplus
}
#endif

#endif /** KAMAILIOSIPMETHODSUPPORTEDTABLE_H */
