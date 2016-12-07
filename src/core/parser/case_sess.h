/*
 * Session-Expires Header Field Name Parsing Macros
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
 * \brief Parser :: Session-Expires Header Field Name Parsing Macros
 *
 * \ingroup parser
 */



#ifndef CASE_SESS_H
#define CASE_SESS_H


#define sessionexpi_res_CASE                         \
	switch(LOWER_DWORD(val)) {       \
	case _res1_:                     \
		hdr->type = HDR_SESSIONEXPIRES_T;    \
		hdr->name.len = 15;      \
		return (p + 4);          \
						\
	case _res2_:                     \
		hdr->type = HDR_SESSIONEXPIRES_T;    \
		p += 4;                  \
		goto dc_end;             \
	}

#define session_expi_CASE                         \
        if (LOWER_DWORD(val) == _expi_) { \
	        p += 4;                   \
                val = READ(p);            \
                sessionexpi_res_CASE;                 \
		goto other;               \
	}


#define sess_ion__CASE                          \
        if (LOWER_DWORD(val) == _ion__) {  \
                p += 4;                    \
	        val = READ(p);             \
	        session_expi_CASE;                 \
                goto other;                \
        }


#define sess_CASE      \
     p += 4;           \
     val = READ(p);    \
     sess_ion__CASE;        \
     goto other;


#endif /* CASE_SESS_H */
