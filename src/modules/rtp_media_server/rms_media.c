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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
 */

#include "../../core/mem/shm.h"
#include "../../core/sr_module.h"
#include "rtp_media_server.h"

inline static void *ptr_shm_malloc(size_t size)
{
	return shm_malloc(size);
}
inline static void *ptr_shm_realloc(void *ptr, size_t size)
{
	return shm_realloc(ptr, size);
}
inline static void ptr_shm_free(void *ptr)
{
	shm_free(ptr);
}

typedef struct shared_global_vars
{
	MSFactory *ms_factory;
	gen_lock_t lock;
} shared_global_vars_t;


MSFilterDesc *rms_ms_filter_descs[] = {&ms_alaw_dec_desc, &ms_alaw_enc_desc,
		&ms_ulaw_dec_desc, &ms_ulaw_enc_desc, &ms_rtp_send_desc,
		&ms_rtp_recv_desc, &ms_dtmf_gen_desc, &ms_volume_desc,
		&ms_equalizer_desc, &ms_channel_adapter_desc, &ms_audio_mixer_desc,
		&ms_tone_detector_desc, &ms_speex_dec_desc, &ms_speex_enc_desc,
		&ms_speex_ec_desc, &ms_file_player_desc, &ms_file_rec_desc,
		&ms_resample_desc,
		&ms_opus_dec_desc,
		&ms_opus_enc_desc,
		NULL};

static MSFactory *rms_create_factory()
{
	MSFactory *f = ms_factory_new();
	int i;
	for(i = 0; rms_ms_filter_descs[i] != NULL; i++) {
		ms_factory_register_filter(f, rms_ms_filter_descs[i]);
	}
	ms_factory_init_plugins(f);
	ms_factory_enable_statistics(f, TRUE);
	ms_factory_reset_statistics(f);
	return f;
}

int rms_media_init()
{
	//	OrtpMemoryFunctions ortp_memory_functions;
	//	ortp_memory_functions.malloc_fun = ptr_shm_malloc;
	//	ortp_memory_functions.realloc_fun = ptr_shm_realloc;
	//	ortp_memory_functions.free_fun = ptr_shm_free;
	//	ortp_set_memory_functions(&ortp_memory_functions);
	ortp_init();
	return 1;
}

static MSTicker *rms_create_ticker(char *name)
{
	MSTickerParams params;
	params.name = name;
	params.prio = MS_TICKER_PRIO_NORMAL;
	LM_DBG("\n");
	return ms_ticker_new_with_params(&params);
}

void rms_media_destroy(call_leg_media_t *m)
{
	LM_DBG("rtp_session_destroy[%p]\n", m->rtps);
	rtp_session_destroy(m->rtps);
	m->rtps = NULL;
	ms_ticker_destroy(m->ms_ticker);
	m->ms_ticker = NULL;
	ms_factory_destroy(m->ms_factory);
	m->ms_factory = NULL;
}

int create_session_payload(call_leg_media_t *m) {
	LM_INFO("RTP [%p][%d]\n", m->pt, m->pt->type);
	m->rtp_profile=rtp_profile_new("Call profile");
	if(!m->rtp_profile) return 0;
	rtp_profile_set_payload(m->rtp_profile, m->pt->type, m->pt);
	rtp_session_set_profile(m->rtps, m->rtp_profile);
	rtp_session_set_payload_type(m->rtps, m->pt->type);
	return 1;
}

