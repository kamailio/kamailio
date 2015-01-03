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
 * This file defines all registration and handling of all scalars defined in the
 * KAMAILIO-SIP-COMMON-MIB.  Please see KAMAILIO-SIP-COMMON-MIB for the complete
 * descriptions of the individual scalars.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "../../lib/kcore/statistics.h"
#include "../../config.h"

#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>

#include "snmpSIPCommonObjects.h"
#include "snmpstats_globals.h"
#include "utilities.h"

static char *kamailioVersion      = "SIP/2.0";
static unsigned int kamailioEntityType = TC_SIP_ENTITY_ROLE_OTHER;

/* 
 * Initializes the kamailioSIPCommonObjects MIB elements.  This involves:
 *
 *  - Registering all OID's
 *  - Setting up handlers for all OID's
 *
 * This function is mostly auto-generated.
 */
void init_kamailioSIPCommonObjects(void)
{
	static oid kamailioSIPProtocolVersion_oid[] =
		{ KAMAILIO_OID,3,1,1,1,1,1 };

	static oid kamailioSIPServiceStartTime_oid[] =
		{ KAMAILIO_OID,3,1,1,1,1,2 };

	static oid kamailioSIPEntityType_oid[] =
		{ KAMAILIO_OID,3,1,1,1,1,4 };

	static oid kamailioSIPSummaryInRequests_oid[] =
		{ KAMAILIO_OID,3,1,1,1,3,1 };

	static oid kamailioSIPSummaryOutRequests_oid[] =
		{ KAMAILIO_OID,3,1,1,1,3,2 };

	static oid kamailioSIPSummaryInResponses_oid[] =
		{ KAMAILIO_OID,3,1,1,1,3,3 };

	static oid kamailioSIPSummaryOutResponses_oid[] =
		{ KAMAILIO_OID,3,1,1,1,3,4 };

	static oid kamailioSIPSummaryTotalTransactions_oid[] =
		{ KAMAILIO_OID,3,1,1,1,3,5 };

	static oid kamailioSIPCurrentTransactions_oid[] =
		{ KAMAILIO_OID,3,1,1,1,6,1 };

	static oid kamailioSIPNumUnsupportedUris_oid[] =
		{ KAMAILIO_OID,3,1,1,1,8,1 };

	static oid kamailioSIPNumUnsupportedMethods_oid[] =
		{ KAMAILIO_OID,3,1,1,1,8,2 };

	static oid kamailioSIPOtherwiseDiscardedMsgs_oid[] = 
		{ KAMAILIO_OID,3,1,1,1,8,3 };

	DEBUGMSGTL(("kamailioSIPCommonObjects", "Initializing\n"));

	netsnmp_register_scalar(
		netsnmp_create_handler_registration(
			"kamailioSIPProtocolVersion",
			handle_kamailioSIPProtocolVersion,
			kamailioSIPProtocolVersion_oid,
			OID_LENGTH(kamailioSIPProtocolVersion_oid),
			HANDLER_CAN_RONLY)); 

	netsnmp_register_scalar(
		netsnmp_create_handler_registration(
			"kamailioSIPServiceStartTime",
			handle_kamailioSIPServiceStartTime,
			kamailioSIPServiceStartTime_oid,
			OID_LENGTH(kamailioSIPServiceStartTime_oid),
			HANDLER_CAN_RONLY));

	netsnmp_register_scalar(
		netsnmp_create_handler_registration(
			"kamailioSIPEntityType",
			handle_kamailioSIPEntityType,
			kamailioSIPEntityType_oid,
			OID_LENGTH(kamailioSIPEntityType_oid),
			HANDLER_CAN_RONLY));

	netsnmp_register_scalar(
		netsnmp_create_handler_registration(
			"kamailioSIPSummaryInRequests",
			handle_kamailioSIPSummaryInRequests,
			kamailioSIPSummaryInRequests_oid,
			OID_LENGTH(kamailioSIPSummaryInRequests_oid),
			HANDLER_CAN_RONLY));

	netsnmp_register_scalar(
		netsnmp_create_handler_registration(
			"kamailioSIPSummaryOutRequests",
			handle_kamailioSIPSummaryOutRequests,
			kamailioSIPSummaryOutRequests_oid,
			OID_LENGTH(kamailioSIPSummaryOutRequests_oid),
			HANDLER_CAN_RONLY));

	netsnmp_register_scalar(
		netsnmp_create_handler_registration(
			"kamailioSIPSummaryInResponses",
			handle_kamailioSIPSummaryInResponses,
			kamailioSIPSummaryInResponses_oid,
			OID_LENGTH(kamailioSIPSummaryInResponses_oid),
			HANDLER_CAN_RONLY));

	netsnmp_register_scalar(
		netsnmp_create_handler_registration(
			"kamailioSIPSummaryOutResponses",
			handle_kamailioSIPSummaryOutResponses,
			kamailioSIPSummaryOutResponses_oid,
			OID_LENGTH(kamailioSIPSummaryOutResponses_oid),
			HANDLER_CAN_RONLY));

	netsnmp_register_scalar(
		netsnmp_create_handler_registration(
			"kamailioSIPSummaryTotalTransactions",
			handle_kamailioSIPSummaryTotalTransactions,
			kamailioSIPSummaryTotalTransactions_oid,
			OID_LENGTH(kamailioSIPSummaryTotalTransactions_oid),
			HANDLER_CAN_RONLY));

	netsnmp_register_scalar(
		netsnmp_create_handler_registration(
			"kamailioSIPCurrentTransactions",
			handle_kamailioSIPCurrentTransactions,
			kamailioSIPCurrentTransactions_oid,
			OID_LENGTH(kamailioSIPCurrentTransactions_oid),
			HANDLER_CAN_RONLY));

	netsnmp_register_scalar(
		netsnmp_create_handler_registration(
			"kamailioSIPNumUnsupportedUris",
			handle_kamailioSIPNumUnsupportedUris,
			kamailioSIPNumUnsupportedUris_oid,
			OID_LENGTH(kamailioSIPNumUnsupportedUris_oid),
			HANDLER_CAN_RONLY));

	netsnmp_register_scalar(
		netsnmp_create_handler_registration(
			"kamailioSIPNumUnsupportedMethods",
			handle_kamailioSIPNumUnsupportedMethods,
			kamailioSIPNumUnsupportedMethods_oid,
			OID_LENGTH(kamailioSIPNumUnsupportedMethods_oid),
			HANDLER_CAN_RONLY));

	netsnmp_register_scalar(
		netsnmp_create_handler_registration(
			"kamailioSIPOtherwiseDiscardedMsgs",
			handle_kamailioSIPOtherwiseDiscardedMsgs,
			kamailioSIPOtherwiseDiscardedMsgs_oid,
			OID_LENGTH(kamailioSIPOtherwiseDiscardedMsgs_oid),
			HANDLER_CAN_RONLY)); 

}

