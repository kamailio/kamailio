/* 
 * $Id$ 
 *
 * Accept and Accept-Language Header Field Name Parsing Macros
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

#ifndef CASE_ACCE_H
#define CASE_ACCE_H


#define age_CASE                                \
        switch(LOWER_DWORD(val)) {              \
        case _age1_:                            \
	        hdr->type = HDR_ACCEPTLANGUAGE; \
	        hdr->name.len = 15;             \
	        return (p + 4);                 \
                                                \
        case _age2_:                            \
                hdr->type = HDR_ACCEPTLANGUAGE; \
                p += 4;                         \
	        goto dc_end;                    \
        }


#define angu_CASE                  \
        switch(LOWER_DWORD(val)) { \
        case _angu_:               \
		p += 4;            \
		val = READ(p);     \
		age_CASE;          \
		goto other;        \
	}


#define on_CASE                                            \
        if (LOWER_BYTE(*p) == 'o') {                       \
                p++;                                       \
                if (LOWER_BYTE(*p) == 'n') {               \
                        hdr->type = HDR_ACCEPTDISPOSITION; \
                        p++;                               \
                        goto dc_end;                       \
                }                                          \
        }


#define siti_CASE                  \
        switch(LOWER_DWORD(val)) { \
        case _siti_:               \
                p += 4;            \
                val = READ(p);     \
                on_CASE;           \
                goto other;        \
        }


#define ispo_CASE                  \
        switch(LOWER_DWORD(val)) { \
        case _ispo_:               \
                p += 4;            \
                val = READ(p);     \
                siti_CASE;         \
                goto other;        \
        }


#define ptld_CASE                  \
        switch(LOWER_DWORD(val)) { \
        case _pt_l_:               \
		p += 4;            \
		val = READ(p);     \
		angu_CASE;         \
		goto other;        \
                                   \
        case _pt_d_:               \
                p += 4;            \
                val = READ(p);     \
                ispo_CASE;         \
                goto other;        \
	}


#define acce_CASE                           \
    p += 4;                                 \
    val = READ(p);                          \
    ptld_CASE;                              \
                                            \
    if (LOWER_BYTE(*p) == 'p') {            \
            p++;                            \
            if (LOWER_BYTE(*p) == 't') {    \
                    hdr->type = HDR_ACCEPT; \
                    p++;                    \
                    goto dc_end;            \
            }                               \
    }                                       \
    goto other;


#endif /* CASE_ACCE_H */
