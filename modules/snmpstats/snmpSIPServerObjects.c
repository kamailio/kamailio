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
 * This file implements all scalares defines in the KAMAILIO-SIP-SERVER-MIB.  For
 * a full description of the scalars, please see KAMAILIO-SIP-SERVER-MIB.
 *
 */

#include <string.h>

#include "../../lib/kcore/statistics.h"
#include "../../sr_module.h"
#include "../../config.h"
#include "../usrloc/usrloc.h"

#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>
#include "snmpSIPServerObjects.h"

#include "snmpstats_globals.h"
#include "utilities.h"

/* Used for kamailioSIPRegUserLookupCounter.  (See the MIB file for details) */
unsigned int global_UserLookupCounter;


/* Initializes the kamailioSIPServerObjects module.  This involves: 
 *
 *  - Registering all OID's
 *  - Setting up handlers for all OID's
 *
 * This function is mostly auto-generated. */
void init_kamailioSIPServerObjects(void)
{
	static oid kamailioSIPProxyStatefulness_oid[]  =
		{ KAMAILIO_OID,3,1,2,1,3,1 };

	static oid kamailioSIPProxyRecordRoute_oid[]   =
		{ KAMAILIO_OID,3,1,2,1,3,3 };

	static oid kamailioSIPProxyAuthMethod_oid[]    =
		{ KAMAILIO_OID,3,1,2,1,3,4 };

	static oid kamailioSIPNumProxyRequireFailures_oid[]     =
		{ KAMAILIO_OID,3,1,2,1,4,1 };

	static oid kamailioSIPRegMaxContactExpiryDuration_oid[] =
		{ KAMAILIO_OID,3,1,2,1,5,2 };

	static oid kamailioSIPRegMaxUsers_oid[]              =
		{ KAMAILIO_OID,3,1,2,1,5,3 };

	static oid kamailioSIPRegCurrentUsers_oid[]          =
		{ KAMAILIO_OID,3,1,2,1,5,4 };

	static oid kamailioSIPRegDfltRegActiveInterval_oid[] =
		{ KAMAILIO_OID,3,1,2,1,5,5 };

	static oid kamailioSIPRegUserLookupCounter_oid[]     =
		{ KAMAILIO_OID,3,1,2,1,5,8 };

	static oid kamailioSIPRegAcceptedRegistrations_oid[] =
		{ KAMAILIO_OID,3,1,2,1,6,1 };

	static oid kamailioSIPRegRejectedRegistrations_oid[] =
		{ KAMAILIO_OID,3,1,2,1,6,2 };

	DEBUGMSGTL(("kamailioSIPServerObjects", "Initializing\n"));

	netsnmp_register_scalar(
		netsnmp_create_handler_registration(
			"kamailioSIPProxyStatefulness",
			handle_kamailioSIPProxyStatefulness,
			kamailioSIPProxyStatefulness_oid,
			OID_LENGTH(kamailioSIPProxyStatefulness_oid),
			HANDLER_CAN_RONLY));

	netsnmp_register_scalar(
		netsnmp_create_handler_registration(
			"kamailioSIPProxyRecordRoute",
			handle_kamailioSIPProxyRecordRoute,
			kamailioSIPProxyRecordRoute_oid,
			OID_LENGTH(kamailioSIPProxyRecordRoute_oid),
			HANDLER_CAN_RONLY));

	netsnmp_register_scalar(
		netsnmp_create_handler_registration(
			"kamailioSIPProxyAuthMethod",
			handle_kamailioSIPProxyAuthMethod,
			kamailioSIPProxyAuthMethod_oid,
			OID_LENGTH(kamailioSIPProxyAuthMethod_oid),
			HANDLER_CAN_RONLY));

	netsnmp_register_scalar(
		netsnmp_create_handler_registration(
			"kamailioSIPNumProxyRequireFailures",
			handle_kamailioSIPNumProxyRequireFailures,
			kamailioSIPNumProxyRequireFailures_oid,
			OID_LENGTH(kamailioSIPNumProxyRequireFailures_oid),
			HANDLER_CAN_RONLY));

	netsnmp_register_scalar(
		netsnmp_create_handler_registration(
			"kamailioSIPRegMaxContactExpiryDuration",
			handle_kamailioSIPRegMaxContactExpiryDuration,
			kamailioSIPRegMaxContactExpiryDuration_oid,
			OID_LENGTH(kamailioSIPRegMaxContactExpiryDuration_oid),
			HANDLER_CAN_RONLY));

	netsnmp_register_scalar(
		netsnmp_create_handler_registration(
			"kamailioSIPRegMaxUsers",
			handle_kamailioSIPRegMaxUsers,
			kamailioSIPRegMaxUsers_oid,
			OID_LENGTH(kamailioSIPRegMaxUsers_oid),
			HANDLER_CAN_RONLY));

	netsnmp_register_scalar(
		netsnmp_create_handler_registration(
			"kamailioSIPRegCurrentUsers",
			handle_kamailioSIPRegCurrentUsers,
			kamailioSIPRegCurrentUsers_oid,
			OID_LENGTH(kamailioSIPRegCurrentUsers_oid),
			HANDLER_CAN_RONLY));

	netsnmp_register_scalar(
		netsnmp_create_handler_registration(
			"kamailioSIPRegDfltRegActiveInterval",
			handle_kamailioSIPRegDfltRegActiveInterval,
			kamailioSIPRegDfltRegActiveInterval_oid,
			OID_LENGTH(kamailioSIPRegDfltRegActiveInterval_oid),
			HANDLER_CAN_RONLY));

	netsnmp_register_scalar(
		netsnmp_create_handler_registration(
			"kamailioSIPRegUserLookupCounter",
			handle_kamailioSIPRegUserLookupCounter,
			kamailioSIPRegUserLookupCounter_oid,
			OID_LENGTH(kamailioSIPRegUserLookupCounter_oid),
			HANDLER_CAN_RONLY));

	netsnmp_register_scalar(
		netsnmp_create_handler_registration(
			"kamailioSIPRegAcceptedRegistrations",
			handle_kamailioSIPRegAcceptedRegistrations,
			kamailioSIPRegAcceptedRegistrations_oid,
			OID_LENGTH(kamailioSIPRegAcceptedRegistrations_oid),
			HANDLER_CAN_RONLY));

	netsnmp_register_scalar(
		netsnmp_create_handler_registration(
			"kamailioSIPRegRejectedRegistrations",
			handle_kamailioSIPRegRejectedRegistrations,
			kamailioSIPRegRejectedRegistrations_oid,
			OID_LENGTH(kamailioSIPRegRejectedRegistrations_oid),
			HANDLER_CAN_RONLY));
}

