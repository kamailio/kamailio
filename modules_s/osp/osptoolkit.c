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



#include "osptoolkit.h"
#include "osp/osptrans.h"
#include "osp/osposincl.h"
#include "osp/ospossys.h"
#include "osp/osp.h"
#include "../../sr_module.h"

static OSPTTHREADRETURN report_usage_wk(void* usage_arg);



typedef struct _usage_info
{
	OSPTTRANHANDLE	ospvTransaction;	/* In - Transaction handle */
	unsigned	ospvDuration;		/* In - Length of call */
	time_t		ospvStartTime;		/* In - Call start time */
	time_t		ospvEndTime;		/* In - Call end time */
	time_t		ospvAlertTime;		/* In - Call alert time */
	time_t		ospvConnectTime;	/* In - Call connect time */
	unsigned	ospvIsPDDInfoPresent;	/* In - Is PDD Info present */
	unsigned	ospvPostDialDelay;	/* In - Post Dial Delay */
	unsigned	ospvReleaseSource;	/* In - EP that released the call */
	unsigned	ospvReleaseCode;	/* In - Release code */
} usage_info;






void report_usage(
    OSPTTRANHANDLE	ospvTransaction,
    unsigned		ospvReleaseCode,
    unsigned		ospvDuration,
    time_t		ospvStartTime,
    time_t		ospvEndTime,
    time_t		ospvAlertTime,
    time_t		ospvConnectTime,
    unsigned		ospvIsPDDInfoPresent,
    unsigned		ospvPostDialDelay,
    unsigned		ospvReleaseSource)
{
	int errorcode;
	usage_info* usage;
	OSPTTHREADID thread_id;
	OSPTTHRATTR thread_attr;

	DBG("Scheduling usage reporting for transaction-id '%lld'\n",get_transaction_id(ospvTransaction));

	usage = (usage_info *)malloc(sizeof(usage_info));

	usage->ospvTransaction		= ospvTransaction;
	usage->ospvReleaseCode		= ospvReleaseCode;
	usage->ospvDuration		= ospvDuration;
	usage->ospvStartTime		= ospvStartTime;
	usage->ospvEndTime		= ospvEndTime;
	usage->ospvAlertTime		= ospvAlertTime;
	usage->ospvConnectTime		= ospvConnectTime;
	usage->ospvIsPDDInfoPresent	= ospvIsPDDInfoPresent;
	usage->ospvPostDialDelay	= ospvPostDialDelay;
	usage->ospvReleaseSource	= ospvReleaseSource;

	OSPM_THRATTR_INIT(thread_attr, errorcode);

	OSPM_SETDETACHED_STATE(thread_attr,errorcode);

	OSPM_CREATE_THREAD(thread_id, &thread_attr, report_usage_wk, usage, errorcode);

	OSPM_THRATTR_DESTROY(thread_attr);
}





static OSPTTHREADRETURN report_usage_wk(void* usage_arg)
{
	int errorcode;
	int i;
	int MAX_RETRIES = 5;
	usage_info* usage;

	usage = (usage_info *)usage_arg;

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
			(unsigned char*)"", 0, 0, 0, 0, NULL,NULL);

		if (errorcode == 0) {
			DBG("osp: Reported usage for transaction-id '%lld'\n",get_transaction_id(usage->ospvTransaction));
			break;
		} else {
			ERR("osp: Failed to report usage for transaction-id '%lld', code '%d', attempt '%d' of '%d'\n",
			    get_transaction_id(usage->ospvTransaction),errorcode,i,MAX_RETRIES);
		}
	}

	OSPPTransactionDelete(usage->ospvTransaction);

	free(usage);

	OSPTTHREADRETURN_NULL();
}











unsigned long long get_transaction_id(OSPTTRANHANDLE transaction)
{
	OSPTTRANS* context = NULL;
	int errorcode = 0;
	unsigned long long id = 0;

	context = OSPPTransactionGetContext(transaction,&errorcode);

	if (0==errorcode) {
		id = (unsigned long long)context->TransactionID;
	} else {
		ERR("osp: Failed to extract OSP transaction id from transaction handle %d, error %d\n",transaction,errorcode);
	}

	return id;
}


