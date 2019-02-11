/* 
 * Copyright (C) 2005 iptelorg GmbH
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

#ifndef __XCAP_RESULT_CODES_H
#define __XCAP_RESULT_CODES_H

/* result codes returned by XCAP operations */
#define RES_OK						0
#define RES_INTERNAL_ERR			(-1)
#define RES_MEMORY_ERR				(-2)
#define RES_BAD_EVENT_PACKAGE_ERR	(-5)
#define RES_BAD_GATEWAY_ERR			(-6)
#define RES_XCAP_QUERY_ERR			(-7)
#define RES_XCAP_PARSE_ERR			(-8)

#define LAST_XCAP_RES_CODE			RES_XCAP_PARSE_ERR

#endif