/* The following are handlers for the scalars in the KAMAILIO-SIP-SERVER-MIB. */

int handle_kamailioSIPProxyStatefulness(netsnmp_mib_handler *handler,
	netsnmp_handler_registration *reginfo,
	netsnmp_agent_request_info   *reqinfo,
	netsnmp_request_info         *requests)
{
	int statefullness;

	if (module_loaded("dialog") || module_loaded("dialog_ng")) 
	{
		statefullness = PROXY_STATEFULNESS_CALL_STATEFUL;
	}
	else if (module_loaded("tm"))
	{
		statefullness = PROXY_STATEFULNESS_TRANSACTION_STATEFUL;
	}
	else
	{
		statefullness = PROXY_STATEFULNESS_STATELESS;
	}

	if (reqinfo->mode==MODE_GET) {
		snmp_set_var_typed_value(requests->requestvb, ASN_INTEGER,
			(u_char *) &statefullness, sizeof(int));
		return SNMP_ERR_NOERROR;
	}

	return SNMP_ERR_GENERR;
}


int handle_kamailioSIPProxyRecordRoute(netsnmp_mib_handler *handler,
	netsnmp_handler_registration *reginfo,
	netsnmp_agent_request_info   *reqinfo,
	netsnmp_request_info         *requests)
{
	/* If the rr module is loaded, then we support record route.  otherwise,
	 * we want to return 2, which is false for SNMP TruthValue. */
	int supportRecordRoute = TC_FALSE;

	if (module_loaded("rr"))
	{
		supportRecordRoute = TC_TRUE;
	}
    
	if (reqinfo->mode==MODE_GET) {
		snmp_set_var_typed_value(requests->requestvb, ASN_INTEGER,
			(u_char *) &supportRecordRoute, sizeof(int));
		return SNMP_ERR_NOERROR;
	}

	return SNMP_ERR_GENERR;
}

int handle_kamailioSIPProxyAuthMethod(netsnmp_mib_handler *handler,
	netsnmp_handler_registration *reginfo,
	netsnmp_agent_request_info   *reqinfo,
	netsnmp_request_info         *requests)
{

	/* The result needs to be returned as an SNMP bit field. */
	unsigned int auth_bitfield = SIP_AUTH_METHOD_NONE;
	
	if (module_loaded("tls")) {
		auth_bitfield |=  SIP_AUTH_METHOD_TLS;
		auth_bitfield &= ~SIP_AUTH_METHOD_NONE;
	}
	
	/* We can have both tls and auth loaded simultaneously.  Therefore we
	 * use an if instead of a else/else-if. */
	if (module_loaded("auth")) {
		auth_bitfield |= SIP_AUTH_METHOD_DIGEST;
		auth_bitfield &= ~SIP_AUTH_METHOD_NONE;
	}

	if (reqinfo->mode==MODE_GET) {
		snmp_set_var_typed_value(requests->requestvb, ASN_OCTET_STR,
			(u_char *) &auth_bitfield, 1);
		return SNMP_ERR_NOERROR;
	}

	return SNMP_ERR_GENERR;
}

