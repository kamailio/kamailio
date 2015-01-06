/**
 *
 * Copyright (C) 2009 SIP-Router.org
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

#ifndef _TH_MSG_H_
#define _TH_MSG_H_

#include "../../parser/msg_parser.h"

int th_mask_via(sip_msg_t *msg);
int th_mask_callid(sip_msg_t *msg);
int th_mask_contact(sip_msg_t *msg);
int th_mask_record_route(sip_msg_t *msg);
int th_unmask_via(sip_msg_t *msg, str *cookie);
int th_unmask_callid(sip_msg_t *msg);
int th_flip_record_route(sip_msg_t *msg, int mode);
int th_unmask_ruri(sip_msg_t *msg);
int th_unmask_route(sip_msg_t *msg);
int th_unmask_refer_to(sip_msg_t *msg);
int th_update_hdr_replaces(sip_msg_t *msg);
char* th_msg_update(sip_msg_t *msg, unsigned int *olen);
int th_add_via_cookie(sip_msg_t *msg, struct via_body *via);
int th_add_hdr_cookie(sip_msg_t *msg);
hdr_field_t *th_get_hdr_cookie(sip_msg_t *msg);
int th_add_cookie(sip_msg_t *msg);
int th_route_direction(sip_msg_t *msg);
char* th_get_cookie(sip_msg_t *msg, int *clen);
int th_del_cookie(sip_msg_t *msg);
int th_skip_msg(sip_msg_t *msg);

#endif
