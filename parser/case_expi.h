/*
 * $Id$
 *
 * Expires header field parser macros
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


#ifndef CASE_EXPI_H
#define CASE_EXPI_H


#define EXPI_RES_CASE                    \
        switch(val) {                    \
        case _res1_:                     \
		hdr->type = HDR_EXPIRES; \
		hdr->name.len = 7;       \
                *(p + 3) = '\0';         \
		return (p + 4);          \
                                         \
        case _res2_:                     \
		hdr->type = HDR_EXPIRES; \
		p += 4;                  \
		goto dc_end;             \
        }


#define Expi_CASE         \
        p += 4;           \
        val = READ(p);    \
        EXPI_RES_CASE;    \
                          \
        val = unify(val); \
        EXPI_RES_CASE;    \
        goto other;


#endif /* CASE_EXPI_H */
