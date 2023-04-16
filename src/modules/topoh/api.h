/**
 *
 * Copyright (C) 2016 kamailio.org
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*!
 * \file
 * \brief Kamailio topoh ::
 * \ingroup topoh
 * Module: \ref topoh
 */

#ifndef _TH_API_H_
#define _TH_API_H_

#include "../../core/sr_module.h"

typedef int (*topoh_mask_callid_f)(str *icallid, str *ocallid);
typedef int (*topoh_unmask_callid_f)(str *icallid, str *ocallid);


typedef struct topoh_api {
	topoh_mask_callid_f mask_callid;
	topoh_unmask_callid_f unmask_callid;
} topoh_api_t;

typedef int (*bind_topoh_f)(topoh_api_t* api);
int bind_topoh(topoh_api_t* api);

/**
 * @brief Load the topoh API
 */
static inline int topoh_load_api(topoh_api_t *api)
{
	bind_topoh_f bindtopoh;

	bindtopoh = (bind_topoh_f)find_export("bind_topoh", 0, 0);
	if(bindtopoh == 0) {
		LM_ERR("cannot find bind_topoh\n");
		return -1;
	}
	if(bindtopoh(api)<0)
	{
		LM_ERR("cannot bind topoh api\n");
		return -1;
	}
	return 0;
}

#endif
