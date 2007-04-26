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

#include <string.h>
#include <osp/osp.h>
#include "../../dset.h"
#include "../../usr_avp.h"
#include "../../mem/mem.h"
#include "../auth/api.h"
#include "orig_transaction.h"
#include "destination.h"
#include "osptoolkit.h"
#include "sipheader.h"
#include "usage.h"

extern char* _osp_device_ip;
extern char* _osp_device_port;
extern int _osp_max_dests;
extern OSPTPROVHANDLE _osp_provider;
extern auth_api_t osp_auth;

const int OSP_FIRST_ROUTE = 1;
const int OSP_NEXT_ROUTE = 0;
const int OSP_MAIN_ROUTE = 1;
const int OSP_BRANCH_ROUTE = 0;
const str OSP_CALLING_NAME = {"_osp_calling_translated_", 24};

static int ospLoadRoutes(struct sip_msg* msg, OSPTTRANHANDLE transaction, int destcount, char* source, char* sourcedev, time_t authtime);
static int ospPrepareDestination(struct sip_msg* msg, int isfirst, int type);

/*
 * Get routes from AuthRsp
 * param msg SIP message
 * param transaction Transaction handle
 * param destcount Expected destination count
 * param source Source IP
 * param sourcedev Source device IP
 * param authtime Request authorization time
 * return MODULE_RETURNCODE_TRUE success, MODULE_RETURNCODE_FALSE failure
 */
static int ospLoadRoutes(
    struct sip_msg* msg, 
    OSPTTRANHANDLE transaction, 
    int destcount, 
    char* source, 
    char* sourcedev, 
    time_t authtime)
{
    int count;
    int errorcode;
    osp_dest* dest;
    osp_dest dests[OSP_DEF_DESTS];
    OSPE_DEST_PROT protocol;
    OSPE_DEST_OSP_ENABLED enabled;
    int result = MODULE_RETURNCODE_TRUE;
    
    LOG(L_DBG, "osp: ospLoadRoutes\n");

    for (count = 0; count < destcount; count++) {
        /* This is necessary becuase we will save destinations in reverse order */
        dest = ospInitDestination(&dests[count]);

        if (dest == NULL) {
            result = MODULE_RETURNCODE_FALSE;
            break;
        }

        if (count == 0) {
            errorcode = OSPPTransactionGetFirstDestination(
                transaction,
                sizeof(dest->validafter),
                dest->validafter,
                dest->validuntil,
                &dest->timelimit,
                &dest->callidsize,
                (void*)dest->callid,
                sizeof(dest->called),
                dest->called,
                sizeof(dest->calling),
                dest->calling,
                sizeof(dest->host),
                dest->host,
                sizeof(dest->destdev),
                dest->destdev,
                &dest->tokensize,
                dest->token);
        } else {
            errorcode = OSPPTransactionGetNextDestination(
                transaction,
                0,
                sizeof(dest->validafter),
                dest->validafter,
                dest->validuntil,
                &dest->timelimit,
                &dest->callidsize,
                (void*)dest->callid,
                sizeof(dest->called),
                dest->called,
                sizeof(dest->calling),
                dest->calling,
                sizeof(dest->host),
                dest->host,
                sizeof(dest->destdev),
                dest->destdev,
                &dest->tokensize,
                dest->token);
        }
        
        if (errorcode != 0) {
            LOG(L_ERR, 
                "osp: ERROR: failed to load routes (%d) expected '%d' current '%d'\n", 
                errorcode, 
                destcount, 
                count);
            result = MODULE_RETURNCODE_FALSE;
            break;
        }

        errorcode = OSPPTransactionGetDestProtocol(transaction, &protocol);
        if (errorcode != 0) {
            /* This does not mean an ERROR. The OSP server may not support OSP 2.1.1 */
            LOG(L_DBG, "osp: cannot get dest protocol (%d)\n", errorcode);
            protocol = OSPE_DEST_PROT_SIP;
        }
        switch (protocol) {
            case OSPE_DEST_PROT_H323_LRQ:
            case OSPE_DEST_PROT_H323_SETUP:
            case OSPE_DEST_PROT_IAX:
                dest->supported = 0;
                break;
            case OSPE_DEST_PROT_SIP:
            case OSPE_DEST_PROT_UNDEFINED:
            case OSPE_DEST_PROT_UNKNOWN:
            default:
                dest->supported = 1;
                break;
        }

        errorcode = OSPPTransactionIsDestOSPEnabled(transaction, &enabled);
        if (errorcode != 0) {
            /* This does not mean an ERROR. The OSP server may not support OSP 2.1.1 */
            LOG(L_DBG, "osp: cannot get dest OSP version (%d)\n", errorcode);
        } else if (enabled == OSPE_OSP_FALSE) {
            /* Destination device does not support OSP. Do not send token to it */
            dest->token[0] = '\0';
            dest->tokensize = 0;
        }

        errorcode = OSPPTransactionGetDestNetworkId(transaction, dest->networkid);
        if (errorcode != 0) {
            /* This does not mean an ERROR. The OSP server may not support OSP 2.1.1 */
            LOG(L_DBG, "osp: cannot get dest network ID (%d)\n", errorcode);
            dest->networkid[0] = '\0';
        }

        strcpy(dest->source, source);
        strcpy(dest->srcdev, sourcedev);
        dest->type = OSPC_SOURCE;
        dest->tid = ospGetTransactionId(transaction);
        dest->authtime = authtime;

        LOG(L_INFO,
            "osp: get destination '%d': "
            "valid after '%s' "
            "valid until '%s' "
            "time limit '%i' seconds "
            "call id '%.*s' "
            "calling number '%s' "
            "called number '%s' "
            "host '%s' "
            "supported '%d' "
            "network id '%s' "
            "token size '%i'\n",
            count, 
            dest->validafter, 
            dest->validuntil, 
            dest->timelimit, 
            dest->callidsize, 
            dest->callid, 
            dest->calling, 
            dest->called, 
            dest->host, 
            dest->supported,
            dest->networkid, 
            dest->tokensize);
    }

    /* 
     * Save destination in reverse order,
     * when we start searching avps the destinations
     * will be in order 
     */
    if (result == MODULE_RETURNCODE_TRUE) {
        for(count = destcount -1; count >= 0; count--) {
            ospSaveOrigDestination(&dests[count]);
        }
    }

    return result;
}

