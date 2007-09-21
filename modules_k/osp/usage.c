/*
 * openser osp module. 
 *
 * This module enables openser to communicate with an Open Settlement 
 * Protocol (OSP) server.  The Open Settlement Protocol is an ETSI 
 * defined standard for Inter-Domain VoIP pricing, authorization
 * and usage exchange.  The technical specifications for OSP 
 * (ETSI TS 101 321 V4.1.1) are available at www.etsi.org.
 *
 * Uli Abend was the original contributor to this module.
 * 
 * Copyright (C) 2001-2005 Fhg Fokus
 *
 * This file is part of openser, a free SIP server.
 *
 * openser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * openser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 * 
 * History:
 * ---------
 *  2006-03-13  RR functions are loaded via API function (bogdan)
 */

#include "../rr/api.h"
#include "../../usr_avp.h"
#include "usage.h"
#include "destination.h"
#include "osptoolkit.h"
#include "sipheader.h"

#define OSP_ORIG_COOKIE     "osp-o"
#define OSP_TERM_COOKIE     "osp-t"

#define OSP_RELEASE_ORIG    0
#define OSP_RELEASE_TERM    1

extern char* _osp_device_ip;
extern OSPTPROVHANDLE _osp_provider;
extern str OSP_ORIGDEST_NAME;
extern struct rr_binds osp_rr;

static void ospRecordTransaction(struct sip_msg* msg, unsigned long long tranid, char* uac, char* from, char* to, time_t authtime, int isorig, unsigned destinationCount);
static int ospBuildUsageFromDestination(OSPTTRANHANDLE transaction, osp_dest* dest, int lastcode);
static int ospReportUsageFromDestination(OSPTTRANHANDLE transaction, osp_dest* dest);
static int ospReportUsageFromCookie(struct sip_msg* msg, char* cooky, OSPTCALLID* callid, int release, int isorig);

/*
 * Create OSP cookie and insert it into Record-Route header
 * param msg SIP message
 * param tansaction Transaction handle
 * param uac Source IP
 * param from
 * param to
 * param authtime Request authorization time
 * param isorig Originate / Terminate
 */
static void ospRecordTransaction(
    struct sip_msg* msg, 
    unsigned long long tranid, 
    char* uac, 
    char* from, 
    char* to, 
    time_t authtime, 
    int isorig,
    unsigned destinationCount)
{
    str cookie;
    char buffer[OSP_STRBUF_SIZE];

    if (osp_rr.add_rr_param == 0) {
        LM_WARN(
            "add_rr_param function is not found, "
            "cannot record information about the "
            "OSP transaction\n");
        return;
    }

    cookie.s = buffer;

    if (isorig == 1) {
        cookie.len = snprintf(
            buffer,
            sizeof(buffer),
            ";%s=t%llu_s%s_T%d_c%d",
            OSP_ORIG_COOKIE,
            tranid,
            uac,
            (unsigned int)authtime,
            destinationCount);
    } else {
        cookie.len = snprintf(
            buffer,
            sizeof(buffer),
            ";%s=t%llu_s%s_T%d",
            OSP_TERM_COOKIE,
            tranid,
            uac,
            (unsigned int)authtime);
    }

    if (cookie.len < 0) {
        LM_ERR("failed to create OSP cookie\n");
        return;
    }

    LM_DBG("adding RR parameter '%s'\n", buffer);
    osp_rr.add_rr_param(msg, &cookie);
}

/*
 * Create OSP originate cookie and insert it into Record-Route header
 * param msg SIP message
 * param tansaction Transaction handle
 * param uac Source IP
 * param from
 * param to
 * param authtime Request authorization time
 */
void ospRecordOrigTransaction(
    struct sip_msg* msg, 
    unsigned long long tranid, 
    char* uac, 
    char* from, 
    char* to, 
    time_t authtime,
    unsigned destinationCount)
{
    int isorig = 1;

    ospRecordTransaction(msg, tranid, uac, from, to, authtime, isorig, destinationCount);
}