int handle_kamailioSIPProtocolVersion(netsnmp_mib_handler *handler,
		netsnmp_handler_registration *reginfo,
		netsnmp_agent_request_info   *reqinfo,
		netsnmp_request_info         *requests)
{    
	if (reqinfo->mode==MODE_GET) {
		snmp_set_var_typed_value(requests->requestvb, ASN_OCTET_STR,
			(u_char *) kamailioVersion, 7);
		return SNMP_ERR_NOERROR;
	}

        return SNMP_ERR_GENERR;
}

/* 
 * The scalar represents what sysUpTime was when Kamailio first started.  This
 * data was stored in a file when SNMPStats first started up, as a result of a
 * call to spawn_sysUpTime_child() 
 */
int handle_kamailioSIPServiceStartTime(netsnmp_mib_handler *handler,
		netsnmp_handler_registration *reginfo,
		netsnmp_agent_request_info   *reqinfo,
		netsnmp_request_info         *requests)
{

	int elapsedTime = 0;
	char buffer[SNMPGET_MAX_BUFFER];

	FILE *theFile = fopen(SNMPGET_TEMP_FILE, "r");

	/* Open the file created by spawn_sysUpTime_child(), and parse our the
	 * required data.  */
	if (theFile == NULL) {
		LM_ERR("failed to read sysUpTime file at %s\n",
				SNMPGET_TEMP_FILE);
	} else {
		if(fgets(buffer, SNMPGET_MAX_BUFFER, theFile)==NULL)
			LM_ERR("failed to read from sysUpTime file at %s\n",
					SNMPGET_TEMP_FILE);

		/* Find the positions of '(' and ')' so we can extract out the
		 * timeticks value. */
		char *openBracePosition   = strchr(buffer, '(');
		char *closedBracePosition = strchr(buffer, ')');

		/* Make sure that both the '(' and ')' exist in the file, and
		 * that '(' occurs earlier than the ')'.  If all these
		 * conditions are true, then attempt to convert the string into
		 * an integer. */
		if (openBracePosition != NULL && closedBracePosition != NULL &&
				openBracePosition < closedBracePosition) {
			elapsedTime = (int) strtol(++openBracePosition, NULL, 10);
		}

		fclose(theFile);
	}

	if (reqinfo->mode == MODE_GET) { 
		snmp_set_var_typed_value(requests->requestvb, ASN_TIMETICKS,
			(u_char *) &elapsedTime, sizeof(elapsedTime));
		return SNMP_ERR_NOERROR;
	}
       
	return SNMP_ERR_GENERR;
}


