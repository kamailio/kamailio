/* 
 * Copyright (C) 2008 iptelorg GmbH
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
 * @file 
 * @brief  ser/kamailio/openser compatibility macros & vars.
 */
/* 
 * History:
 * --------
 *  2008-11-29  initial version (andrei)
 */


#ifndef _sr_compat_h
#define _sr_compat_h

/** max compat mode: support as many features as possible from all xSERs */
#define SR_COMPAT_MAX 0
/** maximum compatibility mode with ser */
#define SR_COMPAT_SER 1
/** maximum compatibility mode with kamailio/openser */
#define SR_COMPAT_KAMAILIO 2
#define SR_COMPAT_OPENSER 2


extern int sr_compat;
extern int sr_cfg_compat;

#endif /* _sr_compat_h */
