/*
 * 32-bit Digest parameter name parser
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "param_parser.h"
#include "digest_keys.h"
#include "../../trim.h"
#include "../../ut.h"

#define LOWER_BYTE(b) ((b) | 0x20)
#define LOWER_DWORD(d) ((d) | 0x20202020)

/*
 * Parse short (less than 4 bytes) parameter names
 */
#define PARSE_SHORT                                                   \
	switch(LOWER_BYTE(*p)) {                                      \
	case 'u':                                                     \
		if (LOWER_BYTE(*(p + 1)) == 'r') {                    \
			if (LOWER_BYTE(*(p + 2)) == 'i') {            \
				*_type = PAR_URI;                     \
                                p += 3;                               \
				goto end;                             \
			}                                             \
		}                                                     \
		break;                                                \
                                                                      \
	case 'q':                                                     \
		if (LOWER_BYTE(*(p + 1)) == 'o') {                    \
			if (LOWER_BYTE(*(p + 2)) == 'p') {            \
				*_type = PAR_QOP;                     \
                                p += 3;                               \
				goto end;                             \
			}                                             \
		}                                                     \
		break;                                                \
                                                                      \
	case 'n':                                                     \
		if (LOWER_BYTE(*(p + 1)) == 'c') {                    \
			*_type = PAR_NC;                              \
                        p += 2;                                       \
			goto end;                                     \
		}                                                     \
		break;                                                \
	}


/*
 * Read 4-bytes from memory and store them in an integer variable
 * Reading byte by byte ensures, that the code works also on HW which
 * does not allow reading 4-bytes at once from unaligned memory position
 * (Sparc for example)
 */
#define READ(val) \
(*(val + 0) + (*(val + 1) << 8) + (*(val + 2) << 16) + (*(val + 3) << 24))


#define name_CASE                      \
        switch(LOWER_DWORD(val)) {     \
        case _name_:                   \
		*_type = PAR_USERNAME; \
                p += 4;                \
		goto end;              \
        }


#define user_CASE         \
        p += 4;           \
        val = READ(p);    \
        name_CASE;        \
        goto other;


#define real_CASE                         \
        p += 4;                           \
        if (LOWER_BYTE(*p) == 'm') {      \
		*_type = PAR_REALM;       \
                p++;                      \
		goto end;                 \
	}


#define nonc_CASE                         \
        p += 4;                           \
        if (LOWER_BYTE(*p) == 'e') {      \
	        *_type = PAR_NONCE;       \
                p++;                      \
		goto end;                 \
	}


#define onse_CASE                      \
        switch(LOWER_DWORD(val)) {     \
        case _onse_:                   \
		*_type = PAR_RESPONSE; \
                p += 4;                \
		goto end;              \
        }


#define resp_CASE         \
        p += 4;           \
        val = READ(p);    \
        onse_CASE;        \
        goto other;


#define cnon_CASE                                 \
        p += 4;                                   \
        if (LOWER_BYTE(*p) == 'c') {              \
		p++;                              \
		if (LOWER_BYTE(*p) == 'e') {      \
			*_type = PAR_CNONCE;      \
                        p++;                      \
			goto end;                 \
		}                                 \
	}                                         \
        goto other;


#define opaq_CASE                                 \
        p += 4;                                   \
        if (LOWER_BYTE(*p) == 'u') {              \
		p++;                              \
		if (LOWER_BYTE(*p) == 'e') {      \
			*_type = PAR_OPAQUE;      \
                        p++;                      \
			goto end;                 \
		}                                 \
	}                                         \
        goto other;


#define rith_CASE                                 \
        switch(LOWER_DWORD(val)) {                \
	case _rith_:                              \
		p += 4;                           \
		if (LOWER_BYTE(*p) == 'm') {      \
			*_type = PAR_ALGORITHM;   \
                        p++;                      \
			goto end;                 \
		}                                 \
		goto other;                       \
	}


#define algo_CASE         \
        p += 4;           \
        val = READ(p);    \
        rith_CASE;        \
        goto other


#define FIRST_QUATERNIONS       \
        case _user_: user_CASE; \
        case _real_: real_CASE; \
        case _nonc_: nonc_CASE; \
        case _resp_: resp_CASE; \
        case _cnon_: cnon_CASE; \
        case _opaq_: opaq_CASE; \
        case _algo_: algo_CASE;




int parse_param_name(str* _s, dig_par_t* _type)
{
        register char* p;
        register int val;
	char* end;
	
	end = _s->s + _s->len;
	
	p = _s->s;
	val = READ(p);
	
	if (_s->len < 4) {
		goto other;
	}
	
        switch(LOWER_DWORD(val)) {
	FIRST_QUATERNIONS;
	default:
		PARSE_SHORT;
		goto other;
        }

 end:
	_s->len -= p - _s->s;
	_s->s = p;

	trim_leading(_s);
	if (_s->s[0] == '=') {
		return 0;
	}
	
 other:
	p = q_memchr(p, '=', end - p);
	if (!p) {
		return -1; /* Parse error */
	} else {
		*_type = PAR_OTHER;
		_s->len -= p - _s->s;
		_s->s = p;
		return 0;
	}
}
