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

#ifndef rms_session_info_h
#define rms_session_info_h
// #include "rtp_media_server.h"
#include "rms_media.h"
//typedef struct rms_action rms_action_t;
typedef struct rms_session_info rms_session_info_t;
// struct call_leg_media;
// typedef struct call_leg_media call_leg_media_t;

typedef enum rms_action_type {
	RMS_NONE,
	RMS_START,
	RMS_STOP,
	RMS_HANGUP,
	RMS_PLAY,
	RMS_BRIDGING,
	RMS_BRIDGED,
	RMS_DONE,
} rms_action_type_t;

typedef struct rms_tm_info
{
	unsigned int hash_index;
	unsigned int label;
} rms_tm_info_t;

typedef struct rms_action
{
	struct rms_action *next;
	struct rms_action *prev;
	str param;
	str route;
	rms_action_type_t type;
	rms_tm_info_t tm_info;
	struct rms_session_info *si;
	struct cell *cell;
} rms_action_t;

int rms_check_msg(struct sip_msg *msg);
rms_action_t *rms_action_new(rms_action_type_t t);
int init_rms_session_list();
rms_session_info_t *rms_session_search(struct sip_msg *msg);
rms_session_info_t *rms_session_search_sync(struct sip_msg *msg);
void rms_session_add(rms_session_info_t *si);
void rms_session_rm(rms_session_info_t *si);
int rms_session_free(rms_session_info_t *si);
rms_session_info_t *rms_session_new(struct sip_msg *msg);
rms_session_info_t *rms_session_new_bleg(struct sip_msg *msg);
int rms_sessions_dump_f(struct sip_msg *msg, char *param1, char *param2);
rms_session_info_t *rms_get_session_list(void);

typedef struct ms_res
{
	AudioStream *audio_stream;
	RtpProfile *rtp_profile;
} ms_res_t;

typedef enum rms_session_state {
	RMS_ST_DEFAULT,
	RMS_ST_CONNECTING,
	RMS_ST_CONNECTED,
	RMS_ST_DISCONNECTING,
	RMS_ST_DISCONNECTED,
} rms_session_state_t;

typedef struct rms_session_info
{
	struct rms_session_info *next;
	struct rms_session_info *prev;
	rms_sdp_info_t sdp_info_offer;
	rms_sdp_info_t sdp_info_answer;
	str callid;
	str local_ip;
	int local_port;
	str local_uri;
	str local_tag;
	str remote_uri;
	str remote_tag;
	str contact_uri;
	int cseq;
	ms_res_t ms;
	call_leg_media_t media;
	rms_action_t action;
	rms_session_info_t *bridged_si;
	rms_session_state_t state;
} rms_session_info_t;

#endif
