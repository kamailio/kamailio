/*
 * $Id$
 *
 * Authoriazation header field parser macros
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


#ifndef CASE_AUTH_H
#define CASE_AUTH_H


#define AUTH_ATIO_CASE                                 \
        if (val == _atio_) {                           \
	        p += 4;                                \
		switch(*p) {                           \
		case 'n':                              \
		case 'N':                              \
		        hdr->type = HDR_AUTHORIZATION; \
			p++;                           \
			goto dc_end;                   \
                                                       \
		default: goto other;                   \
		}                                      \
	}
	             

#define AUTH_ORIZ_CASE            \
        if (val == _oriz_) {      \
                p += 4;           \
	        val = READ(p);    \
	        AUTH_ATIO_CASE;   \
          			  \
                val = unify(val); \
	        AUTH_ATIO_CASE;   \
                                  \
                goto other;       \
        }


#define Auth_CASE      \
     p += 4;           \
     val = READ(p);    \
     AUTH_ORIZ_CASE;   \
                       \
     val = unify(val); \
     AUTH_ORIZ_CASE;   \
                       \
     goto other;


#endif /* CASE_AUTH_H */
