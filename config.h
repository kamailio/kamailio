/*
 * Copyright (C) 2001-2003 FhG Fokus
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * Kamailio is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * Kamailio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

/*!
 * \file
 * \brief Kamailio core :: Configuration options
 * \author jiri, andrei
 *
 * These settings are settable by the user before compilation
 *
 * \ingroup core
 * Module: \ref core
 */



#ifndef config_h
#define config_h

#include "types.h"

#define SIP_PORT  5060 /*!< default SIP port if none specified */
#define SIPS_PORT 5061 /*!< default SIP port for TLS if none specified */

#define CFG_FILE CFG_DIR NAME ".cfg"

#define TLS_PKEY_FILE "cert.pem" 	/*!< The certificate private key file */
#define TLS_CERT_FILE "cert.pem"	/*!< The certificate file */
#define TLS_CA_FILE 0			/*!< no CA list file by default */
#define TLS_CRL_FILE 0 /*!< no CRL by default */

#define MAX_LISTEN 16			/*!< maximum number of addresses on which we will listen */

#define CHILD_NO    8			/*!< default number of child processes started */

#define RT_NO 2 			/*!< routing tables number */
#define FAILURE_RT_NO RT_NO 		/*!< on_failure routing tables number */
#define ONREPLY_RT_NO RT_NO 		/*!< on_reply routing tables number */
#define BRANCH_RT_NO RT_NO 		/*!< branch_route routing tables number */
#define ONSEND_RT_NO 1  		/*!< onsend_route routing tables number */
#define EVENT_RT_NO RT_NO 		/*!< event_route routing tables number */
#define DEFAULT_RT 0 			/*!< default routing table */

#define MAX_URI_SIZE 1024		/*!< Max URI size used when rewriting URIs */

#define MAX_PATH_SIZE 256 		/*!< Maximum length of path header buffer */

#define MAX_INSTANCE_SIZE 256 		/*!< Maximum length of +sip.instance contact header param value buffer */

#define MAX_RUID_SIZE 65		/*!< Maximum length of ruid for location records */

#define MAX_UA_SIZE 255			/*!< Maximum length of user-agent for location records */

#define MY_VIA "Via: SIP/2.0/UDP "
#define MY_VIA_LEN (sizeof(MY_VIA) - 1)

#define ROUTE_PREFIX "Route: "
#define ROUTE_PREFIX_LEN (sizeof(ROUTE_PREFIX) - 1)

#define ROUTE_SEPARATOR ", "
#define ROUTE_SEPARATOR_LEN (sizeof(ROUTE_SEPARATOR) - 1)

#define CONTENT_LENGTH "Content-Length: "
#define CONTENT_LENGTH_LEN (sizeof(CONTENT_LENGTH)-1)

#define USER_AGENT "User-Agent: " NAME \
		" (" VERSION " (" ARCH "/" OS_QUOTED "))"
#define USER_AGENT_LEN (sizeof(USER_AGENT)-1)

#define SERVER_HDR "Server: " NAME \
		" (" VERSION " (" ARCH "/" OS_QUOTED "))"
#define SERVER_HDR_LEN (sizeof(SERVER_HDR)-1)

#define MAX_WARNING_LEN  256
		
#define MY_BRANCH ";branch="
#define MY_BRANCH_LEN (sizeof(MY_BRANCH) - 1)

#define MAX_PORT_LEN 7 /* ':' + max 5 letters + \0 */
#define CRLF "\r\n"
#define CRLF_LEN (sizeof(CRLF) - 1)

#define RECEIVED        ";received="
#define RECEIVED_LEN (sizeof(RECEIVED) - 1)

#define TRANSPORT_PARAM ";transport="
#define TRANSPORT_PARAM_LEN (sizeof(TRANSPORT_PARAM) - 1)

#define COMP_PARAM ";comp="
#define COMP_PARAM_LEN (sizeof(COMP_PARAM)-1)

#define SIGCOMP_NAME "sigcomp"
#define SIGCOMP_NAME_LEN (sizeof(SIGCOMP_NAME)-1)

#define SERGZ_NAME "sergz"
#define SERGZ_NAME_LEN (sizeof(SERGZ_NAME)-1)

#define TOTAG_TOKEN ";tag="
#define TOTAG_TOKEN_LEN (sizeof(TOTAG_TOKEN)-1)

#define RPORT ";rport="
#define RPORT_LEN (sizeof(RPORT) - 1)

#define ID_PARAM ";i="
#define ID_PARAM_LEN (sizeof(ID_PARAM) - 1)

#define SRV_UDP_PREFIX "_sip._udp."
#define SRV_UDP_PREFIX_LEN (sizeof(SRV_UDP_PREFIX) - 1)

#define SRV_TCP_PREFIX "_sip._tcp."
#define SRV_TCP_PREFIX_LEN (sizeof(SRV_TCP_PREFIX) - 1)

