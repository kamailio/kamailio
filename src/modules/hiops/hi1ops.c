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
#include "hi1ops.h"


unsigned int sock = 0;
HI1Socket_t *hi1Socket = NULL;
int worker_process = 0;
char *hi1SocketString = NULL;
HI1DataType_t hi1_data = BER;

char *hi1_eventrb = NULL;
int hi1_eventrb_no = DEFAULT_RT;
HI1EventType_t hi1_event = ACTIVATED;

asn_dec_rval_t rval;
int *hi1_len_listed_p = NULL;
HI1_Listed_P_t *hi1_head_listed_p = NULL;

HI1_Operation_t *hi1_operation = NULL;
Notification_t *notification = NULL;
OBJECT_IDENTIFIER_t *domainid = NULL;
LawfulInterceptionIdentifier_t *lawfulInterceptionIdentifier = NULL;
CommunicationIdentifier_t *communicationIdentifier = NULL;
TimeStamp_t *timeStamp = NULL;
National_HI1_ASN1parameters_t *national_hi1_asn1parameters = NULL;
Target_Information_t *target_information = NULL;




//gen_lock_t *lock;

/**
 *
 * This function is called from mod_init function, When the module is loaded to allocate shared memory for all foked process.
 * @return success:1, error:-1
 */
int hi1_shm_memory(){
    hi1_len_listed_p = shm_malloc(sizeof(int));     //Shared variable between all fork process.This is lenght of linked list.
    if (hi1_len_listed_p == NULL) {
                LM_ERR("cannot allocate shm memory hi1_len_listed_p in hi1_constructor\n");
        return -1;
    }
    *hi1_len_listed_p = 0;          //default value for lenght of linked list.

    hi1_head_listed_p = shm_malloc(sizeof(HI1_Listed_P_t));     //Shared varibale,This is pointer of head of linked list.
    if (hi1_head_listed_p == NULL) {
                LM_ERR("cannot allocate shm memory hi1_head_listed_p in hi1_constructor\n");
        return -1;
    }

    hi1_head_listed_p->hi1DataInfo.lawfulInterceptionIdentifier[0] = '\0';
    hi1_head_listed_p->hi1DataInfo.target_Information[0] = '\0';
    hi1_head_listed_p->next = NULL; //default value of first node
    return 1;
}

//int hi1_constructor(){
//
//    //default value for all varibales.
//    sock = 0;
//    hi1Socket = NULL;
////    worker_process = shm_malloc(sizeof(int));
////    if (worker_process == NULL) {
////                LM_ERR("cannot allocate shm memory worker_process in hi1_constructor\n");
////        return -1;
////    }
//
//    worker_process = 0;
////    hi1_worker_process = 0;
//
//    hi1SocketString = NULL;
//    hi1_data = BER;
//
//
//
//    hi1_eventrb = NULL;    /* default is the main route block*/
//    hi1_eventrb_no = DEFAULT_RT;  /* default number of the main route block*/
//    hi1_event = ACTIVATED;     /*default*/
//
////    asn_dec_rval_t rval;
////    hi1_head_listed_p = NULL;
////    int *hi1_len_listed_p = NULL;
//    hi1_operation = NULL;
//    notification = NULL;
//    domainid = NULL;
//    lawfulInterceptionIdentifier = NULL;
//    communicationIdentifier = NULL;
//    timeStamp = NULL;
//    national_hi1_asn1parameters = NULL;
//    target_information = NULL;
//
//
//    hi1_len_listed_p = shm_malloc(sizeof(int));     //Shared variable between all fork process.This is lenght of linked list.
//    if (hi1_len_listed_p == NULL) {
//                LM_ERR("cannot allocate shm memory hi1_len_listed_p in hi1_constructor\n");
//        return -1;
//    }
//    *hi1_len_listed_p = 0;          //default value for lenght of linked list.
//
//    hi1_head_listed_p = shm_malloc(sizeof(HI1_Listed_P_t));     //Shared varibale,This is pointer of head of linked list.
//    if (hi1_head_listed_p == NULL) {
//                LM_ERR("cannot allocate shm memory hi1_head_listed_p in hi1_constructor\n");
//        return -1;
//    }
//
////    hi1_head_listed_p->hi1_operation = NULL;    //default value of first node
////    hi1_head_listed_p->liid = NULL;
////    hi1_head_listed_p->len = 0;
////    hi1_head_listed_p->liid = NULL;
//    hi1_head_listed_p->next = NULL; //default value of first node
//    return 1;
//}

