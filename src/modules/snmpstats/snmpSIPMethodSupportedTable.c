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
 * The file implements the kamailioSIPMethodSupportedTable.  The table is
 * populated by looking to see which modules are loaded, and guessing what SIP
 * Methods they  provide.  It is quite possible that this initial implementation
 * is not very good at guessing.  This should be fixed in future releases as
 * more information becomes available.  
 *
 * For full details, please see the KAMAILIO-SIP-COMMON-MIB.
 *
 */

#include "../../sr_module.h"
#include "../../mem/mem.h"

#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>

#include <net-snmp/library/snmp_assert.h>

#include "snmpstats_globals.h"
#include "snmpSIPMethodSupportedTable.h"

static netsnmp_handler_registration *my_handler = NULL;
static netsnmp_table_array_callbacks cb;

oid    kamailioSIPMethodSupportedTable_oid[] = 
	{ kamailioSIPMethodSupportedTable_TABLE_OID };

size_t kamailioSIPMethodSupportedTable_oid_len = 
	OID_LENGTH(kamailioSIPMethodSupportedTable_oid);


/* Create a row at the given index, containing stringToRegister, and insert it
 * into the table.  Note that stringToRegister will be copied, so it is not
 * necessary to pre-allocate this string anywhere. */
void createRow(int index, char *stringToRegister) {

	kamailioSIPMethodSupportedTable_context *theRow;

	oid  *OIDIndex;
	char *copiedString;
	int  stringLength;

	theRow = SNMP_MALLOC_TYPEDEF(kamailioSIPMethodSupportedTable_context);

	if (theRow == NULL) {
		LM_ERR("failed to create a row for kamailioSIPMethodSupportedTable\n");
		return;
	}

	OIDIndex = pkg_malloc(sizeof(oid));

	if (OIDIndex == NULL) {
		free(theRow);
		LM_ERR("failed to create a row for kamailioSIPMethodSupportedTable\n");
		return;
	}

	stringLength = strlen(stringToRegister);

	copiedString = pkg_malloc((stringLength + 1) * sizeof(char));

	if (copiedString == NULL) {
		LM_ERR("failed to create a row for kamailioSIPMethodSupportedTable\n");
		return;
	}

	strcpy(copiedString, stringToRegister);

	OIDIndex[0] = index;

	theRow->index.len  = 1;
	theRow->index.oids = OIDIndex;
	theRow->kamailioSIPMethodSupportedIndex = index;

	theRow->kamailioSIPMethodName     = (unsigned char*) copiedString;
	theRow->kamailioSIPMethodName_len = stringLength;

	CONTAINER_INSERT(cb.container, theRow);
}


/* Initializes the kamailioSIPMethodSupportedTable, and populates the tables 
 * contents */
void init_kamailioSIPMethodSupportedTable(void)
{
	initialize_table_kamailioSIPMethodSupportedTable();

	/* Tables is defined as follows:
	 *
	 * 	1)  METHOD_INVITE
	 *  	2)  METHOD_CANCEL
	 *	3)  METHOD_ACK
	 *	4)  METHOD_BYE
	 *	5)  METHOD_INFO
	 *	6)  METHOD_OPTIONS
	 *	7)  METHOD_UPDATE
	 *	8)  METHOD_REGISTER
	 *	9)  METHOD_MESSAGE
	 *	10) METHOD_SUBSCRIBE
	 *	11) METHOD_NOTIFY
	 *	12) METHOD_PRACK
	 *	13) METHOD_REFER
	 *	14) METHOD_PUBLISH
	 *
	 * We should keep these indices fixed.  For example if we don't support
	 * METHOD_REGISTER but we do support METHOD_MESSAGE, then METHOD_MESSAGE
	 * should still be at index 9.  
	 *
	 * NOTE: My way of checking what METHODS we support is probably wrong.
	 * Please feel free to correct it! */
	
	createRow(1, "METHOD_INVITE");
	createRow(2, "METHOD_CANCEL");
	createRow(3, "METHOD_ACK");
	createRow(4, "METHOD_BYE");

	if (module_loaded("options") || module_loaded("siputils")) {
		createRow(6, "METHOD_OPTIONS");
	}

	createRow(7, "METHOD_UPDATE");

	if (module_loaded("registrar")) {
		createRow(8, "METHOD_REGISTER");
		createRow(10, "METHOD_SUBSCRIBE");
		createRow(11, "METHOD_NOTIFY");
	}

	createRow(5,  "METHOD_INFO");
	createRow(9,  "METHOD_MESSAGE");
	createRow(12, "METHOD_PRACK");
	createRow(13, "METHOD_REFER");
	createRow(14, "METHOD_PUBLISH");
}