int create_call_leg_media(call_leg_media_t *m)
{
	if (!m) {
		return 0;
	}
	rms_stop_media(m);
	m->ms_factory = rms_create_factory();
	// Create caller RTP session
	LM_INFO("RTP session [%s:%d]<>[%s:%d]\n", m->local_ip.s, m->local_port,
			m->remote_ip.s, m->remote_port);
	m->rtps = ms_create_duplex_rtp_session(m->local_ip.s, m->local_port,
			m->local_port + 1, ms_factory_get_mtu(m->ms_factory));

	create_session_payload(m);

	rtp_session_set_remote_addr_full(m->rtps, m->remote_ip.s, m->remote_port,
			m->remote_ip.s, m->remote_port + 1);

	rtp_session_enable_rtcp(m->rtps, FALSE);
	// create caller filters : rtprecv1/rtpsend1/encoder1/decoder1
	m->ms_rtprecv = ms_factory_create_filter(m->ms_factory, MS_RTP_RECV_ID);
	m->ms_rtpsend = ms_factory_create_filter(m->ms_factory, MS_RTP_SEND_ID);

	LM_INFO("codec[%s] rtprecv[%p] rtpsend[%p] rate[%dHz]\n", m->pt->mime_type,
			m->ms_rtprecv, m->ms_rtpsend, m->pt->clock_rate);
	m->ms_encoder = ms_factory_create_encoder(m->ms_factory, m->pt->mime_type);
	if(!m->ms_encoder) {
		LM_ERR("creating encoder failed.\n");
		return 0;
	}
	m->ms_decoder = ms_factory_create_decoder(m->ms_factory, m->pt->mime_type);

	/* set filter params */
	ms_filter_call_method(m->ms_rtpsend, MS_RTP_SEND_SET_SESSION, m->rtps);
	ms_filter_call_method(m->ms_rtprecv, MS_RTP_RECV_SET_SESSION, m->rtps);
	return 1;
}


static void rms_player_eof(
		void *user_data, MSFilter *f, unsigned int event, void *event_data)
{
	if(event == MS_FILE_PLAYER_EOF) {
		rms_action_t *a = (rms_action_t *)user_data;
		a->type = RMS_DONE;
	}
	MS_UNUSED(f), MS_UNUSED(event_data);
}


int rms_get_dtmf(call_leg_media_t *m, char dtmf)
{
	//	static void tone_detected_cb(void *data, MSFilter *f, unsigned int event_id, MSToneDetectorEvent *ev) {
	//			MS_UNUSED(data), MS_UNUSED(f), MS_UNUSED(event_id), MS_UNUSED(ev);
	//				ms_tester_tone_detected = TRUE;
	//	}
	return 1;
}

int rms_playfile(call_leg_media_t *m, rms_action_t *a)
{
	int channels = 1;
	int file_sample_rate = 8000;
	if(!m->ms_player)
		return 0;
	ms_filter_add_notify_callback(m->ms_player, rms_player_eof, a, TRUE);
	ms_filter_call_method(
			m->ms_player, MS_FILE_PLAYER_OPEN, (void *)a->param.s);
	ms_filter_call_method(m->ms_player, MS_FILE_PLAYER_START, NULL);
	ms_filter_call_method(m->ms_player, MS_FILTER_GET_SAMPLE_RATE, &file_sample_rate);
	ms_filter_call_method(m->ms_player, MS_FILTER_GET_NCHANNELS, &channels);

	if (m->ms_resampler) {
		ms_filter_call_method(m->ms_resampler, MS_FILTER_SET_SAMPLE_RATE, &file_sample_rate);
		LM_INFO("clock[%d]file[%d]\n", m->pt->clock_rate, file_sample_rate);
		ms_filter_call_method(m->ms_resampler, MS_FILTER_SET_OUTPUT_SAMPLE_RATE, &m->pt->clock_rate);
		ms_filter_call_method(m->ms_resampler, MS_FILTER_SET_OUTPUT_NCHANNELS, &m->pt->channels);
	}
	LM_INFO("[%s]clock[%d][%d]\n", m->pt->mime_type, m->pt->clock_rate, file_sample_rate);
	return 1;
}

