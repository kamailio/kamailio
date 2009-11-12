/* 
 * $Id$
 * 
 * Copyright (C) 2009 iptelorg GmbH
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
/*
 * sctp_sockopts.h - portability fixes / compatibility defines for some
 *                   sctp socket options
 */
/*
 * History:
 * --------
 *  2009-11-12  initial version (andrei)
*/

#ifndef __sctp_sockopts_h
#define __sctp_sockopts_h

#include <netinet/sctp.h>

#ifndef SCTP_DELAYED_SACK
#ifdef	SCTP_DELAYED_ACK
/* on linux lksctp/libsctp <= 1.0.11 (and possible future versions)
 * SCTP_DELAYED_ACK is used instead of SCTP_DELAYED_SACK (typo?)
 */
#define	SCTP_DELAYED_SACK SCTP_DELAYED_ACK
#endif	/* SCTP_DELAYED_ACK */
#endif /* SCTP_DELAYED_SACK */

#endif /*__sctp_sockopts_h*/

/* vi: set ts=4 sw=4 tw=79:ai:cindent: */
