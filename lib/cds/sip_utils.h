#ifndef __COMMON_SIP_UTILS_H
#define __COMMON_SIP_UTILS_H

#ifdef SER /* SER only helper routines */

#include <parser/msg_parser.h>

/* returns negative value on error, positive when message contains 
 * no Expires header and 0 if everything ok */
int get_expiration_value(struct sip_msg *m, int *value);

/* returns 1 if the message has Subscription-Status: terminated (hack!) */
int is_terminating_notify(struct sip_msg *m);

#endif

#endif
