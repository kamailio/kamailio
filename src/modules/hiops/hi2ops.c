/*
 * Copyright (C) 2019 Mojtaba Esfandiari.S, Nasim-Telecom
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
 */

#include "hi2ops.h"

HI2_Session_list_t *hi2_head_session_list = NULL;
int *hi2_len_session_list = NULL;
int mid_ims_session = 0;    //Not activated Mid-IMS-Session. refer to ETSI TS 133 107 V14. cluase 7A.3.1
int provisinal_responses = 0;
int successful_responses = 1;
int redirection_responses = 0;
int request_failure_responses = 0;
int server_failure_responses = 0;
int global_failure_responses = 0;
htable_api_t htable_api;


int hi2_shm_memory(){
    hi2_len_session_list = shm_malloc(sizeof(int));     //Shared variable between all fork process.This is lenght of linked list.
    if (hi2_len_session_list == NULL) {
                LM_ERR("cannot allocate shm memory hi2_len_session_list in hi2_constructor\n");
        return -1;
    }
    *hi2_len_session_list = 0;          //default value for lenght of linked list.

    hi2_head_session_list = shm_malloc(sizeof(HI2_Session_list_t));     //Shared varibale,This is pointer of head of linked list.
    if (hi2_head_session_list == NULL) {
                LM_ERR("cannot allocate shm memory hi2_head_session_list in hi2_constructor\n");
        return -1;
    }

    hi2_head_session_list->hi2Session.lawfulInterceptionIdentifier[0] = '\0';
    hi2_head_session_list->hi2Session.target_Information[0] = '\0';
    hi2_head_session_list->hi2Session.gprscorrelation[0] = '\0';
    hi2_head_session_list->hi2Session.sipMessageHeaderOffer[0] = '\0';
    hi2_head_session_list->hi2Session.sipMessageHeaderAnswer[0] = '\0';
    hi2_head_session_list->hi2Session.sdpOffer[0] = '\0';
    hi2_head_session_list->hi2Session.sdpAnswer[0] = '\0';
    hi2_head_session_list->hi2Session.mediaSecFailureIndication[0] = '\0';
    hi2_head_session_list->hi2Session.pANIHeaderInfo[0] = '\0';
    hi2_head_session_list->hi2Session.totag[0] = '\0';
    hi2_head_session_list->hi2Session.fromtag[0] = '\0';
    hi2_head_session_list->hi2Session.counter = 0;
    return 1;
}


int hi2_parse_from_header(struct sip_msg *msg)
{
    struct to_body *from_b;

//    if(!msg->from && (parse_headers(msg, HDR_FROM_F, 0) == -1 || !msg->from)) {
    if(!msg->from) {
                LM_ERR("bad msg or missing FROM header\n");
        return -1;
    }

    /* maybe the header is already parsed! */
    if(msg->from->parsed)
        return 0;

    /* bad luck! :-( - we have to parse it */
    /* first, get some memory */
    from_b = pkg_malloc(sizeof(struct to_body));
    if(from_b == 0) {
                LM_ERR("out of pkg_memory\n");
        goto error;
    }

    /* now parse it!! */
    memset(from_b, 0, sizeof(struct to_body));
    parse_to(msg->from->body.s, msg->from->body.s + msg->from->body.len + 1,
             from_b);
    if(from_b->error == PARSE_ERROR) {
                LM_ERR("bad From header [%.*s]\n", msg->from->body.len,
                       msg->from->body.s);
        goto error;
    }
    msg->from->parsed = from_b;

    return 0;

error:
    free_from(from_b);
    return -1;
}

/*! \brief
 * This method is used to parse the from header.It is take from parse_from.c
 *
 * \note It was decided not to parse
 * anything in core that is not *needed* so this method gets called by
 * rad_acc module and any other modules that needs the To header.
 *
 * params: msg : sip msg
 * returns 0 on success,
 *        -1 on failure.
 */
int hi2_parse_to_header(struct sip_msg *msg)
{
    struct to_body *to_b;

//    if(!msg->to && (parse_headers(msg, HDR_TO_F, 0) == -1 || !msg->to)) {
    if(!msg->to) {
                LM_ERR("bad msg or missing FROM header\n");
        return -1;
    }

    /* maybe the header is already parsed! */
    if(msg->to->parsed)
        return 0;

    /* bad luck! :-( - we have to parse it */
    /* first, get some memory */
    to_b = pkg_malloc(sizeof(struct to_body));
    if(to_b == 0) {
                LM_ERR("out of pkg_memory\n");
        goto error;
    }

    /* now parse it!! */
    memset(to_b, 0, sizeof(struct to_body));
    parse_to(msg->to->body.s, msg->to->body.s + msg->to->body.len + 1,
             to_b);
    if(to_b->error == PARSE_ERROR) {
                LM_ERR("bad To header [%.*s]\n", msg->to->body.len,
                       msg->to->body.s);
                goto  error;
    }
    msg->to->parsed = to_b;

    return 0;

error:
    free_to(to_b);
    return -1;

}

int hi2_parse_cseg_header(struct sip_msg *msg)
{
    struct cseq_body *cseqbody;

    if(!msg->cseq) {
                LM_ERR("bad msg or missing CSeg header\n");
        return -1;
    }

    /* maybe the header is already parsed! */
    if(msg->cseq->parsed)
        return 0;

    /* bad luck! :-( - we have to parse it */
    /* first, get some memory */
    cseqbody = pkg_malloc(sizeof(struct cseq_body));
    if(cseqbody == 0) {
        LM_ERR("out of pkg_memory\n");
        goto error;
    }

    /* now parse it!! */
    memset(cseqbody, 0, sizeof(struct cseq_body));
    if(parse_cseq(msg->cseq->body.s, msg->cseq->body.s + msg->cseq->body.len+1, cseqbody)==0) {
                LM_ERR("error while parsing cseg\n");
        goto error;
    }


    if(cseqbody->error == PARSE_ERROR) {
                LM_ERR("bad CSeg header [%.*s]\n", msg->cseq->body.len,
                       msg->cseq->body.s);
        goto error;
    }

    msg->cseq->parsed = cseqbody;
    return 0;
error:

    if(cseqbody)
        free_cseq(cseqbody);
    return -1;

}

// will return the method ID for a reply by inspecting the Cseq header
int get_method_from_reply(struct sip_msg *reply)
{
    struct cseq_body *cseq;

    if(reply->first_line.type != SIP_REPLY)
        return -1;

    if(!reply->cseq && parse_headers(reply, HDR_CSEQ_F, 0) < 0) {
                LM_ERR("failed to parse the CSeq header\n");
        return -1;
    }
    if(!reply->cseq) {
                LM_ERR("missing CSeq header\n");
        return -1;
    }
    cseq = reply->cseq->parsed;
    return cseq->method_id;
}


int hi2_message_is_initial_request(struct sip_msg *msg){
    struct to_body *parsed_to;
    struct sip_uri turi;

    parsed_to = (struct to_body *) msg->to->parsed;
    if (parse_uri(parsed_to->uri.s, parsed_to->uri.len, &turi) || !turi.user.len) {
                LM_ERR("unable to extract username to URI (To header)\n");
        return -1;
    }

    if (parsed_to->tag_value.len)
        return 0;
    else
        return 1;
}

