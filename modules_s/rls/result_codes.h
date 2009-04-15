#ifndef __RESULT_CODES_H
#define __RESULT_CODES_H

#include <xcap/xcap_result_codes.h>

/* result codes (non negative numbers) */
#define RES_NOT_FOUND			1

/* errors */
#define RES_PARSE_HEADERS_ERR	(-10)
#define RES_EXPIRATION_INTERVAL_TOO_SHORT	(-11)
#define RES_SUBSCRIPTION_TERMINATED	(-12)
#define RES_SUBSCRIPTION_REJECTED	(-13)

#endif