int handle_kamailioSIPEntityType(netsnmp_mib_handler *handler,
		netsnmp_handler_registration *reginfo,
		netsnmp_agent_request_info   *reqinfo,
		netsnmp_request_info         *requests)
{
	if (reqinfo->mode==MODE_GET) {
		snmp_set_var_typed_value(requests->requestvb, ASN_OCTET_STR,
			(u_char *) &kamailioEntityType, 1);
		return SNMP_ERR_NOERROR;
	}

	return SNMP_ERR_GENERR;
}


int handle_kamailioSIPSummaryInRequests(netsnmp_mib_handler *handler,
		netsnmp_handler_registration *reginfo,
		netsnmp_agent_request_info   *reqinfo,
		netsnmp_request_info         *requests)
{
	int numRequests = get_statistic("rcv_requests");
   
	if (reqinfo->mode==MODE_GET) {
		snmp_set_var_typed_value(requests->requestvb, ASN_COUNTER,
			(u_char *) &numRequests, sizeof(int));
		return SNMP_ERR_NOERROR;
	}

	return SNMP_ERR_GENERR;
}

int handle_kamailioSIPSummaryOutRequests(netsnmp_mib_handler *handler,
		netsnmp_handler_registration *reginfo,
		netsnmp_agent_request_info   *reqinfo,
		netsnmp_request_info         *requests)
{
   int out_requests = get_statistic("fwd_requests"); 
	
	if (reqinfo->mode==MODE_GET) {
		snmp_set_var_typed_value(requests->requestvb, ASN_COUNTER,
			(u_char *) &out_requests, sizeof(int));
		return SNMP_ERR_NOERROR;
	}

	return SNMP_ERR_GENERR;
}

int handle_kamailioSIPSummaryInResponses(netsnmp_mib_handler *handler,
		netsnmp_handler_registration *reginfo,
		netsnmp_agent_request_info   *reqinfo,
		netsnmp_request_info         *requests)
{
	int result = get_statistic("rcv_replies"); 
	
	if (reqinfo->mode==MODE_GET) {
		snmp_set_var_typed_value(requests->requestvb, ASN_COUNTER,
			(u_char *) &result, sizeof(int));
		return SNMP_ERR_NOERROR;
	}

	return SNMP_ERR_GENERR;
}

int handle_kamailioSIPSummaryOutResponses(netsnmp_mib_handler *handler,
		netsnmp_handler_registration *reginfo,
		netsnmp_agent_request_info   *reqinfo,
		netsnmp_request_info         *requests)
{
	/* We can find the number of outbound responses sent by adding three
	 * sources
	 *
	 *   1) fwd_replies from core_stats 
	 *   2) local_replies and relayed_replies from the tm module 
	 *   3) sent_replies from the sl module. 
	 */
	
	int fwd_replies     = get_statistic("fwd_replies");
	int local_replies   = get_statistic("local_replies");
	int relayed_replies = get_statistic("relayed_replies");
	int sent_replies    = get_statistic("sent_replies");

	int result = 
		fwd_replies + local_replies + relayed_replies + sent_replies;
	
	if (reqinfo->mode==MODE_GET) {
		snmp_set_var_typed_value(requests->requestvb, ASN_COUNTER,
			(u_char *) &result, sizeof(int));
		return SNMP_ERR_NOERROR;
	}

	return SNMP_ERR_GENERR;
}

int handle_kamailioSIPSummaryTotalTransactions(netsnmp_mib_handler *handler,
		netsnmp_handler_registration *reginfo,
		netsnmp_agent_request_info   *reqinfo,
		netsnmp_request_info         *requests)
{
	/* We can find the total number of transactions by summing 
	 * UAC_transactions and UAS_transactions.  We don't need to add
	 * inuse_transactions because this will already be accounted for in the
	 * two other statistics. */
	int result = 
		get_statistic("UAS_transactions") + 
		get_statistic("UAC_transactions");

	if (reqinfo->mode==MODE_GET) {
		snmp_set_var_typed_value(requests->requestvb, ASN_COUNTER,
			(u_char *) &result, sizeof(int));
		return SNMP_ERR_NOERROR;
	}

	return SNMP_ERR_GENERR;
}

int handle_kamailioSIPCurrentTransactions(netsnmp_mib_handler *handler,
		netsnmp_handler_registration *reginfo,
		netsnmp_agent_request_info   *reqinfo,
		netsnmp_request_info         *requests)
{
	int result = get_statistic("inuse_transactions");
	
	if (reqinfo->mode==MODE_GET) {
		snmp_set_var_typed_value(requests->requestvb, ASN_GAUGE,
			(u_char *) &result, sizeof(int));
		return SNMP_ERR_NOERROR;
	}

	return SNMP_ERR_GENERR;
}