int hi2_iri_sip_exist_in_htable(struct sip_msg *msg){

    char msg_md5[32];
    str smd5[5];
    smd5[0].s = msg->callid->name.s;
    smd5[0].len = msg->callid->len;

    smd5[1].s = msg->cseq->name.s;
    smd5[1].len = msg->cseq->len;

    if (msg->first_line.type == SIP_REQUEST) {
        smd5[2].s = msg->first_line.u.request.uri.s;
        smd5[2].len = msg->first_line.u.request.uri.len;
        smd5[3].len = 0;
        smd5[3].s = NULL;

    } else{
        smd5[2].s = msg->first_line.u.reply.status.s;
        smd5[2].len = msg->first_line.u.reply.status.len;
        smd5[3].s = msg->first_line.u.reply.reason.s;
        smd5[3].len = msg->first_line.u.reply.reason.len;
    }

    smd5[4].s = ip_addr2a(&msg->rcv);
    smd5[4].len = strlen(smd5[4].s);

    MD5StringArray(msg_md5, smd5, 5);

    str ht, key;
    ht.s = "checkertrance";
    ht.len = 13;

    key.s = msg_md5;
    key.len = 32;

    int_str value;
    value.n = 1;

    unsigned int val;

    if (htable_api.get_expire(&ht, &key, &val) != 0) {
                LM_INFO("ERROR while insert new record in htable\n");
        return -1;
    } else {
        if (val) {
                    LM_INFO("The message is retransmited:%d\n", val);
//                    LM_INFO("MD5 message: <%.*s>\n", 32, msg_md5);
            return 1;
        } else
                    LM_INFO ("The message is not retransmited:%d\n", val);
                LM_INFO("MD5 message: <%.*s>\n", 32, msg_md5);
    }

    if (htable_api.set(&ht, &key, 0, &value, 1) != 0) {
                LM_INFO("ERROR while insert new record in htable\n");
        return -1;
    }

//            LM_INFO("insert new record in htable successfully\n");
    return 0;
}

struct HI2_Session_list *hi2_session_activated(struct sip_msg *msg){
    str  callid;
    char correlation[32];

    HI2_Session_list_t *tmp_session;

    //set correlation identify for this transaction basec callid parameter.
    callid.s = msg->callid->name.s;
    callid.len = msg->callid->len;
    MD5StringArray(correlation, &callid, 1);

    tmp_session = hi2_head_session_list->next;
    while (tmp_session){
        if (strncmp(tmp_session->hi2Session.gprscorrelation, correlation, 32) == 0)
        {
            LM_INFO("the session is found.\n");
            return tmp_session;
        }
        tmp_session = tmp_session->next;
    }
    LM_INFO("the session is not found\n");
    return NULL;
}


int hi2_add_to_session_list(struct sip_msg *msg, struct HI1DataInfo *hi1DataInfo, int methodtype){
    HI2_Session_list_t *tmp_session;
    struct to_body *parsed_from, *parsed_to;
    str  callid;
    char correlation[32];


//    LM_INFO("Strp-1\n");
    parsed_from = (struct to_body *) msg->from->parsed;
    parsed_to = (struct to_body *) msg->to->parsed;

    tmp_session = shm_malloc(sizeof(HI2_Session_list_t));
    if (tmp_session == NULL){
                LM_ERR("cannot allocate shm memory for tmp_session in hi2_add_to_session_list function\n");
        return -1;
    }

    //set correlation identify for this transaction basec callid parameter.
    callid.s = msg->callid->name.s;
    callid.len = msg->callid->len;
    MD5StringArray(correlation, &callid, 1);

    //Set primitive parameters in correlation list. other parameters are filling in sequentianl sipMessage or sipRelply messages.
    //In this situation these parametes will be set: pgrscorrelation,fromtag,orginIP.orginPort

    if (mid_ims_session) { //run for mid_ims_session interception.
        tmp_session->hi2Session.lawfulInterceptionIdentifier[0] = '\0';
        tmp_session->hi2Session.target_Information[0] = '\0';
    } else {
        strcpy(tmp_session->hi2Session.lawfulInterceptionIdentifier, hi1DataInfo->lawfulInterceptionIdentifier);
        strcpy(tmp_session->hi2Session.target_Information, hi1DataInfo->target_Information);
    }
    strncpy(tmp_session->hi2Session.gprscorrelation, correlation, 32);
    strncpy(tmp_session->hi2Session.sipMessageHeaderOffer, msg->buf, msg->len);
    tmp_session->hi2Session.sipMessageHeaderAnswer[0] = '\0';
    strncpy(tmp_session->hi2Session.sdpOffer, msg->buf, msg->len);
    tmp_session->hi2Session.sdpAnswer[0] = '\0';
    tmp_session->hi2Session.pANIHeaderInfo[0] = '\0';
    strncpy(tmp_session->hi2Session.fromtag, parsed_from->tag_value.s, parsed_from->tag_value.len);
    if( methodtype == 1 )
        tmp_session->hi2Session.totag[0] = '\0';
    else if (methodtype == 2048)          //because update methos is sequentioal request and it has totag param.
        strncpy(tmp_session->hi2Session.totag, parsed_to->tag_value.s, parsed_to->tag_value.len);
    tmp_session->hi2Session.counter++;
    //todo... Add dome extra information from A-leg party.

//    LM_INFO("Strp-2\n");
    //Add to correlation list.
    LM_INFO("len of session: %d\n", *hi2_len_session_list);
    if (*hi2_len_session_list == 0) {                //This is first hi2 session
                LM_INFO("This is first hi2 session to insert to linked list\n");

        tmp_session->next = NULL;
        hi2_head_session_list->next = tmp_session;
        *hi2_len_session_list = *hi2_len_session_list + 1;

//    } else if (*hi1_len_listed_p > 0){
    } else {
        LM_INFO("This is second or more hi2 session to insert to front list\n");
        tmp_session->next = hi2_head_session_list->next;
        hi2_head_session_list->next = tmp_session;
        *hi2_len_session_list = *hi2_len_session_list + 1;
    }
//    LM_INFO("Strp-3\n");

    LM_INFO("New session with correlation number (%.*s) is added to session list.\n", 32, tmp_session->hi2Session.gprscorrelation);
    //todo... Send it for LEA.(mespio)
    return 1;
}

/**
 *
 * @param msg
 * @param hi1DataInfo
 * @param type, For SIP_Request is 1, For SIP_Reply is 2
 * @return
 */
int hi2_update_to_session_list(struct sip_msg *msg, struct HI1DataInfo *hi1DataInfo, int type){
    struct to_body *parsed_to;
    str  callid;
    char correlation[32];

    HI2_Session_list_t *tmp_session;

    parsed_to = (struct to_body *) msg->to->parsed;

    //set correlation identify for this transaction basec callid parameter.
    callid.s = msg->callid->name.s;
    callid.len = msg->callid->len;
    MD5StringArray(correlation, &callid, 1);

    tmp_session = hi2_head_session_list->next;
    while (tmp_session){
        if (strncmp(tmp_session->hi2Session.gprscorrelation, correlation, 32) == 0)
        {
            if(type == 1) {
                        LM_INFO("the session is found.update liid and target now\n");
                strcpy(tmp_session->hi2Session.lawfulInterceptionIdentifier, hi1DataInfo->lawfulInterceptionIdentifier);
                strcpy(tmp_session->hi2Session.target_Information, hi1DataInfo->target_Information);
            } else if(type == 2){
                        LM_INFO("the session is found.update sipMessageHeaderAnswer and sdpAnswer now\n");
                strncpy(tmp_session->hi2Session.sipMessageHeaderAnswer, msg->buf, msg->len);
                strncpy(tmp_session->hi2Session.sdpAnswer, msg->buf, msg->len);
                strncpy(tmp_session->hi2Session.totag, parsed_to->tag_value.s, parsed_to->tag_value.len);
            }else if(type == 3){
                        LM_INFO("the session is found.update liid, target, sipMessageHeaderAnswer and sdpAnswer now\n");
                strcpy(tmp_session->hi2Session.lawfulInterceptionIdentifier, hi1DataInfo->lawfulInterceptionIdentifier);
                strcpy(tmp_session->hi2Session.target_Information, hi1DataInfo->target_Information);
                strncpy(tmp_session->hi2Session.sipMessageHeaderAnswer, msg->buf, msg->len);
                strncpy(tmp_session->hi2Session.sdpAnswer, msg->buf, msg->len);
                strncpy(tmp_session->hi2Session.totag, parsed_to->tag_value.s, parsed_to->tag_value.len);

            } else if(type == 4) {
                        LM_INFO("the session is found.update liid , target, sdp offer now\n");
                strcpy(tmp_session->hi2Session.lawfulInterceptionIdentifier, hi1DataInfo->lawfulInterceptionIdentifier);
                strcpy(tmp_session->hi2Session.target_Information, hi1DataInfo->target_Information);
                strncpy(tmp_session->hi2Session.sipMessageHeaderOffer, msg->buf, msg->len);
                strncpy(tmp_session->hi2Session.sdpOffer, msg->buf, msg->len);
                strncpy(tmp_session->hi2Session.totag, parsed_to->tag_value.s, parsed_to->tag_value.len);
            }
            return 1;
        }
        tmp_session = tmp_session->next;
    }
    LM_INFO("the session is not found\n");
    return 0;
}