/*
 * Create OSP terminate cookie and insert it into Record-Route header
 * param msg SIP message
 * param tansaction Transaction handle
 * param uac Source IP
 * param from
 * param to
 * param authtime Request authorization time
 */
void ospRecordTermTransaction(
    struct sip_msg* msg, 
    unsigned long long tranid, 
    char* uac, 
    char* from, 
    char* to, 
    time_t authtime)
{
    int isorig = 0;
    unsigned destinationCount = 0; /* N/A */

    ospRecordTransaction(msg, tranid, uac, from, to, authtime, isorig, destinationCount);
}

/*
 * Report OSP usage from OSP cookie
 * param msg SIP message
 * param cookie OSP cookie
 * param callid Call ID
 * param release Who releases the call first. 0 orig, 1 term
 * param isorig Originate / Terminate
 * return
 */
static int ospReportUsageFromCookie(
    struct sip_msg* msg,
    char* cookie, 
    OSPTCALLID* callid, 
    int release,
    int isorig)
{
    char* tmp;
    char* token;
    char tag;
    char* value;
    unsigned long long transactionid = 0;
    char* uac = "";
    time_t authtime = -1;
    time_t endtime = time(NULL);
    char firstvia[OSP_STRBUF_SIZE];
    char from[OSP_STRBUF_SIZE];
    char to[OSP_STRBUF_SIZE];
    char nexthop[OSP_STRBUF_SIZE];
    char* calling;
    char* called;
    char* terminator;
    unsigned issource;
    char* source;
    char srcbuf[OSP_STRBUF_SIZE];
    char* destination;
    char dstbuf[OSP_STRBUF_SIZE];
    char* srcdev;
    char devbuf[OSP_STRBUF_SIZE];
    OSPTTRANHANDLE transaction = -1;
    int errorcode;
    unsigned destinationCount = 0;

    LM_DBG("'%s' isorig '%d'\n", cookie, isorig);
    for (token = strtok_r(cookie, "_", &tmp);
        token;
        token = strtok_r(NULL, "_", &tmp))
    {
        tag = *token;
        value= token + 1;

        switch (tag) {
            case 't':
                transactionid = atoll(value);
                break;
            case 'T':
                authtime = atoi(value);
                break;
            case 's':
                uac = value;
                break;
            case 'c':
                destinationCount = (unsigned)atoi(value);
                break;
            default:
                LM_ERR("unexpected tag '%c' / value '%s'\n", tag, value);
                break;
        }
    }

    ospGetSourceAddress(msg, firstvia, sizeof(firstvia));
    ospGetFromUserpart(msg, from, sizeof(from));
    ospGetToUserpart(msg, to, sizeof(to));
    ospGetNextHop(msg, nexthop, sizeof(nexthop));

    LM_DBG("first via '%s' from '%s' to '%s' next hop '%s'\n",
        firstvia,
        from,
        to,
        nexthop);

    if (release == OSP_RELEASE_ORIG) {
        LM_DBG(
            "orig '%s' released the call, call_id '%.*s' "
			"transaction_id '%lld'\n",
            firstvia,
            callid->ospmCallIdLen,
            callid->ospmCallIdVal,
            transactionid);
        calling = from;
        called = to;
        terminator = nexthop;
    } else {
        release = OSP_RELEASE_TERM;
        LM_DBG(
            "term '%s' released the call, call_id '%.*s' "
			"transaction_id '%lld'\n",
            firstvia,
            callid->ospmCallIdLen,
            callid->ospmCallIdVal,
            transactionid);
        calling = to;
        called = from;
        terminator = firstvia;
    }

    errorcode = OSPPTransactionNew(_osp_provider, &transaction);

    LM_DBG("created transaction handle '%d' (%d)\n", transaction, errorcode);

    if (isorig == 1) {
        issource = OSPC_SOURCE;
        source = _osp_device_ip;
        ospConvertAddress(terminator, dstbuf, sizeof(dstbuf));
        destination = dstbuf;
        ospConvertAddress(uac, devbuf, sizeof(devbuf));
        srcdev = devbuf;
    } else {
        issource = OSPC_DESTINATION;
        ospConvertAddress(uac, srcbuf, sizeof(srcbuf));
        source = srcbuf;
        destination = _osp_device_ip;
        srcdev = "";
    }

    errorcode = OSPPTransactionBuildUsageFromScratch(
        transaction,
        transactionid,
        issource,
        source,
        destination,
        srcdev,
        "",
        calling,
        OSPC_E164,
        called,
        OSPC_E164,
        callid->ospmCallIdLen,
        callid->ospmCallIdVal,
        (enum OSPEFAILREASON)0,
        NULL,
        NULL);

    LM_DBG("built usage handle '%d' (%d)\n", transaction, errorcode);

    if (errorcode == 0 && destinationCount > 0) {
        errorcode = OSPPTransactionSetDestinationCount(
            transaction,
            destinationCount);
    }

    ospReportUsageWrapper(
        transaction,
        10016,
        endtime - authtime,
        authtime,
        endtime,
        0,0,
        0,0,
        release);

    return errorcode;
}