/*
 * Request OSP authorization and routeing
 * param msg SIP message
 * param ignore1
 * param ignore2
 * return MODULE_RETURNCODE_TRUE success, MODULE_RETURNCODE_FALSE failure
 */
int ospRequestRouting(
    struct sip_msg* msg, 
    char* ignore1, 
    char* ignore2)
{
    int errorcode;
    time_t authtime;
    char tmp[OSP_STRBUF_SIZE];
    char source[OSP_E164BUF_SIZE];
    char sourcedev[OSP_STRBUF_SIZE];
    char destination[OSP_E164BUF_SIZE];
    unsigned int callidnumber = 1;
    OSPTCALLID* callids[callidnumber];
    unsigned int logsize = 0;
    char* detaillog = NULL;
    const char** preferred = NULL;
    unsigned int destcount;
    OSPTTRANHANDLE transaction = -1;
    int result = MODULE_RETURNCODE_FALSE;

    LOG(L_DBG, "osp: ospRequestRouting\n");

    authtime = time(NULL);

    destcount = _osp_max_dests;

    if ((errorcode = OSPPTransactionNew(_osp_provider, &transaction)) != 0) {
        LOG(L_ERR, "osp: ERROR: failed to create new OSP transaction (%d)\n", errorcode);
    } else if ((ospGetRpidUserpart(msg, source, sizeof(source)) != 0) &&
        (ospGetFromUserpart(msg, source, sizeof(source)) != 0)) 
    {
        LOG(L_ERR, "osp: ERROR: failed to extract calling number\n");
    } else if ((ospGetUriUserpart(msg, destination, sizeof(destination)) != 0) &&
        (ospGetToUserpart(msg, destination, sizeof(destination)) != 0)) 
    {
        LOG(L_ERR, "osp: ERROR: failed to extract called number\n");
    } else if (ospGetCallId(msg, &(callids[0])) != 0) {
        LOG(L_ERR, "osp: ERROR: failed to extract call id\n");
    } else if (ospGetSourceAddress(msg, sourcedev, sizeof(sourcedev)) != 0) {
        LOG(L_ERR, "osp: ERROR: failed to extract source address\n");
    } else {
        ospConvertAddress(sourcedev, tmp, sizeof(tmp));

        LOG(L_INFO,
            "osp: request auth and routing for: "
            "transaction '%i' "
            "source '%s' "
            "source_port '%s' "
            "source_dev '%s' "
            "e164_source '%s' "
            "e164_dest '%s' "
            "call_id '%.*s' "
            "dest_count '%i'\n",
            transaction,
            _osp_device_ip,
            _osp_device_port,
            tmp,                        /* sourcedev in "[x.x.x.x]" or host.domain format */
            source,
            destination,
            callids[0]->ospmCallIdLen,
            callids[0]->ospmCallIdVal,
            destcount
        );    

/* Do not use network ID as port */
#if 0
        if (strlen(_osp_device_port) > 0) {
            OSPPTransactionSetNetworkIds(transaction, _osp_device_port, "");
        }
#endif

        /* try to request authorization */
        errorcode = OSPPTransactionRequestAuthorisation(
            transaction,       /* transaction handle */
            _osp_device_ip,    /* from the configuration file */
            tmp,               /* source of call, protocol specific, in OSP format */
            source,            /* calling number in nodotted e164 notation */
            OSPC_E164,         /* calling number format */
            destination,       /* called number */
            OSPC_E164,         /* called number format */
            "",                /* optional username string, used if no number */
            callidnumber,      /* number of call ids, here always 1 */
            callids,           /* sized-1 array of call ids */
            preferred,         /* preferred destinations, here always NULL */
            &destcount,        /* max destinations, after call dest_count */
            &logsize,          /* size allocated for detaillog (next param) 0=no log */
            detaillog);        /* memory location for detaillog to be stored */

        if ((errorcode == 0) && (destcount > 0)) {
            LOG(L_INFO, 
                "osp: there are '%d' OSP routes, call_id '%.*s' transaction_id '%lld'\n",
                destcount,
                callids[0]->ospmCallIdLen, 
                callids[0]->ospmCallIdVal,
                ospGetTransactionId(transaction));
            ospRecordOrigTransaction(msg, transaction, sourcedev, source, destination, authtime);
            result = ospLoadRoutes(msg, transaction, destcount, _osp_device_ip, sourcedev, authtime);
        } else if ((errorcode == 0) && (destcount == 0)) {
            LOG(L_INFO, 
                "osp: there is 0 osp routes, call_id '%.*s' transaction_id '%lld'\n",
                callids[0]->ospmCallIdLen,
                callids[0]->ospmCallIdVal,
                ospGetTransactionId(transaction));
            /* Must do manually since callback does not work for this case. Do not know why. */
            ospRecordEvent(0, 503);
        } else {
            LOG(L_ERR, 
                "osp: ERROR: failed to request auth and routing (%i), call_id '%.*s' transaction_id '%lld'\n",
                errorcode,
                callids[0]->ospmCallIdLen,
                callids[0]->ospmCallIdVal,
                ospGetTransactionId(transaction));
        }
    }

    if (callids[0] != NULL) {
        OSPPCallIdDelete(&(callids[0]));
    }

    if (transaction != -1) {
        OSPPTransactionDelete(transaction);
    }
    
    return result;
}

