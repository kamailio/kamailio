/*
 * $Id$
 *
 * Copyright (C) 2005 Voice Sistem SRL
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
 *  2005-08-02  first version (bogdan)
 */

#ifndef RR_CB_H_
#define RR_CB_H_

#include "../../str.h"
#include "../../parser/msg_parser.h"


/* callback function prototype */
typedef void (rr_cb_t) (struct sip_msg* req, str *rr_param, void *param);
/* register callback function prototype */
typedef int (*register_rrcb_t)( rr_cb_t f, void *param);




struct rr_callback {
	int id;                   /* id of this callback - useless */
	rr_cb_t* callback;        /* callback function */
	void *param;              /* param to be passed to callback function */
	struct rr_callback* next; /* next callback element*/
};


void destroy_rrcb_lists();


/* register a RR callback */
int register_rrcb(rr_cb_t f, void *param );

/* run RR transaction callbacks */
void run_rr_callbacks( struct sip_msg *req, str *rr_param);


#endif


