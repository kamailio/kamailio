/*
 * Kamailio osp module.
 *
 * This module enables Kamailio to communicate with an Open Settlement
 * Protocol (OSP) server.  The Open Settlement Protocol is an ETSI
 * defined standard for Inter-Domain VoIP pricing, authorization
 * and usage exchange.  The technical specifications for OSP
 * (ETSI TS 101 321 V4.1.1) are available at www.etsi.org.
 *
 * Uli Abend was the original contributor to this module.
 *
 * Copyright (C) 2001-2005 Fhg Fokus
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * Kamailio is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * Kamailio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <string.h>
#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "../../core/str.h"
#include "../../core/dprint.h"
#include "../../core/usr_avp.h"
#include "destination.h"
#include "usage.h"

/* Name of AVP of OSP destination list */
const str OSP_ORIGDEST_NAME = {"_osp_orig_dests_", 16};
const str OSP_TERMDEST_NAME = {"_osp_term_dests_", 16};

static int ospSaveDestination(osp_dest *dest, const str *name);
static void ospRecordCode(int code, osp_dest *dest);
static int ospIsToReportUsage(int code);

/*
 * Initialize destination structure
 * param dest Destination data structure
 * return initialized destination sturcture
 */
osp_dest *ospInitDestination(osp_dest *dest)
{
	memset(dest, 0, sizeof(osp_dest));

	dest->callidsize = sizeof(dest->callid);
	dest->tokensize = sizeof(dest->token);

	LM_DBG("callidsize '%d' tokensize '%d'\n", dest->callidsize,
			dest->tokensize);

	return dest;
}

/* 
 * Save destination as an AVP
 *     name - OSP_ORIGDEST_NAME / OSP_TERMDEST_NAME
 *     value - osp_dest wrapped in a string
 * param dest Destination structure
 * param name Name of AVP
 * return 0 success, -1 failure
 */
static int ospSaveDestination(osp_dest *dest, const str *name)
{
	str wrapper;
	int result = -1;

	wrapper.s = (char *)dest;
	wrapper.len = sizeof(osp_dest);

	/* 
     * add_avp will make a private copy of both the name and value in shared memory 
     * which will be released by TM at the end of the transaction
     */
	if(add_avp(AVP_NAME_STR | AVP_VAL_STR, (int_str)*name, (int_str)wrapper)
			== 0) {
		LM_DBG("destination saved\n");
		result = 0;
	} else {
		LM_ERR("failed to save destination\n");
	}

	return result;
}

/*
 * Save originate destination
 * param dest Originate destination structure
 * return 0 success, -1 failure
 */
int ospSaveOrigDestination(osp_dest *dest)
{
	return ospSaveDestination(dest, &OSP_ORIGDEST_NAME);
}

/*
 * Save terminate destination
 * param dest Terminate destination structure
 * return 0 success, -1 failure
 */
int ospSaveTermDestination(osp_dest *dest)
{
	return ospSaveDestination(dest, &OSP_TERMDEST_NAME);
}

/* 
 * Check if there is an unused and supported originate destination from an AVP
 *     name - OSP_ORIGDEST_NAME
 *     value - osp_dest wrapped in a string
 *     search unused (used==0) & supported (support==1)
 * return 0 success, -1 failure
 */
int ospCheckOrigDestination(void)
{
	struct usr_avp *destavp = NULL;
	int_str destval;
	osp_dest *dest = NULL;
	int result = -1;
	struct search_state st;

	for(destavp = search_first_avp(AVP_NAME_STR | AVP_VAL_STR,
				(int_str)OSP_ORIGDEST_NAME, NULL, &st);
			destavp != NULL; destavp = search_next_avp(&st, NULL)) {
		get_avp_val(destavp, &destval);

		/* OSP destintaion is wrapped in a string */
		dest = (osp_dest *)destval.s.s;

		if(dest->used == 0) {
			if(dest->supported == 1) {
				LM_DBG("orig dest exist\n");
				result = 0;
				break;
			} else {
				LM_DBG("destination does not been supported\n");
			}
		} else {
			LM_DBG("destination has already been used\n");
		}
	}

	if(result == -1) {
		LM_DBG("there is not unused destination\n");
	}

	return result;
}

