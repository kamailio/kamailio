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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <osp/osptrans.h>
#include "../../dprint.h"
#include "osptoolkit.h"

static OSPTTHREADRETURN ospReportUsageWork(void* usagearg);

typedef struct _osp_usage
{
    OSPTTRANHANDLE ospvTransaction;     /* Transaction handle */
    unsigned ospvReleaseCode;           /* Release code */
    unsigned ospvDuration;              /* Length of call */
    time_t ospvStartTime;               /* Call start time */
    time_t ospvEndTime;                 /* Call end time */
    time_t ospvAlertTime;               /* Call alert time */
    time_t ospvConnectTime;             /* Call connect time */
    unsigned ospvIsPDDInfoPresent;      /* Is PDD Info present */
    unsigned ospvPostDialDelay;         /* Post Dial Delay */
    unsigned ospvReleaseSource;         /* EP that released the call */
} osp_usage;

/*
 * Get OSP transaction ID from transaction handle
 * param transaction OSP transaction headle
 * return OSP transaction ID
 */
unsigned long long ospGetTransactionId(
    OSPTTRANHANDLE transaction)
{
    OSPTTRANS* context = NULL;
    unsigned long long id = 0;
    int errorcode = OSPC_ERR_NO_ERROR;

    LOG(L_DBG, "osp: ospGetTransactionId\n");

    context = OSPPTransactionGetContext(transaction, &errorcode);

    if (errorcode == OSPC_ERR_NO_ERROR) {
        id = (unsigned long long)context->TransactionID;
    } else {
        LOG(L_ERR, 
            "osp: ERROR: failed to extract transaction_id from transaction handle %d (%d)\n",
            transaction,
            errorcode);
    }

    return id;
}

/*
 * Create a thread to report OSP usage
 * param ospvTransaction OSP transaction handle
 * param ospvReleaseCode Call release reason
 * param ospvDurating Call duration
 * param ospvStartTime Call start time
 * param ospvEndTime Call end time
 * param ospvAlertTime Call alert time
 * param ospvConnectTime Call connected  time
 * param ospvIsPDDInfoPresent If post dial delay information avaliable
 * param ospvPostDialDelay Post dial delay information
 * param ospvReleaseSource Which side release the call
 */
void ospReportUsageWrapper(
    OSPTTRANHANDLE ospvTransaction,
    unsigned ospvReleaseCode,
    unsigned ospvDuration,
    time_t ospvStartTime,
    time_t ospvEndTime,
    time_t ospvAlertTime,
    time_t ospvConnectTime,
    unsigned ospvIsPDDInfoPresent,
    unsigned ospvPostDialDelay,
    unsigned ospvReleaseSource)
{
    osp_usage* usage;
    OSPTTHREADID threadid;
    OSPTTHRATTR threadattr;
    int errorcode;

    LOG(L_DBG, "osp: ospReportUsageWrapper\n");
    LOG(L_DBG, "osp: schedule usage report for '%llu'\n", ospGetTransactionId(ospvTransaction));

    usage = (osp_usage*)malloc(sizeof(osp_usage));

    usage->ospvTransaction = ospvTransaction;
    usage->ospvReleaseCode = ospvReleaseCode;
    usage->ospvDuration = ospvDuration;
    usage->ospvStartTime = ospvStartTime;
    usage->ospvEndTime = ospvEndTime;
    usage->ospvAlertTime = ospvAlertTime;
    usage->ospvConnectTime = ospvConnectTime;
    usage->ospvIsPDDInfoPresent = ospvIsPDDInfoPresent;
    usage->ospvPostDialDelay = ospvPostDialDelay;
    usage->ospvReleaseSource = ospvReleaseSource;

    OSPM_THRATTR_INIT(threadattr, errorcode);

    OSPM_SETDETACHED_STATE(threadattr, errorcode);

    OSPM_CREATE_THREAD(threadid, &threadattr, ospReportUsageWork, usage, errorcode);

    OSPM_THRATTR_DESTROY(threadattr);
}

/*
 * Report OSP usage thread function
 * param usagearg OSP usage information
 * return
 */
static OSPTTHREADRETURN ospReportUsageWork(
    void* usagearg)
{
    int i;
    const int MAX_RETRIES = 5;
    osp_usage* usage;
    int errorcode;

    LOG(L_DBG, "osp: ospReportUsageWork\n");

    usage = (osp_usage*)usagearg;

    OSPPTransactionRecordFailure(
        usage->ospvTransaction,
        (enum OSPEFAILREASON)usage->ospvReleaseCode);

    for (i = 1; i <= MAX_RETRIES; i++) {
        errorcode = OSPPTransactionReportUsage(
            usage->ospvTransaction,
            usage->ospvDuration,
            usage->ospvStartTime,
            usage->ospvEndTime,
            usage->ospvAlertTime,
            usage->ospvConnectTime,
            usage->ospvIsPDDInfoPresent,
            usage->ospvPostDialDelay,
            usage->ospvReleaseSource,
            (unsigned char*)"", 0, 0, 0, 0, NULL, NULL);

        if (errorcode == OSPC_ERR_NO_ERROR) {
            LOG(L_DBG, 
                "osp: reporte usage for '%llu'\n", 
                ospGetTransactionId(usage->ospvTransaction));
            break;
        } else {
            LOG(L_ERR, 
                "osp: ERROR: failed to report usage for '%llu' (%d) attempt '%d' of '%d'\n",
                ospGetTransactionId(usage->ospvTransaction), 
                errorcode,
                i,
                MAX_RETRIES);
        }
    }

    OSPPTransactionDelete(usage->ospvTransaction);

    free(usage);

    OSPTTHREADRETURN_NULL();
}
