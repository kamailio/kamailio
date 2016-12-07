/* 
 * Copyright (C) 2010 iptelorg GmbH
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/** Parser :: Reason Header Name Parsing Macros.
 * @file 
 *
 * @ingroup parser
 */

#ifndef __case_p_as_h
#define __case_p_as_h


#define SERT_CASE						\
	switch(LOWER_DWORD(val)) {			\
		case _sert_:					\
			p += 4;						\
			val = READ(p);				\
			ED_I_CASE;					\
	}									


#define ED_I_CASE						\
	switch(LOWER_DWORD(val)) {			\
		case _ed_i_:					\
			p += 4;						\
			val = READ(p);				\
			DENT_CASE;					\
	}									


#define DENT_CASE						\
	switch(LOWER_DWORD(val)) {			\
		case _dent_:					\
			p += 4;						\
			val = READ(p);				\
			ITY_CASE;					\
	}									

#define ITY_CASE						\
	switch(LOWER_DWORD(val)) {			\
		case _ity1_:					\
			hdr->type = HDR_PAI_T;		\
			hdr->name.len = 19;			\
			return (p + 4);				\
		case _ity2_:					\
			hdr->type = HDR_PAI_T;		\
			p+=4;						\
			goto dc_end;				\
	}									



#define p_as_CASE		\
	p += 4;				\
	val = READ(p);		\
	SERT_CASE;			\
	goto other;



#endif /*__case_p_as_h*/

/* vi: set ts=4 sw=4 tw=79:ai:cindent: */
