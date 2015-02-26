#include "../cdp_avp/mod_export.h"

#include "ccr.h"
#include "Ro_data.h"
#include "ro_avp.h"
#include "mod.h"

extern cdp_avp_bind_t *cdp_avp;
extern struct cdp_binds cdpb;

int Ro_write_event_type_avps(AAA_AVP_LIST * avp_list, event_type_t * x) {
    AAA_AVP_LIST aList = {0, 0};

    if (x->sip_method) {
        if (!cdp_avp->epcapp.add_SIP_Method(&aList, *(x->sip_method), AVP_DUPLICATE_DATA))
            goto error;
    }

    if (x->event)
        if (!cdp_avp->epcapp.add_Event(&aList, *(x->event), 0))
            goto error;

    if (x->expires)
        if (!cdp_avp->epcapp.add_Expires(avp_list, *(x->expires)))
            goto error;

    if (!cdp_avp->epcapp.add_Event_Type(avp_list, &aList, AVP_FREE_DATA))	//TODO: used to be DONT FREE
        goto error;

    return 1;
error:
    cdp_avp->cdp->AAAFreeAVPList(&aList);
    LM_ERR("error while adding event type avps\n");
    return 0;
}

int Ro_write_time_stamps_avps(AAA_AVP_LIST * avp_list, time_stamps_t* x) {
    AAA_AVP_LIST aList = {0, 0};

    if (x->sip_request_timestamp)
        if (!cdp_avp->epcapp.add_SIP_Request_Timestamp(&aList, *(x->sip_request_timestamp)))
            goto error;

    if (x->sip_request_timestamp_fraction)
        if (!cdp_avp->epcapp.add_SIP_Request_Timestamp_Fraction(&aList,
                *(x->sip_request_timestamp_fraction)))
            goto error;

    if (x->sip_response_timestamp)
        if (!cdp_avp->epcapp.add_SIP_Response_Timestamp(&aList, *(x->sip_response_timestamp)))
            goto error;

    if (x->sip_response_timestamp_fraction)
        if (!cdp_avp->epcapp.add_SIP_Response_Timestamp_Fraction(&aList,
                *(x->sip_response_timestamp_fraction)))
            goto error;

    if (!cdp_avp->epcapp.add_Time_Stamps(avp_list, &aList, AVP_FREE_DATA))	//used to be DONT FREE
        goto error;


    return 1;
error:
    cdp_avp->cdp->AAAFreeAVPList(&aList);
    LM_ERR("error while adding time stamps avps\n");

    return 0;
}

