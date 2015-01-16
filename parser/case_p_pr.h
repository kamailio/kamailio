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

#ifndef __case_p_pr_h
#define __case_p_pr_h


#define EFER_CASE						\
	switch(LOWER_DWORD(val)) {			\
		case _efer_:					\
			p += 4;						\
			val = READ(p);				\
			RED__CASE;					\
	}									


#define RED__CASE						\
	switch(LOWER_DWORD(val)) {			\
		case _red__:					\
			p += 4;						\
			val = READ(p);				\
			IDEN_CASE;					\
	}									


#define IDEN_CASE						\
	switch(LOWER_DWORD(val)) {			\
		case _iden_:					\
			p += 4;						\
			val = READ(p);				\
			TITY_p_pr_CASE;				\
	}									

#define TITY_p_pr_CASE					\
	switch(LOWER_DWORD(val)) {			\
		case _tity_:					\
			hdr->type = HDR_PPI_T;		\
			p+=4;						\
			goto dc_end;				\
	}									



#define p_pr_CASE		\
	p += 4;				\
	val = READ(p);		\
	EFER_CASE;			\
	goto other;



#endif /*__case_p_pr_h*/

/* vi: set ts=4 sw=4 tw=79:ai:cindent: */
