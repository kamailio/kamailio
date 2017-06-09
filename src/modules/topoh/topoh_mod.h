/**
 *
 * Copyright (C) 2009 SIP-Router.org
 * Copyright (C) 2017 Alexandr Dubovikov. SIPCAPTURE.ORG
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

/*!
 * \file
 * \brief Kamailio topoh ::
 * \ingroup topoh
 * Module: \ref topoh
 */

#ifndef _TH_MOD_H_
#define _TH_MOD_H_

extern int th_trust_mode; /* Trust mode by default */
extern int th_allow_ip_check; /* Allow IP check in tables */
extern str db_url;        /* Database URL */
extern str address_table; /* Name of address table */
extern str trust_col;     /* Name of address trust column */
extern str ip_addr_col;   /* Name of ip address column */
extern str mask_col;      /* Name of mask column */
extern str port_col;      /* Name of port column */
extern int peer_tag_mode; /* Matching mode */
extern str tag_col;       /* Name of tag column */

#endif
