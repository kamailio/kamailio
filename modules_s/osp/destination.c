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
#include "../../sr_module.h"
#include "../../mem/mem.h"
#include "../../usr_avp.h"
#include "../../str.h"
#include "destination.h"
#include "usage.h"
#include <time.h>


/* A list of destination URIs */
str ORIG_OSPDESTS_LABEL = STR_STATIC_INIT("_orig_osp_dests_");
str TERM_OSPDESTS_LABEL = STR_STATIC_INIT("_term_osp_dests_");


static int saveDestination(osp_dest* dest, str* label);
static osp_dest* getLastOrigDestination();
static void recordCode(int code, osp_dest* dest);
static int isTimeToReportUsage(int code);




osp_dest* initDestination(osp_dest* dest) {

	memset(dest,0,sizeof(osp_dest));

	dest->sizeofcallid   =  sizeof(dest->callid);
	dest->sizeoftoken    =  sizeof(dest->osptoken);

	return dest;
}


int saveOrigDestination(osp_dest* dest) {
	DBG("osp: Saving originating destination\n");

	return saveDestination(dest,&ORIG_OSPDESTS_LABEL);
}

int saveTermDestination(osp_dest* dest) {
	DBG("osp: Saving terminating destination\n");

	return saveDestination(dest,&TERM_OSPDESTS_LABEL);
}

/** Save destination as an AVP
 *  name - label
 *  value - osp_dest wrapped in a string
 *
 *  Returns: 0 - success, -1 failure
 */
static int saveDestination(osp_dest* dest, str* label) {
	str wrapper;
	int status = -1;

	DBG("osp: Saving destination to avp\n");

	wrapper.s   = (char *)dest;
	wrapper.len = sizeof(osp_dest);

	/* add_avp will make a private copy of both the name and value in shared memory.
	 * memory will be released by TM at the end of the transaction
	 */
	if (add_avp(AVP_NAME_STR|AVP_VAL_STR,(int_str)(*label),(int_str)wrapper) == 0) {
		status = 0;
		DBG("osp: Saved\n");
	} else {
		ERR("osp: Failed to add_avp destination\n");
	}

	return status;
}


/** Retrieved an unused orig destination from an AVP
 *  name - ORIG_OSPDESTS_LABEL
 *  value - osp_dest wrapped in a string
 *  There can be 0, 1 or more orig destinations.  Find the 1st unused destination (used==0),
 *  return it, and mark it as used (used==1).
 *
 *  Returns: NULL on failure
 */
osp_dest* getNextOrigDestination() {
	osp_dest*       retVal   = NULL;
	osp_dest*       dest     = NULL;
	avp_t* dest_avp = NULL;
	struct search_state st;
	int_str         dest_val;

	DBG("osp: Looking for the first unused orig destination\n");

	for (	dest_avp=search_first_avp(AVP_NAME_STR|AVP_VAL_STR,(int_str)ORIG_OSPDESTS_LABEL,&dest_val, &st);
		dest_avp != NULL;
		dest_avp=search_next_avp(&st, &dest_val)) {
		
		/* osp dest is wrapped in a string */
		dest = (osp_dest *)dest_val.s.s;

		if (dest->used == 0) {
			DBG("osp: Found\n");
			break;
		} else {
			DBG("osp: This destination has already been used\n");
		}
	}

	if (dest != NULL && dest->used==0) {
		dest->used = 1;
		retVal = dest;
	} else {
		DBG("osp: There is no unused destinations\n");
	}

	return retVal;
}


/** Retrieved the last used orig destination from an AVP
 *  name - ORIG_OSPDESTS_LABEL
 *  value - osp_dest wrapped in a string
 *  There can be 0, 1 or more destinations.  Find the last used destination (used==1),
 *  and return it.
 *
 *  Returns: NULL on failure
 */
osp_dest* getLastOrigDestination() {

	osp_dest* dest = NULL;
	osp_dest* last_dest = NULL;
	avp_t* dest_avp = NULL;
	struct search_state st;
	int_str   dest_val;

	for (	dest_avp=search_first_avp(AVP_NAME_STR|AVP_VAL_STR,(int_str)ORIG_OSPDESTS_LABEL,&dest_val, &st);
		dest_avp != NULL;
		dest_avp=search_next_avp(&st, &dest_val)) {

		/* osp dest is wrapped in a string */
		dest = (osp_dest *)dest_val.s.s;

		if (dest->used == 1) {
			last_dest = dest;
			DBG("osp: getLastOrigDestination: updating curent destination to '%s'\n",last_dest->destination);
		} else {
			break;
		}
	}

	return last_dest;
}


