/*
 * Include file for Radius 
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
 */

/* see http://www.iptel.org/ietf/aaa/draft-schulzrinne-sipping-radius-accounting-00.txt */

/*
 * WARNING: Don't forget to update the dictionary if you update this file !!!
 */

#ifndef _DICT_H
#define _DICT_H

#define PW_SIP_METHOD			101	/* integer */
#define PW_SIP_RESPONSE_CODE	        102     /* integer */
#define PW_SIP_CSEQ			103	/* string */
#define PW_SIP_TO_TAG			104	/* string */
#define PW_SIP_FROM_TAG			105	/* string */
#define PW_SIP_BRANCH_ID		106     /* string -- Not used */
#define PW_SIP_TRANSLATED_REQ_ID	107     /* string */
#define PW_SIP_SOURCE_IP_ADDRESS	108     /* ipaddr -- Not used */
#define PW_SIP_SOURCE_PORT		109     /* integer -- Not used */

#define PW_SIP_SESSION	                 15     /* SIP service-type */
#define PW_STATUS_FAILED		 15

#endif