/* 
 * Retrieved an unused and supported originate destination from an AVP
 *     name - OSP_ORIGDEST_NAME
 *     value - osp_dest wrapped in a string
 *     There can be 0, 1 or more originate destinations. 
 *     Find the 1st unused destination (used==0) & supported (support==1),
 *     return it, and mark it as used (used==1).
 * return NULL on failure
 */
osp_dest *ospGetNextOrigDestination(void)
{
	struct usr_avp *destavp = NULL;
	int_str destval;
	osp_dest *dest = NULL;
	osp_dest *result = NULL;
	struct search_state st;

	for(destavp = search_first_avp(AVP_NAME_STR | AVP_VAL_STR,
				(int_str)OSP_ORIGDEST_NAME, NULL, &st);
			destavp != NULL; destavp = search_next_avp(&st, NULL)) {
		get_avp_val(destavp, &destval);

		/* OSP destintaion is wrapped in a string */
		dest = (osp_dest *)destval.s.s;

		if(dest->used == 0) {
			if(dest->supported == 1) {
				LM_DBG("orig dest found\n");
				dest->used = 1;
				result = dest;
				break;
			} else {
				/* Make it looks like used */
				dest->used = 1;
				/* 111 means wrong protocol */
				dest->lastcode = 111;
				LM_DBG("destination does not been supported\n");
			}
		} else {
			LM_DBG("destination has already been used\n");
		}
	}

	if(result == NULL) {
		LM_DBG("there is not unused destination\n");
	}

	return result;
}

/*
 * Retrieved the last used originate destination from an AVP
 *    name - OSP_ORIGDEST_NAME
 *    value - osp_dest wrapped in a string
 *    There can be 0, 1 or more destinations. 
 *    Find the last used destination (used==1) & supported (support==1),
 *    and return it.
 *    In normal condition, this one is the current destination. But it may
 *    be wrong for loop condition.
 *  return NULL on failure
 */
osp_dest *ospGetLastOrigDestination(void)
{
	struct usr_avp *destavp = NULL;
	int_str destval;
	osp_dest *dest = NULL;
	osp_dest *lastdest = NULL;
	struct search_state st;

	for(destavp = search_first_avp(AVP_NAME_STR | AVP_VAL_STR,
				(int_str)OSP_ORIGDEST_NAME, NULL, &st);
			destavp != NULL; destavp = search_next_avp(&st, NULL)) {
		get_avp_val(destavp, &destval);

		/* OSP destination is wrapped in a string */
		dest = (osp_dest *)destval.s.s;

		if(dest->used == 1) {
			if(dest->supported == 1) {
				lastdest = dest;
				LM_DBG("curent destination '%s'\n", lastdest->host);
			}
		} else {
			break;
		}
	}

	return lastdest;
}

/* 
 * Retrieved the terminate destination from an AVP
 *     name - OSP_TERMDEST_NAME
 *     value - osp_dest wrapped in a string
 *     There can be 0 or 1 term destinations. Find and return it.
 *  return NULL on failure (no terminate destination)
 */
osp_dest *ospGetTermDestination(void)
{
	int_str destval;
	osp_dest *dest = NULL;

	if(search_first_avp(AVP_NAME_STR | AVP_VAL_STR, (int_str)OSP_TERMDEST_NAME,
			   &destval, 0)
			!= NULL) {
		/* OSP destination is wrapped in a string */
		dest = (osp_dest *)destval.s.s;

		LM_DBG("term dest found\n");
	}

	return dest;
}

/*
 * Record destination status
 * param code Destination status
 * param dest Destination
 */