int hi2_remove_from_session_list(struct sip_msg *msg){
    str  callid;
    char correlation[32];

    HI2_Session_list_t *tmp_session, *del;

    //set correlation identify for this transaction basec callid parameter.
    callid.s = msg->callid->name.s;
    callid.len = msg->callid->len;
    MD5StringArray(correlation, &callid, 1);

    tmp_session = hi2_head_session_list;
    del = tmp_session->next;

    while (del){
        if (strncmp(del->hi2Session.gprscorrelation, correlation, 32) == 0)
        {
            tmp_session->next = del->next;
            shm_free(del);
            *hi2_len_session_list = *hi2_len_session_list -1;

            LM_INFO("the session is found. delete from session list now\n");
            return 1;
        }
        tmp_session = tmp_session->next;
        del = tmp_session->next;
    }
    LM_INFO("the session is not found for deleting from session list.\n");
    return 0;

}


int hi2_iri_interception_based_sip_initial(struct sip_msg *msg, HI1_Listed_P_t *list){    // I mean liid.

    struct to_body *parsed_from, *parsed_to;
    struct sip_uri furi, turi;
    struct HI2_Session_list *tmp_session;
    int flag;
    parsed_from = (struct to_body *) msg->from->parsed;
    if (parse_uri(parsed_from->uri.s, parsed_from->uri.len, &furi) || !furi.user.len) {
                LM_ERR("unable to extract username from URI (From header)\n");
        return -1;
    }

    parsed_to = (struct to_body *) msg->to->parsed;
    if (parse_uri(parsed_to->uri.s, parsed_to->uri.len, &turi) || !turi.user.len) {
                LM_ERR("unable to extract username to URI (To header)\n");
        return -1;
    }

    tmp_session = hi2_session_activated(msg);
    flag = 1;
    while (list) {      // if the target should be intercepte. check in all lists.
        if (strncmp(list->hi1DataInfo.target_Information, furi.user.s, furi.user.len) == 0) {     //The username from URI (From header) is target .
            flag = 0;
            LM_INFO("This is SIP_Request, Initial\n");
//                LM_INFO("the target <%.*s> is originating-Party.\n", furi.user.len, furi.user.s);
                LM_INFO ("sIPMessage:<%.*s>\n", msg->first_line.u.request.method.len,
                                         msg->first_line.u.request.method.s);


//                LM_INFO("originating-Party:<%.*s>\n", furi.user.len, furi.user.s);
//                LM_INFO("terminating-Party:<%.*s>\n", turi.user.len, turi.user.s);
//                LM_INFO ("SIP-URI-From:<%.*s>\n", parsed_from->uri.len, parsed_from->uri.s);
//                LM_INFO ("SIP-URI-To:<%.*s>\n", parsed_to->uri.len, parsed_to->uri.s);
//                LM_INFO ("Event-Type:%s\n", "unfilteredSIPmessage");
//                LM_INFO("Evant-date-time:%ld\n", msg->tval.tv_sec * 1000000 + msg->tval.tv_usec);
//                LM_INFO ("NetworkIdentifier:%s\n", "432");
////                LM_INFO ("GPRSCorrelationNumber:%s\n", correlation);
//                LM_INFO ("HashNumber:%d\n", msg->hash_index);
//                LM_INFO ("Correlation:%s\n", "iri-to-cc");
//                LM_INFO("target:<%s>\n", list->hi1DataInfo.target_Information);
//                LM_INFO ("LawfulInterceptionIdentifier:%s\n", list->hi1DataInfo.lawfulInterceptionIdentifier);

//                LM_INFO("liid:<%s>\n", list->hi1DataInfo.lawfulInterceptionIdentifier);
//                LM_INFO("target:<%s>\n", list->hi1DataInfo.target_Information);
//                LM_INFO("originating-Party:<%.*s>\n", furi.user.len, furi.user.s);
//                LM_INFO("terminating-Party:<%.*s>\n", turi.user.len, turi.user.s);
//                LM_INFO("gPRSCorrelationNumber: <%.*s>\n", 32, correlation);

            if(!tmp_session && msg->REQ_METHOD == METHOD_INVITE)
                hi2_add_to_session_list(msg, &list->hi1DataInfo, 1);

                    LM_INFO(">>>>>>>>>>>>>>>>>Create HI2 file \n ");
//                    LM_INFO(">>>>>>>>>>>>>>>>>Create HI2 file, msg:<%.*s> \n ", msg->len, msg->buf);
            //todo... Create HI2 file and save it into current directory.

            return 1;
        } else if (strncmp(list->hi1DataInfo.target_Information, turi.user.s, turi.user.len) == 0) {  //The username to URI (To header) is target .
                    flag = 0;
                    LM_INFO("This is SIP_Request, Initial\n");
//                LM_INFO("the target <%.*s> is terminating-Party.\n", turi.user.len, turi.user.s);
                LM_INFO ("sIPMessage:<%.*s>\n", msg->first_line.u.request.method.len,
                                 msg->first_line.u.request.method.s);

//                LM_INFO("originating-Party:<%.*s>\n", furi.user.len, furi.user.s);
//                LM_INFO("terminating-Party:<%.*s>\n", turi.user.len, turi.user.s);
//                LM_INFO ("SIP-URI-From:<%.*s>\n", parsed_from->uri.len, parsed_from->uri.s);
//                LM_INFO ("SIP-URI-To:<%.*s>\n", parsed_to->uri.len, parsed_to->uri.s);
//                LM_INFO ("Event-Type:%s\n", "unfilteredSIPmessage");
//                LM_INFO("Evant-date-time:%ld\n", msg->tval.tv_sec * 1000000 + msg->tval.tv_usec);
//                LM_INFO ("NetworkIdentifier:%s\n", "432");
////                LM_INFO ("GPRSCorrelationNumber:%s\n", correlation);
//                LM_INFO ("HashNumber:%d\n", msg->hash_index);
//                LM_INFO ("Correlation:%s\n", "iri-to-cc");
//                LM_INFO("target:<%s>\n", list->hi1DataInfo.target_Information);
//                LM_INFO ("LawfulInterceptionIdentifier:%s\n", list->hi1DataInfo.lawfulInterceptionIdentifier);


//            LM_INFO("liid:<%s>\n", list->hi1DataInfo.lawfulInterceptionIdentifier);
//            LM_INFO("target:<%s>\n", list->hi1DataInfo.target_Information);
//            LM_INFO("originating-Party:<%.*s>\n", furi.user.len, furi.user.s);
//            LM_INFO("terminating-Party:<%.*s>\n", turi.user.len, turi.user.s);
//            LM_INFO("gPRSCorrelationNumber: <%.*s>\n", 32, correlation);

            if(!tmp_session && msg->REQ_METHOD == METHOD_INVITE)
                hi2_add_to_session_list(msg, &list->hi1DataInfo, 1);

                    LM_INFO(">>>>>>>>>>>>>>>>>Create HI2 file \n ");
//                    LM_INFO(">>>>>>>>>>>>>>>>>Create HI2 file, msg:<%.*s> \n ", msg->len, msg->buf);
            //todo... Create HI2 file and save it into current directory.

            return 1;

        }
        list = list->next;
    }
    // The current session is not target, But if mid_ims_session is activated, do this commands.
    if(mid_ims_session && flag && msg->REQ_METHOD == METHOD_INVITE){   //I mean the target does not found in list.
                LM_INFO("This is SIP_Request, Initial, mid-ims-session enable\n");
                LM_INFO("It is no target. Just because mid_ims_session interception is activated.\n");
                LM_INFO ("sIPMessage:<%.*s>\n", msg->first_line.u.request.method.len,
                         msg->first_line.u.request.method.s);

//                LM_INFO("originating-Party:<%.*s>\n", furi.user.len, furi.user.s);
//                LM_INFO("terminating-Party:<%.*s>\n", turi.user.len, turi.user.s);
//                LM_INFO ("SIP-URI-From:<%.*s>\n", parsed_from->uri.len, parsed_from->uri.s);
//                LM_INFO ("SIP-URI-To:<%.*s>\n", parsed_to->uri.len, parsed_to->uri.s);
//                LM_INFO ("Event-Type:%s\n", "unfilteredSIPmessage");
//                LM_INFO("Evant-date-time:%ld\n", msg->tval.tv_sec * 1000000 + msg->tval.tv_usec);
//                LM_INFO ("NetworkIdentifier:%s\n", "432");
////                LM_INFO ("GPRSCorrelationNumber:%s\n", correlation);
//                LM_INFO ("HashNumber:%d\n", msg->hash_index);
//                LM_INFO ("Correlation:%s\n", "iri-to-cc");

//        tmp_session = hi2_session_activated(msg);
        if(!tmp_session)        //todo..........................................
//            if(!tmp_session && msg->REQ_METHOD == METHOD_INVITE)        //todo..........................................
            hi2_add_to_session_list(msg, NULL, 1);

    }
    return 1;

}

