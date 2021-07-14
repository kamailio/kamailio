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



#ifndef _HI2OPS_H
#define _HI2OPS_H

#define HI2CHILDOPERATIONS PROC_NOCHLDINIT


#include "hi1ops.h"
#include "../../core/parser/parse_from.h"
#include "../../core/parser/msg_parser.h"
#include "../../core/parser/parse_uri.h"
#include "../../core/md5.h"
#include "../../core/md5utils.h"
#include "../../core/events.h"

#include "../htable/ht_api.h"
#include "../htable/ht_var.h"
#include "../htable/api.h"



typedef struct HI2Session {
    char	lawfulInterceptionIdentifier[25];
    char    target_Information[256];
    char    gprscorrelation[32];
    char    sipMessageHeaderOffer[256];
    char    sipMessageHeaderAnswer[256];
    char    sdpOffer[256];
    char    sdpAnswer[256];
    char    mediaSecFailureIndication[265];
    char    pANIHeaderInfo[265];
    char    totag[256];
    char    fromtag[256];
    unsigned int    counter;
}HI2Session_t;


typedef struct HI2_Session_list{
    HI2Session_t hi2Session;
    struct HI2_Session_list *next;
} HI2_Session_list_t;

extern HI2_Session_list_t *hi2_head_session_list;
extern int *hi2_len_session_list;

extern int mid_ims_session;
extern int provisinal_responses;
extern int successful_responses;
extern int redirection_responses;
extern int request_failure_responses;
extern int server_failure_responses;
extern int global_failure_responses;

extern htable_api_t htable_api;



int hi2_shm_memory();
int hi2_start_proccess();
int hi2_start_encapsulate();
int hi2_parse_from_header(struct sip_msg *msg);
int hi2_parse_to_header(struct sip_msg *msg);
int hi2_parse_cseg_header(struct sip_msg *msg);
int get_method_from_reply(struct sip_msg *reply);
int hi2_message_is_initial_request(struct sip_msg *msg);
//int hi2_message_is_initial_request(struct to_body *);
struct HI2_Session_list *hi2_session_activated(struct sip_msg *msg);
//struct HI2_Session_list *hi2_session_activated(char *correlation);
//int hi2_add_to_session_list(struct sip_msg *msg, struct HI1DataInfo *hi1DataInfo);
int hi2_add_to_session_list(struct sip_msg *msg, struct HI1DataInfo *hi1DataInfo, int methodtype);
//int hi2_update_to_session_list(struct sip_msg *msg, struct HI1DataInfo *hi1DataInfo);
int hi2_update_to_session_list(struct sip_msg *msg, struct HI1DataInfo *hi1DataInfo, int type);
int hi2_remove_from_session_list(struct sip_msg *msg);
//int hi2_add_to_session_list(struct sip_msg *msg, char *correlation, char *target, char *liid);
int hi2_iri_interception(struct sip_msg *msg, HI1_Listed_P_t *list);
int hi2_iri_interception_based_sip_initial(struct sip_msg *msg, HI1_Listed_P_t *list);
int hi2_iri_interception_based_sip_sequential(struct sip_msg *msg, HI1_Listed_P_t *list);
int hi2_iri_interception_based_sip_reply(struct sip_msg *msg, HI1_Listed_P_t *list);
int hi2_iri_sip_exist_in_htable(struct sip_msg *msg);


int hi2_destructor();



//int hi2_fork_process();
#endif //_HI2OPS_H