#define SRV_TLS_PREFIX "_sips._tcp."
#define SRV_TLS_PREFIX_LEN (sizeof(SRV_TLS_PREFIX) - 1)

#define SRV_SCTP_PREFIX "_sip._sctp."
#define SRV_SCTP_PREFIX_LEN (sizeof(SRV_SCTP_PREFIX) - 1)

#define SRV_MAX_PREFIX_LEN SRV_TLS_PREFIX_LEN

#ifndef PKG_MEM_SIZE
#define PKG_MEM_SIZE 8
#endif
#define PKG_MEM_POOL_SIZE PKG_MEM_SIZE*1024*1024	/*!< used only if PKG_MALLOC is defined*/

#define SHM_MEM_SIZE 64				/*!< used if SH_MEM is defined*/


/* dimensioning buckets in q_malloc */
/*! \brief size of the size2bucket table; everything beyond that asks for
   a variable-size kilo-bucket
 */
#define MAX_FIXED_BLOCK         3072
#define BLOCK_STEP              512		/*!< distance of kilo-buckets */
#define MAX_BUCKET		15		/*!< maximum number of possible buckets */

/*! \brief receive buffer size -- preferably set low to
   avoid terror of excessively huge messages; they are
   useless anyway
*/
#define BUF_SIZE 65535

#define MAX_VIA_LINE_SIZE	240	/*!< forwarding  -- Via buffer dimensioning */
#define MAX_RECEIVED_SIZE	59	/*!< forwarding  -- Via buffer dimensioning - Received header */
#define MAX_RPORT_SIZE		13	/*!< forwarding  -- Via buffer dimensioning - Rport */

#define MAX_BRANCHES_DEFAULT	12	/*!< default maximum number of branches per transaction */
#define MAX_BRANCHES_LIMIT		32	/*!< limit of maximum number of branches per transaction */

#define MAX_PRINT_TEXT 		256	/*!< max length of the text of fifo 'print' command */

#define MAX_REDIRECTION_LEN	512	/*!< maximum length of Contact header field in redirection replies */

/*! \brief used by FIFO statistics in module to terminate line;
   extra whitespaces are used to overwrite remainders of
   previous line if longer than current one
*/
#define CLEANUP_EOL "      \n"

#define MCOOKIE "z9hG4bK"		/*!< magic cookie for transaction matching as defined in RFC3261 */
#define MCOOKIE_LEN (sizeof(MCOOKIE)-1)
/*! \brief Maximum length of values appended to Via-branch parameter */
#define MAX_BRANCH_PARAM_LEN  (MCOOKIE_LEN+8 /*int2hex*/ + 1 /*sep*/ + \
								MD5_LEN /* max(int2hex, MD5_LEN) */ \
								+ 1 /*sep*/ + 8 /*int2hex*/ + \
								1 /*extra space, needed by t_calc_branch*/)

#define DEFAULT_SER_KILL_TIMEOUT 60 	/*!< Kill timeout : seconds */

#define PATH_MAX_GUESS	1024		/*!< maximum path length */

#if defined KAMAILIO_MOD_INTERFACE || defined OPENSER_MOD_INTERFACE || \
		defined MOD_INTERFACE_V1
	#define DEFAULT_DB_URL "mysql://kamailio:kamailiorw@localhost/kamailio"
	#define DEFAULT_DB_URL_LEN (sizeof(DEFAULT_DB_URL) - 1)
	#define DEFAULT_RODB_URL "mysql://kamailioro:kamailioro@localhost/kamailio"
	#define DEFAULT_RODB_URL_LEN (sizeof(DEFAULT_RODB_URL) - 1)
#else
	#define DEFAULT_DB_URL "mysql://ser:heslo@localhost/ser"
	#define DEFAULT_DB_URL_LEN (sizeof(DEFAULT_DB_URL) - 1)
	#define DEFAULT_RODB_URL "mysql://serro:47serro11@localhost/ser"
	#define DEFAULT_RODB_URL_LEN (sizeof(DEFAULT_RODB_URL) - 1)
#endif

#define VERSION_TABLE "version"			/*!< table holding versions of other ser tables */
#define VERSION_TABLE_LEN (sizeof(VERSION_TABLE) - 1)
#define VERSION_COLUMN "table_version"		/*!< Column holding version number in version table */
#define TABLENAME_COLUMN "table_name"		/*!< Column holding module name in version table */

#define MIN_UDP_PACKET        32		/*!< minimum UDP packet size; smaller packets will be dropped silently */

#define MIN_SCTP_PACKET  MIN_UDP_PACKET 	/*!< minimum size of SCTP packet */

#define DEFAULT_RADIUS_CONFIG "/usr/local/etc/radiusclient/radiusclient.conf"	/*!< Default FreeRadius configuration file */

#define DEFAULT_DID "_default"

#define DEFAULT_MAX_WHILE_LOOPS 100		/*!< Maximum allowed iterations for a while (to catch runaways) */

#endif
