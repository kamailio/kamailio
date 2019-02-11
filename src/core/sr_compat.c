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

/*!
 * \file
 * \brief Kamailio core :: ser/kamailio/openser compatibility macros & vars.
 * \ingroup core
 * Module: \ref core
 */

/* 
 * History:
 * --------
 *  2008-11-29  initial version (andrei)
 */


#include "sr_compat.h"

/**
 * compatibility modes:
 *  - SR_COMPAT_SER - strict compatibiliy with ser ($xy is avp)
 *  - SR_COMPAT_KAMAILIO - strict compatibiliy with kamailio ($xy is pv)
 *  - SR_COMPAT_MAX - max compatibiliy ($xy tried as pv, if not found, is avp)
 */
#ifdef SR_SER
#define SR_DEFAULT_COMPAT SR_COMPAT_SER
#elif defined SR_KAMAILIO || defined SR_OPENSER
#define SR_DEFAULT_COMPAT SR_COMPAT_MAX
#elif defined SR_ALL || defined SR_MAX_COMPAT
#define SR_DEFAULT_COMPAT SR_COMPAT_MAX
#else
/* default */
#define SR_DEFAULT_COMPAT SR_COMPAT_MAX
#endif

int sr_compat=SR_DEFAULT_COMPAT;
int sr_cfg_compat=SR_DEFAULT_COMPAT;
