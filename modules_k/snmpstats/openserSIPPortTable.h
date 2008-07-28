/*
 * $Id$
 *
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 *
 * History:
 * --------
 * 2006-11-23 initial version (jmagder)
 * 2007-02-16 Moved all OID registrations from the experimental branch to 
 *            OpenSER's IANA assigned enterprise branch. (jmagder)
 * 
 * Originally Generated with Mib2c using mib2c.array-user.conf.
 *
 * This file defines the openserSIPPortTable prototypes.  For a full description
 * of the table, please see the OPENSER-SIP-COMMON-MIB.
 */

#ifndef OPENSERSIPPORTTABLE_H
#define OPENSERSIPPORTTABLE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <net-snmp/net-snmp-config.h>
#include <net-snmp/library/container.h>
#include <net-snmp/agent/table_array.h>

#include "../../config.h"

#define SIP_PORT_TABLE_STR_INDEX_SIZE 10

/* This strucutre represents a single row in the table. */
typedef struct openserSIPPortTable_context_s 
{

	netsnmp_index index; 
	
	unsigned char openserSIPStringIndex[SIP_PORT_TABLE_STR_INDEX_SIZE];
	unsigned long openserSIPStringIndex_len;

	unsigned char openserSIPTransportRcv[2];
	long openserSIPTransportRcv_len;

	void * data;

} openserSIPPortTable_context;


/*
 * Initializes the openserSIPPortTable module.  
 *
 * Specifically, this function will define the tables structure, and then
 * populate it with the ports and transports that OpenSER is listening on.
 *
 */
void  init_openserSIPPortTable(void);


/* Initialize the openserSIPPortTable table by defining how it is structured */
void  initialize_table_openserSIPPortTable(void);


/*
 * This routine is called to process get requests for elements of the table.
 * The function is mostly left in its auto-generated form 
 */
int   openserSIPPortTable_get_value(netsnmp_request_info *, netsnmp_index *, 
		netsnmp_table_request_info *);

const openserSIPPortTable_context * openserSIPPortTable_get_by_idx(
		netsnmp_index *);

const openserSIPPortTable_context * openserSIPPortTable_get_by_idx_rs(
		netsnmp_index *, int row_status);

/*
 * oid declarations
 */
extern oid    openserSIPPortTable_oid[];
extern size_t openserSIPPortTable_oid_len;

#define openserSIPPortTable_TABLE_OID OPENSER_OID,3,1,1,1,1,5
	
/*
 * column number definitions for table openserSIPPortTable
 */

#define COLUMN_OPENSERSIPTRANSPORTRCV 4

#define openserSIPPortTable_COL_MIN 4
#define openserSIPPortTable_COL_MAX 4


#ifdef __cplusplus
}
#endif
#endif /** OPENSERSIPPORTTABLE_H */