/*
 * Report OSP usage
 * param msg SIP message
 * param whorelease Who releases the call first, 0 orig, 1 term
 * param ignore2
 * return MODULE_RETURNCODE_TRUE success, MODULE_RETURNCODE_FALSE failure
 */
int ospReportUsage(
    struct sip_msg* msg, 
    char* whorelease, 
    char* ignore2)
{
    int release;
    char* tmp;
    char* token;
    char parameters[OSP_HEADERBUF_SIZE];
    int isorig;
    OSPTCALLID* callid = NULL;
    int result = MODULE_RETURNCODE_FALSE;

    ospGetCallId(msg, &callid);

    if (callid != NULL && ospGetRouteParameters(msg, parameters, sizeof(parameters)) == 0) {
        /* Who releases the call first, 0 orig, 1 term */
        if (sscanf(whorelease, "%d", &release) != 1) {
            release = OSP_RELEASE_ORIG;
        }
        LM_DBG("who releases the call first '%d'\n", release);

        for (token = strtok_r(parameters, ";", &tmp);
             token;
             token = strtok_r(NULL, ";", &tmp))
        {
            if (strncmp(token, OSP_ORIG_COOKIE, strlen(OSP_ORIG_COOKIE)) == 0) {
                LM_INFO("report originate duration usage for call_id '%.*s'\n",
                    callid->ospmCallIdLen,
                    callid->ospmCallIdVal);
                isorig = 1;
                ospReportUsageFromCookie(msg, token + strlen(OSP_ORIG_COOKIE) + 1, callid, release, isorig);
                result = MODULE_RETURNCODE_TRUE;
            } else if (strncmp(token, OSP_TERM_COOKIE, strlen(OSP_TERM_COOKIE)) == 0) {
                LM_INFO("report terminate duration usage for call_id '%.*s'\n",
                    callid->ospmCallIdLen,
                    callid->ospmCallIdVal);
                isorig = 0;
                ospReportUsageFromCookie(msg, token + strlen(OSP_TERM_COOKIE) + 1, callid, release, isorig);
                result = MODULE_RETURNCODE_TRUE;
            } else {
                LM_DBG("ignoring parameter '%s'\n", token);
            }
        }
    }

    if (callid != NULL) {
        OSPPCallIdDelete(&callid);
    }

    if (result == MODULE_RETURNCODE_FALSE) {
        LM_DBG("without OSP orig or term usage information\n");
    }

    return result;
}

/*
 * Build OSP usage from destination
 * param transaction OSP transaction handle
 * param dest Destination
 * param lastcode Destination status
 * return 0 success, others failure
 */
