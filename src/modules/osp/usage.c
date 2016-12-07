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
 * 
 * History:
 * ---------
 *  2006-03-13  RR functions are loaded via API function (bogdan)
 */

#include <osp/osp.h>
#include "../rr/api.h"
#include "../../usr_avp.h"
#include "usage.h"
#include "destination.h"
#include "osptoolkit.h"
#include "sipheader.h"

#define OSP_ORIG_COOKIE         "osp-o"
#define OSP_TERM_COOKIE         "osp-t"

#define OSP_RELEASE_ORIG        0
#define OSP_RELEASE_TERM        1

/* The up case tags for the destinations may corrupt OSP cookies */
#define OSP_COOKIE_TRANSID      't'
#define OSP_COOKIE_TRANSIDUP    'T'
#define OSP_COOKIE_SRCIP        's'
#define OSP_COOKIE_SRCIPUP      'S'
#define OSP_COOKIE_AUTHTIME     'a'
#define OSP_COOKIE_AUTHTIMEUP   'A'
#define OSP_COOKIE_DSTCOUNT     'c'
#define OSP_COOKIE_DSTCOUNTUP   'C'

/* Flags for OSP cookies */
#define OSP_COOKIEHAS_TRANSID   (1 << 0)
#define OSP_COOKIEHAS_SRCIP     (1 << 1)
#define OSP_COOKIEHAS_AUTHTIME  (1 << 2)
#define OSP_COOKIEHAS_DSTCOUNT  (1 << 3)
#define OSP_COOKIEHAS_ORIGALL   (OSP_COOKIEHAS_TRANSID | OSP_COOKIEHAS_SRCIP | OSP_COOKIEHAS_AUTHTIME | OSP_COOKIEHAS_DSTCOUNT) 
#define OSP_COOKIEHAS_TERMALL   (OSP_COOKIEHAS_TRANSID | OSP_COOKIEHAS_SRCIP | OSP_COOKIEHAS_AUTHTIME) 

extern char* _osp_device_ip;
extern OSPTPROVHANDLE _osp_provider;
extern str OSP_ORIGDEST_NAME;
extern struct rr_binds osp_rr;

static void ospRecordTransaction(struct sip_msg* msg, unsigned long long transid, char* uac, char* from, char* to, time_t authtime, int isorig, unsigned destinationCount);
static int ospBuildUsageFromDestination(OSPTTRANHANDLE transaction, osp_dest* dest, int lastcode);
static int ospReportUsageFromDestination(OSPTTRANHANDLE transaction, osp_dest* dest);
static int ospReportUsageFromCookie(struct sip_msg* msg, char* cooky, OSPTCALLID* callid, int release, OSPE_MSG_ROLETYPES type);

/*
 * Create OSP cookie and insert it into Record-Route header
 * param msg SIP message
 * param tansid Transaction ID
 * param uac Source IP
 * param from
 * param to
 * param authtime Request authorization time
 * param isorig Originate / Terminate
 * param destinationCount Destination count
 */