/**
 *
 * @param type of val param
 * @param val is from kamailio.cfg file. like this. tcp:127.0.0.1:4683
 * @return success:1, error:-1
 */
int hi1_sock_set_param(modparam_t type, void* val) {

    char *p;
    int len;

//    //default value for all varibales.It is called when the module is loaded.
//    hi1_constructor();

    if ((type & PARAM_STRING)==0){
        LOG(L_CRIT, "BUG: ctl: add_hi1_socket: bad parameter type %d\n",
            type);
        goto error;
    }

    if(hi1Socket == NULL) {
        hi1Socket = pkg_malloc(sizeof(HI1Socket_t));
        if (!hi1Socket) {
            LM_ERR("no pkg memory left to allocate hi1Socket in hi1_sock_set_param\n");
            return -1;
        }
    }

    len = strlen((char *)val);

    if(hi1SocketString == NULL) {
        hi1SocketString = pkg_malloc((len + 1) * sizeof(char));
        if (!hi1SocketString) {
                LM_ERR("no pkg memory left to allocate hi1SocketString in hi1_sock_set_param\n");
            return -1;
        }
        memcpy(hi1SocketString, (char *)val, len);
    }

    p = hi1SocketString;
    hi1Socket->proto = strtok_r(p, ":", &p);
    hi1Socket->ipaddress = strtok_r(p, ":", &p);
    hi1Socket->port = atoi(strtok_r(p, ":", &p));

    return 1;
/*
//    LM_ERR("MESPIO===========================================================");                      //deleted
//    LM_ERR("<mespio>===========================================,poroto:%s \n", hi1Socket->proto);     //deleted
//    LM_ERR("<mespio>===========================================,name:%s \n", hi1Socket->ipaddress);   //deleted
//    LM_ERR("<mespio>===========================================,port:%d \n", hi1Socket->port);        //deleted
*/
error:
    return -1;
}

/**
 * This function is called by hiops to create hi1 socket and listen on it.
 * @return success:1, error:-1
 */
int hi1_socket_open() {
    //create new socket here.
    if(!sock) {
                LM_INFO("create new socket for hi1 port listening in hi1_socket_open\n");
        socket_open(hi1Socket->proto, hi1Socket->ipaddress, hi1Socket->port, &sock);
        return 1;
    }

    LM_ERR("The socket is allocated before, You could not allocate it again in hi1_socket_open\n");
    return -1;
}

/**
 * This function is called by hiops to close hi1 socket.
 * @return success:1, error:-1
 */
//int hi1_socket_close() {
//    pkg_free(hi1SocketString);
//    pkg_free(hi1Socket);
//    socket_close(&sock);
//    return 1;
//}

/**
 * This function is called by hiops as fork process to listen on socket.
 * The HIM parameters are recieved by this socket.
 * @return success:1, error:-1
 */
int hi1_socket_listen() {
    unsigned int new_sock;
    int n, res;
    char welcome[] = "Welcome to HI Manager v.0.1(Beta).\r\nPlease type command here:";

    while (1) {
        /* do accept */
        new_sock = socket_listen(sock);
        LM_DBG("LEMF is connected succesfully, Ready to recieve command to run.\n");   //deleted

        for (;;) {
            n = socket_send(new_sock, welcome);
            if (n < 0) {
                LM_ERR ("ERROR to write Welcome buffer to LEMF, Try again\n");
                socket_close(&new_sock);
                break;
            }
            n = socket_recv(new_sock);
            if (n <= 0) {
                LM_ERR ("ERROR while reading from LEMF socket, Please try again\n");
                socket_close(&new_sock);
                break;
            }else {
                res = hi1_do_analyzer(new_sock);
                if (res == -1) {
                    socket_close(&new_sock);
                    break;
                }
            }
        }
    }
    return 0;
}


/**
 * This function analyze the buffer message and do some works based on it.
 * @return -1: it means bye message is recieved.The connections should be close now.
 *          1: it means Continue conection working.
 */
