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

#define OSP_ORIG_COOKY "osp-o"
#define OSP_TERM_COOKY "osp-t"

#define RELEASE_SOURCE_ORIG 0
#define RELEASE_SOURCE_TERM 1

extern char* _osp_device_ip;
extern OSPTPROVHANDLE _osp_provider;
extern str OSP_ORIGDEST_LABEL;
extern struct rr_binds osp_rrb;

static void ospRecordTransaction(struct sip_msg* msg, OSPTTRANHANDLE transaction, char* uac, char* from, char* to, time_t authtime, int isorig);
static int ospReportUsageFromCookie(struct sip_msg* msg, char* cooky, OSPTCALLID* callid, int isorig);
static int ospBuildUsageFromDestination(OSPTTRANHANDLE transaction, osp_dest* dest, int lastcode);
static int ospReportUsageFromDestination(OSPTTRANHANDLE transaction, osp_dest* dest);

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
    OSPTTRANHANDLE transaction, 
    char* uac, 
    char* from, 
    char* to, 
    time_t authtime, 
    int isorig)
{
#define RTI_BUF_SIZE 1000
    char buffer[RTI_BUF_SIZE];
    str s;

    LOG(L_DBG, "osp: ospRecordTransaction\n");

    if (osp_rrb.add_rr_param == 0) {
        LOG(L_WARN,
            "osp: WARN: add_rr_param function is not found, "
            "cannot record information about the "
            "OSP transaction\n");
        return;
    }

    s.s = buffer;
    s.len = snprintf(
        buffer,
        RTI_BUF_SIZE,
        ";%s=t%llu_s%s_T%d",
        (isorig == 1 ? OSP_ORIG_COOKY : OSP_TERM_COOKY),
        ospGetTransactionId(transaction),
        uac,
        (unsigned int)authtime);
    if (s.len < 0) {
        LOG(L_ERR, "osp: ERROR: failed to create OSP cookie\n");
        return;
    }
    /* truncated? */
    if (s.len >= RTI_BUF_SIZE) {
        s.len = RTI_BUF_SIZE -1;
    }

    LOG(L_DBG, "osp: adding RR parameter '%s'\n", buffer);
    osp_rrb.add_rr_param(msg, &s);
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
    OSPTTRANHANDLE transaction, 
    char* uac, 
    char* from, 
    char* to, 
    time_t authtime)
{
    int isorig = 1;

    LOG(L_DBG, "osp: ospRecordOrigTransaction\n");

    ospRecordTransaction(msg, transaction, uac, from, to, authtime, isorig);
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
    OSPTTRANHANDLE transaction, 
    char* uac, 
    char* from, 
    char* to, 
    time_t authtime)
{
    int isorig = 0;

    LOG(L_DBG, "osp: ospRecordTermTransaction\n");

    ospRecordTransaction(msg, transaction, uac, from, to, authtime, isorig);
}

/*
 * Report OSP usage from OSP cookie
 * param msg SIP message
 * param cookie OSP cookie
 * param callid Call ID
 * param isorig Originate / Terminate
 * return
 */
static int ospReportUsageFromCookie(
    struct sip_msg* msg,
    char* cookie, 
    OSPTCALLID* callid, 
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
    int releasesource;
    char firstvia[OSP_STRBUF_SIZE];
    char from[OSP_STRBUF_SIZE];
    char to[OSP_STRBUF_SIZE];
    char nexthop[OSP_STRBUF_SIZE];
    char* calling;
    char* called;
    char* terminator;
    OSPTTRANHANDLE transaction = -1;
    int errorcode = 0;

    LOG(L_DBG, "osp: ospReportUsageFromCookie\n");
    LOG(L_DBG, "osp: '%s' isorig '%d'\n", cookie, isorig);

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
            default:
                LOG(L_ERR, "osp: ERROR: unexpected tag '%c' / value '%s'\n", tag, value);
                break;
        }
    }

    ospGetSourceAddress(msg, firstvia, sizeof(firstvia));
    ospGetFromUserpart(msg, from, sizeof(from));
    ospGetUriUserpart(msg, to, sizeof(to));
    ospGetNextHop(msg, nexthop, sizeof(nexthop));

    if (strcmp(firstvia, uac) == 0) {
        LOG(L_INFO,
            "osp: originator '%s' released the call, call_id '%.*s' transaction_id '%lld'\n",
            firstvia,
            callid->ospmCallIdLen,
            callid->ospmCallIdVal,
            transactionid);
        releasesource = RELEASE_SOURCE_ORIG;
        calling = from;
        called = to;
        terminator = nexthop;
    } else {
        LOG(L_INFO,
            "osp: terminator '%s' released the call, call_id '%.*s' transaction_id '%lld'\n",
            firstvia,
            callid->ospmCallIdLen,
            callid->ospmCallIdVal,
            transactionid);
        releasesource = RELEASE_SOURCE_TERM;
        calling = to;
        called = from;
        terminator = firstvia;
    }

    errorcode = OSPPTransactionNew(_osp_provider, &transaction);

    LOG(L_DBG, "osp: created new transaction handle '%d' (%d)\n", transaction, errorcode);

    errorcode = OSPPTransactionBuildUsageFromScratch(
        transaction,
        transactionid,
        isorig == 1 ? OSPC_SOURCE : OSPC_DESTINATION,
        isorig == 1 ? _osp_device_ip : uac,
        isorig == 1 ? terminator : _osp_device_ip,
        isorig == 1 ? uac : "",
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

    LOG(L_DBG, "osp: built usage handle '%d' (%d)\n", transaction, errorcode);

    ospReportUsage(
        transaction,
        10016,
        endtime - authtime,
        authtime,
        endtime,
        0,0,
        0,0,
        releasesource);

    return errorcode;
}