int hi2_iri_interception_based_sip_sequential(struct sip_msg *msg, HI1_Listed_P_t *list){     //I mean correlation Number.

    struct to_body *parsed_from, *parsed_to;
    struct sip_uri furi, turi;
    struct HI2_Session_list *tmp_session;

    parsed_from = (struct to_body *) msg->from->parsed;
    if (parse_uri(parsed_from->uri.s, parsed_from->uri.len, &furi) || !furi.user.len) {
                LM_ERR("unable to extract username from URI (From header)\n");
        return -1;
    }

    parsed_to = (struct to_body *) msg->to->parsed;
    if (parse_uri(parsed_to->uri.s, parsed_to->uri.len, &turi) || !turi.user.len) {
                LM_ERR("unable to extract username to URI (To header)\n");
        return -1;
    }


    if(mid_ims_session) {       //If activated, we should check by liid,
        while (list) {      // if the target should be intercepte. check in all lists.
                    LM_INFO("This is SIP_Request, Sequential, mid-ims-session enable\n");
            if (strncmp(list->hi1DataInfo.target_Information, furi.user.s, furi.user.len) == 0) {     //The username from URI (From header) is target .
//                        LM_INFO("the target <%.*s> is originating-Party.\n", furi.user.len, furi.user.s);
                        LM_INFO ("sIPMessage:<%.*s>\n", msg->first_line.u.request.method.len,
                                 msg->first_line.u.request.method.s);

//                        LM_INFO("originating-Party:<%.*s>\n", furi.user.len, furi.user.s);
//                        LM_INFO("terminating-Party:<%.*s>\n", turi.user.len, turi.user.s);
//                        LM_INFO ("SIP-URI-From:<%.*s>\n", parsed_from->uri.len, parsed_from->uri.s);
//                        LM_INFO ("SIP-URI-To:<%.*s>\n", parsed_to->uri.len, parsed_to->uri.s);
//                        LM_INFO ("Event-Type:%s\n", "unfilteredSIPmessage");
//                        LM_INFO("Evant-date-time:%ld\n", msg->tval.tv_sec * 1000000 + msg->tval.tv_usec);
//                        LM_INFO ("NetworkIdentifier:%s\n", "432");
////                        LM_INFO ("GPRSCorrelationNumber:%s\n", correlation);
//                        LM_INFO ("HashNumber:%d\n", msg->hash_index);
//                        LM_INFO ("Correlation:%s\n", "iri-to-cc");
//                        LM_INFO("target:<%s>\n", list->hi1DataInfo.target_Information);
//                        LM_INFO ("LawfulInterceptionIdentifier:%s\n", list->hi1DataInfo.lawfulInterceptionIdentifier);

                        if(msg->REQ_METHOD == METHOD_UPDATE)
                            hi2_update_to_session_list(msg, &list->hi1DataInfo,4);        //mid_ims_session should be updated. update liid and target , sdp offer in session list.
                        else
                            hi2_update_to_session_list(msg, &list->hi1DataInfo,1);        //mid_ims_session should be updated. update liid and target in session list.

                        LM_INFO(">>>>>>>>>>>>>>>>>Create HI2 file \n ");
//                    LM_INFO(">>>>>>>>>>>>>>>>>Create HI2 file, msg:<%.*s> \n ", msg->len, msg->buf);
                //todo... Create HI2 file and save it into current directory.


                return 1;
            } else if (strncmp(list->hi1DataInfo.target_Information, turi.user.s, turi.user.len) == 0) {  //The username to URI (To header) is target .
                        LM_INFO("This is SIP_Request, Sequential, mid-ims-session enable\n");
//                        LM_INFO("the target <%.*s> is terminating-Party.\n", turi.user.len, turi.user.s);
                        LM_INFO ("sIPMessage:<%.*s>\n", msg->first_line.u.request.method.len,
                                 msg->first_line.u.request.method.s);

//                        LM_INFO("originating-Party:<%.*s>\n", furi.user.len, furi.user.s);
//                        LM_INFO("terminating-Party:<%.*s>\n", turi.user.len, turi.user.s);
//                        LM_INFO ("SIP-URI-From:<%.*s>\n", parsed_from->uri.len, parsed_from->uri.s);
//                        LM_INFO ("SIP-URI-To:<%.*s>\n", parsed_to->uri.len, parsed_to->uri.s);
//                        LM_INFO ("Event-Type:%s\n", "unfilteredSIPmessage");
//                        LM_INFO("Evant-date-time:%ld\n", msg->tval.tv_sec * 1000000 + msg->tval.tv_usec);
//                        LM_INFO ("NetworkIdentifier:%s\n", "432");
////                        LM_INFO ("GPRSCorrelationNumber:%s\n", correlation);
//                        LM_INFO ("HashNumber:%d\n", msg->hash_index);
//                        LM_INFO ("Correlation:%s\n", "iri-to-cc");
//                        LM_INFO("target:<%s>\n", list->hi1DataInfo.target_Information);
//                        LM_INFO ("LawfulInterceptionIdentifier:%s\n", list->hi1DataInfo.lawfulInterceptionIdentifier);


                if(msg->REQ_METHOD == METHOD_UPDATE)
                    hi2_update_to_session_list(msg, &list->hi1DataInfo,4);        //mid_ims_session should be updated. update liid and target , sdp offer in session list.
                else
                    hi2_update_to_session_list(msg, &list->hi1DataInfo,1);        //mid_ims_session should be updated. update liid and target in session list.

                        LM_INFO(">>>>>>>>>>>>>>>>>Create HI2 file \n ");
//                    LM_INFO(">>>>>>>>>>>>>>>>>Create HI2 file, msg:<%.*s> \n ", msg->len, msg->buf);
                //todo... Create HI2 file and save it into current directory.

                return 1;
            }
            list = list->next;
        }
        //In this here we just check sip_request, If it was Update method, It could be ass ti list like invite method.
        if(msg->REQ_METHOD == METHOD_UPDATE)
            hi2_add_to_session_list(msg, NULL, 2048);

        return 0;
    } else {                 //If mid_ims_session not to activat, we could use by session-id

        LM_INFO("This is SIP_Request, Sequential, mid-ims-session not to enable\n");
        tmp_session = hi2_session_activated(msg);
        if (tmp_session) {
//                    LM_INFO("the target <%.*s> is originating-Party.\n", furi.user.len, furi.user.s);
                    LM_INFO ("sIPMessage:<%.*s>\n", msg->first_line.u.request.method.len,
                             msg->first_line.u.request.method.s);

//                    LM_INFO("originating-Party:<%.*s>\n", furi.user.len, furi.user.s);
//                    LM_INFO("terminating-Party:<%.*s>\n", turi.user.len, turi.user.s);
//                    LM_INFO ("SIP-URI-From:<%.*s>\n", parsed_from->uri.len, parsed_from->uri.s);
//                    LM_INFO ("SIP-URI-To:<%.*s>\n", parsed_to->uri.len, parsed_to->uri.s);
//                    LM_INFO ("Event-Type:%s\n", "unfilteredSIPmessage");
//                    LM_INFO("Evant-date-time:%ld\n", msg->tval.tv_sec * 1000000 + msg->tval.tv_usec);
//                    LM_INFO ("NetworkIdentifier:%s\n", "432");
////                    LM_INFO ("GPRSCorrelationNumber:%s\n", correlation);
//                    LM_INFO ("HashNumber:%d\n", msg->hash_index);
//                    LM_INFO ("Correlation:%s\n", "iri-to-cc");
//                    LM_INFO("target:<%s>\n", list->hi1DataInfo.target_Information);
//                    LM_INFO ("LawfulInterceptionIdentifier:%s\n", list->hi1DataInfo.lawfulInterceptionIdentifier);

            if(!tmp_session && msg->REQ_METHOD == METHOD_UPDATE)
                    hi2_update_to_session_list(msg, &list->hi1DataInfo,4);        //mid_ims_session should be updated. update liid and target , sdp offer in session list.

                    LM_INFO(">>>>>>>>>>>>>>>>>Create HI2 file \n ");
//                    LM_INFO(">>>>>>>>>>>>>>>>>Create HI2 file, msg:<%.*s> \n ", msg->len, msg->buf);
            //todo..., Create HI2 file and save it inti current directory.
            //todo..., we could use data that is stored in current session.
            return 1;
        } else {
            //nothing to send for LEA, returned.
            //todo... update some information in correlation list from B-leg party, if exist.
            //todo... send to LEA.
            return  0;
        }
    }
    return 0;
}