static void ospRecordTransaction(
    struct sip_msg* msg, 
    unsigned long long transid,
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
        LM_WARN("add_rr_param function is not found, cannot record information about the OSP transaction\n");
        return;
    }

    cookie.s = buffer;

    if (isorig == 1) {
        cookie.len = snprintf(
            buffer,
            sizeof(buffer),
            ";%s=%c%llu_%c%s_%c%d_%c%d",
            OSP_ORIG_COOKIE,
            OSP_COOKIE_TRANSID,
            transid,
            OSP_COOKIE_SRCIP,
            uac,
            OSP_COOKIE_AUTHTIME,
            (unsigned int)authtime,
            OSP_COOKIE_DSTCOUNT,
            destinationCount);
    } else {
        cookie.len = snprintf(
            buffer,
            sizeof(buffer),
            ";%s=%c%llu_%c%s_%c%d",
            OSP_TERM_COOKIE,
            OSP_COOKIE_TRANSID,
            transid,
            OSP_COOKIE_SRCIP,
            uac,
            OSP_COOKIE_AUTHTIME,
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
 * param tansid Transaction ID
 * param uac Source IP
 * param from
 * param to
 * param authtime Request authorization time
 * param destinationCount Destination count
 */
void ospRecordOrigTransaction(
    struct sip_msg* msg, 
    unsigned long long transid, 
    char* uac, 
    char* from, 
    char* to, 
    time_t authtime,
    unsigned destinationCount)
{
    int isorig = 1;

    ospRecordTransaction(msg, transid, uac, from, to, authtime, isorig, destinationCount);
}

/*
 * Create OSP terminate cookie and insert it into Record-Route header
 * param msg SIP message
 * param tansid Transaction ID
 * param uac Source IP
 * param from
 * param to
 * param authtime Request authorization time
 */
void ospRecordTermTransaction(
    struct sip_msg* msg, 
    unsigned long long transid, 
    char* uac, 
    char* from, 
    char* to, 
    time_t authtime)
{
    int isorig = 0;
    unsigned destinationCount = 0; /* N/A */

    ospRecordTransaction(msg, transid, uac, from, to, authtime, isorig, destinationCount);
}

/*
 * Report OSP usage from OSP cookie
 * param msg SIP message
 * param cookie OSP cookie (buffer owned by ospReportUsage, can be modified)
 * param callid Call ID
 * param release Who releases the call first. 0 orig, 1 term
 * param type Usage type
 * return
 */
static int ospReportUsageFromCookie(
    struct sip_msg* msg,
    char* cookie, 
    OSPTCALLID* callid, 
    int release,
    OSPE_MSG_ROLETYPES type)
{
    char* tmp;
    char* token;
    char tag;
    char* value;
    unsigned long long transid = 0;
    time_t authtime = 0;
    unsigned destinationCount = 0;
    time_t duration = 0;
    time_t endtime = time(NULL);
    int cookieflags = 0;
    unsigned releasecode;
    char firstvia[OSP_STRBUF_SIZE];
    char from[OSP_STRBUF_SIZE];
    char to[OSP_STRBUF_SIZE];
    char nexthop[OSP_STRBUF_SIZE];
    char* calling;
    char* called;
    char* originator = NULL;
    char* terminator;
    char* source;
    char srcbuf[OSP_STRBUF_SIZE];
    char* destination;
    char dstbuf[OSP_STRBUF_SIZE];
    char* srcdev;
    char devbuf[OSP_STRBUF_SIZE];
    OSPTTRANHANDLE transaction = -1;
    int errorcode;

    LM_DBG("'%s' type '%d'\n", cookie, type);
    if (cookie != NULL) {
        for (token = strtok_r(cookie, "_", &tmp);
            token;
            token = strtok_r(NULL, "_", &tmp))
        {
            tag = *token;
            value= token + 1;

            switch (tag) {
                case OSP_COOKIE_TRANSID:
                case OSP_COOKIE_TRANSIDUP:
                    transid = atoll(value);
                    cookieflags |= OSP_COOKIEHAS_TRANSID;
                    break;
                case OSP_COOKIE_AUTHTIME:
                case OSP_COOKIE_AUTHTIMEUP:
                    authtime = atoi(value);
                    duration = endtime - authtime;
                    cookieflags |= OSP_COOKIEHAS_AUTHTIME;
                    break;
                case OSP_COOKIE_SRCIP:
                case OSP_COOKIE_SRCIPUP:
                    originator = value;
                    cookieflags |= OSP_COOKIEHAS_SRCIP;
                    break;
                case OSP_COOKIE_DSTCOUNT:
                case OSP_COOKIE_DSTCOUNTUP:
                    destinationCount = (unsigned)atoi(value);
                    cookieflags |= OSP_COOKIEHAS_DSTCOUNT;
                    break;
                default:
                    LM_ERR("unexpected tag '%c' / value '%s'\n", tag, value);
                    break;
            }
        }
    }

    switch (type) {
        case OSPC_DESTINATION:
            if (cookieflags == OSP_COOKIEHAS_TERMALL) {
                releasecode = 10016;
            } else {
                releasecode = 9016;
            }
            break;
        case OSPC_SOURCE:
        case OSPC_OTHER:
        case OSPC_UNDEFINED_ROLE:
        default:
            if (cookieflags == OSP_COOKIEHAS_ORIGALL) {
                releasecode = 10016;
            } else {
                releasecode = 9016;
            }
            break;
    }

    if (releasecode == 9016) {
        transid = 0;
        originator = NULL;
        authtime = 0;
        duration = 0;
        destinationCount = 0;
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
        LM_DBG("orig '%s' released the call, call_id '%.*s' transaction_id '%llu'\n",
            firstvia,
            callid->ospmCallIdLen,
            callid->ospmCallIdVal,
            transid);
        if (originator == NULL) {
            originator = firstvia;
        }
        calling = from;
        called = to;
        terminator = nexthop;
    } else {
        release = OSP_RELEASE_TERM;
        LM_DBG("term '%s' released the call, call_id '%.*s' transaction_id '%llu'\n",
            firstvia,
            callid->ospmCallIdLen,
            callid->ospmCallIdVal,
            transid);
        if (originator == NULL) {
            originator = nexthop;
        }
        calling = to;
        called = from;
        terminator = firstvia;
    }

    errorcode = OSPPTransactionNew(_osp_provider, &transaction);

    LM_DBG("created transaction handle '%d' (%d)\n", transaction, errorcode);

    switch (type) {
        case OSPC_DESTINATION:
            ospConvertAddress(originator, srcbuf, sizeof(srcbuf));
            source = srcbuf;
            destination = _osp_device_ip;
            srcdev = "";
            break;
        case OSPC_SOURCE:
        case OSPC_OTHER:
        case OSPC_UNDEFINED_ROLE:
        default:
            source = _osp_device_ip;
            ospConvertAddress(terminator, dstbuf, sizeof(dstbuf));
            destination = dstbuf;
            ospConvertAddress(originator, devbuf, sizeof(devbuf));
            srcdev = devbuf;
            break;
    }

    errorcode = OSPPTransactionBuildUsageFromScratch(
        transaction,
        transid,
        type,
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

    if ((errorcode == OSPC_ERR_NO_ERROR) && (destinationCount > 0)) {
        errorcode = OSPPTransactionSetDestinationCount(
            transaction,
            destinationCount);
    }

    ospReportUsageWrapper(
        transaction,
        releasecode,
        duration,
        authtime,
        endtime,
        0,
        0,
        0,
        0,
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
    OSPTCALLID* callid = NULL;
    int result = MODULE_RETURNCODE_FALSE;

    ospGetCallId(msg, &callid);

    if (callid != NULL) {
        /* Who releases the call first, 0 orig, 1 term */
        if (sscanf(whorelease, "%d", &release) != 1 || (release != OSP_RELEASE_ORIG && release != OSP_RELEASE_TERM)) {
            release = OSP_RELEASE_ORIG;
        }
        LM_DBG("who releases the call first '%d'\n", release);

        if (ospGetRouteParameters(msg, parameters, sizeof(parameters)) == 0) {
            for (token = strtok_r(parameters, ";", &tmp);
                 token;
                 token = strtok_r(NULL, ";", &tmp))
            {
                if (strncmp(token, OSP_ORIG_COOKIE, strlen(OSP_ORIG_COOKIE)) == 0) {
                    LM_INFO("report orig duration for call_id '%.*s'\n",
                        callid->ospmCallIdLen,
                        callid->ospmCallIdVal);
                    ospReportUsageFromCookie(msg, token + strlen(OSP_ORIG_COOKIE) + 1, callid, release, OSPC_SOURCE);
                    result = MODULE_RETURNCODE_TRUE;
                } else if (strncmp(token, OSP_TERM_COOKIE, strlen(OSP_TERM_COOKIE)) == 0) {
                    LM_INFO("report term duration for call_id '%.*s'\n",
                        callid->ospmCallIdLen,
                        callid->ospmCallIdVal);
                    ospReportUsageFromCookie(msg, token + strlen(OSP_TERM_COOKIE) + 1, callid, release, OSPC_DESTINATION);
                    result = MODULE_RETURNCODE_TRUE;
                } else {
                    LM_DBG("ignoring parameter '%s'\n", token);
                }
            }
        }

        if (result == MODULE_RETURNCODE_FALSE) {
            LM_DBG("without orig or term OSP information\n");
            LM_INFO("report other duration for call_id '%.*s'\n",
               callid->ospmCallIdLen,
               callid->ospmCallIdVal);
            ospReportUsageFromCookie(msg, NULL, callid, release, OSPC_SOURCE);
            result = MODULE_RETURNCODE_TRUE;
        }

        OSPPCallIdDelete(&callid);
    }

    if (result == MODULE_RETURNCODE_FALSE) {
        LM_ERR("failed to report usage\n");
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
        dest->transid,
        dest->type,
        source,
        dest->host,
        srcdev,
        dest->destdev,
        dest->calling,
        OSPC_E164,
        dest->origcalled,       /* Report original called number */
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
    int errorcode;
	struct search_state st;

    errorcode = OSPPTransactionNew(_osp_provider, &transaction);

    for (destavp = search_first_avp(AVP_NAME_STR | AVP_VAL_STR, (int_str)OSP_ORIGDEST_NAME, NULL, &st);
        destavp != NULL;
		 destavp = search_next_avp(&st, 0))
    {
        get_avp_val(destavp, &destval);

        /* OSP destination is wrapped in a string */
        dest = (osp_dest*)destval.s.s;

        if (dest->used == 1) {
            if (dest->reported == 1) {
                LM_DBG("orig setup already reported\n");
                break;
            } else {
                dest->reported = 1;
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
        LM_INFO("report orig setup for call_id '%.*s' transaction_id '%llu'\n",
            lastused->callidsize,
            lastused->callid,
            lastused->transid);
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
    int errorcode;

    if ((dest = ospGetTermDestination())) {
        if (dest->reported == 0) {
            dest->reported = 1;
            LM_INFO("report term setup for call_id '%.*s' transaction_id '%llu'\n",
                dest->callidsize,
                dest->callid,
                dest->transid);
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
