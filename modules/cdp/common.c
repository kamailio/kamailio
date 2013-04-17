/*
 * common.c
 *
 *  Created on: 10 Apr 2013
 *      Author: jaybeepee
 */

#include "common.h"
#include "diameter_ims_code_avp.h"

int get_accounting_record_type(AAAMessage* msg) {
	AAA_AVP* avp = AAAFindMatchingAVP(msg, 0, AVP_Accounting_Record_Type, 0, 0);
	if (avp && avp->data.len == 4) {
		//assert this is an initial request. we can't move from IDLE with anything else
		return get_4bytes(avp->data.s);
	}
	return -1;
}

int get_result_code(AAAMessage* msg) {
    AAA_AVP *avp;
    AAA_AVP_LIST list;
    list.head = 0;
    list.tail = 0;
    int rc = -1;

    if (!msg) goto error;

    for (avp = msg->avpList.tail; avp; avp = avp->prev) {

        if (avp->code == AVP_Result_Code) {
            rc = get_4bytes(avp->data.s);
            goto finish;
        } else if (avp->code == AVP_Experimental_Result) {
            list = AAAUngroupAVPS(avp->data);
            for (avp = list.head; avp; avp = avp->next) {
                if (avp->code == AVP_IMS_Experimental_Result_Code) {
                    rc = get_4bytes(avp->data.s);
                    AAAFreeAVPList(&list);
                    goto finish;
                }
            }
            AAAFreeAVPList(&list);
        }
    }
finish:
    return rc;
error:
    LM_ERR("get_result_code(): no AAAMessage or Result Code not found\n");
    return -1;
}