int hi2_iri_interception_based_sip_reply(struct sip_msg *msg, HI1_Listed_P_t *list){      //I mean correlation Number.

    struct to_body *parsed_from, *parsed_to;
    struct sip_uri furi, turi;
    int status_code, reply_cseq_method_id, flag;
    struct HI2_Session_list *tmp_session;

    parsed_from = (struct to_body *) msg->from->parsed;
    if (parse_uri(parsed_from->uri.s, parsed_from->uri.len, &furi) || !furi.user.len) {
                LM_ERR("unable to extract username from URI (From header)\n");
        return -1;
    }

    parsed_to = (struct to_body *) msg->to->parsed;
    if (parse_uri(parsed_to->uri.s, parsed_to->uri.len, &turi) || !turi.user.len) {
                LM_ERR("unable to extract username to URI (To header)\n");
        return -1;
    }

    status_code = msg->first_line.u.reply.statuscode;
    reply_cseq_method_id = get_method_from_reply(msg);      //refer to msg_parser.h to see all method_id
    flag = 1;

//    if(status_code > 299 || status_code < 200)  //for now nothing to do. //todo... update.
//        return 0;

    //Just 200 reply is used.

    if(mid_ims_session) {
        while (list) {      // if the target should be intercepte. check in all lists.
            if (strncmp(list->hi1DataInfo.target_Information, furi.user.s, furi.user.len) == 0) {     //The username from URI (From header) is target .
                flag = 0;
                if(status_code == 200){
                    if(reply_cseq_method_id == 1 || reply_cseq_method_id == 2048){      //reply of INVITE method and update method,
                        LM_INFO("This is SIP_Reply, mid-ims-session enable, 200Ok,  INvite, Update\n");
                        hi2_update_to_session_list(msg, &list->hi1DataInfo, 3);         //update liid, target, sipMessageHeaderAnswer and sdpAnswer
                        if(successful_responses)
                                    LM_INFO(">>>>>>>>>>>>>>>>>Create HI2 file \n ");
//                    LM_INFO(">>>>>>>>>>>>>>>>>Create HI2 file, msg:<%.*s> \n ", msg->len, msg->buf);
                        //todo... Create HI2 file and save it into current directory.

                    } else if (reply_cseq_method_id == 8 || reply_cseq_method_id == 2) {  //reply of BYE or Cancel method,
                        LM_INFO("This is SIP_Reply, mid-ims-session enable, 200Ok,  BYE or Cancel\n");
                        hi2_remove_from_session_list(msg);
                        if(successful_responses)
                                    LM_INFO(">>>>>>>>>>>>>>>>>Create HI2 file \n ");
//                    LM_INFO(">>>>>>>>>>>>>>>>>Create HI2 file, msg:<%.*s> \n ", msg->len, msg->buf);
                        //todo... Create HI2 file and save it into current directory.
                    }
                }else {   //other SIP_Reply except of 200 OK.
                        LM_INFO("This is SIP_Reply, mid-ims-session enable, Other reply,\n");

                        if(status_code >= 100 && status_code <= 199){
                                    LM_INFO("This is SIP_Reply, mid-ims-session enable, 1xx,\n");
                            if (status_code == 183)
                                hi2_update_to_session_list(msg, &list->hi1DataInfo, 3);         //update liid, target, sipMessageHeaderAnswer and sdpAnswer
                            if(provisinal_responses || status_code == 180 || status_code == 183)
                                        LM_INFO(">>>>>>>>>>>>>>>>>Create HI2 file \n ");
//                    LM_INFO(">>>>>>>>>>>>>>>>>Create HI2 file, msg:<%.*s> \n ", msg->len, msg->buf);

                            //todo... Create HI2 file and save it into current directory.
                            //todo...
                        } else if(status_code >= 300 && status_code <= 399 && redirection_responses){
                                    LM_INFO("This is SIP_Reply, mid-ims-session enable, 3xx,\n");
                                    LM_INFO(">>>>>>>>>>>>>>>>>Create HI2 file \n ");
//                    LM_INFO(">>>>>>>>>>>>>>>>>Create HI2 file, msg:<%.*s> \n ", msg->len, msg->buf);
                            //todo... Create HI2 file and save it into current directory.
                            //todo...
                        } else if(status_code >= 400 && status_code <= 499){
                                    LM_INFO("This is SIP_Reply, mid-ims-session enable, 4xx,\n");
                                    hi2_remove_from_session_list(msg);
                                    if(request_failure_responses)
                                                LM_INFO(">>>>>>>>>>>>>>>>>Create HI2 file \n ");
//                    LM_INFO(">>>>>>>>>>>>>>>>>Create HI2 file, msg:<%.*s> \n ", msg->len, msg->buf);
                            //todo... Create HI2 file and save it into current directory.
                            //todo...
                        }else if(status_code >= 500 && status_code <= 599){
                                    LM_INFO("This is SIP_Reply, mid-ims-session enable, 5xx,\n");
                                    hi2_remove_from_session_list(msg);
                                    if(server_failure_responses)
                                                LM_INFO(">>>>>>>>>>>>>>>>>Create HI2 file \n ");
//                    LM_INFO(">>>>>>>>>>>>>>>>>Create HI2 file, msg:<%.*s> \n ", msg->len, msg->buf);
                            //todo... Create HI2 file and save it into current directory.
                            //todo...
                        }else if(status_code >= 600 && status_code <= 699){
                                    LM_INFO("This is SIP_Reply, mid-ims-session enable, 6xx,\n");
                                    hi2_remove_from_session_list(msg);
                                    if(global_failure_responses)
                                                LM_INFO(">>>>>>>>>>>>>>>>>Create HI2 file \n ");
//                    LM_INFO(">>>>>>>>>>>>>>>>>Create HI2 file, msg:<%.*s> \n ", msg->len, msg->buf);
                            //todo... Create HI2 file and save it into current directory.
                            //todo...
                        }

                    //todo..., based on configuration, it could be send these messages to LEA. the default not to .send
                }

                return 1;
            } else if (strncmp(list->hi1DataInfo.target_Information, turi.user.s, turi.user.len) == 0) {  //The username to URI (To header) is target .
                flag = 0;
                if(status_code == 200){
                    if(reply_cseq_method_id == 1 || reply_cseq_method_id == 2048){      //reply of INVITE method and update method,
                                LM_INFO("This is SIP_Reply, mid-ims-session enable, 200Ok,  INvite, Update\n");
                        hi2_update_to_session_list(msg, &list->hi1DataInfo, 3);         //update sipMessageHeaderAnswer and sdpAnswer
                        if(successful_responses)
                                    LM_INFO(">>>>>>>>>>>>>>>>>Create HI2 file \n ");
//                    LM_INFO(">>>>>>>>>>>>>>>>>Create HI2 file, msg:<%.*s> \n ", msg->len, msg->buf);
                        //todo... Create HI2 file and save it into current directory.

                    } else if (reply_cseq_method_id == 8 || reply_cseq_method_id == 2) {  //reply of BYE or Cancel method,
                                LM_INFO("This is SIP_Reply, mid-ims-session enable, 200Ok,  BYE or Cancel\n");
                        hi2_remove_from_session_list(msg);
                        if(successful_responses)
                                    LM_INFO(">>>>>>>>>>>>>>>>>Create HI2 file \n ");
//                    LM_INFO(">>>>>>>>>>>>>>>>>Create HI2 file, msg:<%.*s> \n ", msg->len, msg->buf);
                        //todo... Create HI2 file and save it into current directory.
                    }
                }else {   //other SIP_Reply except of 200 OK.
                            LM_INFO("This is SIP_Reply, mid-ims-session enable, Other reply,\n");

                    if(status_code >= 100 && status_code <= 199){
                                LM_INFO("This is SIP_Reply, mid-ims-session enable, 1xx,\n");
                        if (status_code == 183)
                            hi2_update_to_session_list(msg, &list->hi1DataInfo, 3);         //update sipMessageHeaderAnswer and sdpAnswer
                        if(provisinal_responses || status_code == 180 || status_code == 183)
                                    LM_INFO(">>>>>>>>>>>>>>>>>Create HI2 file \n ");
//                    LM_INFO(">>>>>>>>>>>>>>>>>Create HI2 file, msg:<%.*s> \n ", msg->len, msg->buf);
                        //todo... Create HI2 file and save it into current directory.
                        //todo...
                    } else if(status_code >= 300 && status_code <= 399 && redirection_responses){
                                LM_INFO("This is SIP_Reply, mid-ims-session enable, 3xx,\n");
                                LM_INFO(">>>>>>>>>>>>>>>>>Create HI2 file \n ");
//                    LM_INFO(">>>>>>>>>>>>>>>>>Create HI2 file, msg:<%.*s> \n ", msg->len, msg->buf);
                        //todo... Create HI2 file and save it into current directory.
                        //todo...
                    } else if(status_code >= 400 && status_code <= 499){
                                LM_INFO("This is SIP_Reply, mid-ims-session enable, 4xx,\n");
                        hi2_remove_from_session_list(msg);
                        if(request_failure_responses)
                                    LM_INFO(">>>>>>>>>>>>>>>>>Create HI2 file \n ");
//                    LM_INFO(">>>>>>>>>>>>>>>>>Create HI2 file, msg:<%.*s> \n ", msg->len, msg->buf);
                        //todo... Create HI2 file and save it into current directory.
                        //todo...
                    }else if(status_code >= 500 && status_code <= 599){
                                LM_INFO("This is SIP_Reply, mid-ims-session enable, 5xx,\n");
                        hi2_remove_from_session_list(msg);
                        if(server_failure_responses)
                                    LM_INFO(">>>>>>>>>>>>>>>>>Create HI2 file \n ");
//                    LM_INFO(">>>>>>>>>>>>>>>>>Create HI2 file, msg:<%.*s> \n ", msg->len, msg->buf);
                        //todo... Create HI2 file and save it into current directory.
                        //todo...
                    }else if(status_code >= 600 && status_code <= 699){
                                LM_INFO("This is SIP_Reply, mid-ims-session enable, 6xx,\n");
                        hi2_remove_from_session_list(msg);
                        if(global_failure_responses)
                                    LM_INFO(">>>>>>>>>>>>>>>>>Create HI2 file \n ");
//                    LM_INFO(">>>>>>>>>>>>>>>>>Create HI2 file, msg:<%.*s> \n ", msg->len, msg->buf);
                        //todo... Create HI2 file and save it into current directory.
                        //todo...
                    }

                    //todo..., based on configuration, it could be send these messages to LEA. the default not to .send
                }

                LM_INFO("the target <%.*s> is terminating-Party.\n", turi.user.len, turi.user.s);
                LM_INFO ("sIPMessage:<%.*s>\n", msg->first_line.u.request.method.len,
                         msg->first_line.u.request.method.s);

                return 1;
            }
            list = list->next;
        }
        if (flag){     //if there is no liid in list.
            if(status_code == 200){
                if(reply_cseq_method_id == 1  || reply_cseq_method_id == 2048){      //reply of INVITE method and update method,
                            LM_INFO("This is SIP_Reply, mid-ims-session enable, no target, 200Ok,  INvite, Update\n");
                        hi2_update_to_session_list(msg, &list->hi1DataInfo, 2);         //update sipMessageHeaderAnswer and sdpAnswer

                } else if (reply_cseq_method_id == 8 || reply_cseq_method_id == 2) {  //reply of BYE or Cancel method,
                            LM_INFO("This is SIP_Reply, mid-ims-session enable, 200Ok,  BYE or Cancel\n");
                        hi2_remove_from_session_list(msg);
                }
            }else {   //other SIP_Reply except of 200 OK.
                        LM_INFO("This is SIP_Reply, mid-ims-session enable, no target, Other reply,\n");
                if(status_code >= 100 && status_code <= 199){
                            LM_INFO("This is SIP_Reply, mid-ims-session enable, 1xx,\n");
                            if(status_code == 183)      //session progress
                                    hi2_update_to_session_list(msg, &list->hi1DataInfo, 2);         //update sipMessageHeaderAnswer and sdpAnswer
//                                LM_INFO(">>>>>>>>>>>>>>>>>Create HI2 file \n ");
                } else if(status_code >= 300 && status_code <= 399){
                            LM_INFO("This is SIP_Reply, mid-ims-session enable, 3xx,\n");
//                            LM_INFO(">>>>>>>>>>>>>>>>>Create HI2 file \n ");
                } else if(status_code >= 400 && status_code <= 499){
                            LM_INFO("This is SIP_Reply, mid-ims-session enable, 4xx,\n");
//                    if(request_failure_responses)
//                                LM_INFO(">>>>>>>>>>>>>>>>>Create HI2 file \n ");
                    hi2_remove_from_session_list(msg);
                }else if(status_code >= 500 && status_code <= 599){
                            LM_INFO("This is SIP_Reply, mid-ims-session enable, 5xx,\n");
//                    if(server_failure_responses)
//                                LM_INFO(">>>>>>>>>>>>>>>>>Create HI2 file \n ");
                    hi2_remove_from_session_list(msg);
                }else if(status_code >= 600 && status_code <= 699){
                            LM_INFO("This is SIP_Reply, mid-ims-session enable, 6xx,\n");
//                    if(global_failure_responses)
//                                LM_INFO(">>>>>>>>>>>>>>>>>Create HI2 file \n ");
                    hi2_remove_from_session_list(msg);
                }
                //todo..., based on configuration, it could be send these messages to LEA. the default not to .send
            }

        }
        //nothing to send for LEA, returned.
        return 0;

    } else {
        tmp_session = hi2_session_activated(msg);
        if (tmp_session) {
            if (status_code == 200) {
                if (reply_cseq_method_id == 1 || reply_cseq_method_id == 2048) {      //reply of INVITE method and update method,
                            LM_INFO("This is SIP_Reply, mid-ims-session not to enable, 200Ok,  INvite, Update\n");
                    hi2_update_to_session_list(msg, &list->hi1DataInfo, 2);         //update sipMessageHeaderAnswer and sdpAnswer
                    if(successful_responses)
                                LM_INFO(">>>>>>>>>>>>>>>>>Create HI2 file \n ");
//                    LM_INFO(">>>>>>>>>>>>>>>>>Create HI2 file, msg:<%.*s> \n ", msg->len, msg->buf);
                    //todo... Create HI2 file and save it into current directory.

                } else if (reply_cseq_method_id == 8 || reply_cseq_method_id == 2) {  //reply of BYE or Cancel method,
                            LM_INFO("This is SIP_Reply, mid-ims-session not to  enable, 200Ok,  BYE or Cancel\n");
                    hi2_remove_from_session_list(msg);
                    if(successful_responses)
                                LM_INFO(">>>>>>>>>>>>>>>>>Create HI2 file \n ");
//                    LM_INFO(">>>>>>>>>>>>>>>>>Create HI2 file, msg:<%.*s> \n ", msg->len, msg->buf);
                    //todo... Create HI2 file and save it into current directory.
                }
            } else {   //other SIP_Reply except of 200 OK.
                        LM_INFO("This is SIP_Reply, mid-ims-session enable, Other reply,\n");

                if(status_code >= 100 && status_code <= 199){
                            LM_INFO("This is SIP_Reply, mid-ims-session enable, 1xx,\n");
                    if (status_code == 183)
                        hi2_update_to_session_list(msg, &list->hi1DataInfo, 2);         //update sipMessageHeaderAnswer and sdpAnswer
                    if(provisinal_responses || status_code == 180 || status_code == 183)
                                LM_INFO(">>>>>>>>>>>>>>>>>Create HI2 file \n ");
//                    LM_INFO(">>>>>>>>>>>>>>>>>Create HI2 file, msg:<%.*s> \n ", msg->len, msg->buf);

                    //todo... Create HI2 file and save it into current directory.
                    //todo...
                } else if(status_code >= 300 && status_code <= 399 && redirection_responses){
                            LM_INFO("This is SIP_Reply, mid-ims-session enable, 3xx,\n");
                            LM_INFO(">>>>>>>>>>>>>>>>>Create HI2 file \n ");
//                    LM_INFO(">>>>>>>>>>>>>>>>>Create HI2 file, msg:<%.*s> \n ", msg->len, msg->buf);
                    //todo... Create HI2 file and save it into current directory.
                    //todo...
                } else if(status_code >= 400 && status_code <= 499){
                            LM_INFO("This is SIP_Reply, mid-ims-session enable, 4xx,\n");
                    hi2_remove_from_session_list(msg);
                    if(request_failure_responses)
                                LM_INFO(">>>>>>>>>>>>>>>>>Create HI2 file \n ");
//                    LM_INFO(">>>>>>>>>>>>>>>>>Create HI2 file, msg:<%.*s> \n ", msg->len, msg->buf);
                    //todo... Create HI2 file and save it into current directory.
                    //todo...
                }else if(status_code >= 500 && status_code <= 599){
                            LM_INFO("This is SIP_Reply, mid-ims-session enable, 5xx,\n");
                    hi2_remove_from_session_list(msg);
                    if(server_failure_responses)
                                LM_INFO(">>>>>>>>>>>>>>>>>Create HI2 file \n ");
//                    LM_INFO(">>>>>>>>>>>>>>>>>Create HI2 file, msg:<%.*s> \n ", msg->len, msg->buf);
                    //todo... Create HI2 file and save it into current directory.
                    //todo...
                }else if(status_code >= 600 && status_code <= 699){
                            LM_INFO("This is SIP_Reply, mid-ims-session enable, 6xx,\n");
                    hi2_remove_from_session_list(msg);
                    if(global_failure_responses)
                                LM_INFO(">>>>>>>>>>>>>>>>>Create HI2 file \n ");
//                    LM_INFO(">>>>>>>>>>>>>>>>>Create HI2 file, msg:<%.*s> \n ", msg->len, msg->buf);
                    //todo... Create HI2 file and save it into current directory.
                    //todo...
                }

                //todo..., based on configuration, it could be send these messages to LEA. the default not to .send
            }

            return 1;
        } else {
            //nothing to send for LEA, returned.
            //todo... update some information in correlation list from B-leg party, if exist.
            //todo... send to LEA.
        }
        return 0;
    }

}

