/* 
 * $Id$ 
 *
 * Supported Header Field Name Parsing Macros
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


#ifndef CASE_SUPP_H
#define CASE_SUPP_H


#define ORTE_CASE                                  \
        switch(LOWER_DWORD(val)) {                 \
        case _orte_:                               \
	        p += 4;                            \
	        if (LOWER_BYTE(*p) == 'd') {       \
		        hdr->type = HDR_SUPPORTED; \
                        p++;                       \
                        goto dc_end;               \
                }                                  \
                goto other;                        \
        }


#define supp_CASE         \
        p += 4;           \
        val = READ(p);    \
        ORTE_CASE;        \
        goto other;


#endif /* CASE_SUPP_H */
