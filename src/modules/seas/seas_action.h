/* $Id$
 *
 * Copyright (C) 2006-2007 VozTelecom Sistemas S.L
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


#include "seas.h"/*as_p*/

#define REPLY_PROV 1
#define REPLY_FIN  2
#define REPLY_FIN_DLG  3
#define UAC_REQ  4
#define AC_RES_FAIL 5
#define SL_MSG  6
#define AC_CANCEL 7
#define JAIN_PONG 8


#define AC_FAIL_UNKNOWN 0x01

#define FAKED_REPLY_FLAG 0x02

struct as_uac_param{
   struct as_entry *who;
   int uac_id;
   unsigned int label;
   char processor_id;
   char destroy_cb_set;
   struct cell* inviteT;
};


/**
 * ACTION processing functions
 */
int ac_reply(as_p the_as,unsigned char processor_id,unsigned int flags,char *action,int len);
int ac_sl_msg(as_p the_as,unsigned char processor_id,unsigned int flags,char *action,int len);
int ac_uac_req(as_p the_as,unsigned char processor_id,unsigned int flags,char *action,int len);
int ac_encode_msg(as_p the_as,unsigned char processor_id,unsigned int flags,char *action,int len);
int ac_cancel(as_p the_as,unsigned char processor_id,unsigned int flags,char *action,int len);

/**
 * Utility functions
 */
int forward_sl_request(struct sip_msg *msg,str *uri,int proto);
int extract_allowed_headers(struct sip_msg *my_msg,int allow_vias,int allow_Rroutes,hdr_flags_t forbidden_hdrs,char *headers,int headers_len);

/**
 * Action Dispatcher process functions
 */
int dispatch_actions(void);
int process_action(as_p my_as);

/**
 * Callback Functions
 */
void uac_cb(struct cell* t, int type, struct tmcb_params*);
void uac_cleanup_cb(struct cell* t, int type, struct tmcb_params*);

/**
 * Event creating functions
 */
int as_action_fail_resp(int uac_id,int sip_error,char *err_buf,int err_len);
char* create_as_action_reply(struct cell *c,struct tmcb_params *params,int uac_id,char processor_id,int *evt_len);
