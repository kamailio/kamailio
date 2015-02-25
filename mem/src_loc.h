/* 
 * Copyright (C) 2009 iptelorg GmbH
 *
 * This file is part of Kamailio, a free SIP server.
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

/**
 * \file
 * \brief Helper definitions for internal memory manager
 * 
 * Helper definitions for internal memory manager, defines for src location
 * (function name, module a.s.o.), used for recording a *malloc()/ *free()
 * caller. Expects MOD_NAME defined for modules (if it's not defined "core"
 * will be assumed).
 * 
 * Defines:
 * - _SRC_FUNCTION_  - current function name
 * - _SRC_FILENAME_  - current .c filename
 * - _SRC_LINE_      - current line
 * - _SRC_MODULE_    - module name, lib name or "<core>" (depends on MOD_NAME
 * being properly set)
 * - _SRC_LOC_       - module name + file name
 * \ingroup mem
 */


#ifndef __src_loc_h
#define __src_loc_h


/* C >= 99 has __func__, older gcc versions have __FUNCTION__ */
#ifndef _SRC_FUNCTION_
#	if __STDC_VERSION__ < 199901L
#		if __GNUC__ >= 2
#			define _SRC_FUNCTION_ __FUNCTION__
#		else
#			define _SRC_FUNCTION_ ""
#		endif
#	else
#		define _SRC_FUNCTION_ __func__
#	endif /* __STDC_VERSION_ < 199901L */
#endif /* _FUNC_NAME_ */


#ifndef _SRC_FILENAME_
#	define _SRC_FILENAME_ __FILE__
#endif /* _SRC_FILENAME_ */


#ifndef _SRC_LINE_
#	define _SRC_LINE_ __LINE__
#endif /* _SRC_LINE_ */


#ifndef _SRC_MODULE_
#	ifdef MOD_NAME
#		define _SRC_MODULE_ MOD_NAME
#	else
#		define _SRC_MODULE_ "<core>"
#	endif /* MOD_NAME */
#endif /* _SRC_MODULE_ */


#ifndef _SRC_LOC_
#	define _SRC_LOC_ _SRC_MODULE_ ": " _SRC_FILENAME_
#endif /*_SRC_LOC_ */


#endif /*__src_loc_h*/

/* vi: set ts=4 sw=4 tw=79:ai:cindent: */
