/* 
 * P-Asserted-Network-Info Field Name Parsing Macros
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
 * \brief Parser :: P-Asserted-Network-Info Field Name Parsing Macros
 *
 * \ingroup parser
 */

#ifndef CASE_PANI_H
#define CASE_PANI_H

#include "keys.h"

#define fo_CASE                                \
    if (LOWER_BYTE(*p) == 'o') {               \
            hdr->type = HDR_PANI_T; \
            p++;                               \
            goto dc_end;               \
    }                                          \
    goto other;

#define _INF_CASE              \
    val = READ(p);             \
    switch(LOWER_DWORD(val)) { \
    case __inf_:               \
	p += 4;                \
	fo_CASE;               \
    }                          \
    
#define WORK_CASE              \
    val = READ(p);             \
    switch(LOWER_DWORD(val)) { \
    case _work_:               \
	p += 4;                \
	_INF_CASE;               \
    }                          \
    
#define _NET_CASE              \
    val = READ(p);             \
    switch(LOWER_DWORD(val)) { \
    case __net_:               \
	p += 4;                \
	WORK_CASE;               \
    }                          \

#define CESS_CASE              \
    val = READ(p);             \
    switch(LOWER_DWORD(val)) { \
    case _cess_:               \
	p += 4;                \
	_NET_CASE;               \
    }                          \
    
#define p_ac_CASE              \
    p += 4;                \
    CESS_CASE;               \
    goto other;

#endif /* CASE_PANI_H */