/*
 * Check if there is a route
 * param msg SIP message
 * param ignore1
 * param ignore2
 * return MODULE_RETURNCODE_TRUE success, MODULE_RETURNCODE_FALSE failure
 */
int ospCheckRoute(
    struct sip_msg* msg, 
    char* ignore1, 
    char* ignore2)
{
    LOG(L_DBG, "osp: ospCheckRoute\n");

    if (ospCheckOrigDestination() == 0) {
        return MODULE_RETURNCODE_TRUE;
    } else {
        return MODULE_RETURNCODE_FALSE;
    }
}

/*
 * Append route specific OSP headers
 *     This function only works in branch route block. 
 *     This function is only for SER. SER does not support rewrite_uri in BRANCH_ROUTE.
 * param msg SIP message
 * param ignore1
 * param ignore2
 * return MODULE_RETURNCODE_TRUE success MODULE_RETURNCODE_FALSE failed
 */
int ospAppendHeaders(
    struct sip_msg* msg, 
    char* ignore1, 
    char* ignore2)
{
    osp_dest* dest;
    int result = MODULE_RETURNCODE_FALSE;

    LOG(L_DBG, "osp: ospAppendHeaders\n");

    dest = ospGetLastOrigDestination();
    if (dest != NULL) {
        ospAddOspHeader(msg, dest->token, dest->tokensize);
        result = MODULE_RETURNCODE_TRUE;
    } else {
        LOG(L_ERR, "osp: ERROR: failed to get last used destination\n");
    }

    return result;
}

