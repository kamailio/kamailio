/*
 *  $Id$
 */



#ifndef config_h
#define config_h

#include "types.h"

/* default sip port if none specified */
#define SIP_PORT 5060

#define CFG_FILE "/etc/ser/ser.cfg"


/* maximum number of addresses on which we will listen */
#define MAX_LISTEN 16

/* default number of child processes started */
#define CHILD_NO    8

#define RT_NO 10 /* routing tables number */
#define REPLY_RT_NO 10 /* reply routing tables number */
#define DEFAULT_RT 0 /* default routing table */

#define MAX_REC_LEV 100 /* maximum number of recursive calls */
#define ROUTE_MAX_REC_LEV 10 /* maximum number of recursive calls
							   for route()*/

#define MAX_URI_SIZE 1024	/* used when rewriting URIs */

#define MY_VIA "Via: SIP/2.0/UDP "
#define MY_VIA_LEN 17

#define CONTENT_LEN "Content-Length: 0"
#define CONTENT_LEN_LEN 17

#define USER_AGENT "User-Agent: Sip EXpress router"\
		"(" VERSION " (" ARCH "/" OS"))"
#define USER_AGENT_LEN (sizeof(USER_AGENT)-1)

#define SERVER_HDR "Server: Sip EXpress router"\
		"(" VERSION " (" ARCH "/" OS"))"
#define SERVER_HDR_LEN (sizeof(SERVER_HDR)-1)

#define MAX_WARNING_LEN  256
		
#define MY_BRANCH ";branch="
#define MY_BRANCH_LEN 8


#define MAX_PORT_LEN 7 /* ':' + max 5 letters + \0 */
#define CRLF "\r\n"
#define CRLF_LEN 2

#define RECEIVED   ";received="
#define RECEIVED_LEN 10

#define SRV_PREFIX "_sip._udp."
#define SRV_PREFIX_LEN 10

/*used only if PKG_MALLOC is defined*/
#define PKG_MEM_POOL_SIZE 1024*1024

/*used if SH_MEM is defined*/
#define SHM_MEM_SIZE 128 

#define TIMER_TICK 1

/* dimensioning buckets in q_malloc */
/* size of the size2bucket table; everything beyond that asks for
   a variable-size kilo-bucket
 */
#define MAX_FIXED_BLOCK         3072
/* distance of kilo-buckets */
#define BLOCK_STEP                      512
/* maximum number of possible buckets */
#define MAX_BUCKET		15

/* receive buffer size -- preferably set low to
   avoid terror of excessively huge messages; they are
   useless anyway
*/
#define BUF_SIZE 3040

/* forwarding  -- Via buffer dimensioning */
#define MAX_VIA_LINE_SIZE	240
#define MAX_RECEIVED_SIZE	57

/* maximum number of branches per transaction */
#define MAX_BRANCHES    4

/* maximum length of a FIFO server command */
#define MAX_FIFO_COMMAND 512

/* buffer dimensions for FIFO server */
#define MAX_CONSUME_BUFFER 1024
/* where reply pipes may be opened */
#define FIFO_DIR "/tmp/"
/* max length of the text of fifo 'print' command */
#define MAX_PRINT_TEXT 256

/* maximum length of Contact heder field in redirection replies */
#define MAX_REDIRECTION_LEN 512
#endif
