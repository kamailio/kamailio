/*
 * Copyright (C) 2007 iptelorg GmbH
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
 * 2007-05-28	lightweight parser implemented for build_local_reparse()
 *              function. Basically copy-pasted from the core parser (Miklos)
 */

#include "../../parser/keys.h"
#include "../../parser/hf.h"
#include "../../parser/parser_f.h"
#include "lw_parser.h"

/* macros from the core parser */
#define LOWER_BYTE(b) ((b) | 0x20)
#define LOWER_DWORD(d) ((d) | 0x20202020)

#define READ(val) \
(*(val + 0) + (*(val + 1) << 8) + (*(val + 2) << 16) + (*(val + 3) << 24))

/*
 * lightweight header field name parser
 * used by build_local_reparse() function in order to construct ACK or CANCEL request
 * from the INVITE buffer
 * this parser supports only the header fields which are needed by build_local_reparse()
 */
char *lw_get_hf_name(char *begin, char *end,
			enum _hdr_types_t *type)
{

	char		*p;
	unsigned int	val;

	if (end - begin < 4) {
		*type = HDR_ERROR_T;
		return begin;
	}

	p = begin;
	val = LOWER_DWORD(READ(p));

	switch(val) {

	case _cseq_:	/* Cseq */
		*type = HDR_CSEQ_T;
		p += 4;
		break;

	case _via1_:	/* Via */
	case _via2_:
		*type = HDR_VIA_T;
		p += 3;
		break;

	case _from_:	/* From */
		*type = HDR_FROM_T;
		p += 4;
		break;

	case _to12_:	/* To */
		*type = HDR_TO_T;
		p += 2;
		break;

	case _requ_:	/* Require */
		p += 4;
		val = LOWER_DWORD(READ(p));

		switch(val) {

		case _ire1_:
		case _ire2_:
			p += 3;
			*type = HDR_REQUIRE_T;
			break;

		default:
			p -= 4;
			*type = HDR_OTHER_T;
			break;
		}
		break;

	case _prox_:	/* Proxy-Require */

		if ((LOWER_DWORD(READ(p+4)) == _y_re_)
		&& (LOWER_DWORD(READ(p+8)) == _quir_)
		&& (LOWER_BYTE(*(p+12)) == 'e')) {

			p += 13;
			*type = HDR_PROXYREQUIRE_T;
			break;

		} else {
			*type = HDR_OTHER_T;
			break;
		}

	case _cont_:	/* Content-Length */

		if ((LOWER_DWORD(READ(p+4)) == _ent__)
		&& (LOWER_DWORD(READ(p+8)) == _leng_)
		&& (LOWER_BYTE(*(p+12)) == 't')
		&& (LOWER_BYTE(*(p+13)) == 'h')) {

			p += 14;
			*type = HDR_CONTENTLENGTH_T;
			break;
		} else {
			*type = HDR_OTHER_T;
			break;
		}

	case _call_:	/* Call-Id */

		p += 4;
		val = LOWER_DWORD(READ(p));

		switch(val) {

		case __id1_:
		case __id2_:
			p += 3;
			*type = HDR_CALLID_T;
			break;

		default:
			p -= 4;
			*type = HDR_OTHER_T;
			break;
		}
		break;

	case _rout_:	/* Route */

		if (LOWER_BYTE(*(p+4)) == 'e') {
			p += 5;
			*type = HDR_ROUTE_T;
			break;
		} else {
			*type = HDR_OTHER_T;
			break;
		}

	case _max__:	/* Max-Forwards */

		if ((LOWER_DWORD(READ(p+4)) == _forw_)
		&& (LOWER_DWORD(READ(p+8)) == _ards_)) {

			p += 12;
			*type = HDR_MAXFORWARDS_T;
			break;
		} else {
			*type = HDR_OTHER_T;
			break;
		}

	default:
		/* compact headers */
		switch(LOWER_BYTE(*p)) {

		case 'v':	/* Via */
			if ((*(p+1) == ' ') || (*(p+1) == ':')) {
				p++;
				*type = HDR_VIA_T;
				break;
			}
			*type = HDR_OTHER_T;
			break;

		case 'f':	/* From */
			if ((*(p+1) == ' ') || (*(p+1) == ':')) {
				p++;
				*type = HDR_FROM_T;
				break;
			}
			*type = HDR_OTHER_T;
			break;

		case 't':	/* To */
			if (LOWER_BYTE(*(p+1)) == 'o') {
				p += 2;
				*type = HDR_TO_T;
				break;
			}
			if ((*(p+1) == ' ') || (*(p+1) == ':')) {
				p++;
				*type = HDR_TO_T;
				break;
			}
			*type = HDR_OTHER_T;
			break;

		case 'l':	/* Content-Length */
			if ((*(p+1) == ' ') || (*(p+1) == ':')) {
				p++;
				*type = HDR_CONTENTLENGTH_T;
				break;
			}
			*type = HDR_OTHER_T;
			break;

		case 'i':	/* Call-Id */
			if ((*(p+1) == ' ') || (*(p+1) == ':')) {
				p++;
				*type = HDR_CALLID_T;
				break;
			}
			*type = HDR_OTHER_T;
			break;

		default:
			*type = HDR_OTHER_T;
			break;
		}
	}

	return p;
}

/* returns a pointer to the next line */
char *lw_next_line(char *buf, char *buf_end)
{
	char	*c;

	c = buf;
	do {
		while ((c < buf_end) && (*c != '\n')) c++;
		if (c < buf_end) c++;

	} while ((c < buf_end) &&
		((*c == ' ') || (*c == '\t')));	/* next line begins with whitespace line folding */

	return c;
}

#ifdef USE_DNS_FAILOVER
/* returns the pointer to the first VIA header */
char *lw_find_via(char *buf, char *buf_end)
{
	char		*p;
	unsigned int	val;

	/* skip the first line */
	p = eat_line(buf, buf_end - buf);

	while (buf_end - p > 4) {
		val = LOWER_DWORD(READ(p));
		if ((val == _via1_) || (val == _via2_)
		|| ((LOWER_BYTE(*p) == 'v')		/* compact header */
			&& ((*(p+1) == ' ') || (*(p+1) == ':')) )
				) return p;

		p = lw_next_line(p, buf_end);
	}
	/* not found */
	return 0;
}
#endif
