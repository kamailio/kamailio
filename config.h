/*
 *  $Id $
 */



#ifndef config_h
#define config_h

/* default sip port if none specified */
#define SIP_PORT 5060

#define CFG_FILE "./ser.cfg"


/* maximum number of addresses on which we will listen */
#define MAX_LISTEN 16

/* default number of child processes started */
#define CHILD_NO    8

#define RT_NO 10 /* routing tables number */
#define DEFAULT_RT 0 /* default routing table */

#define MAX_REC_LEV 100 /* maximum number of recursive calls */
#define ROUTE_MAX_REC_LEV 10 /* maximum number of recursive calls
							   for route()*/

#define MAX_URI_SIZE 1024	/* used when rewriting URIs */

#define MY_VIA "Via: SIP/2.0/UDP "
#define MY_VIA_LEN 17

#define MY_BRANCH ";branch=0"
#define MY_BRANCH_LEN 9


#define MAX_PORT_LEN 7 /* ':' + max 5 letters + \0 */
#define CRLF "\r\n"
#define CRLF_LEN 2

#define RECEIVED ";received="
#define RECEIVED_LEN 10

/*used only if PKG_MALLOC is defined*/
#define PKG_MEM_POOL_SIZE 1024*1024

/*used if SH_MEM is defined*/
#define SHM_MEM_SIZE 128*1024*1024

#define TIMER_TICK 1
#define LONG_SLEEP	3600

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
   avoid terror of excessively huge messages
*/
#define BUF_SIZE (MAX_FIXED_BLOCK-32)

/* forwarding */
#define MAX_VIA_LINE_SIZE	240
#define MAX_RECEIVED_SIZE	57

#endif
