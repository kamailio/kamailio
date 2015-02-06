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

/** Kamailio core :: version strings.
 * @file ver.c
 * @ingroup: core
 */

#include "ver_defs.h"
#include "autover.h" /* REPO_VER, REPO_STATE */


const char full_version[] = SER_FULL_VERSION " " REPO_VER;
const char ver_name[] = NAME;
const char ver_version[] = VERSION;
const char ver_arch[] = ARCH;
const char ver_os[] = OS_QUOTED;
const char ver_id[] = REPO_HASH " " REPO_STATE;
#ifdef VERSION_NODATE
const char ver_compiled_time[] =  "" ;
#else
#ifdef VERSION_DATE
const char ver_compiled_time[] =  VERSION_DATE ;
#else
const char ver_compiled_time[] =  __TIME__ " " __DATE__ ;
#endif
#endif
const char ver_compiler[] = COMPILER;

const char ver_flags[] = SER_COMPILE_FLAGS;

/** hash-state. */
const char repo_ver[] = REPO_VER;
/** git hash. */
const char repo_hash[] = REPO_HASH;
/** state of the repository: dirty (uncommited changes) or "" */
const char repo_state[] = REPO_STATE;

/* vi: set ts=4 sw=4 tw=79:ai:cindent: */
