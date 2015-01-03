/*
 * SNMPStats Module 
 * Copyright (C) 2006 SOMA Networks, INC.
 * Written by: Jeffrey Magder (jmagder@somanetworks.com)
 *
 * Copyright (C) 2013 Edvina AB, Sollentuna, Sweden
 * Modifications by: Olle E. Johansson (oej@edvina.net)
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
 * Originally Generated with mib2c using mib2c.array-user.conf
 *
 * This file implements the kamailioSIPPortTable.  For a full description of the table,
 * please see the KAMAILIO-SIP-COMMON-MIB.
 *
 */


#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>
#include <net-snmp/library/snmp_assert.h>

#include "../../lib/kcore/statistics.h"
#include "../../mem/mem.h"

#include "snmpstats_globals.h"
#include "snmpSIPPortTable.h"

static     netsnmp_handler_registration *my_handler = NULL;
static     netsnmp_table_array_callbacks cb;

oid    kamailioSIPPortTable_oid[]   = { kamailioSIPPortTable_TABLE_OID };
size_t kamailioSIPPortTable_oid_len = OID_LENGTH(kamailioSIPPortTable_oid);


/* Returns a new OID with the following structure:
 *
 * 	ipType.NUM_IP_OCTETS.ipAddress[0].ipAddress[1]...ipAddress[NUM_IP_OCTETS].portNumber
 *
 * sizeOfOID will be assigned the length of the oid.  
 * 
 * Note: This function returns a newly allocated block of memory.  Make sure to
 * deallocate the memory when you no longer need it. 
 */
static oid *createIndex(int ipType, int *ipAddress, int *sizeOfOID) 
{
	oid *currentOIDIndex;
	int i;
	int family = ipType == 1 ? AF_INET : AF_INET6;
	int num_octets = family == AF_INET ? NUM_IP_OCTETS : NUM_IPV6_OCTETS;

	/* The size needs to be large enough such that it can store the ipType
	 * (one octet), the prefixed length (one octet), the number of
	 * octets to the IP Address (NUM_IP_OCTETS), and the port. */
	*sizeOfOID = num_octets + 3;

	/* Allocate space for the OID Index.  */
	currentOIDIndex = pkg_malloc((*sizeOfOID) * sizeof(oid));
	LM_DBG("----> Size of OID %d \n", *sizeOfOID);

	if (currentOIDIndex == NULL) {
		LM_ERR("failed to create a row for kamailioSIPPortTable\n");
		*sizeOfOID = 0;
		return NULL;
	}

	/* Assign the OID Index */
	currentOIDIndex[0] = ipType;
	currentOIDIndex[1] = num_octets;
		
	for (i = 0; i < num_octets; i++) {
		currentOIDIndex[i+2] = ipAddress[i];
	}

	/* Extract out the port number */
	currentOIDIndex[num_octets + 2] = ipAddress[num_octets];
	LM_DBG("----> Port number %d Family %s \n", ipAddress[num_octets], ipType == 1 ? "IPv4" : "IPv6");

	return currentOIDIndex;
}


/* Will return an existing row indexed by the parameter list if one exists, and
 * return a new one otherwise.  If the row is new, then the provided index will be
 * assigned to the new row.
 *
 * Note: NULL will be returned on an error 
 */
kamailioSIPPortTable_context *getRow(int ipType, int *ipAddress)
{
	int lengthOfOID;
	oid *currentOIDIndex = createIndex(ipType, ipAddress, &lengthOfOID);
	netsnmp_index theIndex;
	kamailioSIPPortTable_context *rowToReturn;
	int num_octets = ipType == 1 ? NUM_IP_OCTETS : NUM_IPV6_OCTETS;

	if (currentOIDIndex == NULL)
	{
		return NULL;
	}

	theIndex.oids = currentOIDIndex;
	theIndex.len  = lengthOfOID;

	/* Lets check to see if there is an existing row. */
	rowToReturn = CONTAINER_FIND(cb.container, &theIndex);
	
	/* We found an existing row, so there is no need to create a new one.
	 * Let's return it to the caller. */
	if (rowToReturn != NULL) 
	{
		/* We don't need the index we allocated anymore, because the
		 * existing row already has its own copy, so free the memory */
		pkg_free(currentOIDIndex);

		return rowToReturn;
	}
	
	/* If we are here then the row doesn't exist yet.  So lets create it. */
	rowToReturn = SNMP_MALLOC_TYPEDEF(kamailioSIPPortTable_context);

	/* Not enough memory to create the new row. */
	if (rowToReturn == NULL) {
		pkg_free(currentOIDIndex);
		return NULL;
	}

	/* Assign the Container Index. */
	rowToReturn->index.len  = lengthOfOID;
	rowToReturn->index.oids = currentOIDIndex;

	memcpy(rowToReturn->kamailioSIPStringIndex, currentOIDIndex, num_octets + 3);
	rowToReturn->kamailioSIPStringIndex_len = num_octets + 3;

	/* Insert the new row into the table */
	CONTAINER_INSERT(cb.container, rowToReturn);

	return rowToReturn;
}


