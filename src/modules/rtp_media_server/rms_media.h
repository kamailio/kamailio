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

#ifndef rms_media_h
#define rms_media_h

#include "../../core/mem/shm.h"
#include <mediastreamer2/mediastream.h>
#include <mediastreamer2/msrtp.h>
#include <mediastreamer2/dtmfgen.h>
#include <mediastreamer2/msfileplayer.h>
#include <mediastreamer2/msfilerec.h>
#include <mediastreamer2/msrtp.h>
#include <mediastreamer2/mstonedetector.h>
#include <mediastreamer2/msfilter.h>
#include <mediastreamer2/mscommon.h>
#include <ortp/ortp.h>
#include <ortp/port.h>

#include "rtp_media_server.h"
typedef struct rms_action rms_action_t;


typedef struct call_leg_media
{
	MSFactory *ms_factory;
	RtpSession *rtps;
	RtpProfile *rtp_profile;
	PayloadType *pt;
	MSTicker *ms_ticker;
	MSFilter *ms_encoder;
	MSFilter *ms_decoder;
	MSFilter *ms_rtprecv;
	MSFilter *ms_rtpsend;
	MSFilter *ms_resampler;
	MSFilter *ms_player;
	MSFilter *ms_recorder;
	MSFilter *ms_dtmfgen;
	MSFilter *ms_tonedet;
	MSFilter *ms_voidsource;
	MSFilter *ms_voidsink;
	str local_ip;
	int local_port;
	str remote_ip;
	int remote_port;
	const struct rms_dialog_info *di;
} call_leg_media_t;

int create_call_leg_media(call_leg_media_t *m);
int create_session_payload(call_leg_media_t *m);

int rms_media_init();
void rms_media_destroy();

MSFactory *rms_get_factory();

int rms_stop_media(call_leg_media_t *m);
int rms_playfile(call_leg_media_t *m, rms_action_t *a);
int rms_start_media(call_leg_media_t *m, char *file_name);
int rms_bridge(call_leg_media_t *m1, call_leg_media_t *m2);
int rms_stop_bridge(call_leg_media_t *m1, call_leg_media_t *m2);

extern MSFilterDesc ms_pcap_file_player_desc;
extern MSFilterDesc ms_rtp_send_desc;
extern MSFilterDesc ms_rtp_recv_desc;
extern MSFilterDesc ms_udp_send_desc;
extern MSFilterDesc ms_alaw_dec_desc;
extern MSFilterDesc ms_alaw_enc_desc;
extern MSFilterDesc ms_ulaw_dec_desc;
extern MSFilterDesc ms_ulaw_enc_desc;
extern MSFilterDesc ms_dtmf_gen_desc;
extern MSFilterDesc ms_volume_desc;
extern MSFilterDesc ms_equalizer_desc;
extern MSFilterDesc ms_channel_adapter_desc;
extern MSFilterDesc ms_audio_mixer_desc;
extern MSFilterDesc ms_tone_detector_desc;
extern MSFilterDesc ms_genericplc_desc;
extern MSFilterDesc ms_file_player_desc;
extern MSFilterDesc ms_file_rec_desc;
extern MSFilterDesc ms_vad_dtx_desc;
extern MSFilterDesc ms_speex_dec_desc;
extern MSFilterDesc ms_speex_enc_desc;
extern MSFilterDesc ms_speex_ec_desc;
extern MSFilterDesc ms_opus_enc_desc;
extern MSFilterDesc ms_opus_dec_desc;
extern MSFilterDesc ms_resample_desc;

#endif
