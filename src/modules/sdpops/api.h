/*
 * Copyright (C) 2011 Daniel-Constantin Mierla (asipto.com)
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

#ifndef SDPOPS_API_H
#define SDPOPS_API_H
#include "../../core/str.h"

typedef int (*sdp_with_media_t)(struct sip_msg *, str *);
typedef int (*sdp_with_transport_t)(struct sip_msg *, str *, int);
typedef int (*sdp_with_ice_t)(struct sip_msg *);
typedef int (*sdp_keep_media_t)(struct sip_msg *, str *, str *);
typedef int (*sdp_remove_media_t)(struct sip_msg *, str *);
typedef int (*sdp_remove_media_type_t)(struct sip_msg *, str *, str *);

typedef struct sdpops_binds
{
	sdp_with_media_t sdp_with_media;
	sdp_with_media_t sdp_with_active_media;
	sdp_with_transport_t sdp_with_transport;
	sdp_with_media_t sdp_with_codecs_by_id;
	sdp_with_media_t sdp_with_codecs_by_name;
	sdp_with_ice_t sdp_with_ice;
	sdp_keep_media_t sdp_keep_codecs_by_id;
	sdp_keep_media_t sdp_keep_codecs_by_name;
	sdp_remove_media_t sdp_remove_media;
	sdp_remove_media_t sdp_remove_transport;
	sdp_remove_media_type_t sdp_remove_line_by_prefix;
	sdp_remove_media_type_t sdp_remove_codecs_by_id;
	sdp_remove_media_type_t sdp_remove_codecs_by_name;
} sdpops_api_t;

typedef int (*bind_sdpops_f)(sdpops_api_t *);

int bind_sdpops(struct sdpops_binds *);

inline static int sdpops_load_api(sdpops_api_t *sob)
{
	bind_sdpops_f bind_sdpops_exports;
	if(!(bind_sdpops_exports =
					   (bind_sdpops_f)find_export("bind_sdpops", 1, 0))) {
		LM_ERR("Failed to import bind_sdpops\n");
		return -1;
	}
	return bind_sdpops_exports(sob);
}

#endif /*SDPOPS_API_H*/
