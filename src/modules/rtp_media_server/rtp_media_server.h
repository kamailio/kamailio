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


ser_lock_t session_list_mutex;


typedef struct rms
{
	int udp_start_port;
	int udp_end_port;
	int udp_last_port;
	char *local_ip;
} rms_t;

struct tm_binds tmb;

typedef struct ms_res
{
	AudioStream *audio_stream;
	RtpProfile *rtp_profile;
} ms_res_t;

typedef enum rms_action_type
{
	RMS_NONE,
	RMS_START,
	RMS_STOP,
	RMS_HANGUP,
	RMS_PLAY,
	RMS_DONE,
} rms_action_type_t;

typedef struct rms_action
{
	struct rms_action *next;
	struct rms_action *prev;
	str param;
	str route;
	rms_action_type_t type;
} rms_action_t;

typedef struct rms_session_info
{
	struct rms_session_info *next;
	struct rms_session_info *prev;
	rms_sdp_info_t sdp_info_offer;
	rms_sdp_info_t sdp_info_answer;
	str callid;
	str local_ip;
	str local_uri;
	str local_tag;
	str remote_uri;
	str remote_tag;
	str contact_uri;
	int cseq;
	ms_res_t ms;
	call_leg_media_t caller_media;
	call_leg_media_t callee_media;
	rms_action_t action;
} rms_session_info_t;

#endif
