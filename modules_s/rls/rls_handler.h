#ifndef __RLS_HANDLER_H
#define __RLS_HANDLER_H

#include "../../parser/msg_parser.h"

int handle_rls_subscription(struct sip_msg* _m, const char *xcap_server, char *send_bad_resp);

#endif
