/*
 * $Id$
 *
 * Proxy-Authorization and Proxy-Require header field parser macros
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


#ifndef CASE_PROX_H
#define CASE_PROX_H


#define ION_CASE                           \
        switch(val) {                      \
        case _ion1_:                       \
	        hdr->type = HDR_PROXYAUTH; \
	        hdr->name.len = 19;        \
                *(p + 3) = '\0';           \
	        return (p + 4);            \
                                           \
        case _ion2_:                       \
                hdr->type = HDR_PROXYAUTH; \
                p += 4;                    \
	        goto dc_end;               \
        }


#define IZAT_CASE                 \
        switch(val) {             \
        case _izat_:              \
                p += 4;           \
                val = READ(p);    \
                ION_CASE;         \
                                  \
                val = unify(val); \
                ION_CASE;         \
                goto other;       \
        }


#define THOR_CASE                 \
        switch(val) {             \
        case _thor_:              \
                p += 4;           \
                val = READ(p);    \
                IZAT_CASE;        \
                                  \
                val = unify(val); \
                IZAT_CASE;        \
                goto other;       \
        }


#define QUIR_CASE                                     \
        switch(val) {                                 \
        case _quir_:                                  \
	        p += 4;                               \
                switch(*p) {                          \
                case 'e':                             \
                case 'E':                             \
                        hdr->type = HDR_PROXYREQUIRE; \
	                p++;                          \
                        goto dc_end;                  \
                }                                     \
                goto other;                           \
        }


#define PROX2_CASE                \
        switch(val) {             \
        case _y_Au_:              \
                p += 4;           \
                val = READ(p);    \
                THOR_CASE;        \
                                  \
                val = unify(val); \
                THOR_CASE;        \
                goto other;       \
                                  \
        case _y_Re_:              \
                p += 4;           \
                val = READ(p);    \
                QUIR_CASE;        \
                                  \
                val = unify(val); \
                QUIR_CASE;        \
                goto other;       \
        }


#define Prox_CASE         \
        p += 4;           \
        val = READ(p);    \
        PROX2_CASE;       \
                          \
        val = unify(val); \
        PROX2_CASE;       \
        goto other;


#endif /* CASE_PROX_H */
