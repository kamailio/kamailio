/*
 * Rtpengine Module
 *
 * Copyright (C) 2003-2008 Sippy Software, Inc., http://www.sippysoft.com
 * Copyright (C) 2014-2015 Sipwise GmbH, http://www.sipwise.com
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Kamailio is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * Kamailio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef _RTPENGINE_API_H_
#define _RTPENGINE_API_H_

#include "../../core/parser/msg_parser.h"

typedef int (*rtpengine_start_recording_f)(struct sip_msg *msg);
typedef int (*rtpengine_answer_f)(struct sip_msg *msg, str *str);
typedef int (*rtpengine_offer_f)(struct sip_msg *msg, str *str);
typedef int (*rtpengine_delete_f)(struct sip_msg *msg, str *str);

typedef struct rtpengine_api
{
	rtpengine_start_recording_f rtpengine_start_recording;
	rtpengine_answer_f rtpengine_answer;
	rtpengine_offer_f rtpengine_offer;
	rtpengine_delete_f rtpengine_delete;
} rtpengine_api_t;

typedef int (*bind_rtpengine_f)(rtpengine_api_t *api);

/**
 * @brief Load the RTPENGINE API
 */
static inline int rtpengine_load_api(rtpengine_api_t *api)
{
	bind_rtpengine_f bindrtpengine;

	bindrtpengine = (bind_rtpengine_f)find_export("bind_rtpengine", 0, 0);
	if(bindrtpengine == 0) {
		LM_ERR("cannot find bind_rtpengine\n");
		return -1;
	}
	if(bindrtpengine(api) < 0) {
		LM_ERR("cannot bind rtpengine api\n");
		return -1;
	}

	return 0;
}


#endif /* _RTPENGINE_API_H_ */
