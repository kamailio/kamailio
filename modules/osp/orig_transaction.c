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
#include <osp/osp.h>
#include "../../dset.h"
#include "../../usr_avp.h"
#include "../../mem/mem.h"
#include "../../modules/siputils/siputils.h"
#include "orig_transaction.h"
#include "destination.h"
#include "osptoolkit.h"
#include "sipheader.h"
#include "usage.h"

extern char* _osp_device_ip;
extern char* _osp_device_port;
extern int _osp_max_dests;
extern int _osp_redir_uri;
extern int_str _osp_snid_avpname;
extern unsigned short _osp_snid_avptype;
extern OSPTPROVHANDLE _osp_provider;
extern siputils_api_t osp_siputils;

const int OSP_FIRST_ROUTE = 1;
const int OSP_NEXT_ROUTE = 0;
const int OSP_MAIN_ROUTE = 1;
const int OSP_BRANCH_ROUTE = 0;
const str OSP_CALLING_NAME = {"_osp_calling_translated_", 24};

static int ospLoadRoutes(OSPTTRANHANDLE transaction, int destcount, char* source, char* sourcedev, char* origcalled, time_t authtime);
static int ospPrepareDestination(struct sip_msg* msg, int isfirst, int type, int format);
static int ospSetRpid(struct sip_msg* msg, osp_dest* dest);

/*
 * Get routes from AuthRsp
 * param transaction Transaction handle
 * param destcount Expected destination count
 * param source Source IP
 * param sourcedev Source device IP
 * param origcalled Original called number
 * param authtime Request authorization time
 * return 0 success, -1 failure
 */
