#ifndef  _MSG_TRANSLATOR_H
#define _MSG_TRANSLATOR_H

#include "msg_parser.h"

char * build_req_buf_from_sip_req  (struct sip_msg* msg, unsigned int *returned_len);

char * build_res_buf_from_sip_res(struct sip_msg* msg, unsigned int *returned_len);

char * build_res_buf_from_sip_req(unsigned int code , char *text , struct sip_msg* msg, unsigned int *returned_len);




#endif