/** Retrieved the term destination from an AVP
 *  name - TERM_OSPDESTS_LABEL
 *  value - osp_dest wrapped in a string
 *  There can be 0 or 1 term destinations.  Find and return it.
 *
 *  Returns: NULL on failure (no term destination)
 */
osp_dest* getTermDestination() {
	osp_dest* term_dest = NULL;
	avp_t* dest_avp = NULL;
	int_str   dest_val;

	dest_avp=search_first_avp(AVP_NAME_STR|AVP_VAL_STR,(int_str)TERM_OSPDESTS_LABEL,&dest_val, 0);

	if (dest_avp) {
		/* osp dest is wrapped in a string */
		term_dest = (osp_dest *)dest_val.s.s;
	}

	return term_dest;
}




void recordEvent(int client_code, int server_code) {
	DBG("osp: recordEvent: client code %d / server code %d\n",client_code, server_code);

	osp_dest* dest;

	if (client_code!=0 && (dest=getLastOrigDestination())) {
		recordCode(client_code,dest);

		if (isTimeToReportUsage(client_code)==0) {
			reportOrigCallSetUpUsage();
		}
	} 

	if (server_code!=0 && (dest=getTermDestination())) {
		recordCode(server_code,dest);

		if (isTimeToReportUsage(server_code)==0) {
			reportTermCallSetUpUsage();
		}
	}
}



static int isTimeToReportUsage(int code)
{
	int isTime;

	switch (code) {
		case 200:
		case 202:
		case 487:
			isTime = 0;
			DBG("Time to report call set up usage for code '%d'\n",code);
			break;

		default:
			isTime = -1;
			DBG("Do not report call set up usage for code '%d' yet\n",code);
			break;
	}

	return isTime;
}




void recordCode(int code, osp_dest* dest) {

	DBG("osp: recordCode: recording code %d\n",code);

	dest->last_code = code;

	switch (code) {
		case 100:
			if (!dest->time_100) {
				dest->time_100 = time(NULL);
			} else {
				DBG("osp: recordCode: 100 has already been recorded\n");
			}
			break;
		case 180:
		case 181:
		case 182:
		case 183:
			if (!dest->time_180) {
				dest->time_180 = time(NULL);
			} else {
				DBG("osp: recordCode: 180, 181, 182 or 183 has allready been recorded\n");
			}
			break;
		case 200:
		case 202:
			if (!dest->time_200) {
				dest->time_200 = time(NULL);
			} else {
				DBG("osp: recordCode: 200 or 202 has allready been recorded\n");
			}
			break;
		default:
			DBG("osp: recordCode: will not record time for this code\n");
	}

}






void dumpDebugInfo() {

	osp_dest*       dest     = NULL;
	avp_t* dest_avp = NULL;
	struct search_state st;
	int_str         dest_val;
	int             i = 0;

	DBG("osp: dumpDebugInfo: IN\n");

	for (	dest_avp=search_first_avp(AVP_NAME_STR|AVP_VAL_STR,(int_str)ORIG_OSPDESTS_LABEL,&dest_val, &st);
		dest_avp != NULL;
		dest_avp=search_next_avp(&st, &dest_val)) {

		/* osp dest is wrapped in a string */
		dest = (osp_dest *)dest_val.s.s;

		DBG("osp: dumpDebugInfo: .....orig index...'%d'\n", i);

		dumbDestDebugInfo(dest);

		i++;
	}
	if (i==0) {
		DBG("osp: dumpDebugInfo: There is no orig OSPDESTS AVP\n");
	}

	dest_avp=search_first_avp(AVP_NAME_STR|AVP_VAL_STR,(int_str)TERM_OSPDESTS_LABEL,&dest_val, 0);

	if (dest_avp) {
		/* osp dest is wrapped in a string */
		dest = (osp_dest *)dest_val.s.s;

		DBG("osp: dumpDebugInfo: .....destination......\n");

		dumbDestDebugInfo(dest);
	} else {
		DBG("osp: dumpDebugInfo: There is no dest OSPDESTS AVP\n");
	}


	DBG("osp: dumpDebugInfo: OUT\n");
}




void dumbDestDebugInfo(osp_dest *dest) {
	DBG("osp: dumpDebugInfo: dest->destination...'%s'\n", dest->destination);
	DBG("osp: dumpDebugInfo: dest->used..........'%d'\n", dest->used);
	DBG("osp: dumpDebugInfo: dest->last_code.....'%d'\n", dest->last_code);
	DBG("osp: dumpDebugInfo: dest->time_100......'%d'\n", (unsigned int)dest->time_100);
	DBG("osp: dumpDebugInfo: dest->time_180......'%d'\n", (unsigned int)dest->time_180);
	DBG("osp: dumpDebugInfo: dest->time_200......'%d'\n", (unsigned int)dest->time_200);
}

