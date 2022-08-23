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

#include "defs.h"
#include "nats_pub.h"

extern int *nats_pub_worker_pipes;
extern int nats_pub_workers_num;
extern nats_pub_worker_t *nats_pub_workers;
int pub_worker = 0;

int fixup_publish_get_value(void **param, int param_no)
{
	if(param_no == 1 || param_no == 2) {
		return fixup_spve_null(param, 1);
	}
	LM_ERR("invalid parameter number <%d>\n", param_no);
	return -1;
}

int fixup_publish_get_value_free(void **param, int param_no)
{
	if(param_no == 1 || param_no == 2) {
		fixup_free_spve_null(param, 1);
		return 0;
	}
	LM_ERR("invalid parameter number <%d>\n", param_no);
	return -1;
}

nats_pub_delivery_ptr _nats_pub_delivery_new(str subject, str payload)
{
	nats_pub_delivery_ptr p =
			(nats_pub_delivery_ptr)shm_malloc(sizeof(nats_pub_delivery));
	memset(p, 0, sizeof(nats_pub_delivery));

	p->subject = shm_malloc(subject.len + 1);
	strcpy(p->subject, subject.s);
	p->subject[subject.len] = '\0';

	p->payload = shm_malloc(payload.len + 1);
	strcpy(p->payload, payload.s);
	p->payload[payload.len] = '\0';

	return p;
}

static int _w_nats_publish_f(str subj, str payload, int worker)
{
	nats_pub_delivery_ptr ptr = _nats_pub_delivery_new(subj, payload);
	if(write(nats_pub_worker_pipes[worker], &ptr, sizeof(ptr)) != sizeof(ptr)) {
		LM_ERR("failed to publish message %d, write to "
			   "command pipe: %s\n",
				getpid(), strerror(errno));
	}
	return 1;
}

int w_nats_publish_f(sip_msg_t *msg, char *subj, char *payload)
{
	str subj_s = STR_NULL;
	str payload_s = STR_NULL;
	if(fixup_get_svalue(msg, (gparam_t *)subj, &subj_s) < 0) {
		LM_ERR("failed to get subj value\n");
		return -1;
	}
	if(fixup_get_svalue(msg, (gparam_t *)payload, &payload_s) < 0) {
		LM_ERR("failed to get subj value\n");
		return -1;
	}

	return w_nats_publish(msg, subj_s, payload_s);
}

int w_nats_publish(sip_msg_t *msg, str subj_s, str payload_s)
{
	// round-robin pub workers
	pub_worker++;
	if(pub_worker >= nats_pub_workers_num) {
		pub_worker = 0;
	}

	return _w_nats_publish_f(subj_s, payload_s, pub_worker);
}

void _nats_pub_worker_cb(uv_poll_t *handle, int status, int events)
{
	natsStatus s = NATS_OK;
	nats_pub_delivery_ptr ptr;
	nats_pub_worker_t *worker =
			(nats_pub_worker_t *)uv_handle_get_data((uv_handle_t *)handle);

	if(read(worker->fd, &ptr, sizeof(ptr)) != sizeof(ptr)) {
		LM_ERR("failed to read from command pipe: %s\n", strerror(errno));
		return;
	}
	if((s = natsConnection_PublishString(worker->nc->conn, ptr->subject, ptr->payload))
			!= NATS_OK) {
		LM_ERR("could not publish to subject [%s] payload [%s] error [%s]\n", ptr->subject, ptr->payload,
				natsStatus_GetText(s));
	}
	nats_pub_free_delivery_ptr(ptr);
}

void nats_pub_free_delivery_ptr(nats_pub_delivery_ptr ptr)
{
	if(ptr == NULL)
		return;
	if(ptr->subject)
		shm_free(ptr->subject);
	if(ptr->payload)
		shm_free(ptr->payload);
	shm_free(ptr);
}
