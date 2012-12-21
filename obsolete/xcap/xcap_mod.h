#ifndef __XCAP_MOD_H
#define __XCAP_MOD_H

#include <xcap/xcap_client.h>
#include "../../parser/msg_parser.h"

typedef int (*fill_xcap_params_func)(struct sip_msg *m, xcap_query_params_t *params);

#endif
