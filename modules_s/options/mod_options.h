/*
 * $Id$
 *
 * Options Reply Module
 *
 * Copyright (C) 2001-2003 FhG Fokus
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

#ifndef OPT_RPL_H
#define OPT_RPL_H

#define ACPT_STR "Accept: "
#define ACPT_STR_LEN 8
#define ACPT_ENC_STR "Accept-Encoding: "
#define ACPT_ENC_STR_LEN 17
#define ACPT_LAN_STR "Accept-Language: "
#define ACPT_LAN_STR_LEN 17
#define SUPT_STR "Support: "
#define SUPT_STR_LEN 9
#define HF_SEP_STR "\r\n"
#define HF_SEP_STR_LEN 2

/* 
 * I think RFC3261 is not precise if a proxy should accept any
 * or no body (because it is not the endpoint of the media)
 */
#define ACPT_DEF "*/*"
#define ACPT_DEF_LEN 3
#define ACPT_ENC_DEF ""
#define ACPT_ENC_DEF_LEN 0
#define ACPT_LAN_DEF "en"
#define ACPT_LAN_DEF_LEN 2
#define SUPT_DEF ""
#define SUPT_DEF_LEN 0

#endif /* OPT_RPL_H */