/*
 * Build SIP message for destination
 * param msg SIP message
 * param isfirst Is first destination
 * param type Main or branch route block
 * return MODULE_RETURNCODE_TRUE success MODULE_RETURNCODE_FALSE failure
 */
static int ospPrepareDestination(
    struct sip_msg* msg, 
    int isfirst,
    int type)
{
    str newuri = {NULL, 0};
    int result = MODULE_RETURNCODE_FALSE;

    LOG(L_DBG, "osp: ospPrepareDestination\n");

    osp_dest* dest = ospGetNextOrigDestination();

    if (dest != NULL) {
        ospRebuildDestionationUri(&newuri, dest->called, dest->host, "");

        LOG(L_INFO, 
            "osp: prepare route to URI '%.*s' for call_id '%.*s' transaction_id '%lld'\n",
            newuri.len,
            newuri.s,
            dest->callidsize,
            dest->callid,
            dest->tid);

        if (type == OSP_MAIN_ROUTE) {
            if (isfirst == OSP_FIRST_ROUTE) {
                rewrite_uri(msg, &newuri);
            } else {
                append_branch(msg, newuri.s, newuri.len, NULL, 0, 0, NULL);
            }

            result = MODULE_RETURNCODE_TRUE;
        } else {
            LOG(L_ERR, "osp: ERROR: unsupported route block type\n");
        }
    } else {
        LOG(L_DBG, "osp: there is no more routes\n");
        ospReportOrigSetupUsage();
    }

    if (newuri.len > 0) {
        pkg_free(newuri.s);
    }
    
    return result;
}

/*
 * Prepare OSP first route
 *     This function prepare the first OSP route
 *     This function is only for SER. SER does not support rewrite_uri in BRANCH_ROUTE.
 * param msg SIP message
 * param ignore1
 * param ignore2
 * return MODULE_RETURNCODE_TRUE success, MODULE_RETURNCODE_FALSE failure
 */
int ospPrepareFirstRoute(
    struct sip_msg* msg, 
    char* ignore1, 
    char* ignore2)
{
    int result = MODULE_RETURNCODE_TRUE;

    LOG(L_DBG, "osp: ospPrepareFirstRoute\n");

    result = ospPrepareDestination(msg, OSP_FIRST_ROUTE, OSP_MAIN_ROUTE);

    return result;
}

/*
 * Prepare OSP next route
 *     This function prepare the next OSP route
 *     This function is only for SER. SER does not support rewrite_uri in BRANCH_ROUTE.
 * param msg SIP message
 * param ignore1
 * param ignore2
 * return MODULE_RETURNCODE_TRUE success, MODULE_RETURNCODE_FALSE failure
 */
int ospPrepareNextRoute(
    struct sip_msg* msg, 
    char* ignore1, 
    char* ignore2)
{
    int result = MODULE_RETURNCODE_TRUE;

    LOG(L_DBG, "osp: ospPrepareNextRoute\n");

    result = ospPrepareDestination(msg, OSP_NEXT_ROUTE, OSP_MAIN_ROUTE);

    return result;
}

/*
 * Prepare all OSP routes
 *     This function does not work in branch route block.
 * param msg SIP message
 * param ignore1
 * param ignore2
 * return MODULE_RETURNCODE_TRUE success, MODULE_RETURNCODE_FALSE failure
 */
int ospPrepareAllRoutes(
    struct sip_msg* msg, 
    char* ignore1, 
    char* ignore2)
{
    int result = MODULE_RETURNCODE_TRUE;

    LOG(L_DBG, "osp: ospPrepareAllRoutes\n");

    for(result = ospPrepareDestination(msg, OSP_FIRST_ROUTE, OSP_MAIN_ROUTE);
        result == MODULE_RETURNCODE_TRUE;
        result = ospPrepareDestination(msg, OSP_NEXT_ROUTE, OSP_MAIN_ROUTE))
    {
    }

    return MODULE_RETURNCODE_TRUE;
}