int hi2_iri_interception(struct sip_msg *msg, HI1_Listed_P_t *list){
    struct to_body *parsed_from, *parsed_to;
    struct cseq_body *parsed_cseg;
    struct sip_uri furi, turi;
    str callid;
    char correlation[32];
    int via_test;

    if (hi2_parse_from_header(msg) < 0) {
                LM_ERR("error while parsing from header ! \n");
        return -1;
    }

    if (hi2_parse_to_header(msg) < 0) {
                LM_ERR("error while parsing to header ! \n");
        return -1;
    }

//    if (hi2_parse_cseg_header(msg) < 0) {
//                LM_ERR("error while parsing cseg header ! \n");
//        return -1;
//    }

//    via_test = received_via_test(msg);

    //***************************************** Start Processing**************************************
    if (msg->first_line.type == SIP_REQUEST) {       // if the sipMessage is sip_Request.
//        if (via_test && msg->first_line.type == SIP_REQUEST) {       // if the sipMessage is sip_Request.
        if (hi2_message_is_initial_request(msg)) {    //  if the sipMessage is initial request of transaction.
                    LM_INFO("This is initial sIPMessage.\n");
                    hi2_iri_interception_based_sip_initial(msg, list);
                    //todo... send to LEA (option)
        } else {     // the sipMessage is sequential request of transaction.
                    LM_INFO ("This is sequential sIPMessage.\n");
                    hi2_iri_interception_based_sip_sequential(msg, list);
                    //todo... send to LEA (option)
        }

    }else if (msg->first_line.type == SIP_REPLY) {     //   if the sipMessage is sip_Reply.
//    }else if (!via_test && msg->first_line.type == SIP_REPLY) {     //   if the sipMessage is sip_Reply.
                   hi2_iri_interception_based_sip_reply(msg, list);
        //todo...
    }
    return 0;
}


