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
#ifndef __case_reas_h
#define __case_reas_h


#define ON_CASE							\
	switch(LOWER_DWORD(val)) {			\
		case _on1_:						\
			hdr->type = HDR_REASON_T;	\
			hdr->name.len = 6;			\
			return (p + 3);				\
		case _on2_:						\
			hdr->type = HDR_REASON_T;	\
			hdr->name.len = 7;			\
			return (p + 4);				\
		case _on3_:						\
			hdr->type = HDR_REASON_T;	\
			p+=4;						\
			goto dc_end;				\
	}									\
	if ((LOWER_DWORD(val)&0x00ffffff) ==\
				(_on1_&0x00ffffff)){	\
			hdr->type = HDR_REASON_T;	\
			hdr->name.len = 6;			\
			return (p+3);				\
	}



#define reas_CASE		\
	p += 4;				\
	val = SAFE_READ(p, end - p);	\
	ON_CASE;			\
	goto other;




#endif /*__case_reas_h*/

/* vi: set ts=4 sw=4 tw=79:ai:cindent: */