/*
 * Will create rows for this table from theList.  The final parameter snmpIndex
 * can point to any integer >= zero.  All rows created by this function will be
 * indexed starting at snmpIndex++.  The parameter is implemented as a pointer
 * to an integer so that if the function is called again with another
 * 'protocol', we can continue from the last index. 
 */
static void createRowsFromIPList(int *theList, int listSize, int protocol, 
		int *snmpIndex, int family) {

	kamailioSIPPortTable_context *currentRow;
	int num_octets = family == AF_INET ? NUM_IP_OCTETS : NUM_IPV6_OCTETS;
	int curIndexOfIP;
	int curSocketIdx;
	int valueToAssign;

	if (protocol == PROTO_UDP) 
	{
		valueToAssign = TC_TRANSPORT_PROTOCOL_UDP;
	}
	else if (protocol == PROTO_TCP) 
	{
		valueToAssign = TC_TRANSPORT_PROTOCOL_TCP;
	}
	else if (protocol == PROTO_TLS)
	{
		valueToAssign = TC_TRANSPORT_PROTOCOL_TLS;
	}
	else if (protocol == PROTO_SCTP)
	{
		valueToAssign = TC_SIP_TRANSPORT_PROTOCOL_SCTP;
	}
	else 
	{
		valueToAssign = TC_TRANSPORT_PROTOCOL_OTHER;
	}
	
	/* Create all rows with respect to the given protocol */
	for (curSocketIdx=0; curSocketIdx < listSize; curSocketIdx++) {

		curIndexOfIP   = (num_octets + 1) * curSocketIdx;
		
		/* Retrieve an existing row, or a new row if one doesn't
		 * already exist. 
		 * RFC 4001 defined IPv4 as 1, IPv6 as 2
		*/
		currentRow = getRow(family == AF_INET? 1 : 2, &theList[curIndexOfIP]);

		if (currentRow == NULL) {
			LM_ERR("failed to create all the "
					"rows for the kamailioSIPPortTable\n");
			return;
		}

		currentRow->kamailioSIPTransportRcv[0]  |= valueToAssign;
		currentRow->kamailioSIPTransportRcv_len = 1;
	}
}

/*
 * Initializes the kamailioSIPPortTable module.  
 *
 * Specifically, this function will define the tables structure, and then
 * populate it with the ports and transports that Kamailio is listening on.
 *
 */
