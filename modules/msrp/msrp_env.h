/**
 * Copyright (C) 2012 Daniel-Constantin Mierla (asipto.com)
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
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

#ifndef _MSRP_ENV_H_
#define _MSRP_ENV_H_

#include "../../parser/msg_parser.h"
#include "msrp_parser.h"

#define MSRP_ENV_SRCINFO	(1<<0)
#define MSRP_ENV_DSTINFO	(1<<1)

typedef struct msrp_env {
	msrp_frame_t *msrp;
	struct dest_info srcinfo;
	struct dest_info dstinfo;
	int envflags;
	int sndflags;
	int rplflags;
} msrp_env_t;

msrp_env_t *msrp_get_env(void);
void msrp_reset_env(void);
msrp_frame_t *msrp_get_current_frame(void);
int msrp_set_current_frame(msrp_frame_t *mf);
sip_msg_t *msrp_fake_sipmsg(msrp_frame_t *mf);

int msrp_env_set_dstinfo(msrp_frame_t *mf, str *addr, str *fsock, int flags);
int msrp_env_set_sndflags(msrp_frame_t *mf, int flags);
int msrp_env_set_rplflags(msrp_frame_t *mf, int flags);

#endif
