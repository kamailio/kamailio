/*
 * $Id$
 *
 * Digest Authentication - Radius support
 * Definitions not found in radiusclient.h
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
 * -------
 * 2003-03-09: Based on ser_radius.h from radius_auth (janakj)
 */

#ifndef SER_RADIUS_H
#define SER_RADIUS_H

/* Service types */
#define PW_CALL_CHECK                   10
#define PW_EMERGENCY_CALL               13

/* Attributes*/
#define PW_DIGEST_RESPONSE	        206	/* string */
#define PW_DIGEST_ATTRIBUTES	        207	/* string */

#define PW_SIP_URI_USER                 208     /* string */
#define PW_SIP_METHOD                   209     /* int */
#define PW_SIP_REQ_URI                  210     /* string */
#define PW_SIP_CC                       212     /* string */

#define PW_DIGEST_REALM		        1063	/* string */
#define	PW_DIGEST_NONCE		        1064	/* string */
#define	PW_DIGEST_METHOD	        1065	/* string */
#define	PW_DIGEST_URI		        1066	/* string */
#define	PW_DIGEST_QOP		        1067	/* string */
#define	PW_DIGEST_ALGORITHM	        1068	/* string */
#define	PW_DIGEST_BODY_DIGEST	        1069	/* string */
#define	PW_DIGEST_CNONCE	        1070	/* string */
#define	PW_DIGEST_NONCE_COUNT	        1071	/* string */
#define	PW_DIGEST_USER_NAME	        1072	/* string */


#endif /* SER_RADIUS_H */
