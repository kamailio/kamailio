/*
 * $Id$
 *
 * Common function needed by authorize
 * and challenge related functions
 */

#ifndef COMMON_H
#define COMMON_H

#include "../../parser/msg_parser.h"


/*
 * Send a response
 */
int send_resp(struct sip_msg* _m, int _code, char* _reason, char* _hdr, int _hdr_len);


#endif /* COMMON_H */