int hi1_do_analyzer(int s) {
    if ((strcmp(buffer, "bye") == 0) || (strcmp(buffer, "bye\r\n") == 0)) {
        LM_ERR("close current sock.\n");
        return -1;
    } else if (strcmp(buffer, "activated") == 0 || (strcmp(buffer, "activated\r\n") == 0)) {
        LM_INFO("here is activated functions\n");
        if (hi1_do_liactivated(s) == -1){
            LM_ERR("Error While reading new buffer in activated functions\n");
            //do contineue again.
//            return 1;
        }

    } else if ((strcmp(buffer, "deactivated") == 0) || (strcmp(buffer, "deactivated\r\n") == 0)) {
        LM_INFO("here is deactivated functions\n");
        if (hi1_do_lideactivated(s) == -1){
                    LM_ERR("Error While reading new buffer in deactivated functions\n");
            //do contineue again.
//            return 1;
        }

        //todo...
    } else if ((strcmp(buffer, "modified") == 0) || (strcmp(buffer, "modified\r\n") == 0)) {
                LM_INFO("here is modified functions\n");
//        LM_INFO("======================================Step1\n");
        if (hi1_do_limodified(s) == -1){
                    LM_ERR("Error While reading new buffer in deactivated functions\n");
            //do contineue again.
//            return 1;
        }

        //todo...
    }else if ((strcmp(buffer, "count") == 0) || (strcmp(buffer, "count\r\n") == 0)) {
        LM_INFO("here is count functions\n");
        LM_INFO("Len of listed_p is: %d\n",*hi1_len_listed_p);
        LM_INFO(">>>>.Address of head is: ,%p\n",hi1_head_listed_p);


        hi1_operation = NULL;
        HI1_Listed_P_t *tmp = hi1_head_listed_p->next;
        if (!tmp)
            LM_DBG("There is no active liid in hiops service,\n");
        else
            while (tmp) {
                        LM_INFO("the record:  liid(%s) with terget information(%s) is activated in HiOPS-Service List by LEMF.\n", tmp->hi1DataInfo.lawfulInterceptionIdentifier, tmp->hi1DataInfo.target_Information);
                tmp = tmp->next;
            }

//        return 1;
        //todo...
    }
//    LM_ERR("buffer is:%s\r\n", buffer);
    return 1;
}

/**
 * This is add new hi1ops to list
 * @param bufsize is new packet
 * @return 1:success, -1:error
 */
int hi1_do_liactivated(int s){
    //todo...

    ///* Decode the input buffer as Rectangle type */
    LM_DBG("hi1ops socekt is waiting to recieve new hi1 packet from LEMF.\r\n");
    int n = socket_recv(s);
    if (n <= 0) {
        LM_ERR ("ERROR while reading from socket in hi1_do_liactivated function\n");
        return -1;
    }

    hi1_operation = NULL;
    rval = ber_decode(0, &asn_DEF_HI1_Operation, (void **) &hi1_operation, buffer, n);
    if (rval.code != RC_OK) {
        LM_ERR("Error While reading buffer in hi1_do_liactivated function\n");
        return -1;
    }

    /* Print the decoded Rectangle type as XML */
//        xer_fprint(stdout, &asn_DEF_HI1_Operation, hi1_operation);
//    xer_fprint(stdout, &asn_DEF_HI1_Operation, hi1_operation);
//    LM_INFO("New hi1 Packet is recieved successfully. add to list now.\n");

//    return  (hi1_add_to_listed_p(hi1_operation) == -1);
    return  hi1_add_to_listed_p(hi1_operation);
}

int hi1_do_lideactivated(int s){
    //todo...

    ///* Decode the input buffer as Rectangle type */
    LM_DBG("hi1ops socekt is waiting to recieve new hi1 packet from LEMF.\r\n");
    int n = socket_recv(s);
    if (n <= 0) {
                LM_ERR ("ERROR while reading from socket in hi1_do_lideactivated function\n");
        return -1;
    }

    hi1_operation = NULL;
    rval = ber_decode(0, &asn_DEF_HI1_Operation, (void **) &hi1_operation, buffer, n);
    if (rval.code != RC_OK) {
                LM_ERR("Error While reading buffer in hi1_do_lideactivated function\n");
        return -1;
    }

    /* Print the decoded Rectangle type as XML */
//        xer_fprint(stdout, &asn_DEF_HI1_Operation, hi1_operation);
//    xer_fprint(stdout, &asn_DEF_HI1_Operation, hi1_operation);
//            LM_INFO("New hi1 Packet is recieved successfully. add to list now.\n");

//    return  (hi1_add_to_listed_p(hi1_operation) == -1);

    return  hi1_delete_from_listed_p(hi1_operation);
}

