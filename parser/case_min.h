/*
 * Min-SE Header Field Name Parsing Macros
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
 * \brief Parser :: Min-SE Header Field Name Parsing Macros
 *
 * \ingroup parser
 */


#ifndef CASE_MIN_H
#define CASE_MIN_H


#define RES_CASE                            \
	switch(LOWER_DWORD(val)) {              \
		case _res1_:    /* "res:" */        \
			hdr->type = HDR_MIN_EXPIRES_T;  \
			hdr->name.len = 11;             \
			return (p + 4);                 \
		case _res2_:    /* "res " */        \
			hdr->type = HDR_MIN_EXPIRES_T;  \
			p+=4;                           \
			goto dc_end;                    \
	}

#define MIN2_CASE                           \
	if (LOWER_BYTE(*p) == 's') {            \
		p++;                                \
		if (LOWER_BYTE(*p) == 'e') {        \
			hdr->type = HDR_MIN_SE_T;       \
			p++;                            \
			goto dc_end;                    \
		}                                   \
	} else if (LOWER_DWORD(val) == _expi_) { \
		p += 4;                             \
		val = READ(p);                      \
		RES_CASE;                           \
	}

#define min_CASE                            \
	p += 4;                                 \
	val = READ(p);                          \
	MIN2_CASE;                              \
	goto other;


#endif /* CASE_MIN_H */
