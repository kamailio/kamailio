/*
 * Copyright (C) 2017-2018 Julien Chavanton jchavanton@gmail.com
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

#ifndef rms_h
#define rms_h

#include "../../core/data_lump.h"
#include "../../core/sr_module.h"
#include "../../core/mod_fix.h"
#include "../../core/parser/sdp/sdp_helpr_funcs.h"
#include "../../core/parser/parse_from.h"
#include "../../core/parser/parse_content.h"
#include "../../core/data_lump_rpl.h"
#include "../../core/clist.h"
#include "../../core/parser/contact/parse_contact.h"

#include "../tm/tm_load.h"
#include "../sdpops/api.h"

#include "rms_util.h"
#include "rms_sdp.h"
#include "rms_media.h"
#include "rms_dialog_info.h"

extern gen_lock_t *dialog_list_mutex;


typedef struct rms
{
	int udp_start_port;
	int udp_end_port;
	int udp_last_port;
	char *local_ip;
} rms_t;

extern struct tm_binds tmb;


#endif
