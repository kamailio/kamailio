/*
 * $Id$
 */

#ifndef _SL_FUNCS_H
#define SL_FUNCS_H

#include "../../msg_parser.h"

#define SL_RPL_WAIT_TIME  2  // in sec

int sl_startup();
int sl_send_reply(struct sip_msg*,int,char*);
int sl_filter_ACK(struct sip_msg* );


#endif


