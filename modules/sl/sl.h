/*
 * Copyright (C) 2001-2006 FhG FOKUS
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

/**
 * @file
 * @brief SL - API definitions
 * @ingroup sl
 * Module: @ref sl
 */
#ifndef _SL_H_
#define _SL_H_

#include "../../sr_module.h"
#include "../../parser/msg_parser.h"

/* callback for SL events */
#define SLCB_REPLY_READY       (1<<0)  /* stateless reply ready to be sent */
#define SLCB_ACK_FILTERED      (1<<1)  /* stateless ACK filtered */

/**
 * @brief SL callback parameter structure
 */
typedef struct sl_cbp {
	unsigned int type;     /* type of callback */
	sip_msg_t *req;        /* SIP request to reply to, or filtered ACK */
	int  code;             /* reply status code */
	str  *reason;          /* reply reason phrase */
	str  *reply;           /* raw content of the reply to be sent */
	struct dest_info *dst; /* info about destination address */
	void *cbp;             /* parameter from callback registration */
} sl_cbp_t;

/**
 * @brief SL callback function prototype
 */
typedef void (*sl_cbf_f)(sl_cbp_t *slcbp);

/**
 * @brief SL callback structure definition
 */
typedef struct sl_cbelem {
	unsigned int type;         /* type of callback - can be a mask of types */
	sl_cbf_f cbf;              /* pointer to callback function */
	void *cbp;                 /* param to callback function */
	struct sl_cbelem* next;    /* next sl_cbelem value */
} sl_cbelem_t;

void sl_destroy_callbacks_list(void);

typedef int (*sl_register_cb_f)(sl_cbelem_t *cbe);
int sl_register_callback(sl_cbelem_t *cbe);

void sl_run_callbacks(unsigned int type, struct sip_msg *req,
		int code, char *reason, str *reply, struct dest_info *dst);

/* prototypes for SL API funtions */
typedef int (*get_reply_totag_f)(struct sip_msg *msg, str *tag);
typedef int (*send_reply_f)(struct sip_msg *msg, int code, str *reason);
typedef int (*sl_send_reply_f)(struct sip_msg *msg, int code, char *reason);
typedef int (*sl_send_sreply_f)(struct sip_msg *msg, int code, str *reason);
typedef int (*sl_send_dreply_f)(struct sip_msg *msg, int code, str *reason,
		str *tag);

/**
 * @brief Stateless (sl) API structure
 */
typedef struct sl_api {
	sl_send_reply_f  zreply; /* send sl reply, reason is charz */
	sl_send_sreply_f sreply; /* send sl reply, reason is str */
	sl_send_dreply_f dreply; /* send sl reply with tag, reason is str */
	send_reply_f     freply; /* send sl reply with tag, reason is str */
	get_reply_totag_f get_reply_totag;
	sl_register_cb_f  register_cb;
} sl_api_t;

typedef int (*bind_sl_f)(sl_api_t* api);

/**
 * @brief Load the SL API
 */
static inline int sl_load_api(sl_api_t *slb)
{
	bind_sl_f bindsl;

	bindsl = (bind_sl_f)find_export("bind_sl", 0, 0);
	if ( bindsl == 0) {
		LM_ERR("cannot find bind_sl\n");
		return -1;
	}
	if (bindsl(slb)==-1)
	{
		LM_ERR("cannot bind sl api\n");
		return -1;
	}
	return 0;
}

#endif /* _SL_H_ */