int handle_kamailioSIPNumProxyRequireFailures(netsnmp_mib_handler *handler,
	netsnmp_handler_registration *reginfo,
	netsnmp_agent_request_info   *reqinfo,
	netsnmp_request_info         *requests)
{
	int result = get_statistic("bad_msg_hdr"); 
   
	if (reqinfo->mode==MODE_GET) {
		snmp_set_var_typed_value(requests->requestvb, ASN_COUNTER,
			(u_char *) &result, sizeof(int));
		return SNMP_ERR_NOERROR;
	}

	return SNMP_ERR_GENERR;
}

int handle_kamailioSIPRegMaxContactExpiryDuration(netsnmp_mib_handler *handler,
	netsnmp_handler_registration *reginfo,
	netsnmp_agent_request_info   *reqinfo,
	netsnmp_request_info         *requests)
{
	int result = get_statistic("max_expires");
    
	if (reqinfo->mode==MODE_GET) {
		snmp_set_var_typed_value(requests->requestvb, ASN_UNSIGNED,
			(u_char *) &result, sizeof(int));
		return SNMP_ERR_NOERROR;
	}

	return SNMP_ERR_GENERR;
}

int handle_kamailioSIPRegMaxUsers(netsnmp_mib_handler *handler,
	netsnmp_handler_registration *reginfo,
	netsnmp_agent_request_info   *reqinfo,
	netsnmp_request_info         *requests)
{

	/* Kamailio doesn't currently have a parameterized maximum number of
	 * users.  So we return the maximum value an unsigned32 SNMP datatype
	 * can hold.  */
	unsigned int result = 0xffffffff; 
	
	if (reqinfo->mode==MODE_GET) {
		snmp_set_var_typed_value(requests->requestvb, ASN_UNSIGNED,
			(u_char *) &result, sizeof(unsigned int));
		return SNMP_ERR_NOERROR;
	}

	return SNMP_ERR_GENERR;
}

int handle_kamailioSIPRegCurrentUsers(netsnmp_mib_handler *handler,
	netsnmp_handler_registration *reginfo,
	netsnmp_agent_request_info   *reqinfo,
	netsnmp_request_info         *requests)
{
	int max_users = 0;

	max_users = get_statistic("registered_users");
	
	if (reqinfo->mode==MODE_GET) {
		snmp_set_var_typed_value(requests->requestvb, ASN_GAUGE,
			(u_char *) &max_users, sizeof(int));
		return SNMP_ERR_NOERROR;
	}

	return SNMP_ERR_GENERR;
}

int handle_kamailioSIPRegDfltRegActiveInterval(netsnmp_mib_handler *handler,
	netsnmp_handler_registration *reginfo,
	netsnmp_agent_request_info   *reqinfo,
	netsnmp_request_info         *requests)
{
	int result = get_statistic("default_expire");
	
	if (reqinfo->mode==MODE_GET) {
		snmp_set_var_typed_value(requests->requestvb, ASN_UNSIGNED,
			(u_char *) &result, sizeof(int));
		return SNMP_ERR_NOERROR;
	}

	return SNMP_ERR_GENERR;
}


int handle_kamailioSIPRegUserLookupCounter(netsnmp_mib_handler *handler,
	netsnmp_handler_registration *reginfo,
	netsnmp_agent_request_info   *reqinfo,
	netsnmp_request_info         *requests)
{
	int result = ++global_UserLookupCounter;

	/* If we have had so many requests that we've hit our maximum index,
	 * then we reset our counter back to 1.  For this not to cause problems,
	 * it will be required that old rows belonging to the table 
	 * kamailioSIPRegUserLookupTable are eventually deleted. */
	if (global_UserLookupCounter > MAX_USER_LOOKUP_COUNTER) {
		global_UserLookupCounter = 1;
	}

	if (reqinfo->mode==MODE_GET) {
		snmp_set_var_typed_value(requests->requestvb, ASN_COUNTER,
			(u_char *) &result, sizeof(int));
		return SNMP_ERR_NOERROR;
	}

	return SNMP_ERR_GENERR;
}

int handle_kamailioSIPRegAcceptedRegistrations(netsnmp_mib_handler *handler,
	netsnmp_handler_registration *reginfo,
	netsnmp_agent_request_info   *reqinfo,
	netsnmp_request_info         *requests)
{
	int result = get_statistic("accepted_regs");
	
	if (reqinfo->mode==MODE_GET) {
		snmp_set_var_typed_value(requests->requestvb, ASN_COUNTER,
			(u_char *) &result, sizeof(int));
		return SNMP_ERR_NOERROR;
	}

	return SNMP_ERR_GENERR;
}

int handle_kamailioSIPRegRejectedRegistrations(netsnmp_mib_handler *handler,
	netsnmp_handler_registration *reginfo,
	netsnmp_agent_request_info   *reqinfo,
	netsnmp_request_info         *requests)
{
	int result = get_statistic("rejected_regs");
	
	if (reqinfo->mode==MODE_GET) {
		snmp_set_var_typed_value(requests->requestvb, ASN_COUNTER,
			(u_char *) &result, sizeof(int));
		return SNMP_ERR_NOERROR;
	}

	return SNMP_ERR_GENERR;
}

