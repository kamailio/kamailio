/*
 *  $Id$
 *
 * Copyright (C) 2001-2003 Fhg Fokus
 *
 * This file is part of ser, a free SIP server.
 *
 * ser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * For a license to use the ser software under conditions
 * other than those described here, or to purchase support for this
 * software, please contact iptel.org by e-mail at the following addresses:
 *    info@iptel.org
 *
 * ser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * History:
 * --------
 * 2003-04-05  DEFAULT_*_URL introduced (jiri)
 * 2003-07-04  fixed SRV lookup prefix for TLS/sips (andrei)
 */




#ifndef config_h
#define config_h

#include "types.h"

/* default sip port if none specified */
#define SIP_PORT  5060
#define SIPS_PORT 5061

#define CFG_FILE CFG_DIR "ser.cfg"

#define TLS_PKEY_FILE CFG_DIR "cert.pem" 
#define TLS_CERT_FILE CFG_DIR "cert.pem"
#define TLS_CA_FILE 0 /* no CA list file by default */


/* maximum number of addresses on which we will listen */
#define MAX_LISTEN 16

/* default number of child processes started */
#define CHILD_NO    8

#define RT_NO 10 /* routing tables number */
#define FAILURE_RT_NO RT_NO /* on_failure routing tables number */
#define ONREPLY_RT_NO RT_NO /* on_reply routing tables number */
#define DEFAULT_RT 0 /* default routing table */

#define MAX_REC_LEV 100 /* maximum number of recursive calls */
#define ROUTE_MAX_REC_LEV 100 /* maximum number of recursive calls
							   for route()*/

#define MAX_URI_SIZE 1024	/* used when rewriting URIs */

#define MY_VIA "Via: SIP/2.0/UDP "
#define MY_VIA_LEN 17

#define CONTENT_LENGTH "Content-Length: "
#define CONTENT_LENGTH_LEN (sizeof(CONTENT_LENGTH)-1)

#define USER_AGENT "User-Agent: Sip EXpress router"\
		"(" VERSION " (" ARCH "/" OS"))"
#define USER_AGENT_LEN (sizeof(USER_AGENT)-1)

#define SERVER_HDR "Server: Sip EXpress router "\
		"(" VERSION " (" ARCH "/" OS"))"
#define SERVER_HDR_LEN (sizeof(SERVER_HDR)-1)

#define MAX_WARNING_LEN  256
		
#define MY_BRANCH ";branch="
#define MY_BRANCH_LEN 8


#define MAX_PORT_LEN 7 /* ':' + max 5 letters + \0 */
#define CRLF "\r\n"
#define CRLF_LEN 2

#define RECEIVED        ";received="
#define RECEIVED_LEN 10

#define TRANSPORT_PARAM ";transport="
#define TRANSPORT_PARAM_LEN 11

#define TOTAG_TOKEN ";tag="
#define TOTAG_TOKEN_LEN (sizeof(TOTAG_TOKEN)-1)

#define RPORT ";rport="
#define RPORT_LEN 7

#define ID_PARAM ";i="
#define ID_PARAM_LEN 3

#define SRV_UDP_PREFIX "_sip._udp."
#define SRV_TCP_PREFIX "_sip._tcp."
#define SRV_TLS_PREFIX "_sips._tcp."
#define SRV_UDP_PREFIX_LEN 10
#define SRV_TCP_PREFIX_LEN 10
#define SRV_TLS_PREFIX_LEN 11
#define SRV_MAX_PREFIX_LEN 11

/*used only if PKG_MALLOC is defined*/
#define PKG_MEM_POOL_SIZE 1024*1024

/*used if SH_MEM is defined*/
#define SHM_MEM_SIZE 32

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
#define BUF_SIZE 65535

/* forwarding  -- Via buffer dimensioning */
#define MAX_VIA_LINE_SIZE	240
#define MAX_RECEIVED_SIZE	57
#define MAX_RPORT_SIZE		13

/* maximum number of branches per transaction */
#define MAX_BRANCHES    4

/* maximum length of a FIFO server command */
#define MAX_FIFO_COMMAND 512

/* buffer dimensions for FIFO server */
#define MAX_CONSUME_BUFFER 1024
/* where reply pipes may be opened */
#define FIFO_DIR "/tmp/"
#define FIFO_DIR_LEN 5
/* max length of the text of fifo 'print' command */
#define MAX_PRINT_TEXT 256

/* maximum length of Contact heder field in redirection replies */
#define MAX_REDIRECTION_LEN 512

/* used by FIFO statistics in module to terminate line;
   extra whitespaces are used to overwrite remainders of
   previous line if longer than current one
*/
#define CLEANUP_EOL "      \n"

/* how patient is ser with FIFO clients not awaiting a reply? 
	4 x 80ms = 0.32 sec
*/
#define FIFO_REPLY_RETRIES	4
#define FIFO_REPLY_WAIT		80000

/* magic cookie for transaction matching as defined in RFC3261 */
#define MCOOKIE "z9hG4bK"
#define MCOOKIE_LEN (sizeof(MCOOKIE)-1)
/* Maximum length of values appended to Via-branch parameter */
#define MAX_BRANCH_PARAM_LEN  (MCOOKIE_LEN+48)


/* maximum path length */
#define PATH_MAX_GUESS	1024

#define DEFAULT_DB_URL "sql://ser:heslo@localhost/ser"
#define DEFAULT_RODB_URL "sql://serro:47serro11@localhost/ser"


#endif
