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
 * Originally Generated with Mib2c using mib2c.array-user.conf.
 *
 * This file defines the kamailioSIPPortTable prototypes.  For a full description
 * of the table, please see the KAMAILIO-SIP-COMMON-MIB.
 */

#ifndef KAMAILIOSIPPORTTABLE_H
#define KAMAILIOSIPPORTTABLE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <net-snmp/net-snmp-config.h>
#include <net-snmp/library/container.h>
#include <net-snmp/agent/table_array.h>

#include "../../config.h"

#define SIP_PORT_TABLE_STR_INDEX_SIZE 22

/* This structure represents a single row in the table. */
typedef struct kamailioSIPPortTable_context_s 
{

	netsnmp_index index; 
	
	unsigned char kamailioSIPStringIndex[SIP_PORT_TABLE_STR_INDEX_SIZE];
	unsigned long kamailioSIPStringIndex_len;

	unsigned char kamailioSIPTransportRcv[2];
	long kamailioSIPTransportRcv_len;

	void * data;

} kamailioSIPPortTable_context;


/*
 * Initializes the kamailioSIPPortTable module.  
 *
 * Specifically, this function will define the tables structure, and then
 * populate it with the ports and transports that Kamailio is listening on.
 *
 */
void  init_kamailioSIPPortTable(void);


/* Initialize the kamailioSIPPortTable table by defining how it is structured */
void  initialize_table_kamailioSIPPortTable(void);


/*
 * This routine is called to process get requests for elements of the table.
 * The function is mostly left in its auto-generated form 
 */
int   kamailioSIPPortTable_get_value(netsnmp_request_info *, netsnmp_index *, 
		netsnmp_table_request_info *);

const kamailioSIPPortTable_context *kamailioSIPPortTable_get_by_idx(netsnmp_index *);

const kamailioSIPPortTable_context * kamailioSIPPortTable_get_by_idx_rs(netsnmp_index *, int row_status);

/*
 * oid declarations
 */
extern oid    kamailioSIPPortTable_oid[];
extern size_t kamailioSIPPortTable_oid_len;

#define kamailioSIPPortTable_TABLE_OID KAMAILIO_OID,3,1,1,1,1,5
	
/*
 * column number definitions for table kamailioSIPPortTable
 */

#define COLUMN_KAMAILIOSIPTRANSPORTRCV 4

#define kamailioSIPPortTable_COL_MIN 4
#define kamailioSIPPortTable_COL_MAX 4


#ifdef __cplusplus
}
#endif
#endif /** KAMAILIOSIPPORTTABLE_H */
