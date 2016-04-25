/*
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
 */

#ifndef __NSQ_READER_H_
#define __NSQ_READER_H_

#include <json.h>

#include "../../sr_module.h"
#include "../../fmsg.h"
#include "nsq.h"
#include "nsq_json.h"


int nsq_pv_get_event_payload(struct sip_msg *msg, pv_param_t *param, pv_value_t *res);
int nsq_consumer_fire_event(char *routename);
int nsq_consumer_event(char *payload, char *channel, char *topic);

void nsq_message_handler(struct NSQReader *rdr, struct NSQDConnection *conn, struct NSQMessage *msg, void *ctx);

#endif /* __NSQ_READER_H_ */
