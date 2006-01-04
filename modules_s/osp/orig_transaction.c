/*
 * ser osp module. 
 *
 * This module enables ser to communicate with an Open Settlement 
 * Protocol (OSP) server.  The Open Settlement Protocol is an ETSI 
 * defined standard for Inter-Domain VoIP pricing, authorization
 * and usage exchange.  The technical specifications for OSP 
 * (ETSI TS 101 321 V4.1.1) are available at www.etsi.org.
 *
 * Uli Abend was the original contributor to this module.
 * 
 * Copyright (C) 2001-2005 Fhg Fokus
 *
 * This file is part of ser, a free SIP server.
 *
 * ser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * For a license to use the ser software under conditions
 * other than those described here, or to purchase support for this
 * software, please contact iptel.org by e-mail at the following addresses:
 *    info@iptel.org
 *
 * ser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#include "osp_mod.h"
#include "osptoolkit.h"
#include "orig_transaction.h"
#include "sipheader.h"
#include "destination.h"
#include "usage.h"
#include "osp/osp.h"
#include "../../sr_module.h"
#include "../../locking.h"
#include "../../mem/mem.h"
#include "../../dset.h"

extern int   _max_destinations;
extern char* _device_ip;
extern char* _device_port;
extern OSPTPROVHANDLE _provider;


const int FIRST_ROUTE = 1;
const int NEXT_ROUTE  = 0;


static int loadosproutes(     struct sip_msg* msg, OSPTTRANHANDLE transaction, int expectedDestCount, char* source, char* source_dev, time_t time_ath);
static int prepareDestination(struct sip_msg* msg, int isFirst);







int requestosprouting(struct sip_msg* msg, char* ignore1, char* ignore2) {

	int res;     /* used for function results */
	int valid;   /* returncode for this function */

	/* parameters for API call */
	char osp_source_dev[200];
	char e164_source[1000];
	char e164_dest [1000];
	unsigned int number_callids = 1;
	OSPTCALLID* call_ids[number_callids];
	unsigned int log_size = 0;
	char* detail_log = NULL;
	const char** preferred = NULL;
	unsigned int dest_count;
	OSPTTRANHANDLE transaction = -1;
	time_t time_auth;

	valid = MODULE_RETURNCODE_FALSE;

	time_auth = time(NULL);

	dest_count = _max_destinations;

	if (0!= (res=OSPPTransactionNew(_provider, &transaction))) {
		ERR("osp: Failed to create a new OSP transaction id %d\n",res);
	} else if (0 != getFromUserpart(msg, e164_source,sizeof(e164_source))) {
		ERR("osp: Failed to extract calling number\n");
	} else if (0 != getToUserpart(msg, e164_dest,sizeof(e164_dest))) {
		ERR("osp: Failed to extract called number\n");
	} else if (0 != getCallId(msg, &(call_ids[0]))) {
		ERR("osp: Failed to extract call id\n");
	} else if (0 != getSourceAddress(msg,osp_source_dev,sizeof(osp_source_dev))) {
		ERR("osp: Failed to extract source address\n");
	} else {
		DBG("osp: Requesting OSP authorization and routing for: "
		    "transaction-handle '%i' \n"
		    "osp_source '%s' "
		    "osp_source_port '%s' "
		    "osp_source_dev '%s' "
		    "e164_source '%s' "
		    "e164_dest '%s' "
		    "call-id '%.*s' "
		    "dest_count '%i'",
		    transaction,
		    _device_ip,
		    _device_port,
		    osp_source_dev,
		    e164_source,
		    e164_dest,
		    call_ids[0]->ospmCallIdLen,
		    call_ids[0]->ospmCallIdVal,
		    dest_count
		);	


		if (strlen(_device_port) > 0) {
			OSPPTransactionSetNetworkIds(transaction,_device_port,"");
		}

		/* try to request authorization */
		res = OSPPTransactionRequestAuthorisation(
		transaction,       /* transaction handle */
		_device_ip,         /* from the configuration file */
		osp_source_dev,    /* source of call, protocol specific */
		e164_source,       /* calling number in nodotted e164 notation */
		OSPC_E164,
		e164_dest,         /* called number */
		OSPC_E164,
		"",                /* optional username string, used if no number */
		number_callids,    /* number of call ids, here always 1 */
		call_ids,          /* sized-1 array of call ids */
		preferred,         /* preferred destinations, here always NULL */
		&dest_count,       /* max destinations, after call dest_count */
		&log_size,          /* size allocated for detaillog (next param) 0=no log */
		detail_log);       /* memory location for detaillog to be stored */

		if (res == 0 && dest_count > 0) {
			DBG("osp: there is %d osp routes, call-id '%.*s', transaction-id '%lld'\n",
			    dest_count,call_ids[0]->ospmCallIdLen, call_ids[0]->ospmCallIdVal,get_transaction_id(transaction));
			record_orig_transaction(msg,transaction,osp_source_dev,e164_source,e164_dest,time_auth);
			valid = loadosproutes(msg,transaction,dest_count,_device_ip,osp_source_dev,time_auth);
		} else if (res == 0 && dest_count == 0) {
			DBG("osp: there is 0 osp routes, the route is blocked, call-id '%.*s', transaction-id '%lld'\n",
			    call_ids[0]->ospmCallIdLen,call_ids[0]->ospmCallIdVal,get_transaction_id(transaction));
		} else {
			ERR("osp: OSPPTransactionRequestAuthorisation returned %i, call-id '%.*s', transaction-id '%lld'\n",
				res,call_ids[0]->ospmCallIdLen,call_ids[0]->ospmCallIdVal,get_transaction_id(transaction));
		}
	}

	if (call_ids[0]!=NULL) {
		OSPPCallIdDelete(&(call_ids[0]));
	}

	if (transaction!=-1) {
		OSPPTransactionDelete(transaction);
	}
	
	return valid;
}


