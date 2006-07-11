#ifndef __COMMON_SIP_UTILS_H
#define __COMMON_SIP_UTILS_H

#ifdef SER /* SER only helper routines */

#include <parser/msg_parser.h>

/* returns negative value on error, positive when message contains 
 * no Expires header and 0 if everything ok */
int get_expiration_value(struct sip_msg *m, int *value);

/* returns 1 if the message has Subscription-Status: terminated (hack!) */
int is_terminating_notify(struct sip_msg *m);

/* returns 1 if given extension is in Supported headers, 
 * 0 if not or an error occured while parsing */
int supports_extension(struct sip_msg *m, str *extension);

/* returns 1 if given extension is in Require headers, 
 * 0 if not or an error occured while parsing */
int requires_extension(struct sip_msg *m, str *extension);

/**
 * Verifies presence of the To-tag in message. Returns 1 if
 * the tag is present, 0 if not, -1 on error.
 */
int has_to_tag(struct sip_msg *_m);

#endif

#endif
