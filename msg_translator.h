#ifndef  _MSG_TRANSLATOR_H
#define _MSG_TRANSLATOR_H

#include "msg_parser.h"

char * build_buf_from_sip_request(struct sip_msg* msg, unsigned int *returned_len);





#endif