int rms_start_media(call_leg_media_t *m, char *file_name)
{
	MSConnectionHelper h;
	int channels = 1;
	int file_sample_rate = 8000;

	if (!m) {
		goto error;
	}
	rms_stop_media(m);

	m->ms_ticker = rms_create_ticker(NULL);
	if(!m->ms_ticker)
		goto error;
	m->ms_player = ms_factory_create_filter(m->ms_factory, MS_FILE_PLAYER_ID);
	if(!m->ms_player)
		goto error;


	// m->ms_recorder = ms_factory_create_filter(m->ms_factory, MS_FILE_PLAYER_ID);
	m->ms_voidsink = ms_factory_create_filter(m->ms_factory, MS_VOID_SINK_ID);
	if(!m->ms_voidsink)
		goto error;
	LM_INFO("m[%p]call-id[%p]pt[%s]\n", m, m->di->callid.s, m->pt->mime_type);

	ms_filter_call_method(m->ms_player, MS_FILTER_SET_OUTPUT_NCHANNELS, &channels);
	ms_filter_call_method_noarg(m->ms_player, MS_FILE_PLAYER_START);
	ms_filter_call_method(m->ms_player, MS_FILTER_GET_SAMPLE_RATE, &file_sample_rate);
	if (strcasecmp(m->pt->mime_type,"opus") == 0) {
		ms_filter_call_method(m->ms_encoder, MS_FILTER_SET_SAMPLE_RATE, &file_sample_rate);
		ms_filter_call_method(m->ms_encoder, MS_FILTER_SET_NCHANNELS, &channels);
	} else {
		m->ms_resampler = ms_factory_create_filter(m->ms_factory, MS_RESAMPLE_ID);
		if (!m->ms_resampler) goto error;
	}

	if (m->ms_resampler) {
		ms_filter_call_method(m->ms_resampler, MS_FILTER_SET_SAMPLE_RATE, &file_sample_rate);
		ms_filter_call_method(m->ms_resampler, MS_FILTER_SET_OUTPUT_SAMPLE_RATE, &m->pt->clock_rate);
	}

	// sending graph
	ms_connection_helper_start(&h);
	ms_connection_helper_link(&h, m->ms_player, -1, 0);
	if (m->ms_resampler)
		ms_connection_helper_link(&h, m->ms_resampler, 0, 0);
	ms_connection_helper_link(&h, m->ms_encoder, 0, 0);
	ms_connection_helper_link(&h, m->ms_rtpsend, 0, -1);

	// receiving graph
	ms_connection_helper_start(&h);
	ms_connection_helper_link(&h, m->ms_rtprecv, -1, 0);
	// ms_connection_helper_link(&h, m->ms_decoder, 0, 0);
	ms_connection_helper_link(&h, m->ms_voidsink, 0, -1);

	ms_ticker_attach_multiple(m->ms_ticker, m->ms_player, m->ms_rtprecv, NULL);
	return 1;
error:
	LM_ERR(" can not start media!\n");
	return 0;
}

int rms_stop_media(call_leg_media_t *m)
{
	MSConnectionHelper h;
	if(!m->ms_ticker) {
		LM_ERR("RMS STOP MEDIA\n");
		return -1;
	}
	if(m->ms_player)
		ms_ticker_detach(m->ms_ticker, m->ms_player);
	if(m->ms_rtprecv)
		ms_ticker_detach(m->ms_ticker, m->ms_rtprecv);

	rtp_stats_display(
			rtp_session_get_stats(m->rtps), " AUDIO SESSION'S RTP STATISTICS ");
	ms_factory_log_statistics(m->ms_factory);

	/*dismantle the sending graph*/
	ms_connection_helper_start(&h);
	if(m->ms_player)
		ms_connection_helper_unlink(&h, m->ms_player, -1, 0);
	if(m->ms_resampler)
		ms_connection_helper_unlink(&h, m->ms_resampler, 0, 0);
	if(m->ms_encoder)
		ms_connection_helper_unlink(&h, m->ms_encoder, 0, 0);
	if(m->ms_rtpsend)
		ms_connection_helper_unlink(&h, m->ms_rtpsend, 0, -1);
	/*dismantle the receiving graph*/
	ms_connection_helper_start(&h);
	if(m->ms_rtprecv)
		ms_connection_helper_unlink(&h, m->ms_rtprecv, -1, 0);
	if(m->ms_voidsink)
		ms_connection_helper_unlink(&h, m->ms_voidsink, 0, -1);

	if(m->ms_player)
		ms_filter_destroy(m->ms_player);
	if(m->ms_resampler)
		ms_filter_destroy(m->ms_resampler);
	if(m->ms_encoder)
		ms_filter_destroy(m->ms_encoder);
	if(m->ms_rtpsend) {
		LM_ERR("detroy rtpsend\n");
		ms_filter_destroy(m->ms_rtpsend);
	} else {
		LM_ERR("no rtpsend\n");
	}
	if(m->ms_rtprecv)
		ms_filter_destroy(m->ms_rtprecv);
	if(m->ms_voidsink)
		ms_filter_destroy(m->ms_voidsink);

	rms_media_destroy(m);
	return 1;
}