static void ospRecordCode(int code, osp_dest *dest)
{
	LM_DBG("code '%d'\n", code);
	dest->lastcode = code;

	switch(code) {
		case 100:
			if(!dest->time100) {
				dest->time100 = time(NULL);
			} else {
				LM_DBG("100 already recorded\n");
			}
			break;
		case 180:
		case 181:
		case 182:
		case 183:
			if(!dest->time180) {
				dest->time180 = time(NULL);
			} else {
				LM_DBG("180, 181, 182 or 183 allready recorded\n");
			}
			break;
		case 200:
		case 202:
			if(!dest->time200) {
				dest->time200 = time(NULL);
			} else {
				LM_DBG("200 or 202 allready recorded\n");
			}
			break;
		default:
			LM_DBG("will not record time for '%d'\n", code);
	}
}

/*
 * Check destination status for reporting usage
 * param code Destination status
 * return 1 should report, 0 should not report
 */
static int ospIsToReportUsage(int code)
{
	int istime = 0;

	LM_DBG("code '%d'\n", code);
	if(code >= 200) {
		istime = 1;
	}

	return istime;
}

/*
 * Report call setup usage for both client and server side
 * param clientcode Client status
 * param servercode Server status
 */
void ospRecordEvent(int clientcode, int servercode)
{
	osp_dest *dest;

	LM_DBG("client status '%d'\n", clientcode);
	if((clientcode != 0) && (dest = ospGetLastOrigDestination())) {
		ospRecordCode(clientcode, dest);

		if(ospIsToReportUsage(servercode) == 1) {
			ospReportOrigSetupUsage();
		}
	}

	LM_DBG("server status '%d'\n", servercode);
	if((servercode != 0) && (dest = ospGetTermDestination())) {
		ospRecordCode(servercode, dest);

		if(ospIsToReportUsage(servercode) == 1) {
			ospReportTermSetupUsage();
		}
	}
}

/*
 * Dump destination information
 * param dest Destination
 */
void ospDumpDestination(osp_dest *dest)
{
	LM_DBG("dest->host..........'%s'\n", dest->host);
	LM_DBG("dest->used..........'%d'\n", dest->used);
	LM_DBG("dest->lastcode......'%d'\n", dest->lastcode);
	LM_DBG("dest->time100.......'%d'\n", (unsigned int)dest->time100);
	LM_DBG("dest->time180.......'%d'\n", (unsigned int)dest->time180);
	LM_DBG("dest->time200.......'%d'\n", (unsigned int)dest->time200);
}

/*
 * Dump all destination information
 */
void ospDumpAllDestination(void)
{
	struct usr_avp *destavp = NULL;
	int_str destval;
	osp_dest *dest = NULL;
	int count = 0;
	struct search_state st;

	for(destavp = search_first_avp(AVP_NAME_STR | AVP_VAL_STR,
				(int_str)OSP_ORIGDEST_NAME, NULL, &st);
			destavp != NULL; destavp = search_next_avp(&st, NULL)) {
		get_avp_val(destavp, &destval);

		/* OSP destination is wrapped in a string */
		dest = (osp_dest *)destval.s.s;

		LM_DBG("....originate '%d'....\n", count++);

		ospDumpDestination(dest);
	}
	if(count == 0) {
		LM_DBG("there is not originate destination AVP\n");
	}

	if(search_first_avp(AVP_NAME_STR | AVP_VAL_STR, (int_str)OSP_TERMDEST_NAME,
			   &destval, 0)
			!= NULL) {
		/* OSP destination is wrapped in a string */
		dest = (osp_dest *)destval.s.s;

		LM_DBG("....terminate....\n");

		ospDumpDestination(dest);
	} else {
		LM_DBG("there is not terminate destination AVP\n");
	}
}

/*
 * Convert address to "[x.x.x.x]" or "host.domain" format
 * param src Source address
 * param dst Destination address
 * param buffersize Size of dst buffer
 */
void ospConvertAddress(char *src, char *dst, int buffersize)
{
	struct in_addr inp;

	if(inet_aton(src, &inp) != 0) {
		snprintf(dst, buffersize, "[%s]", src);
	} else {
		snprintf(dst, buffersize, "%s", src);
	}
}
