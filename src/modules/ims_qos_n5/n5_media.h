/*
 * ims_qos_n5 - P-CSCF media authorisation over N5 (Npcf_PolicyAuthorization)
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

#ifndef _IMS_QOS_N5_MEDIA_H_
#define _IMS_QOS_N5_MEDIA_H_

#include <jansson.h>
#include "../../core/parser/msg_parser.h"

/* Build the medComponents map (TS 29.514 MediaComponent, keyed by medCompN)
 * from an SDP offer/answer pair. ue_sdp/peer_sdp are the messages carrying
 * the UE's and the far end's SDP. The UE's connection address is written
 * to ue_ip (buffer of ue_ip_size) when found. Returns a new JSON object, or
 * NULL if no media component could be derived. */
json_t *n5_build_med_components(struct sip_msg *ue_sdp,
		struct sip_msg *peer_sdp, char *ue_ip, int ue_ip_size);

#endif