int rms_bridge(call_leg_media_t *m1, call_leg_media_t *m2)
{
	MSConnectionHelper h;
	m1->ms_ticker = rms_create_ticker(NULL);
	LM_NOTICE("[%p][%p][%p][%p]\n", m1->ms_rtprecv, m1->ms_rtpsend,
			m2->ms_rtprecv, m2->ms_rtpsend);
	// direction 1
	ms_connection_helper_start(&h);
	ms_connection_helper_link(&h, m1->ms_rtprecv, -1, 0);
	ms_connection_helper_link(&h, m2->ms_rtpsend, 0, -1);

	LM_NOTICE("[%p][%p][%p][%p]2\n", m1->ms_rtprecv, m1->ms_rtpsend,
			m2->ms_rtprecv, m2->ms_rtpsend);
	// direction 2
	ms_connection_helper_start(&h);
	ms_connection_helper_link(&h, m2->ms_rtprecv, -1, 0);
	ms_connection_helper_link(&h, m1->ms_rtpsend, 0, -1);

	ms_ticker_attach_multiple(
			m1->ms_ticker, m1->ms_rtprecv, m2->ms_rtprecv, NULL);

	return 1;
}

int rms_stop_bridge(call_leg_media_t *m1, call_leg_media_t *m2)
{
	MSConnectionHelper h;
	MSTicker *ticker = NULL;

	if(m1->ms_ticker) {
		ticker = m1->ms_ticker;
	}
	if(m2->ms_ticker) {
		ticker = m2->ms_ticker;
	}
	if(!ticker)
		return -1;

	if(m1->ms_rtprecv)
		ms_ticker_detach(ticker, m1->ms_rtprecv);
	if(m1->ms_rtpsend)
		ms_ticker_detach(ticker, m1->ms_rtpsend);
	if(m2->ms_rtprecv)
		ms_ticker_detach(ticker, m2->ms_rtprecv);
	if(m2->ms_rtpsend)
		ms_ticker_detach(ticker, m2->ms_rtpsend);

	ms_connection_helper_start(&h);
	if(m1->ms_rtprecv)
		ms_connection_helper_unlink(&h, m1->ms_rtprecv, -1, 0);
	if(m2->ms_rtpsend)
		ms_connection_helper_unlink(&h, m2->ms_rtpsend, 0, -1);

	ms_connection_helper_start(&h);
	if(m2->ms_rtprecv)
		ms_connection_helper_unlink(&h, m2->ms_rtprecv, -1, 0);
	if(m1->ms_rtpsend)
		ms_connection_helper_unlink(&h, m1->ms_rtpsend, 0, -1);

	rtp_stats_display(rtp_session_get_stats(m1->rtps),
			" AUDIO BRIDGE offer RTP STATISTICS ");

	rtp_stats_display(rtp_session_get_stats(m2->rtps),
			" AUDIO BRIDGE answer RTP STATISTICS ");

	if(m1->ms_rtpsend)
		ms_filter_destroy(m1->ms_rtpsend);
	if(m1->ms_rtprecv)
		ms_filter_destroy(m1->ms_rtprecv);
	if(m2->ms_rtpsend)
		ms_filter_destroy(m2->ms_rtpsend);
	if(m2->ms_rtprecv)
		ms_filter_destroy(m2->ms_rtprecv);

	rtp_session_destroy(m1->rtps);
	rtp_session_destroy(m2->rtps);
	if(m1->ms_ticker)
		ms_ticker_destroy(m1->ms_ticker);
	if(m2->ms_ticker)
		ms_ticker_destroy(m2->ms_ticker);
	m1->ms_ticker = NULL;
	m2->ms_ticker = NULL;
	ms_factory_log_statistics(m1->ms_factory);
	return 1;
}
