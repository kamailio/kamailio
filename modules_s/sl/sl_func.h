/*
 * $Id$
 */

#ifndef _SL_FUNC_H
#define SL_FUNC_H

#include "../../parse_msg.h"


int st_startup();
int st_send_reply(struct sip_msg*,int,char*);
int st_filter_ACK();


#endif


