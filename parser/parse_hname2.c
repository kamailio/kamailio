/* 
 * $Id$ 
 *
 * Fast 32-bit Header Field Name Parser
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
 *
 * History:
 * --------
 * 2003-02-28 scratchpad compatibility abandoned (jiri)
 * 2003-01-27 next baby-step to removing ZT - PRESERVE_ZT (jiri)
 */


#include "../comp_defs.h"
#include "parse_hname2.h"
#include "keys.h"
#include "../ut.h"  /* q_memchr */

#define LOWER_BYTE(b) ((b) | 0x20)
#define LOWER_DWORD(d) ((d) | 0x20202020)

/*
 * Skip all whitechars and return position of the first
 * non-white char
 */
static inline char* skip_ws(char* p, unsigned int size)
{
	char* end;
	
	end = p + size;
	for(; p < end; p++) {
		if ((*p != ' ') && (*p != '\t')) return p;
	}
	return p;
}
	
/*
 * Parser macros
 */
#include "case_via.h"      /* Via */
#include "case_from.h"     /* From */
#include "case_to.h"       /* To */
#include "case_cseq.h"     /* CSeq */
#include "case_call.h"     /* Call-ID */
#include "case_cont.h"     /* Contact, Content-Type, Content-Length */
#include "case_rout.h"     /* Route */
#include "case_max.h"      /* Max-Forwards */
#include "case_reco.h"     /* Record-Route */
#include "case_auth.h"     /* Authorization */
#include "case_expi.h"     /* Expires */
#include "case_prox.h"     /* Proxy-Authorization, Proxy-Require */
#include "case_allo.h"     /* Allow */
#include "case_unsu.h"     /* Unsupported */
#include "case_requ.h"     /* Require */
#include "case_supp.h"     /* Supported */
#include "case_www.h"      /* WWW-Authenticate */
#include "case_even.h"     /* Event */


#define READ(val) \
(*(val + 0) + (*(val + 1) << 8) + (*(val + 2) << 16) + (*(val + 3) << 24))


#define FIRST_QUATERNIONS       \
        case _via1_: via1_CASE; \
	case _from_: from_CASE; \
	case _to12_: to12_CASE; \
	case _cseq_: cseq_CASE; \
	case _call_: call_CASE; \
	case _cont_: cont_CASE; \
	case _rout_: rout_CASE; \
	case _max__: max_CASE;  \
	case _reco_: reco_CASE; \
	case _via2_: via2_CASE; \
	case _auth_: auth_CASE; \
	case _expi_: expi_CASE; \
	case _prox_: prox_CASE; \
	case _allo_: allo_CASE; \
	case _unsu_: unsu_CASE; \
	case _requ_: requ_CASE; \
	case _supp_: supp_CASE; \
        case _www__: www_CASE;  \
        case _even_: even_CASE;


#define PARSE_COMPACT(id)          \
        switch(*(p + 1)) {         \
        case ' ':                  \
	        hdr->type = id;    \
	        p += 2;            \
	        goto dc_end;       \
	                           \
        case ':':                  \
	        hdr->type = id;    \
	        hdr->name.len = 1; \
	        return (p + 2);    \
        }                            


char* parse_hname2(char* begin, char* end, struct hdr_field* hdr)
{
	register char* p;
	register unsigned int val;

	if ((end - begin) < 4) {
		hdr->type = HDR_ERROR;
		return begin;
	}

	p = begin;

	val = LOWER_DWORD(READ(p));
	hdr->name.s = begin;

	switch(val) {
	FIRST_QUATERNIONS;

	default:
		switch(LOWER_BYTE(*p)) {
		case 't':                           
			switch(LOWER_BYTE(*(p + 1))) {          
			case 'o':                   
			case ' ':                   
				hdr->type = HDR_TO; 
				p += 2;             
				goto dc_end;        
				
			case ':':                   
				hdr->type = HDR_TO; 
				hdr->name.len = 1;  
				return (p + 2);     
			}                           
			break;

		case 'v': PARSE_COMPACT(HDR_VIA);           break;
		case 'f': PARSE_COMPACT(HDR_FROM);          break;
		case 'i': PARSE_COMPACT(HDR_CALLID);        break;
		case 'm': PARSE_COMPACT(HDR_CONTACT);       break;
		case 'l': PARSE_COMPACT(HDR_CONTENTLENGTH); break;
		case 'c': PARSE_COMPACT(HDR_CONTENTTYPE);   break;
		case 'k': PARSE_COMPACT(HDR_SUPPORTED);     break;
		case 'o': PARSE_COMPACT(HDR_EVENT);         break;
		}
		goto other;
        }

	     /* Double colon hasn't been found yet */
 dc_end:
       	p = skip_ws(p, end - p);
	if (*p != ':') {   
	        goto other;
	} else {
		hdr->name.len = p - hdr->name.s;
		return (p + 1);
	}

	     /* Unknown header type */
 other:    
	p = q_memchr(p, ':', end - p);
	if (!p) {        /* No double colon found, error.. */
		hdr->type = HDR_ERROR;
		hdr->name.s = 0;
		hdr->name.len = 0;
		return 0;
	} else {
		hdr->type = HDR_OTHER;
		hdr->name.len = p - hdr->name.s;
		return (p + 1);
	}
}
