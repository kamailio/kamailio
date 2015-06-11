#ifndef SDPOPS_API_H
#define SDPOPS_API_H
#include "../../str.h"

typedef int (*sdp_with_media_t)(struct sip_msg*, str*);
typedef int (*sdp_with_transport_t)(struct sip_msg*, str*, int);
typedef int (*sdp_with_ice_t)(struct sip_msg*);
typedef int (*sdp_keep_media_t)(struct sip_msg*, str*, str*);
typedef int (*sdp_remove_media_t)(struct sip_msg*, str*);

typedef struct sdpops_binds {
	sdp_with_media_t     sdp_with_media;
	sdp_with_media_t     sdp_with_active_media;
	sdp_with_transport_t sdp_with_transport;
	sdp_with_media_t     sdp_with_codecs_by_id;
	sdp_with_media_t     sdp_with_codecs_by_name;
	sdp_with_ice_t       sdp_with_ice;
	sdp_keep_media_t     sdp_keep_codecs_by_id;
	sdp_keep_media_t     sdp_keep_codecs_by_name;
	sdp_remove_media_t   sdp_remove_media;
	sdp_remove_media_t   sdp_remove_transport;
	sdp_remove_media_t   sdp_remove_line_by_prefix;
	sdp_remove_media_t   sdp_remove_codecs_by_id;
	sdp_remove_media_t   sdp_remove_codecs_by_name;
} sdpops_api_t;

typedef int (*bind_sdpops_f)(sdpops_api_t*);

int bind_sdpops(struct sdpops_binds*);

inline static int sdpops_load_api(sdpops_api_t *sob)
{
	bind_sdpops_f bind_sdpops_exports;
	if (!(bind_sdpops_exports = (bind_sdpops_f)find_export("bind_sdpops", 1, 0)))
	{
		LM_ERR("Failed to import bind_sdpops\n");
		return -1;
	}
	return bind_sdpops_exports(sob);
}

#endif /*SDPOPS_API_H*/
