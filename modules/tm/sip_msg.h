/*
 * $Id$
 *
 * Copyright (C) 2001-2003 FhG Fokus
 *
 * This file is part of ser, a free SIP server.
 *
 * ser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * For a license to use the ser software under conditions
 * other than those described here, or to purchase support for this
 * software, please contact iptel.org by e-mail at the following addresses:
 *    info@iptel.org
 *
 * ser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */



#ifndef _SIP_MSG_H
#define _SIP_MSG_H

#include "defs.h"


#include "../../parser/msg_parser.h"
#include "../../mem/shm_mem.h"

/* Allow postponing the cloning of SIP msg:
 * t_newtran() copies the requests to shm mem without the lumps,
 * and t_forward_nonack() clones the lumps later when it is called
 * the first time.
 * Replies use only one memory block.
 */

#include "../../atomic_ops.h" /* membar_depends() */

/* msg is a reply: one memory block was allocated
 * msg is a request: two memory blocks were allocated:
 *	- one for the sip_msg struct
 *	- another one for the lumps which is linked to
 *		add_rm, body_lumps, or reply_lump. 
 */
#define  _sip_msg_free(_free_func, _p_msg) \
		do{ \
			if (_p_msg->first_line.type==SIP_REPLY) { \
				_free_func( (_p_msg) ); \
			} else { \
				membar_depends(); \
				if ((_p_msg)->add_rm) \
					_free_func((_p_msg)->add_rm); \
				else if ((_p_msg)->body_lumps) \
					_free_func((_p_msg)->body_lumps); \
				else if ((_p_msg)->reply_lump) \
					_free_func((_p_msg)->reply_lump); \
									  \
				_free_func( (_p_msg) ); \
			} \
		}while(0)


#define  sip_msg_free(_p_msg) _sip_msg_free(shm_free, _p_msg)
#define  sip_msg_free_unsafe(_p_msg) _sip_msg_free(shm_free_unsafe, _p_msg)


struct sip_msg*  sip_msg_cloner( struct sip_msg *org_msg, int *sip_msg_len );

extern unsigned char lumps_are_cloned;

int save_msg_lumps( struct sip_msg *shm_msg, struct sip_msg *pkg_msg);


#endif
