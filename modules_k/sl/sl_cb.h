/*
 * $Id$
 *
 * Copyright (C) 2006 Voice Sistem SRL
 *
 * This file is part of openser, a free SIP server.
 *
 * openser is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * openser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 *
 * History:
 * ---------
 *  2006-03-29  first version (bogdan)
 */

#ifndef SL_CB_H_
#define SL_CB_H_

#include "../../str.h"
#include "../../ip_addr.h"
#include "../../parser/msg_parser.h"


struct sl_cb_param {
	str  *buffer;
	int  buf_len;
	int  code;
	char *reason;
	union sockaddr_union *dst;
	void *param;
};

/* callback function prototype */
typedef void (sl_cb_t) (struct sip_msg* req, struct sl_cb_param *sl_param);
/* register callback function prototype */
typedef int (*register_slcb_t)( sl_cb_t f, void *param);




struct sl_callback {
	int id;                    /* id of this callback - useless */
	sl_cb_t* callback;         /* callback function */
	void* param;               /* param to be passed to callback function */
	struct sl_callback* next;  /* next callback element*/
};


void destroy_slcb_lists();


/* register a SL callback */
int register_slcb(sl_cb_t f, void *param );

/* run SL transaction callbacks */
void run_sl_callbacks( struct sip_msg *req, str *buffer, int code,
		char *reason, union sockaddr_union *to);


#endif


