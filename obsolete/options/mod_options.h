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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef _MOD_OPTIONS_H
#define _MOD_OPTIONS_H

#define ACPT_STR "Accept: "
#define ACPT_STR_LEN (sizeof(ACPT_STR) - 1)

#define ACPT_ENC_STR "Accept-Encoding: "
#define ACPT_ENC_STR_LEN (sizeof(ACPT_ENC_STR) - 1)

#define ACPT_LAN_STR "Accept-Language: "
#define ACPT_LAN_STR_LEN (sizeof(ACPT_LAN_STR) - 1)

#define SUPT_STR "Supported: "
#define SUPT_STR_LEN (sizeof(SUPT_STR) - 1)

#define CONT_STR "Contact: <sip:"
#define CONT_STR_LEN (sizeof(CONT_STR) - 1)

#endif /* _MOD_OPTIONS_H */