static int ospBuildUsageFromDestination(
    OSPTTRANHANDLE transaction, 
    osp_dest* dest, 
    int lastcode)
{
    int errorcode;
    char addr[OSP_STRBUF_SIZE];
    char* source;
    char* srcdev;

    dest->reported = 1;

    if (dest->type == OSPC_SOURCE) {
        ospConvertAddress(dest->srcdev, addr, sizeof(addr));
        source = dest->source;
        srcdev = addr;
    } else {
        ospConvertAddress(dest->source, addr, sizeof(addr));
        source = addr;
        srcdev = dest->srcdev;
    }

    errorcode = OSPPTransactionBuildUsageFromScratch(
        transaction,
        dest->tid,
        dest->type,
        source,
        dest->host,
        srcdev,
        dest->destdev,
        dest->calling,
        OSPC_E164,
        dest->called,
        OSPC_E164,
        dest->callidsize,
        dest->callid,
        (enum OSPEFAILREASON)lastcode,
        NULL,
        NULL);

    return errorcode;
}

/*
 * Report OSP usage from destination
 * param transaction OSP transaction handle
 * param dest Destination
 * return 0 success
 */
static int ospReportUsageFromDestination(
    OSPTTRANHANDLE transaction, 
    osp_dest* dest)
{
    ospReportUsageWrapper(
        transaction,                                          /* In - Transaction handle */
        dest->lastcode,                                       /* In - Release Code */    
        0,                                                    /* In - Length of call */
        dest->authtime,                                       /* In - Call start time */
        0,                                                    /* In - Call end time */
        dest->time180,                                        /* In - Call alert time */
        dest->time200,                                        /* In - Call connect time */
        dest->time180 ? 1 : 0,                                /* In - Is PDD Info present */
        dest->time180 ? dest->time180 - dest->authtime : 0,   /* In - Post Dial Delay */
        0);

    return 0;
}

/*
 * Report originate call setup usage
 */
void ospReportOrigSetupUsage(void)
{
    osp_dest* dest = NULL;
    osp_dest* lastused = NULL;
    struct usr_avp* destavp = NULL;
    int_str destval;
    OSPTTRANHANDLE transaction = -1;
    int lastcode = 0;
    int errorcode = 0;

    errorcode = OSPPTransactionNew(_osp_provider, &transaction);

    for (destavp = search_first_avp(AVP_NAME_STR | AVP_VAL_STR, (int_str)OSP_ORIGDEST_NAME, NULL, 0);
        destavp != NULL;
        destavp = search_next_avp(destavp, NULL))
    {
        get_avp_val(destavp, &destval);

        /* OSP destination is wrapped in a string */
        dest = (osp_dest*)destval.s.s;

        if (dest->used == 1) {
            if (dest->reported == 1) {
                LM_DBG("orig setup already reported\n");
                break;
            }

            LM_DBG("iterating through used destination\n");

            ospDumpDestination(dest);

            lastused = dest;

            errorcode = ospBuildUsageFromDestination(transaction, dest, lastcode);

            lastcode = dest->lastcode;
        } else {
            LM_DBG("destination has not been used, breaking out\n");
            break;
        }
    }

    if (lastused) {
        LM_INFO("report orig setup for call_id '%.*s' transaction_id '%lld'\n",
            lastused->callidsize,
            lastused->callid,
            lastused->tid);
        errorcode = ospReportUsageFromDestination(transaction, lastused);
    } else {
        /* If a Toolkit transaction handle was created, but we did not find
         * any destinations to report, we need to release the handle. Otherwise,
         * the ospReportUsageFromDestination will release it.
         */
        OSPPTransactionDelete(transaction);
    }
}

/*
 * Report terminate call setup usage
 */
void ospReportTermSetupUsage(void)
{
    osp_dest* dest = NULL;
    OSPTTRANHANDLE transaction = -1;
    int errorcode = 0;

    if ((dest = ospGetTermDestination())) {
        if (dest->reported == 0) {
            LM_INFO("report term setup for call_id '%.*s' "
				"transaction_id '%lld'\n",
                dest->callidsize,
                dest->callid,
                dest->tid);
            errorcode = OSPPTransactionNew(_osp_provider, &transaction);
            errorcode = ospBuildUsageFromDestination(transaction, dest, 0);
            errorcode = ospReportUsageFromDestination(transaction, dest);
        } else {
            LM_DBG("term setup already reported\n");
        }
    } else {
        LM_ERR("without term setup to report\n");
    }
}
