/*
 * $Id$
 */

#ifndef error_h
#define error_h

#define E_UNSPEC      -1
#define E_OUT_OF_MEM  -2
#define E_BAD_RE      -3
/* #define E_BAD_ADDRESS -4 */
#define E_BUG         -5
#define E_CFG         -6
#define E_NO_SOCKET		-7
/* unresolveable topmost Via */
#define E_BAD_VIA		-8
/* incomplete transaction tupel */
#define E_BAD_TUPEL		-9
/* script programming error */
#define E_SCRIPT		-10
/* error in exceution of external tools */
#define E_EXEC			-11
/* too many branches demanded */
#define E_TOO_MANY_BRANCHES -12

#define E_SEND		  -477
/* unresolveable next-hop address */
#define E_BAD_ADDRESS -478
/* unparseable URI */
#define E_BAD_URI 	  -479
/* misformated request */
#define E_BAD_REQ	  -400

/* error in server */
#define E_BAD_SERVER	  -500


#define MAX_REASON_LEN	128

#include "str.h"

/* processing status of the last command */
extern int ser_error;
extern int prev_ser_error;

struct sip_msg;

/* ser error -> SIP error */
int err2reason_phrase( int ser_error, int *sip_error, 
                char *phrase, int etl, char *signature );

/* SIP error core -> SIP text */
char *error_text( int code );

/* return pkg_malloc-ed reply status in status->s */
void get_reply_status( str *status, struct sip_msg *reply, int code );

#endif
