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
 *
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

#include "defs.h"


#include "sip_msg.h"
#include "../../dprint.h"
#include "../../mem/mem.h"
#include "../../data_lump.h"
#include "../../data_lump_rpl.h"
#include "../../ut.h"
#include "../../sip_msg_clone.h"
#include "../../fix_lumps.h"


/**
 * @brief Clone a SIP message
 * @warning Cloner does not clone all hdr_field headers (From, To, etc.). Pointers will reference pkg memory. Dereferencing will crash ser!
 * @param org_msg Original SIP message
 * @param sip_msg_len Length of the SIP message
 * @return Cloned SIP message, or NULL on error
 */
struct sip_msg*  sip_msg_cloner( struct sip_msg *org_msg, int *sip_msg_len )
{
	/* take care of the lumps only for replies if the msg cloning is 
	   postponed */
	if (org_msg->first_line.type==SIP_REPLY)
		/*cloning all the lumps*/
		return sip_msg_shm_clone(org_msg, sip_msg_len, 1);
	/* don't clone the lumps */
	return sip_msg_shm_clone(org_msg, sip_msg_len, 0);
}

/**
 * @brief Indicates wheter we have already cloned the msg lumps or not
 */
unsigned char lumps_are_cloned = 0;



/**
 * @brief Wrapper function for msg_lump_cloner() with some additional sanity checks
 * @param shm_msg SIP message in shared memory
 * @param pkg_msg SIP message in private memory
 * @return 0 on success, -1 on error
 */
int save_msg_lumps( struct sip_msg *shm_msg, struct sip_msg *pkg_msg)
{
	int ret;
	struct lump* add_rm;
	struct lump* body_lumps;
	struct lump_rpl* reply_lump;
	
	/* make sure that we do not clone the lumps twice */
	if (lumps_are_cloned) {
		LOG(L_DBG, "DEBUG: save_msg_lumps: lumps have been already cloned\n" );
		return 0;
	}
	/* sanity checks */
	if (unlikely(!shm_msg || ((shm_msg->msg_flags & FL_SHM_CLONE)==0))) {
		LOG(L_ERR, "ERROR: save_msg_lumps: BUG, there is no shmem-ized message"
			" (shm_msg=%p)\n", shm_msg);
		return -1;
	}
	if (unlikely(shm_msg->first_line.type!=SIP_REQUEST)) {
		LOG(L_ERR, "ERROR: save_msg_lumps: BUG, the function should be called only for requests\n" );
		return -1;
	}

#ifdef EXTRA_DEBUG
	membar_depends();
	if (shm_msg->add_rm || shm_msg->body_lumps || shm_msg->reply_lump) {
		LOG(L_ERR, "ERROR: save_msg_lumps: BUG, trying to overwrite the already cloned lumps\n");
		return -1;
	}
#endif

	/* needless to clone the lumps for ACK, they will not be used again */
	if (shm_msg->REQ_METHOD == METHOD_ACK)
		return 0;

	/* clean possible previous added vias/clen header or else they would 
	 * get propagated in the failure routes */
	free_via_clen_lump(&pkg_msg->add_rm);

	lumps_are_cloned = 1;
	ret=msg_lump_cloner(pkg_msg, &add_rm, &body_lumps, &reply_lump);
	if (likely(ret==0)){
		/* make sure the lumps are fully written before adding them to
		   shm_msg (in case someone accesses it in the same time) */
		membar_write();
		shm_msg->add_rm = add_rm;
		shm_msg->body_lumps = body_lumps;
		shm_msg->reply_lump = reply_lump;
	}
	return ret<0?-1:0;
}
