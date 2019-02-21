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

#ifndef _HI1OPS_H
#define _HI1OPS_H

#include "../../core/mod_fix.h"
#include "../../core/mem/shm_mem.h"
#include "HI1-Operation.h"
#include "tcp_socket.h"

#define HI1_SOCKET_DEFAULT "tcp:127.0.0.1:4683"

typedef enum { BER=0, XML, JSON } HI1DataType_t;
typedef enum { ACTIVATED=0, DEACTIVATED, MODIFIED } HI1EventType_t;

typedef OCTET_STRING_t Target_Information_t;


/* declaration parameters for hi1*/
typedef struct HI1Socket {
    char *proto;
    char *ipaddress;
    unsigned int port;
} HI1Socket_t;

typedef struct HI1DataInfo {
    char	lawfulInterceptionIdentifier[25];
    char    target_Information[256];
} HI1DataInfo_t;

typedef struct HI1_Listed_P{
    HI1DataInfo_t hi1DataInfo;
    struct HI1_Listed_P *next;
} HI1_Listed_P_t;

extern unsigned int sock;
extern HI1Socket_t *hi1Socket;
extern int worker_process;
extern char *hi1SocketString;
extern HI1DataType_t hi1_data;


extern asn_dec_rval_t rval; /* Decoder return value */

extern HI1_Listed_P_t *hi1_head_listed_p ;
extern int *hi1_len_listed_p;

extern char *hi1_eventrb;    /* default is the main route block*/
extern int hi1_eventrb_no;  /* default number of the main route block*/
extern HI1EventType_t hi1_event;  /*activated, deac*/


extern HI1_Operation_t *hi1_operation;
extern Notification_t *notification;
extern OBJECT_IDENTIFIER_t *domainid;
extern LawfulInterceptionIdentifier_t *lawfulInterceptionIdentifier;
extern CommunicationIdentifier_t *communicationIdentifier;
extern TimeStamp_t *timeStamp;
extern National_HI1_ASN1parameters_t *national_hi1_asn1parameters;
extern Target_Information_t *target_information;

int hi1_shm_memory();
int hi1_sock_set_param(modparam_t type, void* val);
int hi1_socket_open();
//int hi1_socket_close();
int hi1_socket_listen();
int hi1_do_analyzer(int s);
int hi1_do_liactivated(int s);
int hi1_do_lideactivated(int s);
int hi1_do_limodified(int s);
int hi1_add_to_listed_p(HI1_Operation_t *hi1);
int hi1_delete_from_listed_p(HI1_Operation_t *hi1);
int hi1_modified_from_listed_p(HI1_Operation_t *hi1);

int hi1_run_eventrb();
int hi1_eventrb_init();
int hi1_pv_get_liid(struct sip_msg *msg, pv_param_t *param, pv_value_t *res);
int hi1_pv_get_target(struct sip_msg *msg, pv_param_t *param, pv_value_t *res);
int hi1_pv_get_event(struct sip_msg *msg, pv_param_t *param, pv_value_t *res);
int hi1_destructor();


#endif  //_HI1OPS_H