int Ro_write_ims_information_avps(AAA_AVP_LIST * avp_list, ims_information_t* x) {
    str_list_slot_t * sl = 0;
    AAA_AVP_LIST aList = {0, 0};
    AAA_AVP_LIST aList2 = {0, 0};
    service_specific_info_list_element_t * info = 0;
    ioi_list_element_t * ioi_elem = 0;

    if (x->event_type)
        if (!Ro_write_event_type_avps(&aList2, x->event_type))
            goto error;
    if (x->role_of_node)
        if (!cdp_avp->epcapp.add_Role_Of_Node(&aList2, *(x->role_of_node))) goto error;

    if (!cdp_avp->epcapp.add_Node_Functionality(&aList2, x->node_functionality))
        goto error;

    if (x->user_session_id)
        if (!cdp_avp->epcapp.add_User_Session_Id(&aList2, *(x->user_session_id), 0))
            goto error;

    for (sl = x->calling_party_address.head; sl; sl = sl->next) {
        if (!cdp_avp->epcapp.add_Calling_Party_Address(&aList2, sl->data, 0))
            goto error;
    }

    if (x->called_party_address)
        if (!cdp_avp->epcapp.add_Called_Party_Address(&aList2, *(x->called_party_address), 0))
            goto error;
    
    if (x->incoming_trunk_id && x->outgoing_trunk_id) {
	if (!cdp_avp->epcapp.add_Outgoing_Trunk_Group_Id(&aList, *(x->outgoing_trunk_id), 0))
	    goto error;
	    
	if (!cdp_avp->epcapp.add_Incoming_Trunk_Group_Id(&aList, *(x->incoming_trunk_id), 0))
	    goto error;
	    
	if (!cdp_avp->epcapp.add_Trunk_Group_Id(&aList2, &aList, 0))
            goto error;
	cdp_avp->cdp->AAAFreeAVPList(&aList);
        aList.head = aList.tail = 0;
    }
    
    if (x->access_network_info) {
		cdp_avp->imsapp.add_Access_Network_Information(&aList2, *(x->access_network_info), 0);
    }

    for (sl = x->called_asserted_identity.head; sl; sl = sl->next) {
        if (!cdp_avp->epcapp.add_Called_Asserted_Identity(&aList2, sl->data, 0))
            goto error;
    }

    if (x->requested_party_address)
        if (!cdp_avp->epcapp.add_Requested_Party_Address(&aList2, *(x->requested_party_address), 0))
            goto error;
    if (x->time_stamps)
        if (!Ro_write_time_stamps_avps(&aList2, x->time_stamps))
            goto error;

    for (ioi_elem = x->ioi.head; ioi_elem; ioi_elem = ioi_elem->next) {

        if (ioi_elem->info.originating_ioi)
            if (!cdp_avp->epcapp.add_Originating_IOI(&aList, *(ioi_elem->info.originating_ioi), 0))
                goto error;

        if (ioi_elem->info.terminating_ioi)
            if (!cdp_avp->epcapp.add_Terminating_IOI(&aList, *(ioi_elem->info.terminating_ioi), 0))
                goto error;

        if (!cdp_avp->epcapp.add_Inter_Operator_Identifier(&aList2, &aList, 0))
            goto error;
	cdp_avp->cdp->AAAFreeAVPList(&aList);
        aList.head = aList.tail = 0;
    }

    if (x->icid)
        if (!cdp_avp->epcapp.add_IMS_Charging_Identifier(&aList2, *(x->icid), 0))
            goto error;

    if (x->service_id)
        if (!cdp_avp->epcapp.add_Service_ID(&aList2, *(x->service_id), 0))
            goto error;

    for (info = x->service_specific_info.head; info; info = info->next) {

        if (info->info.data)
            if (!cdp_avp->epcapp.add_Service_Specific_Data(&aList, *(info->info.data), 0))
                goto error;
        if (info->info.type)
            if (!cdp_avp->epcapp.add_Service_Specific_Type(&aList, *(info->info.type)))
                goto error;

        if (!cdp_avp->epcapp.add_Service_Specific_Info(&aList2, &aList, 0))
            goto error;
	cdp_avp->cdp->AAAFreeAVPList(&aList);
        aList.head = aList.tail = 0;
    }

    if (x->cause_code)
        if (!cdp_avp->epcapp.add_Cause_Code(&aList2, *(x->cause_code)))
            goto error;

    if (!cdp_avp->epcapp.add_IMS_Information(avp_list, &aList2, AVP_FREE_DATA))//TODO check why not DONT FREE DATA
        goto error;

    return 1;
error:
    /*free aList*/
    cdp_avp->cdp->AAAFreeAVPList(&aList);
    cdp_avp->cdp->AAAFreeAVPList(&aList2);
    LM_ERR("could not add ims information avps\n");
    return 0;
}

int Ro_write_service_information_avps(AAA_AVP_LIST * avp_list, service_information_t* x) {
    subscription_id_list_element_t * elem = 0;
    AAA_AVP_LIST aList = {0, 0};

    for (elem = x->subscription_id.head; elem; elem = elem->next) {

        if (!cdp_avp->ccapp.add_Subscription_Id_Group(&aList, elem->s.type, elem->s.id, 0))
            goto error;

    }

    if (x->ims_information)
        if (!Ro_write_ims_information_avps(&aList, x->ims_information))
            goto error;

    if (!cdp_avp->epcapp.add_Service_Information(avp_list, &aList, AVP_FREE_DATA)) //TODO: use to be dont free
        goto error;

    return 1;
error:
    cdp_avp->cdp->AAAFreeAVPList(&aList);
    return 0;
}

AAAMessage * Ro_write_CCR_avps(AAAMessage * ccr, Ro_CCR_t* x) {

    if (!ccr) return 0;

    if (!cdp_avp->base.add_Origin_Host(&(ccr->avpList), x->origin_host, 0)) goto error;
    if (!cdp_avp->base.add_Origin_Realm(&(ccr->avpList), x->origin_realm, 0)) goto error;
    if (!ro_add_destination_realm_avp(ccr, x->destination_realm)) goto error;

    if (!cdp_avp->base.add_Accounting_Record_Type(&(ccr->avpList), x->acct_record_type)) goto error;
    if (!cdp_avp->base.add_Accounting_Record_Number(&(ccr->avpList), x->acct_record_number)) goto error;

    if (x->user_name)
        if (!cdp_avp->base.add_User_Name(&(ccr->avpList), *(x->user_name), AVP_DUPLICATE_DATA)) goto error;

    if (x->acct_interim_interval)
        if (!cdp_avp->base.add_Acct_Interim_Interval(&(ccr->avpList), *(x->acct_interim_interval))) goto error;

    if (x->origin_state_id)
        if (!cdp_avp->base.add_Origin_State_Id(&(ccr->avpList), *(x->origin_state_id))) goto error;

    if (x->event_timestamp)
        if (!cdp_avp->base.add_Event_Timestamp(&(ccr->avpList), *(x->event_timestamp))) goto error;

    if (x->service_context_id)
        if (!cdp_avp->ccapp.add_Service_Context_Id(&(ccr->avpList), *(x->service_context_id), 0)) goto error;

    if (x->service_information)
        if (!Ro_write_service_information_avps(&(ccr->avpList), x->service_information))
            goto error;
    return ccr;
error:
    cdp_avp->cdp->AAAFreeMessage(&ccr);
    return 0;
}