int hi1_do_limodified(int s){
    //todo...
//            LM_INFO("======================================Step2\n");
    ///* Decode the input buffer as Rectangle type */
            LM_DBG("hi1ops socekt is waiting to recieve new hi1 packet from LEMF.\r\n");
    int n = socket_recv(s);
    if (n <= 0) {
                LM_ERR ("ERROR while reading from socket in hi1_do_limodified function\n");
        return -1;
    }

    hi1_operation = NULL;
    rval = ber_decode(0, &asn_DEF_HI1_Operation, (void **) &hi1_operation, buffer, n);
    if (rval.code != RC_OK) {
                LM_ERR("Error While reading buffer in hi1_do_limodified function\n");
        return -1;
    }

    /* Print the decoded Rectangle type as XML */
//        xer_fprint(stdout, &asn_DEF_HI1_Operation, hi1_operation);
//    xer_fprint(stdout, &asn_DEF_HI1_Operation, hi1_operation);
//            LM_INFO("New hi1 Packet is recieved successfully. add to list now.\n");

//    return  (hi1_add_to_listed_p(hi1_operation) == -1);
    return  hi1_modified_from_listed_p(hi1_operation);
}

/**
 * This function add new hi1ops packet to hi1_listed_p.
 * @param hi1 is new packet that should be added to hi1_listed_p
 * @return 1: success, -1:error
 */

int hi1_add_to_listed_p(HI1_Operation_t *hi1){

//    int ret;
//
//    struct run_act_ctx ra_ctx;
//    struct sip_msg *msg;        //should be solved. Not impelemented yet.
    HI1_Listed_P_t *tmp_list;

    tmp_list = shm_malloc(sizeof(HI1_Listed_P_t));
    if (tmp_list == NULL){
                LM_ERR("cannot allocate shm memory for tmp_list in hi1_add_to_listed_p function\n");
        return -1;
    }

//    strncpy(tmp_list->hi1DataInfo.lawfulInterceptionIdentifier, (char *)hi1->choice.liActivated.lawfulInterceptionIdentifier.buf, hi1->choice.liActivated.lawfulInterceptionIdentifier.size);
//    strncpy(tmp_list->hi1DataInfo.target_Information, (char *)hi1->choice.liActivated.target_Information->buf, hi1->choice.liActivated.target_Information->size);

    strncpy(tmp_list->hi1DataInfo.lawfulInterceptionIdentifier, (char *)hi1->choice.liActivated.lawfulInterceptionIdentifier.buf, hi1->choice.liActivated.lawfulInterceptionIdentifier.size);
    strncpy(tmp_list->hi1DataInfo.target_Information, "+981", strlen("+981"));


//    LM_INFO(">>>>.address of new tmp node is: %p\n",tmp_list);
//    LM_INFO(">>>>.Address of head is: %p\n",hi1_head_listed_p);

    if (*hi1_len_listed_p == 0){                //This is first hi1 packet
        LM_INFO("This is first hi1 packet to insert to linked list\n");

        tmp_list->next = NULL;
        hi1_head_listed_p->next = tmp_list;
        *hi1_len_listed_p = *hi1_len_listed_p + 1;

//    } else if (*hi1_len_listed_p > 0){
    } else {
        LM_INFO("This is second or more hi1 packet to insert to  front list\n");
        tmp_list->next = hi1_head_listed_p->next;
        hi1_head_listed_p->next = tmp_list;
        *hi1_len_listed_p = *hi1_len_listed_p + 1;

    }

    LM_INFO("New liid(%s) with terget information(%s) is added to HiOPS-Service List by LEMF.\n", tmp_list->hi1DataInfo.lawfulInterceptionIdentifier, tmp_list->hi1DataInfo.target_Information);
//            LM_INFO("New liid(%d) is added to HiOPS-Service List by LEMF.\n", hi1->choice.liActivated.lawfulInterceptionIdentifier.buf);
//
    /* exec routing script */
    if(hi1_eventrb_no != DEFAULT_RT) {
        hi1_event = ACTIVATED;
        return hi1_run_eventrb();
    }
    return 1;
}