static int loadosproutes(struct sip_msg* msg, OSPTTRANHANDLE transaction, int expectedDestCount, char* source, char* source_dev, time_t time_auth) {

	int result = MODULE_RETURNCODE_TRUE;
	int res;
	int count;

	osp_dest  dests[MAX_DESTS];
	osp_dest* dest;
	
	for (count = 0; count < expectedDestCount; count++) {

		dest = initDestination(&dests[count]);

		if (dest == NULL) {
			result = MODULE_RETURNCODE_FALSE;
			break;
		}

		if (count==0) {
			res = OSPPTransactionGetFirstDestination(
				transaction,
				sizeof(dest->validafter),
				dest->validafter,
				dest->validuntil,
				&dest->timelimit,
				&dest->sizeofcallid,
				(void*)dest->callid,
				sizeof(dest->callednumber),
				dest->callednumber,
				sizeof(dest->callingnumber),
				dest->callingnumber,
				sizeof(dest->destination),
				dest->destination,
				sizeof(dest->destinationdevice),
				dest->destinationdevice,
				&dest->sizeoftoken,
				dest->osptoken);
		} else {
			res = OSPPTransactionGetNextDestination(
				transaction,
				0,
				sizeof(dest->validafter),
				dest->validafter,
				dest->validuntil,
				&dest->timelimit,
				&dest->sizeofcallid,
				(void*)dest->callid,
				sizeof(dest->callednumber),
				dest->callednumber,
				sizeof(dest->callingnumber),
				dest->callingnumber,
				sizeof(dest->destination),
				dest->destination,
				sizeof(dest->destinationdevice),
				dest->destinationdevice,
				&dest->sizeoftoken,
				dest->osptoken);
		}

		
		if (res != 0) {
			ERR("osp: getDestination %d failed, expected number %d, current count %d\n",res,expectedDestCount,count);
			result = MODULE_RETURNCODE_FALSE;
			break;
		}

		OSPPTransactionGetDestNetworkId(transaction,dest->network_id);
		strcpy(dest->source,source);
		strcpy(dest->sourcedevice,source_dev);
		dest->type = OSPC_SOURCE;
		dest->tid = get_transaction_id(transaction);
		dest->time_auth = time_auth;

		DBG("osp: getDestination %d returned the following information: "
		    "valid after '%s' "
		    "valid until '%s' "
		    "time limit '%i' seconds "
		    "call-id '%.*s' "
		    "calling number '%s' "
		    "called number '%s' "
		    "destination '%s' "
		    "network id '%s' "
		    "bn token size '%i' ",
		    count, dest->validafter, dest->validuntil, dest->timelimit, dest->sizeofcallid, dest->callid, dest->callingnumber, dest->callednumber, 
		    dest->destination, dest->network_id, dest->sizeoftoken);
	}

	/* save destination in reverse order,
	 * this way, when we start searching avps the destinations
	 * will be in order 
	 */
	if (result == MODULE_RETURNCODE_TRUE) {
		for(count = expectedDestCount -1; count >= 0; count--) {
			saveOrigDestination(&dests[count]);
		}
	}

	return result;
}





int preparefirstosproute(struct sip_msg* msg, char* ignore1, char* ignore2) {
	int result = MODULE_RETURNCODE_TRUE;

	DBG("osp: Preparing 1st route\n");

	result = prepareDestination(msg,FIRST_ROUTE);

	return result;
}




int preparenextosproute(struct sip_msg* msg, char* ignore1, char* ignore2) {
	int result = MODULE_RETURNCODE_TRUE;

	DBG("osp: Preparing next route\n");

	result = prepareDestination(msg,NEXT_ROUTE);


	return result;
}




int prepareallosproutes(struct sip_msg* msg, char* ignore1, char* ignore2) {
	int result = MODULE_RETURNCODE_TRUE;

	for( result = preparefirstosproute(msg,ignore1,ignore2);
	     result == MODULE_RETURNCODE_TRUE;
	     result = preparenextosproute(msg,ignore1,ignore2)) {
	}

	return MODULE_RETURNCODE_TRUE;
}




int prepareDestination(struct sip_msg* msg, int isFirst) {
	int result = MODULE_RETURNCODE_TRUE;
	str newuri = {NULL,0};

	osp_dest* dest = getNextOrigDestination();

	if (dest != NULL) {

		rebuildDestionationUri(&newuri, dest->destination, dest->network_id, dest->callednumber);

		DBG("osp: Preparing route to uri '%.*s' for call-id '%.*s' transaction-id '%lld'\n",newuri.len,newuri.s,dest->sizeofcallid,dest->callid,dest->tid);

		if (isFirst == FIRST_ROUTE) {
			rewrite_uri(msg, &newuri);
			addOspHeader(msg,dest->osptoken,dest->sizeoftoken);
		} else {
			append_branch(msg, newuri.s, newuri.len, 0, 0, 0, NULL);
		}

	} else {
		DBG("osp: There is no more routes\n");

		reportOrigCallSetUpUsage();

		/* Terminating call set-up usage will be reported before the proxy replies with
		 * '503 - Service Unavailable' and a tm call back updates the term destination code.
		 * So, we will do it manually.
		 * We may need to make 503 configurable, just in case a user decides to reply with
		 * a different code.  Other options - trigger call-set up usage reporting from the cpl
		 * (after replying with an error code), or maybe use a different tm callback.
		 */
		recordEvent(0,503);
		reportTermCallSetUpUsage();

		result = MODULE_RETURNCODE_FALSE;
	}

	if (newuri.len > 0) {
		pkg_free(newuri.s);
	}
	
	return result;
}