//int hi2_iri_interception_old_2(struct sip_msg *msg, HI1_Listed_P_t *list) {
//    struct to_body *parsed_from, *parsed_to;
//    struct cseq_body *parsed_cseg;
//    struct sip_uri furi, turi;
//    str callid;
//    char correlation[32];
//
//    if (hi2_parse_from_header(msg) < 0) {
//                LM_ERR("error while parsing from header ! \n");
//        return -1;
//    }
//
//    if (hi2_parse_to_header(msg) < 0) {
//                LM_ERR("error while parsing to header ! \n");
//        return -1;
//    }
//
//    if (hi2_parse_cseg_header(msg) < 0) {
//                LM_ERR("error while parsing cseg header ! \n");
//        return -1;
//    }
//
//    parsed_from = (struct to_body *) msg->from->parsed;
//    if (parse_uri(parsed_from->uri.s, parsed_from->uri.len, &furi) || !furi.user.len) {
//                LM_ERR("unable to extract username from URI (From header)\n");
//        return -1;
//    }
//
//    parsed_to = (struct to_body *) msg->to->parsed;
//    if (parse_uri(parsed_to->uri.s, parsed_to->uri.len, &turi) || !turi.user.len) {
//                LM_ERR("unable to extract username to URI (To header)\n");
//        return -1;
//    }
//
//    parsed_cseg = (struct cseq_body *) msg->cseq->parsed;
//
//    if (hi2_message_is_initial_request(parsed_to)) {
//                LM_INFO("This is initial sIPMessage.\n");
//        hi2_add_to_session_list(msg);
//    } else
//                LM_INFO ("This is sequential sIPMessage.\n");
////            LM_INFO("This is sequential sIPMessage of <%.*s>.\n", parsed_cseg->method.len, parsed_cseg->method.s);
//
//
//    callid.s = msg->callid->name.s;
//    callid.len = msg->callid->len;
//    MD5StringArray(correlation, &callid, 1);
//
//    return 0;
//
//    struct HI2_Session_list *tmp = hi2_session_activated(correlation);
//    if (tmp) {
//        //todo...
//    } else
//        while (list) {
//            if (strncmp(list->hi1DataInfo.target_Information, furi.user.s, furi.user.len) ==
//                0) {     //The username from URI (From header) is target .
//                        LM_INFO("the target <%.*s> is originating-Party.\n", furi.user.len, furi.user.s);
//                //some data that is needed to generate hi2.
//                if (msg->first_line.type == SIP_REQUEST && hi2_message_is_initial_request(parsed_to)) {
//                    hi2_add_to_session_list(msg, list->hi1DataInfo.target_Information,
//                                            list->hi1DataInfo.lawfulInterceptionIdentifier);
//
//                            LM_INFO ("sIPMessage:<%.*s>\n", msg->first_line.u.request.method.len,
//                                     msg->first_line.u.request.method.s);
//                } else if (msg->first_line.type == SIP_REPLY)
//                            LM_INFO ("sIPMessage:%d <%.*s>\n", msg->first_line.u.reply.statuscode,
//                                     msg->first_line.u.reply.reason.len, msg->first_line.u.reply.reason.s);
//
//                        LM_INFO("liid:<%s>\n", list->hi1DataInfo.lawfulInterceptionIdentifier);
//                        LM_INFO("target:<%s>\n", list->hi1DataInfo.target_Information);
//                        LM_INFO("originating-Party:<%.*s>\n", furi.user.len, furi.user.s);
//                        LM_INFO("terminating-Party:<%.*s>\n", turi.user.len, turi.user.s);
//                        LM_INFO("gPRSCorrelationNumber: <%.*s>\n", 32, correlation);
//
//                return 1;
//            } else if (strncmp(list->hi1DataInfo.target_Information, turi.user.s, turi.user.len) ==
//                       0) {  //The username to URI (To header) is target .
//                        LM_INFO("the target <%.*s> is terminating-Party.\n", turi.user.len, turi.user.s);
//                //some data that is needed to generate hi2.
//                if (msg->first_line.type == SIP_REQUEST)
//                            LM_INFO ("sIPMessage:<%.*s>\n", msg->first_line.u.request.method.len,
//                                     msg->first_line.u.request.method.s);
//                else if (msg->first_line.type == SIP_REPLY)
//                            LM_INFO ("sIPMessage:%d <%.*s>\n", msg->first_line.u.reply.statuscode,
//                                     msg->first_line.u.reply.reason.len, msg->first_line.u.reply.reason.s);
//
//                        LM_INFO("liid:<%s>\n", list->hi1DataInfo.lawfulInterceptionIdentifier);
//                        LM_INFO("target:<%s>\n", list->hi1DataInfo.target_Information);
//                        LM_INFO("originating-Party:<%.*s>\n", furi.user.len, furi.user.s);
//                        LM_INFO("terminating-Party:<%.*s>\n", turi.user.len, turi.user.s);
//                        LM_INFO("gPRSCorrelationNumber: <%.*s>\n", 32, correlation);
//
//                return 1;
//
//            }
//            list = list->next;
//        }
//    return 0;
//}
//
//int hi2_iri_interception_old(struct sip_msg *msg, HI1_Listed_P_t *list) {
//    struct to_body *parsed_from, *parsed_to;
//    struct cseq_body *parsed_cseg;
//    struct sip_uri furi, turi;
//    str  callid;
//    char correlation[32];
//
//    if (hi2_parse_from_header(msg) < 0) {
//                LM_ERR("error while parsing from header ! \n");
//        return -1;
//    }
//
//    if (hi2_parse_to_header(msg) < 0) {
//                LM_ERR("error while parsing to header ! \n");
//        return -1;
//    }
//
//    if (hi2_parse_cseg_header(msg) < 0) {
//                LM_ERR("error while parsing cseg header ! \n");
//        return -1;
//    }
//
//    parsed_from = (struct to_body *) msg->from->parsed;
//    if (parse_uri(parsed_from->uri.s, parsed_from->uri.len, &furi) || !furi.user.len) {
//                LM_ERR("unable to extract username from URI (From header)\n");
//        return -1;
//    }
//
//    parsed_to = (struct to_body *) msg->to->parsed;
//    if (parse_uri(parsed_to->uri.s, parsed_to->uri.len, &turi) || !turi.user.len) {
//                LM_ERR("unable to extract username to URI (To header)\n");
//        return -1;
//    }
//
//    parsed_cseg = (struct cseq_body *)msg->cseq->parsed;
//
//    if (hi2_message_is_initial_request(parsed_to)) {
//                LM_INFO("This is initial sIPMessage.\n");
//        hi2_add_to_session_list(msg);
//    }else
//                LM_INFO("This is sequential sIPMessage.\n");
////            LM_INFO("This is sequential sIPMessage of <%.*s>.\n", parsed_cseg->method.len, parsed_cseg->method.s);
//
//
//    callid.s = msg->callid->name.s;
//    callid.len = msg->callid->len;
//    MD5StringArray(correlation, &callid, 1);
//
//    return 0;
//
//    while (list) {
//        if (strncmp(list->hi1DataInfo.target_Information, furi.user.s, furi.user.len) ==
//            0) {     //The username from URI (From header) is target .
//                    LM_INFO("the target <%.*s> is originating-Party.\n", furi.user.len, furi.user.s);
//            //some data that is needed to generate hi2.
//            if (msg->first_line.type == SIP_REQUEST)
//                        LM_INFO ("sIPMessage:<%.*s>\n", msg->first_line.u.request.method.len,
//                                 msg->first_line.u.request.method.s);
//            else if (msg->first_line.type == SIP_REPLY)
//                        LM_INFO ("sIPMessage:%d <%.*s>\n", msg->first_line.u.reply.statuscode,
//                                 msg->first_line.u.reply.reason.len, msg->first_line.u.reply.reason.s);
//
//                    LM_INFO("liid:<%s>\n", list->hi1DataInfo.lawfulInterceptionIdentifier);
//                    LM_INFO("target:<%s>\n", list->hi1DataInfo.target_Information);
//                    LM_INFO("originating-Party:<%.*s>\n", furi.user.len, furi.user.s);
//                    LM_INFO("terminating-Party:<%.*s>\n", turi.user.len, turi.user.s);
//                    LM_INFO("gPRSCorrelationNumber: <%.*s>\n", 32, correlation);
//
//            return 1;
//        } else if (strncmp(list->hi1DataInfo.target_Information, turi.user.s, turi.user.len) ==
//                   0) {  //The username to URI (To header) is target .
//                    LM_INFO("the target <%.*s> is terminating-Party.\n", turi.user.len, turi.user.s);
//            //some data that is needed to generate hi2.
//            if (msg->first_line.type == SIP_REQUEST)
//                        LM_INFO ("sIPMessage:<%.*s>\n", msg->first_line.u.request.method.len,
//                                 msg->first_line.u.request.method.s);
//            else if (msg->first_line.type == SIP_REPLY)
//                        LM_INFO ("sIPMessage:%d <%.*s>\n", msg->first_line.u.reply.statuscode,
//                                 msg->first_line.u.reply.reason.len, msg->first_line.u.reply.reason.s);
//
//                    LM_INFO("liid:<%s>\n", list->hi1DataInfo.lawfulInterceptionIdentifier);
//                    LM_INFO("target:<%s>\n", list->hi1DataInfo.target_Information);
//                    LM_INFO("originating-Party:<%.*s>\n", furi.user.len, furi.user.s);
//                    LM_INFO("terminating-Party:<%.*s>\n", turi.user.len, turi.user.s);
//                    LM_INFO("gPRSCorrelationNumber: <%.*s>\n", 32, correlation);
//
//            return 1;
//
//        }
//        list = list->next;
//    }
//    return 0;
//}

int hi2_destructor(){
    HI2_Session_list_t *first, *free;
    first = hi2_head_session_list->next;
    if (!first)
                LM_ERR("There is no active session in HiOPS-Service,\n");
    else
        while (first) {
            free = first;
            LM_INFO("the session with session(%s) is free from HiOPS-Service List by LEMF.\n", free->hi2Session.gprscorrelation);
            first = first->next;
            shm_free(free);
        }

    //delete hi1_head_listed_p->next as a pointer of list
    LM_INFO("The main head of session link is free from HiOPS-Service List by LEMF.\n");
    shm_free(hi2_head_session_list);
    shm_free(hi2_len_session_list);

    return 1;
}


/**
 * This function do some extera works while the packet from/to target. Also the hi2operation will do as fork process.
 * @return
 */
//int hi2_fork_process(){
//    int pid;
//    pid = fork();
//    if (pid < 0)
//        return -1; /* error */
//    if (pid == 0) {
//        /* child space, initialize the config framework */
//        sleep(10);
//                LM_INFO(">>>>>>>Here is place for hi2 fork process.>>>>>>>\n");
//        sleep(10);
//        /* Add some extra works here... */
////                return 0;
//        exit(0);
//    }
//    return 1;
//
//}