/**
 * This function delete specific hi1ops packet from hi1_listed_p.
 * @param hi1 is new packet that should be added to hi1_listed_p
 * @return 1: success, -1:error
 */
int hi1_delete_from_listed_p(HI1_Operation_t *hi1){
    HI1_Listed_P_t *del, *tmp;

    tmp = hi1_head_listed_p;
    del = tmp->next;

    while (del){
        if (strcmp(del->hi1DataInfo.lawfulInterceptionIdentifier, (char *)hi1->choice.liDeactivated.lawfulInterceptionIdentifier.buf) == 0) //This node should be deleted.
//        if (strcmp(del->hi1DataInfo.lawfulInterceptionIdentifier, "111") == 0) //This node should be deleted.
        {
            tmp->next = del->next;
            shm_free(del);
            *hi1_len_listed_p = *hi1_len_listed_p - 1;

                    LM_INFO("the record with liid(%s) and terget information(%s) is deleted from HiOPS-Service List by LEMF.\n", del->hi1DataInfo.lawfulInterceptionIdentifier, del->hi1DataInfo.target_Information);

//            return 1;
//            break;
            goto end;
        }
        tmp = tmp->next;
        del = tmp->next;
    }
            LM_INFO("the record with liid(%s) and terget information(%s) is not found in HiOPS-Service List by LEMF.\n", (char *)hi1->choice.liDeactivated.lawfulInterceptionIdentifier.buf, (char *)hi1->choice.liDeactivated.target_Information->buf);

end:
    /* exec routing script */
    if(hi1_eventrb_no != DEFAULT_RT) {
        hi1_event = DEACTIVATED;
        return hi1_run_eventrb();
    }

    return 1;
}

/**
 * This code is modified just tarrget, if you want to modify liid, You should run lideactive function, and add new liid.
 * @param hi1
 * @return
 */
int hi1_modified_from_listed_p(HI1_Operation_t *hi1){
    HI1_Listed_P_t *tmp;

    tmp = hi1_head_listed_p->next;
    while (tmp){
//                LM_INFO("======================================Step5\n");
        if (strcmp(tmp->hi1DataInfo.lawfulInterceptionIdentifier, (char *)hi1->choice.liDeactivated.lawfulInterceptionIdentifier.buf) == 0) //This node should be deleted.
//        if (strcmp(del->hi1DataInfo.lawfulInterceptionIdentifier, "111") == 0) //This node should be deleted.
        {
            LM_INFO("the record with liid(%s) is modified, Terget(%s) is modified to(%s) in HiOPS-Service List by LEMF.\n", tmp->hi1DataInfo.lawfulInterceptionIdentifier, tmp->hi1DataInfo.target_Information,
                    (char *)hi1->choice.liModified.target_Information->buf);

            bzero(tmp->hi1DataInfo.target_Information, 256);
//            strncpy(tmp_list->hi1DataInfo.lawfulInterceptionIdentifier, (char *)hi1->choice.liActivated.lawfulInterceptionIdentifier.buf, hi1->choice.liActivated.lawfulInterceptionIdentifier.size);
            strncpy(tmp->hi1DataInfo.target_Information, (char *)hi1->choice.liActivated.target_Information->buf, hi1->choice.liActivated.target_Information->size);

//            return 1;
//            break;
            goto end;
        }
        tmp = tmp->next;
    }
    LM_INFO("the record with liid(%s) and terget information(%s) is not found in HiOPS-Service List by LEMF.\n", (char *)hi1->choice.liDeactivated.lawfulInterceptionIdentifier.buf, (char *)hi1->choice.liDeactivated.target_Information->buf);

end:
    /* exec routing script */
    if(hi1_eventrb_no != DEFAULT_RT) {
        hi1_event = MODIFIED;
        return hi1_run_eventrb();
    }
    return 1;
}



int hi1_run_eventrb(){
    int ret;
    struct run_act_ctx ra_ctx;
    struct sip_msg *msg;        //should be solved. Not impelemented yet.


    init_run_actions_ctx(&ra_ctx);

    if (run_actions(&ra_ctx, main_rt.rlist[hi1_eventrb_no], msg) < 0) {
        ret = -1;
                LM_DBG("Error while trying run script in route block\n");
        return ret;
    }
    return 1;
}

/**
 * This function is initiate the route block that is called after hi1_do_analyzer function.
 * @return 1: success, -1:error
 */
