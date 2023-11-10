/*
 * NATS module interface
 *
 * Copyright (C) 2021 Voxcom Inc
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * Kamailio is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * Kamailio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 *
 */

#ifndef __NATS_PUB_H_
#define __NATS_PUB_H_

#include <fcntl.h>
#include "../../core/fmsg.h"
#include "../../core/mod_fix.h"

typedef struct _nats_pub_delivery
{
	char *subject;
	char *payload;
	char *reply;
} nats_pub_delivery, *nats_pub_delivery_ptr;

nats_pub_delivery_ptr _nats_pub_delivery_new(
		str subject, str payload, str reply);
void nats_pub_free_delivery_ptr(nats_pub_delivery_ptr ptr);
int w_nats_publish_f(sip_msg_t *msg, char *subj, char *payload, char *reply);
int w_nats_publish(sip_msg_t *msg, str subj_s, str payload_s, str reply_s);
int fixup_publish_get_value(void **param, int param_no);
int fixup_publish_get_value_free(void **param, int param_no);

#endif
