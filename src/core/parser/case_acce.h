/*
 * Accept, Accept-Language, Accept-Contact, Accept-Disposition Header Field Name Parsing Macros
 *
 * Copyright (C) 2001-2003 FhG Fokus
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * Kamailio is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * Kamailio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

/*! \file 
 * \brief Parser :: Accept, Accept-Language, Accept-Contact, Accept-Disposition Header Field Name Parsing Macros
 *
 * \ingroup parser
 */

#ifndef CASE_ACCE_H
#define CASE_ACCE_H


#define age_CASE                                \
        switch(LOWER_DWORD(val)) {              \
        case _age1_:                            \
	        hdr->type = HDR_ACCEPTLANGUAGE_T; \
	        hdr->name.len = 15;             \
	        return (p + 4);                 \
                                                \
        case _age2_:                            \
                hdr->type = HDR_ACCEPTLANGUAGE_T; \
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


#define accept_contact_ct_CASE                             \
        if (LOWER_BYTE(*p) == 'c') {                       \
                p++;                                       \
                if (LOWER_BYTE(*p) == 't') {               \
                        hdr->type = HDR_ACCEPTCONTACT_T;   \
                        p++;                               \
                        goto dc_end;                       \
                }                                          \
        }

#define accept_c_onta_CASE         \
        switch(LOWER_DWORD(val)) { \
        case _onta_:               \
                p += 4;            \
                val = READ(p);     \
                accept_contact_ct_CASE;  \
                goto other;        \
        }


#define ptldc_CASE                 \
        switch(LOWER_DWORD(val)) { \
        case _pt_l_:               \
		p += 4;            \
		val = READ(p);     \
		angu_CASE;         \
		goto other;        \
                                   \
        case _pt_c_:               \
                p += 4;            \
                val = READ(p);     \
                accept_c_onta_CASE;\
                goto other;        \
	}


#define acce_CASE                           \
    p += 4;                                 \
    val = READ(p);                          \
    ptldc_CASE;                             \
                                            \
    if (LOWER_BYTE(*p) == 'p') {            \
            p++;                            \
            if (LOWER_BYTE(*p) == 't') {    \
                    hdr->type = HDR_ACCEPT_T; \
                    p++;                    \
                    goto dc_end;            \
            }                               \
    }                                       \
    goto other;


#endif /* CASE_ACCE_H */
