/* 
 * Call-ID Header Name Parsing Macros
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
 *
 */

/*! \file 
 * \brief Parser :: Call-ID Header Name Parsing Macros
 *
 * \ingroup parser
 *
 */

#ifndef CASE_CALL_H
#define CASE_CALL_H

#include "case_c_inf.h"

#define ID_CASE                      \
     switch(LOWER_DWORD(val)) {      \
     case __id1_:                    \
	     hdr->type = HDR_CALLID_T; \
	     hdr->name.len = 7;      \
	     return (p + 4);         \
	                             \
     case __id2_:                    \
	     hdr->type = HDR_CALLID_T; \
	     p += 4;                 \
	     goto dc_end;            \
     C_INFO_CASE;                  \
     }


#define call_CASE      \
     p += 4;           \
     val = READ(p);    \
     ID_CASE;          \
     goto other;


#endif /* CASE_CALL_H */