int handle_kamailioSIPNumUnsupportedUris(netsnmp_mib_handler *handler,
		netsnmp_handler_registration *reginfo,
		netsnmp_agent_request_info   *reqinfo,
		netsnmp_request_info         *requests)
{
   int result = get_statistic("bad_URIs_rcvd");
	
	if (reqinfo->mode==MODE_GET) {
		snmp_set_var_typed_value(requests->requestvb, ASN_COUNTER,
			(u_char *) &result, sizeof(int));
		return SNMP_ERR_NOERROR;
	}

	return SNMP_ERR_GENERR;
}

int handle_kamailioSIPNumUnsupportedMethods(netsnmp_mib_handler *handler,
		netsnmp_handler_registration *reginfo,
		netsnmp_agent_request_info   *reqinfo,
		netsnmp_request_info         *requests)
{
   int result = get_statistic("unsupported_methods");
	
	if (reqinfo->mode==MODE_GET) {
		snmp_set_var_typed_value(requests->requestvb, ASN_COUNTER,
			(u_char *) &result, sizeof(int));
		return SNMP_ERR_NOERROR;
	}

	return SNMP_ERR_GENERR;
}

int handle_kamailioSIPOtherwiseDiscardedMsgs(netsnmp_mib_handler *handler,
		netsnmp_handler_registration *reginfo,
		netsnmp_agent_request_info   *reqinfo,
		netsnmp_request_info         *requests)
{
	/* We should be able to get this number from existing stats layed out in
	 * the following equation: */
	int result = 
		get_statistic("err_requests")  +
		get_statistic("err_replies")   +
		get_statistic("drop_requests") +
		get_statistic("drop_replies"); 
    
	if (reqinfo->mode==MODE_GET) {
		snmp_set_var_typed_value(requests->requestvb, ASN_COUNTER,
			(u_char *) &result, sizeof(int));
		return SNMP_ERR_NOERROR;
	}

	return SNMP_ERR_GENERR;
}

/*
 * Parameter Setting Functions 
 */

/* If type==PARAM_STRING and stringParam is valid, this function will overwrite
 * kamailioEntityType with a bit value corresponding to the IETF's RFC  for 
 * the SIP MIB.  (Textual Convention SipEntityRole).  Anything else is
 * considered an error.
 *
 * Returns 0 on success, -1 on failure.  
 */
int handleSipEntityType( modparam_t type, void* val) 
{
	
	/* By default we start off as "other". */
	static char firstTime = 1;

	if (!stringHandlerSanityCheck(type, val, "sipEntityType")) {
		return -1;
	}

	char *strEntityType = (char *)val;

	/* This is our first time through this function, so we need to change
	 * kamailioEntityType from its default to 0, allowing our bitmasks below
	 * to work as expected. */
	if (firstTime) {
		firstTime = 0;
		kamailioEntityType = 0;
	}	

	/* Begin our string comparison.  This isn't the most efficient approach,
	 * but we don't expect this function to be called anywhere other than at
	 * startup.  So our inefficiency is outweiged by simplicity 
	 */
	if (strcasecmp(strEntityType, "other") == 0) {
		kamailioEntityType |= TC_SIP_ENTITY_ROLE_OTHER;
	}
	else if (strcasecmp(strEntityType, "userAgent") == 0) {
		kamailioEntityType |= TC_SIP_ENTITY_ROLE_USER_AGENT; 
	}
	else if (strcasecmp(strEntityType, "proxyServer") == 0) {
		kamailioEntityType |= TC_SIP_ENTITY_ROLE_PROXY_SERVER; 
	}
	else if (strcasecmp(strEntityType, "redirectServer") == 0) {
		kamailioEntityType |= TC_SIP_ENTITY_ROLE_REDIRECT_SERVER;
	}
	else if (strcasecmp(strEntityType, "registrarServer") == 0) {
		kamailioEntityType |= TC_SIP_ENTITY_ROLE_REGISTRAR_SERVER;
	}
	else if (strcasecmp(strEntityType, "edgeproxyServer") == 0) {
		kamailioEntityType |= TC_SIP_ENTITY_ROLE_EDGEPROXY_SERVER;
	}
	else if (strcasecmp(strEntityType, "sipcaptureServer") == 0) {
		kamailioEntityType |= TC_SIP_ENTITY_ROLE_SIPCAPTURE_SERVER;
	}
	else {
		LM_ERR("The configuration file specified sipEntityType=%s,"
				" an unknown type\n", strEntityType);
		return -1;
	}

	return 0;
}