/* Initialize the kamailioSIPMethodSupportedTable by defining its structure and
 * callback mechanisms */
void initialize_table_kamailioSIPMethodSupportedTable(void)
{
	netsnmp_table_registration_info *table_info;

	if(my_handler) {
		snmp_log(LOG_ERR, "initialize_table_kamailioSIPMethodSupported"
				"Table_handler called again\n");
		return;
	}

	memset(&cb, 0x00, sizeof(cb));

	/** create the table structure itself */
	table_info = SNMP_MALLOC_TYPEDEF(netsnmp_table_registration_info);

	my_handler = 
		netsnmp_create_handler_registration(
			"kamailioSIPMethodSupportedTable",
			netsnmp_table_array_helper_handler,
			kamailioSIPMethodSupportedTable_oid,
			kamailioSIPMethodSupportedTable_oid_len,
			HANDLER_CAN_RONLY);
			
	if (!my_handler || !table_info) {
		snmp_log(LOG_ERR, "malloc failed in initialize_table_kamailio"
				"SIPMethodSupportedTable_handler\n");
		return; 
	}

	netsnmp_table_helper_add_index(table_info, ASN_UNSIGNED);

	table_info->min_column = kamailioSIPMethodSupportedTable_COL_MIN;
	table_info->max_column = kamailioSIPMethodSupportedTable_COL_MAX;

	/***************************************************
	 * registering the table with the master agent
	 */
	cb.get_value = kamailioSIPMethodSupportedTable_get_value;
	cb.container = 
		netsnmp_container_find("kamailioSIPMethodSupportedTable_primary:"
			"kamailioSIPMethodSupportedTable:" "table_container");
	
	DEBUGMSGTL(("initialize_table_kamailioSIPMethodSupportedTable", 
				"Registering table kamailioSIPMethodSupportedTable"
				"as a table array\n"));

	netsnmp_table_container_register(my_handler, table_info, &cb,
			cb.container, 1);

}

/* 
 * This routine is called to process get requests for elements of the table.
 *
 * The function is pretty much left as is from the auto-generated code. 
 */
int kamailioSIPMethodSupportedTable_get_value(
			netsnmp_request_info *request,
			netsnmp_index *item,
			netsnmp_table_request_info *table_info )
{
	netsnmp_variable_list *var = request->requestvb;

	kamailioSIPMethodSupportedTable_context *context = 
		(kamailioSIPMethodSupportedTable_context *)item;

	switch(table_info->colnum) 
	{
		case COLUMN_KAMAILIOSIPMETHODNAME:

			/** SnmpAdminString = ASN_OCTET_STR */
			snmp_set_var_typed_value(var, ASN_OCTET_STR,
					(unsigned char*)
					context->kamailioSIPMethodName,
					context->kamailioSIPMethodName_len );
			break;
	
		default: /** We shouldn't get here */
			snmp_log(LOG_ERR, "unknown column in kamailioSIPMethod"
					"SupportedTable_get_value\n");
			return SNMP_ERR_GENERR;
	}

	return SNMP_ERR_NOERROR;
}

/*
 * kamailioSIPMethodSupportedTable_get_by_idx is an auto-generated function.
 */
const kamailioSIPMethodSupportedTable_context *
	kamailioSIPMethodSupportedTable_get_by_idx(netsnmp_index * hdr)
{
	return (const kamailioSIPMethodSupportedTable_context *)
		CONTAINER_FIND(cb.container, hdr );
}


