/*
 * Copyright (C) 2001-2003 FhG Fokus
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
 * @brief TM :: Message cloning functionality
 * 
 * Cloning a message into shared memory (TM keeps a snapshot
 * of messages in memory); note that many operations, which
 * allocate pkg memory (such as parsing) cannot be used with
 * a cloned message -- it would result in linking pkg structures
 * to shmem msg and eventually in a memory error.
 * 
 * The cloned message is stored in a single memory fragment to
 * save too many shm_mallocs -- these are expensive as they
 * not only take lookup in fragment table but also a shmem lock
 * operation (the same for shm_free)
 * 
 * Allow postponing the cloning of SIP msg:
 * t_newtran() copies the requests to shm mem without the lumps,
 * and t_forward_nonack() clones the lumps later when it is called
 * the first time.
 * @ingroup tm
 */

#ifndef _SIP_MSG_H
#define _SIP_MSG_H

#include "defs.h"


#include "../../parser/msg_parser.h"
#include "../../mem/shm_mem.h"


#include "../../atomic_ops.h" /* membar_depends() */

/**
 * @brief Helper function to free a SIP message
 * 
 * msg is a reply: one memory block was allocated
 * 
 * msg is a request: two memory blocks were allocated:
 * - one for the sip_msg struct
 * - another one for the lumps which is linked to add_rm, body_lumps,
 *   or reply_lump
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


/**
 * @brief Free a SIP message safely, with locking
 */
#define  sip_msg_free(_p_msg) _sip_msg_free(shm_free, _p_msg)
/**
 * @brief Free a SIP message unsafely, without locking
 */
#define  sip_msg_free_unsafe(_p_msg) _sip_msg_free(shm_free_unsafe, _p_msg)

/**
 * @brief Clone a SIP message
 * @warning Cloner does not clone all hdr_field headers (From, To, etc.). Pointers will reference pkg memory. Dereferencing will crash ser!
 * @param org_msg Original SIP message
 * @param sip_msg_len Length of the SIP message
 * @return Cloned SIP message, or NULL on error
 */
struct sip_msg*  sip_msg_cloner( struct sip_msg *org_msg, int *sip_msg_len );

/**
 * @brief Indicates wheter we have already cloned the msg lumps or not
 */
extern unsigned char lumps_are_cloned;

/**
 * @brief Wrapper function for msg_lump_cloner() with some additional sanity checks
 * @param shm_msg SIP message in shared memory
 * @param pkg_msg SIP message in private memory
 * @return 0 on success, -1 on error
 */
int save_msg_lumps( struct sip_msg *shm_msg, struct sip_msg *pkg_msg);


#endif
