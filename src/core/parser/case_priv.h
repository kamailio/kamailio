/* 
 * Call-ID Header Name Parsing Macros
 *
 * Copyright (C) 2001-2003 FhG FOKUS
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
 * \brief Parser :: Call-ID Header Name Parsing Macros
 *
 * \ingroup parser
 */


#ifndef _CASE_PRIV_H
#define _CASE_PRIV_H


#define ACY_CASE					  \
	switch(LOWER_DWORD(val)) {		  \
	case _acy1_:					  \
		hdr->type = HDR_PRIVACY_T;	  \
		hdr->name.len = 7;			  \
		return (p + 4);				  \
									  \
	case _acy2_:					  \
		hdr->type = HDR_PRIVACY_T;	  \
		p += 4;						  \
		goto dc_end;				  \
	}


#define priv_CASE	   \
	p += 4;			   \
	val = READ(p);	   \
	ACY_CASE;		   \
	goto other;



#endif /* _CASE_PRIV_H */
