#ifndef _MF_FUNCS_H
#define _MF_FUNCS_H


#include "../../msg_parser.h"
#include "../../dprint.h"
#include "../../config.h"


int mf_startup();
int decrement_maxfed( struct sip_msg* msg );
int add_maxfwd_header( struct sip_msg* msg , unsigned int val );
int is_maxfwd_zero( struct sip_msg* msg );
int reply_to_maxfwd_zero( struct sip_msg* msg );

#endif

