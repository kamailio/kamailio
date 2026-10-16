/**
 * Copyright (C) 2019-2026 Alexey Volokitin (didww.com)
 * Copyright (C) 2025-2026 Michael Furmur (didww.com)
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * This file is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#ifndef _VL_CURL_DEFS_H_
#define _VL_CURL_DEFS_H_

#include <curl/curl.h>

#define easy_setopt(curl, opt, val)                             \
	if(CURLE_OK != curl_easy_setopt(curl, opt, val)) {          \
		LM_ERR("curl_easy_setopt error for option " #opt "\n"); \
		return -1;                                              \
	}

#define multi_setopt(curl_multi, opt, val)                       \
	if(CURLM_OK != curl_multi_setopt(curl_multi, opt, val)) {    \
		LM_ERR("curl_multi_setopt error for option " #opt "\n"); \
		return -1;                                               \
	}

#endif