/*
 * Report OSP usage
 * param msg SIP message
 * param ignore1
 * param ignore2
 * return MODULE_RETURNCODE_TRUE success, MODULE_RETURNCODE_FALSE failure
 */
int reportospusage(
    struct sip_msg* msg, 
    char* ignore1, 
    char* ignore2)
{
    char parameters[OSP_HEADERBUF_SIZE];
    int  isorig;
    char *tmp;
    char *token;
    OSPTCALLID* callid = NULL;
    int result = MODULE_RETURNCODE_FALSE;

    LOG(L_DBG, "osp: reportospusage\n");

    ospGetCallId(msg, &callid);

    if (callid != NULL && ospGetRouteParameters(msg, parameters, sizeof(parameters)) == 0) {
        for (token = strtok_r(parameters, ";", &tmp);
             token;
             token = strtok_r(NULL, ";", &tmp))
        {
            if (strncmp(token, OSP_ORIG_COOKY, strlen(OSP_ORIG_COOKY)) == 0) {
                LOG(L_INFO,
                    "osp: report originate duration usage for call_id '%.*s'\n",
                    callid->ospmCallIdLen,
                    callid->ospmCallIdVal);
                isorig = 1;
                ospReportUsageFromCookie(msg, token + strlen(OSP_ORIG_COOKY) + 1, callid, isorig);
                result = MODULE_RETURNCODE_TRUE;
            } else if (strncmp(token, OSP_TERM_COOKY, strlen(OSP_TERM_COOKY)) == 0) {
                LOG(L_INFO,
                    "osp: report terminate duration usage for call_id '%.*s'\n",
                    callid->ospmCallIdLen,
                    callid->ospmCallIdVal);
                isorig = 0;
                ospReportUsageFromCookie(msg, token + strlen(OSP_TERM_COOKY) + 1, callid, isorig);
                result = MODULE_RETURNCODE_TRUE;
            } else {
                LOG(L_DBG, "osp: ignoring parameter '%s'\n", token);
            }
        }
    }

    if (callid != NULL) {
        OSPPCallIdDelete(&callid);
    }

    if (result == MODULE_RETURNCODE_FALSE) {
        LOG(L_DBG, "osp: there is not OSP originating or terminating usage information\n");
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

    dest->reported = 1;

    LOG(L_DBG, "osp: ospBuildUsageFromDestination\n");

    errorcode = OSPPTransactionBuildUsageFromScratch(
        transaction,
        dest->tid,
        dest->type,
        dest->source,
        dest->host,
        dest->srcdev,
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
    ospReportUsage(
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

    LOG(L_ALERT, "osp: ospReportOrigSetupUsage\n");

    errorcode = OSPPTransactionNew(_osp_provider, &transaction);

    for (destavp = search_first_avp(AVP_NAME_STR | AVP_VAL_STR, (int_str)OSP_ORIGDEST_LABEL, NULL, 0);
        destavp != NULL;
        destavp = search_next_avp(destavp, NULL)) {

        get_avp_val(destavp, &destval);

        /* OSP destination is wrapped in a string */
        dest = (osp_dest*)destval.s.s;

        if (dest->used == 1) {
            if (dest->reported == 1) {
                LOG(L_DBG, "osp: source usage has already been reported\n");
                break;
            }

            LOG(L_DBG, "osp: iterating through used destination\n");

            ospDumpDestination(dest);

            lastused = dest;

            errorcode = ospBuildUsageFromDestination(transaction, dest, lastcode);

            lastcode = dest->lastcode;
        } else {
            LOG(L_ERR, "osp: ERROR: this destination has not been used, breaking out\n");
            break;
        }
    }

    if (lastused) {
        LOG(L_INFO,
            "osp: reporting originate setup usage for call_id '%.*s' transaction_id '%lld'\n",
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

    LOG(L_ALERT, "osp: ospReportTermSetupUsage\n");

    if ((dest = ospGetTermDestination())) {
        if (dest->reported == 0) {
            LOG(L_INFO,
                "osp: reporting terminate setup usage for call_id '%.*s' transaction_id '%lld'\n",
                dest->callidsize,
                dest->callid,
                dest->tid);
            errorcode = OSPPTransactionNew(_osp_provider, &transaction);
            errorcode = ospBuildUsageFromDestination(transaction, dest, 0);
            errorcode = ospReportUsageFromDestination(transaction, dest);
        } else {
            LOG(L_DBG, "osp: destination usage has already been reported\n");
        }
    } else {
        LOG(L_ERR, "osp: ERROR: there is not terminate destination to report\n");
    }
}
