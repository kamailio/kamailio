/*
 * $Id$
 *
 * Unsupported header field parser macros
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


#ifndef CASE_UNSU_H
#define CASE_UNSU_H


#define TED_CASE                             \
        switch(val) {                        \
        case _ted1_:                         \
                hdr->type = HDR_UNSUPPORTED; \
                hdr->name.len = 11;          \
                *(p + 3) = '\0';             \
	        return (p + 4);              \
                                             \
        case _ted2_:                         \
                hdr->type = HDR_UNSUPPORTED; \
                p += 4;                      \
	        goto dc_end;                 \
        }


#define PPOR_CASE                 \
        switch(val) {             \
        case _ppor_:              \
                p += 4;           \
                val = READ(p);    \
                TED_CASE;         \
                                  \
                val = unify(val); \
                TED_CASE;         \
                goto other;       \
        }


#define Unsu_CASE         \
        p += 4;           \
        val = READ(p);    \
        PPOR_CASE;        \
                          \
        val = unify(val); \
        PPOR_CASE;        \
        goto other;       \


#endif /* CASE_UNSU_H */
