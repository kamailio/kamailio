#ifndef __XCAP_PARAMS_H
#define __XCAP_PARAMS_H

#include <xcap/xcap_client.h>
#include "str.h"

int fill_xcap_params_impl(struct sip_msg *m, xcap_query_params_t *params);
int set_xcap_root(struct sip_msg* m, char *value, char* _x);
int set_xcap_filename(struct sip_msg* m, char *value, char* _x);
extern str default_xcap_root;

#endif
