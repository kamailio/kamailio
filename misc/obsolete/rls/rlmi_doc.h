#ifndef __RLMI_DOCUMENT_H
#define __RLMI_DOCUMENT_H

#include "rl_subscription.h"

/** Creates new RLMI document from subscription information. It will 
 * be allocated in package memory. */
int create_rlmi_document(str *dst, str *content_type_dst, rl_subscription_t *s, int full_info);

#endif