AAAMessage *Ro_new_ccr(AAASession * session, Ro_CCR_t * ro_ccr_data) {

    AAAMessage * ccr = 0;
    ccr = cdp_avp->cdp->AAACreateRequest(IMS_Ro, Diameter_CCR, Flag_Proxyable, session);
    if (!ccr) {
        LM_ERR("could not create CCR\n");
        return 0;
    }
  
    ccr = Ro_write_CCR_avps(ccr, ro_ccr_data);

    return ccr;
}

Ro_CCA_t *Ro_parse_CCA_avps(AAAMessage *cca) {
    if (!cca)
        return 0;

    Ro_CCA_t *ro_cca_data = 0;
    mem_new(ro_cca_data, sizeof (Ro_CCR_t), pkg);
    multiple_services_credit_control_t *mscc = 0;
    mem_new(mscc, sizeof (multiple_services_credit_control_t), pkg);
    granted_services_unit_t *gsu = 0;
    mem_new(gsu, sizeof (granted_services_unit_t), pkg);
    final_unit_indication_t *fui = 0;
    mem_new(fui, sizeof (final_unit_indication_t), pkg);
    mscc->granted_service_unit = gsu;
    mscc->final_unit_action = fui;

    mscc->final_unit_action->action = -1;

    AAA_AVP_LIST* avp_list = &cca->avpList;
    AAA_AVP_LIST mscc_avp_list;
    AAA_AVP_LIST* mscc_avp_list_ptr;

    AAA_AVP *avp = avp_list->head;
    unsigned int x;
    while (avp != NULL) {
        switch (avp->code) {
            case AVP_CC_Request_Type:
                x = get_4bytes(avp->data.s);
                ro_cca_data->cc_request_type = x;
                break;
            case AVP_CC_Request_Number:
                x = get_4bytes(avp->data.s);
                ro_cca_data->cc_request_number = x;
                break;
            case AVP_Multiple_Services_Credit_Control:
                mscc_avp_list = cdp_avp->cdp->AAAUngroupAVPS(avp->data);
                mscc_avp_list_ptr = &mscc_avp_list;
                AAA_AVP *mscc_avp = mscc_avp_list_ptr->head;
                while (mscc_avp != NULL) {
                    LM_DBG("MSCC AVP code is [%i] and data length is [%i]", mscc_avp->code, mscc_avp->data.len);
                    switch (mscc_avp->code) {
                            AAA_AVP_LIST y;
                            AAA_AVP *z;
                        case AVP_Granted_Service_Unit:
                            y = cdp_avp->cdp->AAAUngroupAVPS(mscc_avp->data);
                            z = y.head;
                            while (z) {
                                switch (z->code) {
                                    case AVP_CC_Time:
                                        mscc->granted_service_unit->cc_time = get_4bytes(z->data.s);
                                        break;
                                    default:
                                        LM_ERR("Unsupported Granted Service Unit with code:[%d]\n", z->code);
                                }
                                z = z->next;
                            }
                            cdp_avp->cdp->AAAFreeAVPList(&y);
                            break;
                        case AVP_Validity_Time:
                            mscc->validity_time = get_4bytes(mscc_avp->data.s);
                            break;
                        case AVP_Final_Unit_Indication:
                            y = cdp_avp->cdp->AAAUngroupAVPS(mscc_avp->data);
                            z = y.head;
                            while (z) {
                                switch (z->code) {
                                    case AVP_Final_Unit_Action:
                                        mscc->final_unit_action->action = get_4bytes(z->data.s);
                                        break;
                                    default:
                                        LM_ERR("Unsupported Final Unit Indication AVP.\n");
                                }
                                z = z->next;
                            }
                            cdp_avp->cdp->AAAFreeAVPList(&y);
                    }
                    mscc_avp = mscc_avp->next;
                }
                cdp_avp->cdp->AAAFreeAVPList(mscc_avp_list_ptr);
                break;
            case AVP_Result_Code:
                x = get_4bytes(avp->data.s);
                ro_cca_data->resultcode = x;
                break;
        }
        avp = avp->next;
    }
    ro_cca_data->mscc = mscc;
    return ro_cca_data;

out_of_memory:
    LM_ERR("out of pkg memory\n");
    Ro_free_CCA(ro_cca_data);
    return 0;
}