int hi1_eventrb_init(){
    int route_no;

    if(hi1_eventrb){
        route_no = route_get(&main_rt, hi1_eventrb);
        if (route_no == -1){
            LM_ERR("Failed to hi1 route block number of \"%s\", route_get() function is failed\n", hi1_eventrb);
            return -1;
        }
        if (main_rt.rlist[route_no]==0){
            LM_WARN("The hi1 route block \"%s\" is empty, doesn't exist\n",hi1_eventrb);
            return -1;
        }
        LM_DBG("hi1_eventrb is set successfully to %s route block\n", hi1_eventrb);
        hi1_eventrb_no = route_no;
    }
    return 1;
}

/**
 * This function read liid in current request in route block. $liid
 * @param msg
 * @param param
 * @param res
 * @return
 */
int hi1_pv_get_liid(struct sip_msg *msg, pv_param_t *param, pv_value_t *res) {

    str s_liid;

//    if (hi1_operation->present == HI1_Operation_PR_liActivated)
//    else if (hi1_operation->present == HI1_Operation_PR_liDeactivated)
//        str1.s = (char *)hi1_operation->choice.liDeactivated.lawfulInterceptionIdentifier.buf;
//    else if (hi1_operation->present == HI1_Operation_PR_liModified)
//        str1.s = (char *)hi1_operation->choice.liModified.lawfulInterceptionIdentifier.buf;
    if(hi1_operation->choice.liActivated.lawfulInterceptionIdentifier.size == 0)
        return 0;

    s_liid.s = (char *)hi1_operation->choice.liActivated.lawfulInterceptionIdentifier.buf;
    s_liid.len = strlen(s_liid.s);
    return pv_get_strval(msg, param, res, &s_liid);
}

/**
 * * This function read target in current request in route block. $terget
 * @param msg
 * @param param
 * @param res
 * @return
 */
int hi1_pv_get_target(struct sip_msg *msg, pv_param_t *param, pv_value_t *res){

    str s_target;
//    if (hi1_operation->present == HI1_Operation_PR_liActivated)
//    else if (hi1_operation->present == HI1_Operation_PR_liDeactivated)
//        str1.s = (char *)hi1_operation->choice.liDeactivated.target_Information->buf;
//    else if (hi1_operation->present == HI1_Operation_PR_liModified)
//        str1.s = (char *)hi1_operation->choice.liModified.target_Information->buf;
    if(hi1_operation->choice.liActivated.target_Information == 0)
        return 0;

    s_target.s = (char *)hi1_operation->choice.liActivated.target_Information->buf;
    s_target.len = strlen(s_target.s);
    return pv_get_strval(msg, param, res, &s_target);
}

/**
 * This function return type of event (astivated, deactivated, modified) for subject interception.
 * In cfg file, this parameter is available by $event
 * @param msg
 * @param param
 * @param res
 * @return
 */
int hi1_pv_get_event(struct sip_msg *msg, pv_param_t *param, pv_value_t *res){
    str et;
    switch (hi1_event){
        case ACTIVATED:
            strcpy(et.s, "activated");
            break;
        case DEACTIVATED:
            strcpy(et.s, "deactivated");
            break;
        case MODIFIED:
            strcpy(et.s, "modified");
            break;
    }
    et.len = strlen(et.s);
    return pv_get_strval(msg, param, res, &et);
}


/**
 * This function is called when the module is unloaded. When Kamailio service is stoped.
 * @return
 */
int hi1_destructor(){
    HI1_Listed_P_t *first, *free;
    first = hi1_head_listed_p->next;
    if (!first)
            LM_ERR("There is no active liid in hiops service,\n");
    else
        while (first) {
            free = first;
            LM_INFO("the record with liid(%s) and terget information(%s) is free from HiOPS-Service List by LEMF.\n", free->hi1DataInfo.lawfulInterceptionIdentifier, free->hi1DataInfo.target_Information);
            first = first->next;
            shm_free(free);
        }

    //delete hi1_head_listed_p->next as a pointer of list
    LM_INFO("The main head of listed link is free from HiOPS-Service List by LEMF.\n");
    shm_free(hi1_head_listed_p);
    shm_free(hi1_len_listed_p);

//    shm_free(worker_process);
    pkg_free(hi1SocketString);
    pkg_free(hi1Socket);
    socket_close(&sock);

    return 1;
}



