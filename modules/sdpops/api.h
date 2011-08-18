#ifndef SDPOPS_API_H
#define SDPOPS_API_H
#include "../../str.h"

typedef int (*sdp_with_media_t)(struct sip_msg*, str*);

typedef struct sdpops_binds {
	sdp_with_media_t sdp_with_media;
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
