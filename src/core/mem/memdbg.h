/*
 * Copyright (C) 2006 iptelorg GmbH
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
 * \brief Malloc debug messages
 * \ingroup mem
 */


#ifndef _memdbg_h
#define _memdbg_h

#include "../cfg/cfg.h" /* memdbg*/

extern int memdbg;

#ifdef NO_DEBUG
	#ifdef __SUNPRO_C
		#define MDBG(...)
	#else
		#define MDBG(fmt, args...)
	#endif
#else /* NO_DEBUG */
	#ifdef __SUNPRO_C
		#define MDBG(...) LOG(cfg_get(core, core_cfg, memdbg), __VA_ARGS__)
	#else
		#define MDBG(fmt, args...) \
			LOG(cfg_get(core, core_cfg, memdbg), fmt,  ## args)
	#endif
#endif /* NO_DEBUG */


#endif
