/*
 * $Id$
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
 * ---------
 * 2003-04-04  grand acc cleanup (jiri)
 */

#ifndef _ACC_DEFS_H
#define _ACC_DEFS_H

/* method, in-uri, out-uri, callid, from, status */
#define DEFAULT_LOG_FMT "miocfs"
/* Callid, to-tag (Dst), From, In-uri, Method, Out-uri, 
 * fRom-tag, Status,  To, digest User_id, userPart in-uri,
 * From Uri Userpart 0, To Uri Userpart 1, From uri , To uri,
 * User (digest, from-uri otherwise), 3-digit Status code, 
 * cseq Number  */ 
#define ALL_LOG_FMT "cdfimorstup01FTUSn"
#define ALL_LOG_FMT_LEN (sizeof(ALL_LOG_FMT)-1)

#ifdef SQL_ACC
#	define SQL_ACC_FMT "FTmiofcts0dr"
#	define SQL_MC_FMT  "FTmiofctspdr"
#endif

/* caution: keep consistent with definition of rad_attr */
#ifdef RAD_ACC
#	define RAD_ACC_FMT "UFTocdrn"
#endif


#endif