void init_kamailioSIPPortTable(void)
{
	int curSNMPIndex = 0;

	initialize_table_kamailioSIPPortTable();

	int *UDPList = NULL;
	int *TCPList = NULL;
	int *TLSList = NULL;
	int *SCTPList = NULL;

	int *UDP6List = NULL;
	int *TCP6List = NULL;
	int *TLS6List = NULL;
	int *SCTP6List = NULL;

	int numUDPSockets;
	int numTCPSockets; 
	int numTLSSockets;
	int numSCTPSockets;

	int numUDP6Sockets;
	int numTCP6Sockets; 
	int numTLS6Sockets;
	int numSCTP6Sockets;
	
	/* Retrieve the list of the number of UDP and TCP sockets. */
	numUDPSockets = get_socket_list_from_proto_and_family(&UDPList, PROTO_UDP, AF_INET);
	numUDP6Sockets = get_socket_list_from_proto_and_family(&UDP6List, PROTO_UDP, AF_INET6);
	numTCPSockets = get_socket_list_from_proto_and_family(&TCPList, PROTO_TCP, AF_INET);
	numTCP6Sockets = get_socket_list_from_proto_and_family(&TCP6List, PROTO_TCP, AF_INET6);
	numTLSSockets = get_socket_list_from_proto_and_family(&TLSList, PROTO_TLS, AF_INET);
	numTLS6Sockets = get_socket_list_from_proto_and_family(&TLS6List, PROTO_TLS, AF_INET6);
	numSCTPSockets = get_socket_list_from_proto_and_family(&SCTPList, PROTO_SCTP, AF_INET);
	numSCTP6Sockets = get_socket_list_from_proto_and_family(&SCTP6List, PROTO_SCTP, AF_INET6);

	LM_DBG("-----> Sockets UDP %d UDP6 %d TCP %d TCP6 %d TLS %d TLS6 %d SCTP %d SCTP6 %d\n",
		numUDPSockets, numUDP6Sockets, numTCPSockets, numTCP6Sockets, numTLSSockets, numTLS6Sockets, numSCTPSockets, numSCTP6Sockets);

	/* Generate all rows, using all retrieved interfaces. */
	createRowsFromIPList(UDPList, numUDPSockets, PROTO_UDP, &curSNMPIndex, AF_INET);
	curSNMPIndex = 0;
	createRowsFromIPList(UDP6List, numUDP6Sockets, PROTO_UDP, &curSNMPIndex, AF_INET6);

	curSNMPIndex = 0;
	createRowsFromIPList(TCPList, numTCPSockets, PROTO_TCP, &curSNMPIndex, AF_INET);
	curSNMPIndex = 0;
	createRowsFromIPList(TCP6List, numTCP6Sockets, PROTO_TCP, &curSNMPIndex, AF_INET6);

	curSNMPIndex = 0;
	createRowsFromIPList(TLSList, numTLSSockets, PROTO_TLS, &curSNMPIndex, AF_INET);
	curSNMPIndex = 0;
	createRowsFromIPList(TLS6List, numTLS6Sockets, PROTO_TLS, &curSNMPIndex, AF_INET6);

	curSNMPIndex = 0;
	createRowsFromIPList(SCTPList, numSCTPSockets, PROTO_SCTP, &curSNMPIndex, AF_INET);
	curSNMPIndex = 0;
	createRowsFromIPList(SCTP6List, numSCTP6Sockets, PROTO_SCTP, &curSNMPIndex, AF_INET6);
}

 
/* Initialize the kamailioSIPPortTable table by defining how it is structured */
void initialize_table_kamailioSIPPortTable(void)
{
	netsnmp_table_registration_info *table_info;

	if(my_handler) {
		snmp_log(LOG_ERR, "initialize_table_kamailioSIPPortTable_handler"
				"called again\n");
		return;
	}

	memset(&cb, 0x00, sizeof(cb));

	/* create the table structure itself */
	table_info = SNMP_MALLOC_TYPEDEF(netsnmp_table_registration_info);

	my_handler = netsnmp_create_handler_registration("kamailioSIPPortTable",
			netsnmp_table_array_helper_handler,
			kamailioSIPPortTable_oid,
			kamailioSIPPortTable_oid_len,
			HANDLER_CAN_RONLY);
    
	if (!my_handler || !table_info) {
		snmp_log(LOG_ERR, "malloc failed in "
			 "initialize_table_kamailioSIPPortTable_handler\n");
		return; /** mallocs failed */
	}

	/* Set up the table's structural definition */
	
	/* index: kamailioSIPPortIndex */
	netsnmp_table_helper_add_index(table_info, ASN_OCTET_STR);

	table_info->min_column = kamailioSIPPortTable_COL_MIN;
	table_info->max_column = kamailioSIPPortTable_COL_MAX;

	/* register the table with the master agent */
	cb.get_value = kamailioSIPPortTable_get_value;
	cb.container = netsnmp_container_find("kamailioSIPPortTable_primary:"
			"kamailioSIPPortTable:"
			"table_container");

	
	DEBUGMSGTL(("initialize_table_kamailioSIPPortTable",
				"Registering table kamailioSIPPortTable "
				"as a table array\n"));
	
	netsnmp_table_container_register(my_handler, table_info, &cb,
			cb.container, 1);
}

/*
 * This routine is called to process get requests for elements of the table.
 *
 * The function is mostly left in its auto-generated form 
 */
int kamailioSIPPortTable_get_value(netsnmp_request_info *request, 
		netsnmp_index *item,
		netsnmp_table_request_info *table_info )
{
	netsnmp_variable_list *var = request->requestvb;

	kamailioSIPPortTable_context *context = 
		(kamailioSIPPortTable_context *)item;

	switch(table_info->colnum) 
	{
		
		case COLUMN_KAMAILIOSIPTRANSPORTRCV:
			/** KamailioSIPTransportProtocol = ASN_OCTET_STR */
			snmp_set_var_typed_value(var, ASN_OCTET_STR,
					(unsigned char *)
					&context->kamailioSIPTransportRcv,
					context->kamailioSIPTransportRcv_len );
			break;

		default: /** We shouldn't get here */
			snmp_log(LOG_ERR, "unknown column in "
					"kamailioSIPPortTable_get_value\n");
			return SNMP_ERR_GENERR;
	}

	return SNMP_ERR_NOERROR;
}

/* Auto-generated function */
const kamailioSIPPortTable_context *
kamailioSIPPortTable_get_by_idx(netsnmp_index * hdr)
{
	return (const kamailioSIPPortTable_context *)
		CONTAINER_FIND(cb.container, hdr );
}