static int ospLoadRoutes(
    OSPTTRANHANDLE transaction, 
    int destcount, 
    char* source, 
    char* sourcedev, 
    char* origcalled, 
    time_t authtime)
{
    int count;
    int errorcode;
    osp_dest* dest;
    osp_dest dests[OSP_DEF_DESTS];
    OSPE_DEST_PROT protocol;
    OSPE_DEST_OSP_ENABLED enabled;
    int result = 0;
    
    for (count = 0; count < destcount; count++) {
        /* This is necessary because we will save destinations in reverse order */
        dest = ospInitDestination(&dests[count]);

        if (dest == NULL) {
            result = -1;
            break;
        }

        dest->destinationCount = count + 1;
        strncpy(dest->origcalled, origcalled, sizeof(dest->origcalled) - 1);

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
        
        if (errorcode != OSPC_ERR_NO_ERROR) {
            LM_ERR("failed to load routes (%d) expected '%d' current '%d'\n", 
                errorcode, 
                destcount, 
                count);
            result = -1;
            break;
        }

        errorcode = OSPPTransactionGetDestProtocol(transaction, &protocol);
        if (errorcode != OSPC_ERR_NO_ERROR) {
            /* This does not mean an ERROR. The OSP server may not support OSP 2.1.1 */
            LM_DBG("cannot get dest protocol (%d)\n", errorcode);
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
        if (errorcode != OSPC_ERR_NO_ERROR) {
            /* This does not mean an ERROR. The OSP server may not support OSP 2.1.1 */
            LM_DBG("cannot get dest OSP version (%d)\n", errorcode);
        } else if (enabled == OSPE_OSP_FALSE) {
            /* Destination device does not support OSP. Do not send token to it */
            dest->token[0] = '\0';
            dest->tokensize = 0;
        }

        errorcode = OSPPTransactionGetDestNetworkId(transaction, dest->networkid);
        if (errorcode != OSPC_ERR_NO_ERROR) {
            /* This does not mean an ERROR. The OSP server may not support OSP 2.1.1 */
            LM_DBG("cannot get dest network ID (%d)\n", errorcode);
            dest->networkid[0] = '\0';
        }

        strncpy(dest->source, source, sizeof(dest->source) - 1);
        strncpy(dest->srcdev, sourcedev, sizeof(dest->srcdev) - 1);
        dest->type = OSPC_SOURCE;
        dest->transid = ospGetTransactionId(transaction);
        dest->authtime = authtime;

        LM_INFO("get destination '%d': "
            "valid after '%s' "
            "valid until '%s' "
            "time limit '%d' seconds "
            "call id '%.*s' "
            "calling '%s' "
            "called '%s' "
            "host '%s' "
            "supported '%d' "
            "network id '%s' "
            "token size '%d'\n",
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
    if (result == 0) {
        for(count = destcount -1; count >= 0; count--) {
            if (ospSaveOrigDestination(&dests[count]) == -1) {
                LM_ERR("failed to save originate destination\n");
                /* Report terminate CDR */
                ospRecordEvent(0, 500);
                result = -1;
                break;
            }
        }
    }

    return result;
}

/*
 * Request OSP authorization and routeing
 * param msg SIP message
 * param ignore1
 * param ignore2
 * return MODULE_RETURNCODE_TRUE success, MODULE_RETURNCODE_FALSE failure, MODULE_RETURNCODE_ERROR error
 */
int ospRequestRouting(
    struct sip_msg* msg, 
    char* ignore1, 
    char* ignore2)
{
    int errorcode;
    time_t authtime;
    char called[OSP_E164BUF_SIZE];
    char calling[OSP_E164BUF_SIZE];
    char sourcedev[OSP_STRBUF_SIZE];
    char deviceinfo[OSP_STRBUF_SIZE];
    struct usr_avp* snidavp = NULL;
    int_str snidval;
    char snid[OSP_STRBUF_SIZE];
    unsigned int callidnumber = 1;
    OSPTCALLID* callids[callidnumber];
    unsigned int logsize = 0;
    char* detaillog = NULL;
    const char** preferred = NULL;
    unsigned int destcount;
    OSPTTRANHANDLE transaction = -1;
    int result = MODULE_RETURNCODE_FALSE;

    authtime = time(NULL);

    destcount = _osp_max_dests;

    if ((errorcode = OSPPTransactionNew(_osp_provider, &transaction)) != OSPC_ERR_NO_ERROR) {
        LM_ERR("failed to create new OSP transaction (%d)\n", errorcode);
    } else if ((ospGetRpidUserpart(msg, calling, sizeof(calling)) != 0) &&
        (ospGetFromUserpart(msg, calling, sizeof(calling)) != 0)) 
    {
        LM_ERR("failed to extract calling number\n");
    } else if ((ospGetUriUserpart(msg, called, sizeof(called)) != 0) &&
        (ospGetToUserpart(msg, called, sizeof(called)) != 0)) 
    {
        LM_ERR("failed to extract called number\n");
    } else if (ospGetCallId(msg, &(callids[0])) != 0) {
        LM_ERR("failed to extract call id\n");
    } else if (ospGetSourceAddress(msg, sourcedev, sizeof(sourcedev)) != 0) {
        LM_ERR("failed to extract source deivce address\n");
    } else {
        ospConvertAddress(sourcedev, deviceinfo, sizeof(deviceinfo));

        if ((_osp_snid_avpname.n != 0) &&
            ((snidavp = search_first_avp(_osp_snid_avptype, _osp_snid_avpname, &snidval, 0)) != NULL) &&
            (snidavp->flags & AVP_VAL_STR) && (snidval.s.s && snidval.s.len)) 
        {
            snprintf(snid, sizeof(snid), "%.*s", snidval.s.len, snidval.s.s);
            OSPPTransactionSetNetworkIds(transaction, snid, "");
        } else {
            snid[0] = '\0';
        }

        LM_INFO("request auth and routing for: "
            "source_ip '%s' "
            "source_port '%s' "
            "source_dev '%s' "
            "source_networkid '%s' "
            "calling '%s' "
            "called '%s' "
            "call_id '%.*s' "
            "dest_count '%d'\n",
            _osp_device_ip,
            _osp_device_port,
            deviceinfo,         /* in "[x.x.x.x]" or host.domain format */
            snid,
            calling,
            called,
            callids[0]->ospmCallIdLen,
            callids[0]->ospmCallIdVal,
            destcount
        );    

        /* try to request authorization */
        errorcode = OSPPTransactionRequestAuthorisation(
            transaction,       /* transaction handle */
            _osp_device_ip,    /* from the configuration file */
            deviceinfo,        /* source device of call, protocol specific, in OSP format */
            calling,           /* calling number in nodotted e164 notation */
            OSPC_E164,         /* calling number format */
            called,            /* called number */
            OSPC_E164,         /* called number format */
            "",                /* optional username string, used if no number */
            callidnumber,      /* number of call ids, here always 1 */
            callids,           /* sized-1 array of call ids */
            preferred,         /* preferred destinations, here always NULL */
            &destcount,        /* max destinations, after call dest_count */
            &logsize,          /* size allocated for detaillog (next param) 0=no log */
            detaillog);        /* memory location for detaillog to be stored */

        if ((errorcode == OSPC_ERR_NO_ERROR) &&
            (ospLoadRoutes(transaction, destcount, _osp_device_ip, sourcedev, called, authtime) == 0))
        {
            LM_INFO("there are '%d' OSP routes, call_id '%.*s'\n",
                destcount,
                callids[0]->ospmCallIdLen,
                callids[0]->ospmCallIdVal);
            result = MODULE_RETURNCODE_TRUE;
        } else {
            LM_ERR("failed to request auth and routing (%d), call_id '%.*s'\n",
                errorcode,
                callids[0]->ospmCallIdLen,
                callids[0]->ospmCallIdVal);
            switch (errorcode) {
                case OSPC_ERR_TRAN_ROUTE_BLOCKED:
                    result = -403;
                    break;
                case OSPC_ERR_TRAN_ROUTE_NOT_FOUND:
                    result = -404;
                    break;
                case OSPC_ERR_NO_ERROR:
                    /* AuthRsp ok but ospLoadRoutes fails */
                    result = MODULE_RETURNCODE_ERROR;
                    break;
                default:
                    result = MODULE_RETURNCODE_FALSE;
                    break;
            }
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
    if (ospCheckOrigDestination() == 0) {
        return MODULE_RETURNCODE_TRUE;
    } else {
        return MODULE_RETURNCODE_FALSE;
    }
}

/*
 * Create RPID AVP
 * param msg SIP message
 * param dest Destination structure
 * return 0 success, 1 calling number same, -1 failure
 */
static int ospSetRpid(
    struct sip_msg* msg, 
    osp_dest* dest)
{
    str rpid;
    char calling[OSP_STRBUF_SIZE];
    char source[OSP_STRBUF_SIZE];
    char buffer[OSP_STRBUF_SIZE];
    int result = -1;

    if ((ospGetRpidUserpart(msg, calling, sizeof(calling)) != 0) &&
        (ospGetFromUserpart(msg, calling, sizeof(calling)) !=0))
    {
        LM_ERR("failed to extract calling number\n");
        return result;
    } 

    if (strcmp(calling, dest->calling) == 0) {
        /* Do nothing for this case */ 
        result = 1;
    } else if ((osp_siputils.rpid_avp.s.s == NULL)
				|| (osp_siputils.rpid_avp.s.len == 0)) {
        LM_WARN("rpid_avp is not foune, cannot set rpid avp\n");
        result = -1;
    } else {
        if (dest->source[0] == '[') {
            /* Strip "[]" */
            memset(source, 0, sizeof(source));
            strncpy(source, &dest->source[1], sizeof(source) - 1);
            source[strlen(source) - 1] = '\0';
        }
    
        snprintf(
            buffer, 
            sizeof(buffer), 
            "\"%s\" <sip:%s@%s>", 
            dest->calling, 
            dest->calling, 
            source);

        rpid.s = buffer;
        rpid.len = strlen(buffer);
        add_avp(osp_siputils.rpid_avp_type | AVP_VAL_STR,
				(int_str)osp_siputils.rpid_avp, (int_str)rpid);

        result = 0;
    }

    return result;
}

/*
 * Check if the calling number is translated.
 *     This function checks the avp set by ospPrepareDestination.
 * param msg SIP message
 * param ignore1
 * param ignore2
 * return MODULE_RETURNCODE_TRUE calling number translated MODULE_RETURNCODE_FALSE without transaltion
 */
int ospCheckTranslation(
    struct sip_msg* msg, 
    char* ignore1, 
    char* ignore2)
{
    int_str callingval;
    int result = MODULE_RETURNCODE_FALSE;

    if (search_first_avp(AVP_NAME_STR, (int_str)OSP_CALLING_NAME, &callingval, 0) != NULL) {
        if (callingval.n == 0) {
            LM_DBG("the calling number does not been translated\n");
        } else {
            LM_DBG("the calling number is translated\n");
            result = MODULE_RETURNCODE_TRUE;
        }
    } else {
        LM_ERR("there is not calling translation avp\n");
    }

    return result;
}

/*
 * Build SIP message for destination
 * param msg SIP message
 * param isfirst Is first destination
 * param type Main or branch route block
 * param format URI format
 * return MODULE_RETURNCODE_TRUE success MODULE_RETURNCODE_FALSE failure
 */
static int ospPrepareDestination(
    struct sip_msg* msg, 
    int isfirst,
    int type,
    int format)
{
    int_str val;
    int res;
    str newuri = {NULL, 0};
    int result = MODULE_RETURNCODE_FALSE;

    osp_dest* dest = ospGetNextOrigDestination();

    if (dest != NULL) {
        ospRebuildDestionationUri(&newuri, dest->called, dest->host, "", format);

        LM_INFO("prepare route to URI '%.*s' for call_id '%.*s' transaction_id '%llu'\n",
            newuri.len,
            newuri.s,
            dest->callidsize,
            dest->callid,
            dest->transid);

        if (type == OSP_MAIN_ROUTE) {
            if (isfirst == OSP_FIRST_ROUTE) {
                rewrite_uri(msg, &newuri);
            } else {
                km_append_branch(msg, &newuri, NULL, NULL, Q_UNSPECIFIED, 0, NULL);
            }
            /* Do not add route specific OSP information */
            result = MODULE_RETURNCODE_TRUE;
        } else if (type == OSP_BRANCH_ROUTE) {
            /* For branch route, add route specific OSP information */

            /* Update the Request-Line */
            rewrite_uri(msg, &newuri);

            /* Add OSP token header */
            ospAddOspHeader(msg, dest->token, dest->tokensize);

            /* Add branch-specific OSP Cookie */
            ospRecordOrigTransaction(msg, dest->transid, dest->srcdev, dest->calling, dest->called, dest->authtime, dest->destinationCount);

            /* Add rpid avp for calling number translation */
            res = ospSetRpid(msg, dest);
            switch (res) {
                case 0:
                    /* Calling number is translated */
                    val.n = 1;
                    add_avp(AVP_NAME_STR, (int_str)OSP_CALLING_NAME, val);
                    break;
                default:
                    LM_DBG("cannot set rpid avp\n");
                    /* Just like without calling translation */
                case 1:
                    /* Calling number does not been translated */
                    val.n = 0;
                    add_avp(AVP_NAME_STR, (int_str)OSP_CALLING_NAME, val);
                    break;
            }

            result = MODULE_RETURNCODE_TRUE;
        } else {
            LM_ERR("unsupported route block type\n");
        }
    } else {
        LM_DBG("there is no more routes\n");
        ospReportOrigSetupUsage();
    }

    if (newuri.len > 0) {
        pkg_free(newuri.s);
    }
    
    return result;
}

/*
 * Prepare OSP route
 *     This function only works in branch route block.
 *     This function is only for Kamailio.
 * param msg SIP message
 * param ignore1
 * param ignore2
 * return MODULE_RETURNCODE_TRUE success, MODULE_RETURNCODE_FALSE failure
 */
int ospPrepareRoute(
    struct sip_msg* msg, 
    char* ignore1, 
    char* ignore2)
{
    int result = MODULE_RETURNCODE_TRUE;

    /* The first parameter will be ignored */
    result = ospPrepareDestination(msg, OSP_FIRST_ROUTE, OSP_BRANCH_ROUTE, 0);

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

    for(result = ospPrepareDestination(msg, OSP_FIRST_ROUTE, OSP_MAIN_ROUTE, _osp_redir_uri);
        result == MODULE_RETURNCODE_TRUE;
        result = ospPrepareDestination(msg, OSP_NEXT_ROUTE, OSP_MAIN_ROUTE, _osp_redir_uri))
    {
    }

    return MODULE_RETURNCODE_TRUE;
}

