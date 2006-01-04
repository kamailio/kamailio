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
#include "term_transaction.h"
#include "sipheader.h"
#include "destination.h"
#include "usage.h"
#include "osp/osp.h"
#include "osp/ospb64.h"
#include "../../sr_module.h"
#include "../../locking.h"
#include "../../mem/mem.h"

extern OSPTPROVHANDLE _provider;
extern int _token_format;
extern int _validate_call_id;
extern char* _device_ip;


int checkospheader(struct sip_msg* msg, char* ignore1, char* ignore2) {
	char temp[3000];
	int  sizeoftemp = sizeof(temp);


	if (getOspHeader(msg, temp, &sizeoftemp) != 0) {
		return MODULE_RETURNCODE_FALSE;
	} else {
		return MODULE_RETURNCODE_TRUE;
	}
}




/** validate OSP header */
int validateospheader (struct sip_msg* msg, char* ignore1, char* ignore2) {
	int res; 
	int valid;

	OSPTTRANHANDLE transaction = -1;
	unsigned int authorized = 0;
	unsigned int time_limit = 0;
	unsigned int log_size = 0;
	void *detail_log = NULL;
	OSPTCALLID* call_id = NULL;
	osp_dest  dest;

	char token[3000];
	int sizeoftoken = sizeof(token);

	unsigned callIdLen = 0;
	unsigned char* callIdVal = (unsigned char*)"";

	

	valid = MODULE_RETURNCODE_FALSE;

	initDestination(&dest);

	if (0!= (res=OSPPTransactionNew(_provider, &transaction))) {
		ERR("osp: Failed to create a new OSP transaction id %d\n",res);
	} else if (0 != getFromUserpart(msg, dest.callingnumber, sizeof(dest.callingnumber))) {
		ERR("osp: Failed to extract calling number\n");
	} else if (0 != getToUserpart(msg, dest.callednumber, sizeof(dest.callednumber))) {
		ERR("osp: Failed to extract called number\n");
	} else if (0 != getCallId(msg, &call_id)) {
		ERR("osp: Failed to extract call id\n");
	} else if (0 != getSourceAddress(msg,dest.source,sizeof(dest.source))) {
		ERR("osp: Failed to extract source address\n");
	} else if (0 != getOspHeader(msg, token, &sizeoftoken)) {
		ERR("osp: Failed to extract OSP authorization token\n");
	} else {
		DBG("About to validate OSP token for:\n"
			"transaction-handle = >%i< \n"
			"e164_source = >%s< \n"
			"e164_dest = >%s< \n"
			"validate_call_id = >%s< \n"
			"call-id = >%.*s< \n",
			transaction,
			dest.callingnumber,
			dest.callednumber,
			_validate_call_id==0?"No":"Yes",
			call_id->ospmCallIdLen,
			call_id->ospmCallIdVal);

		if (_validate_call_id != 0) {
			callIdLen = call_id->ospmCallIdLen;
			callIdVal = call_id->ospmCallIdVal;
		}

		res = OSPPTransactionValidateAuthorisation(
			transaction,
			"",
			"",
			"",
			"",
			dest.callingnumber,
			OSPC_E164,
			dest.callednumber,
			OSPC_E164,
			callIdLen,
			callIdVal,
			sizeoftoken,
			token,
			&authorized,
			&time_limit,
			&log_size,
			detail_log,
			_token_format);
	
		memcpy(dest.callid,call_id->ospmCallIdVal,call_id->ospmCallIdLen);
		dest.sizeofcallid = call_id->ospmCallIdLen;
		dest.tid = get_transaction_id(transaction);
		dest.type = OSPC_DESTINATION;
		dest.time_auth = time(NULL);
		strcpy(dest.destination,_device_ip);

		saveTermDestination(&dest);

		if (res == 0 && authorized == 1) {
			DBG("osp: Call is authorized for %d seconds, call-id '%.*s', transaction-id '%lld'",
				time_limit,dest.sizeofcallid,dest.callid,dest.tid);
			record_term_transaction(msg,transaction,dest.source,dest.callingnumber,dest.callednumber,dest.time_auth);
			valid = MODULE_RETURNCODE_TRUE;
		} else {
			ERR("osp: Token is not valid, code %i\n", res);

			/* Update terminating status code to 401 and report terminating set-up usage.
			 * We may need to make 401 configurable, just in case a user decides to reply with
			 * a different code.  Other options - trigger call-set up usage reporting from the cpl
			 * (after replying with an error code), or maybe use a different tm callback.
			 */
			recordEvent(0,401);
			reportTermCallSetUpUsage();
		}
	}

	if (transaction != -1) {
		OSPPTransactionDelete(transaction);
	}

	if (call_id!=NULL) {
		OSPPCallIdDelete(&call_id);
	}
	
	return valid;
}

