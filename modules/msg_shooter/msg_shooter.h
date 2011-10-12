/*$Id$
 *
 * Copyright (C) 2011 iptelorg GmbH
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


#ifndef _MSG_SHOOTER_H
#define _MSG_SHOOTER_H

#include "parser/msg_parser.h"

/* get method of the request */
int smsg_create(struct sip_msg *_msg, char *_param1, char *_param2);

/* free allocated memory */
void smsg_destroy(void);

/* set From and To headers of the request */
int smsg_from_to(struct sip_msg *_msg, char *_param1, char *_param2);

/* append headers and optionally body to the request */
int smsg_append_hdrs(struct sip_msg *_msg, char *_param1, char *_param2);

/* shoots a request to a destination outside of a dialog */
int smsg(struct sip_msg *_msg, char *_param1, char *_param2);

/* sents on_reply route which will be called later */
int smsg_on_reply(struct sip_msg *_msg, char *_param1, char *_param2);

#endif /* _MSG_SHOOTER_H */
