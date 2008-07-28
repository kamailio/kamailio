/*
 * $Id$
 *
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/*! \file
 * \brief TM :: SIP message handling
 *
 * \ingroup tm
 * - Module: \ref tm
 */

#ifndef _SIP_MSG_H
#define _SIP_MSG_H

#include "../../parser/msg_parser.h"
#include "../../mem/shm_mem.h"

#define  sip_msg_free(_p_msg) shm_free( (_p_msg ))
#define  sip_msg_free_unsafe(_p_msg) shm_free_unsafe( (_p_msg) )


struct sip_msg*  sip_msg_cloner( struct sip_msg *org_msg, int *sip_msg_len );


static inline void clean_msg_clone(struct sip_msg *msg,void *min, void *max)
{
	struct hdr_field *hdr;

	/* free header's parsed structures that were added in pkg mem */
	for( hdr=msg->headers ; hdr ; hdr=hdr->next ) {
		if ( hdr->parsed && hdr_allocs_parse(hdr) &&
		(hdr->parsed<min || hdr->parsed>=max)) {
			/* header parsed filed doesn't point inside uas.request memory
			 * chunck -> it was added by failure funcs.-> free it as pkg */
			LM_DBG("removing hdr->parsed %d\n",	hdr->type);
			clean_hdr_field(hdr);
			hdr->parsed = 0;
		}
	}
}



#endif
