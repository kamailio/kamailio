/*
 * SDP parser interface
 *
 * Copyright (C) 2008-2009 SOMA Networks, INC.
 * Copyright (C) 2010 VoIP Embedded, Inc
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  1. Redistributions of source code must retain the above copyright notice,
 *  this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *  notice, this list of conditions and the following disclaimer in the
 *  documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE FREEBSD PROJECT ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL THE FREEBSD PROJECT OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */


#include "../../ut.h"
#include "../../mem/mem.h"
#include "../../mem/shm_mem.h"
#include "../parser_f.h"
#include "../parse_content.h"
#include "sdp.h"
#include "sdp_helpr_funcs.h"

#define USE_PKG_MEM 0
#define USE_SHM_MEM 1

#define HOLD_IP_STR "0.0.0.0"
#define HOLD_IP_LEN 7

/**
 * Creates and initialize a new sdp_info structure
 */
static inline int new_sdp(struct sip_msg* _m)
{
	sdp_info_t* sdp;

	sdp = (sdp_info_t*)pkg_malloc(sizeof(sdp_info_t));
	if (sdp == NULL) {
		LM_ERR("No memory left\n");
		return -1;
	}
	memset( sdp, 0, sizeof(sdp_info_t));
	sdp->type = MSG_BODY_SDP;
	sdp->free = (free_msg_body_f)free_sdp;
	_m->body = (msg_body_t*)sdp;

	return 0;
}

/**
 * Alocate a new session cell.
 */
static inline sdp_session_cell_t *add_sdp_session(sdp_info_t* _sdp, int session_num, str* cnt_disp)
{
	sdp_session_cell_t *session;
	int len;

	len = sizeof(sdp_session_cell_t);
	session = (sdp_session_cell_t*)pkg_malloc(len);
	if (session == NULL) {
		LM_ERR("No memory left\n");
		return NULL;
	}
	memset( session, 0, len);

	session->session_num = session_num;
	if (cnt_disp != NULL) {
		session->cnt_disp.s = cnt_disp->s;
		session->cnt_disp.len = cnt_disp->len;
	}

	/* Insert the new session */
	session->next = _sdp->sessions;
	_sdp->sessions = session;
	_sdp->sessions_num++;

	return session;
}

/**
 * Allocate a new stream cell.
 */
static inline sdp_stream_cell_t *add_sdp_stream(sdp_session_cell_t* _session, int stream_num,
		str* media, str* port, str* transport, str* payloads, int is_rtp, int pf, str* sdp_ip)
{
	sdp_stream_cell_t *stream;
	int len;

	len = sizeof(sdp_stream_cell_t);
	stream = (sdp_stream_cell_t*)pkg_malloc(len);
	if (stream == NULL) {
		LM_ERR("No memory left\n");
		return NULL;
	}
	memset( stream, 0, len);

	stream->stream_num = stream_num;

	stream->media.s = media->s;
	stream->media.len = media->len;
	stream->port.s = port->s;
	stream->port.len = port->len;
	stream->transport.s = transport->s;
	stream->transport.len = transport->len;
	stream->payloads.s = payloads->s;
	stream->payloads.len = payloads->len;

	stream->is_rtp = is_rtp;

	stream->pf = pf;
	stream->ip_addr.s = sdp_ip->s;
	stream->ip_addr.len = sdp_ip->len;

	/* Insert the new stream */
	stream->next = _session->streams;
	_session->streams = stream;
	_session->streams_num++;

	return stream;
}

/**
 * Allocate a new payload.
 */
static inline sdp_payload_attr_t *add_sdp_payload(sdp_stream_cell_t* _stream, int payload_num, str* payload)
{
	sdp_payload_attr_t *payload_attr;
	int len;

	len = sizeof(sdp_payload_attr_t);
	payload_attr = (sdp_payload_attr_t*)pkg_malloc(len);
	if (payload_attr == NULL) {
		LM_ERR("No memory left\n");
		return NULL;
	}
	memset( payload_attr, 0, len);

	payload_attr->payload_num = payload_num;
	payload_attr->rtp_payload.s = payload->s;
	payload_attr->rtp_payload.len = payload->len;

	/* Insert the new payload */
	payload_attr->next = _stream->payload_attr;
	_stream->payload_attr = payload_attr;
	_stream->payloads_num++;

	return payload_attr;
}


/**
 * Initialize fast access pointers.
 */
static inline sdp_payload_attr_t** init_p_payload_attr(sdp_stream_cell_t* _stream, int pkg)
{
	int payloads_num, i;
	sdp_payload_attr_t *payload;

	if (_stream == NULL) {
		LM_ERR("Invalid stream\n");
		return NULL;
	}
	payloads_num = _stream->payloads_num;
	if (payloads_num == 0) {
		LM_ERR("Invalid number of payloads\n");
		return NULL;
	}
	if (pkg == USE_PKG_MEM) {
		_stream->p_payload_attr = (sdp_payload_attr_t**)pkg_malloc(payloads_num * sizeof(sdp_payload_attr_t*));
	} else if (pkg == USE_SHM_MEM) {
		_stream->p_payload_attr = (sdp_payload_attr_t**)shm_malloc(payloads_num * sizeof(sdp_payload_attr_t*));
	} else {
		LM_ERR("undefined memory type\n");
		return NULL;
	}
	if (_stream->p_payload_attr == NULL) {
		LM_ERR("No memory left\n");
		return NULL;
	}

	--payloads_num;
	payload = _stream->payload_attr;
	for (i=0;i<=payloads_num;i++) {
		_stream->p_payload_attr[payloads_num-i] = payload;
		payload = payload->next;
	}

	return _stream->p_payload_attr;
}

/*
 * Setters ...
 */

void set_sdp_payload_attr(sdp_payload_attr_t *payload_attr, str *rtp_enc, str *rtp_clock, str *rtp_params)
{
	if (payload_attr == NULL) {
		LM_ERR("Invalid payload location\n");
		return;
	}
	payload_attr->rtp_enc.s = rtp_enc->s;
	payload_attr->rtp_enc.len = rtp_enc->len;
	payload_attr->rtp_clock.s = rtp_clock->s;
	payload_attr->rtp_clock.len = rtp_clock->len;
	payload_attr->rtp_params.s = rtp_params->s;
	payload_attr->rtp_params.len = rtp_params->len;

	return;
}

void set_sdp_payload_fmtp(sdp_payload_attr_t *payload_attr, str *fmtp_string )
{
	if (payload_attr == NULL) {
		LM_ERR("Invalid payload location\n");
		return;
	}
	payload_attr->fmtp_string.s = fmtp_string->s;
	payload_attr->fmtp_string.len = fmtp_string->len;

	return;
}

/*
 * Getters ....
 */
int get_sdp_session_num(struct sip_msg* _m)
{
	if (_m->body == NULL) return 0;
	if (_m->body->type != MSG_BODY_SDP) return 0;
	return ((sdp_info_t*)_m->body)->sessions_num;
}

int get_sdp_stream_num(struct sip_msg* _m)
{
	if (_m->body == NULL) return 0;
	if (_m->body->type != MSG_BODY_SDP) return 0;
	return ((sdp_info_t*)_m->body)->streams_num;
}

sdp_session_cell_t* get_sdp_session_sdp(struct sdp_info* sdp, int session_num)
{
	sdp_session_cell_t *session;

	session = sdp->sessions;
	if (session_num >= sdp->sessions_num) return NULL;
	while (session) {
		if (session->session_num == session_num) return session;
		session = session->next;
	}
	return NULL;
}

sdp_session_cell_t* get_sdp_session(struct sip_msg* _m, int session_num)
{
	if (_m->body == NULL) return NULL;
	if (_m->body->type != MSG_BODY_SDP) return NULL;
	return get_sdp_session_sdp((sdp_info_t*)_m->body, session_num);
}


sdp_stream_cell_t* get_sdp_stream_sdp(struct sdp_info* sdp, int session_num, int stream_num)
{
	sdp_session_cell_t *session;
	sdp_stream_cell_t *stream;

	if (sdp==NULL) return NULL;
	if (session_num >= sdp->sessions_num) return NULL;
	session = sdp->sessions;
	while (session) {
		if (session->session_num == session_num) {
			if (stream_num >= session->streams_num) return NULL;
			stream = session->streams;
			while (stream) {
				if (stream->stream_num == stream_num) return stream;
				stream = stream->next;
			}
			break;
		} else {
			session = session->next;
		}
	}

	return NULL;
}

sdp_stream_cell_t* get_sdp_stream(struct sip_msg* _m, int session_num, int stream_num)
{
	if (_m->body == NULL) return NULL;
	if (_m->body->type != MSG_BODY_SDP) return NULL;
	return get_sdp_stream_sdp((sdp_info_t*)_m->body, session_num, stream_num);
}


sdp_payload_attr_t* get_sdp_payload4payload(sdp_stream_cell_t *stream, str *rtp_payload)
{
	sdp_payload_attr_t *payload;
	int i;

	if (stream == NULL) {
		LM_ERR("Invalid stream location\n");
		return NULL;
	}
	if (stream->p_payload_attr == NULL) {
		LM_ERR("Invalid access pointer to payloads\n");
		return NULL;
	}

	for (i=0;i<stream->payloads_num;i++) {
		payload = stream->p_payload_attr[i];
		if (rtp_payload->len == payload->rtp_payload.len &&
			(strncmp(rtp_payload->s, payload->rtp_payload.s, rtp_payload->len)==0)) {
			return payload;
		}
	}

	return NULL;
}

sdp_payload_attr_t* get_sdp_payload4index(sdp_stream_cell_t *stream, int index)
{
	if (stream == NULL) {
		LM_ERR("Invalid stream location\n");
		return NULL;
	}
	if (stream->p_payload_attr == NULL) {
		LM_ERR("Invalid access pointer to payloads\n");
		return NULL;
	}
	if (index >= stream->payloads_num) {
		LM_ERR("Out of range index [%d] for payload\n", index);
		return NULL;
	}

	return stream->p_payload_attr[index];
}


/**
 * SDP parser method.
 */
static int parse_sdp_session(str *sdp_body, int session_num, str *cnt_disp, sdp_info_t* _sdp)
{
	str body = *sdp_body;
	str sdp_ip = {NULL,0};
	str sdp_media, sdp_port, sdp_transport, sdp_payload;
	str payload;
	str rtp_payload, rtp_enc, rtp_clock, rtp_params;
	int is_rtp;
	char *bodylimit;
	char *v1p, *o1p, *m1p, *m2p, *c1p, *c2p, *a1p, *a2p, *b1p;
	str tmpstr1;
	int stream_num, payloadnum, pf;
	sdp_session_cell_t *session;
	sdp_stream_cell_t *stream;
	sdp_payload_attr_t *payload_attr;
	int parse_payload_attr;
	str fmtp_string;
	str remote_candidates = {"a:remote-candidates:", 20};

	/* hook the start and lenght of sdp body inside structure
	 * - shorcut useful for multi-part bodies and sdp operations
	 */
	_sdp->text = *sdp_body;
	pf = AF_INET;

	/*
	 * Parsing of SDP body.
	 * Each session starts with v-line and each session may contain a few
	 * media descriptions (each starts with m-line).
	 * We have to change ports in m-lines, and also change IP addresses in
	 * c-lines which can be placed either in session header (fallback for
	 * all medias) or media description.
	 * Ports should be allocated for any media. IPs all should be changed
	 * to the same value (RTP proxy IP), so we can change all c-lines
	 * unconditionally.
	 */
	bodylimit = body.s + body.len;
	v1p = find_sdp_line(body.s, bodylimit, 'v');
	if (v1p == NULL) {
		LM_ERR("no sessions in SDP\n");
		return -1;
	}
	/* get session origin */
	o1p = find_sdp_line(v1p, bodylimit, 'o');
	if (o1p == NULL) {
		LM_ERR("no o= in session\n");
		return -1;
	}
	/* Have this session media description? */
	m1p = find_sdp_line(o1p, bodylimit, 'm');
	if (m1p == NULL) {
		LM_ERR("no m= in session\n");
		return -1;
	}
	/* Allocate a session cell */
	session = add_sdp_session(_sdp, session_num, cnt_disp);
	if (session == NULL) return -1;

	/* Get origin IP */
	tmpstr1.s = o1p;
	tmpstr1.len = bodylimit - tmpstr1.s; /* limit is session limit text */
	if (extract_mediaip(&tmpstr1, &session->o_ip_addr, &session->o_pf,"o=") == -1) {
		LM_ERR("can't extract origin media IP from the message\n");
		return -1;
	}

	/* Find c1p only between session begin and first media.
	 * c1p will give common c= for all medias. */
	c1p = find_sdp_line(o1p, m1p, 'c');

	if (c1p) {
		/* Extract session address */
		tmpstr1.s = c1p;
		tmpstr1.len = bodylimit - tmpstr1.s; /* limit is session limit text */
		if (extract_mediaip(&tmpstr1, &session->ip_addr, &session->pf,"c=") == -1) {
			LM_ERR("can't extract common media IP from the message\n");
			return -1;
		}
	}

	/* Find b1p only between session begin and first media.
	 * b1p will give common b= for all medias. */
	b1p = find_sdp_line(o1p, m1p, 'b');
	if (b1p) {
		tmpstr1.s = b1p;
		tmpstr1.len = m1p - b1p;
		extract_bwidth(&tmpstr1, &session->bw_type, &session->bw_width);
	}

	/* Have session. Iterate media descriptions in session */
	m2p = m1p;
	stream_num = 0;
	for (;;) {
		m1p = m2p; 
		if (m1p == NULL || m1p >= bodylimit)
			break;
		m2p = find_next_sdp_line(m1p, bodylimit, 'm', bodylimit);
		/* c2p will point to per-media "c=" */
		c2p = find_sdp_line(m1p, m2p, 'c');

		sdp_ip.s = NULL;
		sdp_ip.len = 0;
		if (c2p) {
			/* Extract stream address */
			tmpstr1.s = c2p;
			tmpstr1.len = bodylimit - tmpstr1.s; /* limit is session limit text */
			if (extract_mediaip(&tmpstr1, &sdp_ip, &pf,"c=") == -1) {
				LM_ERR("can't extract media IP from the message\n");
				return -1;
			}
		} else {
			if (!c1p) {
				/* No "c=" */
				LM_ERR("can't find media IP in the message\n");
				return -1;
			}
		}

		/* Extract the port on sdp_port */
		tmpstr1.s = m1p;
		tmpstr1.len = m2p - m1p;
		if (extract_media_attr(&tmpstr1, &sdp_media, &sdp_port, &sdp_transport, &sdp_payload, &is_rtp) == -1) {
			LM_ERR("can't extract media attr from the message\n");
			return -1;
		}

		/* Allocate a stream cell */
		stream = add_sdp_stream(session, stream_num, &sdp_media, &sdp_port, &sdp_transport, &sdp_payload, is_rtp, pf, &sdp_ip);
		if (stream == 0) return -1;

        /* Store fast access ptr to raw stream */
        stream->raw_stream.s = tmpstr1.s; 
        stream->raw_stream.len = tmpstr1.len;
                
		/* increment total number of streams */
		_sdp->streams_num++;

		/* b1p will point to per-media "b=" */
		b1p = find_sdp_line(m1p, m2p, 'b');
		if (b1p) {
			tmpstr1.s = b1p;
			tmpstr1.len = m2p - b1p;
			extract_bwidth(&tmpstr1, &stream->bw_type, &stream->bw_width);
		}

		/* Parsing the payloads */
		tmpstr1.s = sdp_payload.s;
		tmpstr1.len = sdp_payload.len;
		payloadnum = 0;
		if (tmpstr1.len != 0) {
			for (;;) {
				a1p = eat_token_end(tmpstr1.s, tmpstr1.s + tmpstr1.len);
				payload.s = tmpstr1.s;
				payload.len = a1p - tmpstr1.s;
				payload_attr = add_sdp_payload(stream, payloadnum, &payload);
				if (payload_attr == NULL) return -1;
				tmpstr1.len -= payload.len;
				tmpstr1.s = a1p;
				a2p = eat_space_end(tmpstr1.s, tmpstr1.s + tmpstr1.len);
				tmpstr1.len -= a2p - a1p;
				tmpstr1.s = a2p;
				if (a1p >= tmpstr1.s)
					break;
				payloadnum++;
			}

			/* Initialize fast access pointers */
			if (NULL == init_p_payload_attr(stream, USE_PKG_MEM)) {
				return -1;
			}
			parse_payload_attr = 1;
		} else {
			parse_payload_attr = 0;
		}

		payload_attr = 0;
		/* Let's figure out the atributes */
		a1p = find_sdp_line(m1p, m2p, 'a');
		a2p = a1p;
		for (;;) {
			a1p = a2p;
			if (a1p == NULL || a1p >= m2p)
				break;
			tmpstr1.s = a2p;
			tmpstr1.len = m2p - a2p;

			if (parse_payload_attr && extract_ptime(&tmpstr1, &stream->ptime) == 0) {
				a1p = stream->ptime.s + stream->ptime.len;
			} else if (parse_payload_attr && extract_sendrecv_mode(&tmpstr1,
					&stream->sendrecv_mode, &stream->is_on_hold) == 0) {
				a1p = stream->sendrecv_mode.s + stream->sendrecv_mode.len;
			} else if (parse_payload_attr && extract_rtpmap(&tmpstr1, &rtp_payload, &rtp_enc, &rtp_clock, &rtp_params) == 0) {
				if (rtp_params.len != 0 && rtp_params.s != NULL) {
					a1p = rtp_params.s + rtp_params.len;
				} else {
					a1p = rtp_clock.s + rtp_clock.len;
				}
				payload_attr = (sdp_payload_attr_t*)get_sdp_payload4payload(stream, &rtp_payload);
				set_sdp_payload_attr(payload_attr, &rtp_enc, &rtp_clock, &rtp_params);
			} else if (extract_rtcp(&tmpstr1, &stream->rtcp_port) == 0) {
				a1p = stream->rtcp_port.s + stream->rtcp_port.len;
			} else if (parse_payload_attr && extract_fmtp(&tmpstr1,&rtp_payload,&fmtp_string) == 0){
				a1p = fmtp_string.s + fmtp_string.len;
				payload_attr = (sdp_payload_attr_t*)get_sdp_payload4payload(stream, &rtp_payload);
				set_sdp_payload_fmtp(payload_attr, &fmtp_string);
			} else if (parse_payload_attr && extract_candidate(&tmpstr1, stream) == 0) {
			        a1p += 2;
			} else if (parse_payload_attr && extract_field(&tmpstr1, &stream->remote_candidates,
								       remote_candidates) == 0) {
			        a1p += 2;
			} else if (extract_accept_types(&tmpstr1, &stream->accept_types) == 0) {
				a1p = stream->accept_types.s + stream->accept_types.len;
			} else if (extract_accept_wrapped_types(&tmpstr1, &stream->accept_wrapped_types) == 0) {
				a1p = stream->accept_wrapped_types.s + stream->accept_wrapped_types.len;
			} else if (extract_max_size(&tmpstr1, &stream->max_size) == 0) {
				a1p = stream->max_size.s + stream->max_size.len;
			} else if (extract_path(&tmpstr1, &stream->path) == 0) {
				a1p = stream->path.s + stream->path.len;
			} else {
				/* unknown a= line, ignore -- jump over it */
				LM_DBG("ignoring unknown type in a= line: `%.*s'\n", tmpstr1.len, tmpstr1.s);
				a1p += 2;
			}

			a2p = find_first_sdp_line(a1p, m2p, 'a', m2p);
		}
		/* Let's detect if the media is on hold by checking
		 * the good old "0.0.0.0" connection address */
		if (!stream->is_on_hold) {
			if (stream->ip_addr.s && stream->ip_addr.len) {
				if (stream->ip_addr.len == HOLD_IP_LEN &&
					strncmp(stream->ip_addr.s, HOLD_IP_STR, HOLD_IP_LEN)==0)
					stream->is_on_hold = RFC2543_HOLD;
			} else if (session->ip_addr.s && session->ip_addr.len) {
				if (session->ip_addr.len == HOLD_IP_LEN &&
					strncmp(session->ip_addr.s, HOLD_IP_STR, HOLD_IP_LEN)==0)
					stream->is_on_hold = RFC2543_HOLD;
			}
		}
		++stream_num;
	} /* Iterate medias/streams in session */
	return 0;
}

static int parse_mixed_content(str *mixed_body, str delimiter, sdp_info_t* _sdp)
{
	int res, no_eoh_found, start_parsing;
	char *bodylimit, *rest;
	char *d1p, *d2p;
	char *ret, *end;
	unsigned int mime;
	str cnt_disp;
	int session_num;
	struct hdr_field hf;

	bodylimit = mixed_body->s + mixed_body->len;
	d1p = find_sdp_line_delimiter(mixed_body->s, bodylimit, delimiter);
	if (d1p == NULL) {
		LM_ERR("empty multipart content\n");
		return -1;
	}
	d2p = d1p;
	session_num = 0;
	for(;;) {
		/* Per-application iteration */
		d1p = d2p;
		if (d1p == NULL || d1p >= bodylimit)
			break; /* No applications left */
		d2p = find_next_sdp_line_delimiter(d1p, bodylimit, delimiter, bodylimit);
		/* d2p is text limit for application parsing */
		memset(&hf,0, sizeof(struct hdr_field));
		rest = eat_line(d1p + delimiter.len + 2, d2p - d1p - delimiter.len - 2);
		if ( rest > d2p ) {
			LM_ERR("Unparsable <%.*s>\n", (int)(d2p-d1p), d1p);
			return -1;
		}
		no_eoh_found = 1;
		start_parsing = 0;
		/*LM_DBG("we need to parse this: <%.*s>\n", d2p-rest, rest); */
		while( rest<d2p && no_eoh_found ) {
			rest = get_sdp_hdr_field(rest, d2p, &hf);
			switch (hf.type){
			case HDR_EOH_T:
				no_eoh_found = 0;
				break;
			case HDR_CONTENTTYPE_T:
				end = hf.body.s + hf.body.len;
				ret = decode_mime_type(hf.body.s, end , &mime);
				if (ret==0)
					return -1;
				if (ret!=end) {
					LM_ERR("the header CONTENT_TYPE contains "
						"more then one mime type :-(!\n");
					return -1;
				}
				if ((mime&0x00ff)==SUBTYPE_ALL || (mime>>16)==TYPE_ALL) {
					LM_ERR("invalid mime with wildcard '*' in Content-Type hdr!\n");
					return -1;
				}
			    	//LM_DBG("HDR_CONTENTTYPE_T: %d:%d %p-> <%.*s:%.*s>\n",mime&0x00ff,mime>>16,
				//			hf.name.s,hf.name.len,hf.name.s,hf.body.len,hf.body.s);
				if (((((unsigned int)mime)>>16) == TYPE_APPLICATION) && ((mime&0x00ff) == SUBTYPE_SDP)) {
			    		/*LM_DBG("start_parsing: %d:%d\n",mime&0x00ff,mime>>16); */
					start_parsing = 1;
				}
				break;
			case HDR_CONTENTDISPOSITION_T:
				cnt_disp.s = hf.body.s;
				cnt_disp.len = hf.body.len;
				break;
			case HDR_ERROR_T:
				return -1;
				break;
			default:
				LM_DBG("unknown header: <%.*s:%.*s>\n",hf.name.len,hf.name.s,hf.body.len,hf.body.s);
			}
		} /* end of while */
		/* and now we need to parse the content */
		if (start_parsing) {
			while (('\n' == *rest) || ('\r' == *rest) || ('\t' == *rest)|| (' ' == *rest)) rest++; /* Skip any whitespace */
			_sdp->raw_sdp.s = rest;
			_sdp->raw_sdp.len = d2p-rest;
			/* LM_DBG("we need to check session %d: <%.*s>\n", session_num, _sdp.raw_sdp.len, _sdp.raw_sdp.s); */
			res = parse_sdp_session(&_sdp->raw_sdp, session_num, &cnt_disp, _sdp);
			if (res != 0) {
				/* _sdp is freed by the calling function */
				return -1;
			}
			session_num++;
		}
	}
	return 0;
}

/**
 * @brief Parse SIP SDP body and store in _m->body.
 *
 * @param _m the SIP message structure
 * @return 0 on success, < 0 on error and 1 if there is no message body
 */
int parse_sdp(struct sip_msg* _m)
{
	int res;
	str body, mp_delimiter;
	int mime;

	if (_m->body) {
		return 0;  /* Already parsed */
	}

	body.s = get_body(_m);
	if (body.s==NULL) {
		LM_ERR("failed to get the message body\n");
		return -1;
	}

	body.len = _m->len -(int)(body.s - _m->buf);
	if (body.len==0) {
		LM_DBG("message body has length zero\n");
		return 1;
	}

	mime = parse_content_type_hdr(_m);
	if (mime <= 0) {
		return -1;
	}
	switch (((unsigned int)mime)>>16) {
	case TYPE_APPLICATION:
		/* LM_DBG("TYPE_APPLICATION: %d\n",((unsigned int)mime)>>16); */
		switch (mime&0x00ff) {
		case SUBTYPE_SDP:
			/* LM_DBG("SUBTYPE_SDP: %d\n",mime&0x00ff); */
			if (new_sdp(_m) < 0) {
				LM_ERR("Can't create sdp\n");
				return -1;
			}
			res = parse_sdp_session(&body, 0, NULL, (sdp_info_t*)_m->body);
			if (res != 0) {
				LM_DBG("failed to parse sdp session - freeing sdp\n");
				free_sdp((sdp_info_t**)(void*)&_m->body);
				return res;
			}
			/* The whole body is SDP */
			((sdp_info_t*)_m->body)->raw_sdp.s = body.s;
			((sdp_info_t*)_m->body)->raw_sdp.len = body.len;
			return res;
			break;
		default:
			LM_DBG("TYPE_APPLICATION: unknown %d\n",mime&0x00ff);
			return -1;
		}
		break;
	case TYPE_MULTIPART:
		/* LM_DBG("TYPE_MULTIPART: %d\n",((unsigned int)mime)>>16); */
		switch (mime&0x00ff) {
		case SUBTYPE_MIXED:
			/* LM_DBG("SUBTYPE_MIXED: %d <%.*s>\n",mime&0x00ff,_m->content_type->body.len,_m->content_type->body.s); */
			if(get_mixed_part_delimiter(&(_m->content_type->body),&mp_delimiter) > 0) {
				/*LM_DBG("got delimiter: <%.*s>\n",mp_delimiter.len,mp_delimiter.s); */
				if (new_sdp(_m) < 0) {
					LM_ERR("Can't create sdp\n");
					return -1;
				}
				res = parse_mixed_content(&body, mp_delimiter, (sdp_info_t*)_m->body);
				if (res != 0) {
					LM_DBG("free_sdp\n");
					free_sdp((sdp_info_t**)(void*)&_m->body);
				}
				return res;
			} else {
				return -1;
			}
			break;
		default:
			LM_DBG("TYPE_MULTIPART: unknown %d\n",mime&0x00ff);
			return -1;
		}
		break;
	default:
		LM_DBG("%d\n",((unsigned int)mime)>>16);
		return -1;
	}

	LM_CRIT("We should not see this!\n");
	return res;
}


/**
 * Free all memory.
 */
void free_sdp(sdp_info_t** _sdp)
{
	sdp_info_t *sdp = *_sdp;
	sdp_session_cell_t *session, *l_session;
	sdp_stream_cell_t *stream, *l_stream;
	sdp_payload_attr_t *payload, *l_payload;
        sdp_ice_attr_t *tmp;

	LM_DBG("_sdp = %p\n", _sdp);
	if (sdp == NULL) return;
	LM_DBG("sdp = %p\n", sdp);
	session = sdp->sessions;
	LM_DBG("session = %p\n", session);
	while (session) {
		l_session = session;
		session = session->next;
		stream = l_session->streams;
		while (stream) {
			l_stream = stream;
			stream = stream->next;
			payload = l_stream->payload_attr;
			while (payload) {
				l_payload = payload;
				payload = payload->next;
				pkg_free(l_payload);
			}
			if (l_stream->p_payload_attr) {
				pkg_free(l_stream->p_payload_attr);
			}
			while (l_stream->ice_attr) {
			    tmp = l_stream->ice_attr->next;
			    pkg_free(l_stream->ice_attr);
			    l_stream->ice_attr = tmp;
			}
			pkg_free(l_stream);
		}
		pkg_free(l_session);
	}
	pkg_free(sdp);
	*_sdp = NULL;
}


void print_sdp_stream(sdp_stream_cell_t *stream, int log_level)
{
	sdp_payload_attr_t *payload;
        sdp_ice_attr_t *ice_attr;

	LOG(log_level , "....stream[%d]:%p=>%p {%p} '%.*s' '%.*s:%.*s:%.*s' '%.*s' [%d] '%.*s' '%.*s:%.*s' (%d)=>%p (%d)=>%p '%.*s' '%.*s' '%.*s' '%.*s' '%.*s' '%.*s' '%.*s'\n",
		stream->stream_num, stream, stream->next,
		stream->p_payload_attr,
		stream->media.len, stream->media.s,
		stream->ip_addr.len, stream->ip_addr.s, stream->port.len, stream->port.s,
		stream->rtcp_port.len, stream->rtcp_port.s,
		stream->transport.len, stream->transport.s, stream->is_rtp,
		stream->payloads.len, stream->payloads.s,
		stream->bw_type.len, stream->bw_type.s, stream->bw_width.len, stream->bw_width.s,
		stream->payloads_num, stream->payload_attr,
		stream->ice_attrs_num, stream->ice_attr,
		stream->sendrecv_mode.len, stream->sendrecv_mode.s,
		stream->ptime.len, stream->ptime.s,
		stream->path.len, stream->path.s,
		stream->max_size.len, stream->max_size.s,
		stream->accept_types.len, stream->accept_types.s,
	        stream->accept_wrapped_types.len, stream->accept_wrapped_types.s,
	    	stream->remote_candidates.len, stream->remote_candidates.s);
	payload = stream->payload_attr;
	while (payload) {
		LOG(log_level, "......payload[%d]:%p=>%p p_payload_attr[%d]:%p '%.*s' '%.*s' '%.*s' '%.*s' '%.*s'\n",
			payload->payload_num, payload, payload->next,
			payload->payload_num, stream->p_payload_attr[payload->payload_num],
			payload->rtp_payload.len, payload->rtp_payload.s,
			payload->rtp_enc.len, payload->rtp_enc.s,
			payload->rtp_clock.len, payload->rtp_clock.s,
			payload->rtp_params.len, payload->rtp_params.s,
			payload->fmtp_string.len, payload->fmtp_string.s);
		payload=payload->next;
	}
	ice_attr = stream->ice_attr;
	while (ice_attr) {
	    LOG(log_level, "......'%.*s' %u\n",
		ice_attr->foundation.len, ice_attr->foundation.s,
		ice_attr->component_id);
	    ice_attr = ice_attr->next;
	}
}

void print_sdp_session(sdp_session_cell_t *session, int log_level)
{
	sdp_stream_cell_t *stream;

	if (session==NULL) {
		LM_ERR("NULL session\n");
		return;
	}
	stream = session->streams;

	LOG(log_level, "..session[%d]:%p=>%p '%.*s' '%.*s' '%.*s' '%.*s:%.*s' (%d)=>%p\n",
		session->session_num, session, session->next,
		session->cnt_disp.len, session->cnt_disp.s,
		session->ip_addr.len, session->ip_addr.s,
		session->o_ip_addr.len, session->o_ip_addr.s,
		session->bw_type.len, session->bw_type.s, session->bw_width.len, session->bw_width.s,
		session->streams_num, session->streams);
	while (stream) {
		print_sdp_stream(stream, log_level);
		stream=stream->next;
	}
}


void print_sdp(sdp_info_t* sdp, int log_level)
{
	sdp_session_cell_t *session;

	if (!sdp)
	{
	    LOG(log_level, "no sdp body\n");
	    return;
	}

	LOG(log_level, "sdp:%p=>%p (%d:%d)\n", sdp, sdp->sessions, sdp->sessions_num, sdp->streams_num);
	session = sdp->sessions;
	while (session) {
		print_sdp_session(session, log_level);
		session = session->next;
	}
}

/*
 * Free cloned stream.
 */
void free_cloned_sdp_stream(sdp_stream_cell_t *_stream)
{
	sdp_stream_cell_t *stream, *l_stream;
	sdp_payload_attr_t *payload, *l_payload;

	stream = _stream;
	while (stream) {
		l_stream = stream;
		stream = stream->next;
		payload = l_stream->payload_attr;
		while (payload) {
			l_payload = payload;
			payload = payload->next;
			shm_free(l_payload);
		}
		if (l_stream->p_payload_attr) {
			shm_free(l_stream->p_payload_attr);
		}
		shm_free(l_stream);
	}
}

/*
 * Free cloned session.
 */
void free_cloned_sdp_session(sdp_session_cell_t *_session)
{
	sdp_session_cell_t *session, *l_session;

	session = _session;
	while (session) {
		l_session = session;
		session = l_session->next;
		free_cloned_sdp_stream(l_session->streams);
		shm_free(l_session);
	}
}

void free_cloned_sdp(sdp_info_t* sdp)
{
	free_cloned_sdp_session(sdp->sessions);
	shm_free(sdp);
}

sdp_payload_attr_t * clone_sdp_payload_attr(sdp_payload_attr_t *attr)
{
	sdp_payload_attr_t * clone_attr;
	int len;
	char *p;

	if (attr == NULL) {
		LM_ERR("arg:NULL\n");
		return NULL;
	}

	len = sizeof(sdp_payload_attr_t) +
			attr->rtp_payload.len +
			attr->rtp_enc.len +
			attr->rtp_clock.len +
			attr->rtp_params.len +
			attr->fmtp_string.len;
	clone_attr = (sdp_payload_attr_t*)shm_malloc(len);
	if (clone_attr == NULL) {
		LM_ERR("no more shm mem (%d)\n",len);
		return NULL;
	}
	memset( clone_attr, 0, len);
	p = (char*)(clone_attr+1);

	clone_attr->payload_num = attr->payload_num;

	if (attr->rtp_payload.len) {
		clone_attr->rtp_payload.s = p;
		clone_attr->rtp_payload.len = attr->rtp_payload.len;
		memcpy( p, attr->rtp_payload.s, attr->rtp_payload.len);
		p += attr->rtp_payload.len;
	}

	if (attr->rtp_enc.len) {
		clone_attr->rtp_enc.s = p;
		clone_attr->rtp_enc.len = attr->rtp_enc.len;
		memcpy( p, attr->rtp_enc.s, attr->rtp_enc.len);
		p += attr->rtp_enc.len;
	}

	if (attr->rtp_clock.len) {
		clone_attr->rtp_clock.s = p;
		clone_attr->rtp_clock.len = attr->rtp_clock.len;
		memcpy( p, attr->rtp_clock.s, attr->rtp_clock.len);
		p += attr->rtp_clock.len;
	}

	if (attr->rtp_params.len) {
		clone_attr->rtp_params.s = p;
		clone_attr->rtp_params.len = attr->rtp_params.len;
		memcpy( p, attr->rtp_params.s, attr->rtp_params.len);
		p += attr->rtp_params.len;
	}

	if (attr->fmtp_string.len) {
		clone_attr->fmtp_string.s = p;
		clone_attr->fmtp_string.len = attr->fmtp_string.len;
		memcpy( p, attr->fmtp_string.s, attr->fmtp_string.len);
		p += attr->fmtp_string.len;
	}

	return clone_attr;
}

sdp_stream_cell_t * clone_sdp_stream_cell(sdp_stream_cell_t *stream)
{
	sdp_stream_cell_t *clone_stream;
	sdp_payload_attr_t *clone_payload_attr, *payload_attr;
	int len, i;
	char *p;

	if (stream == NULL) {
		LM_ERR("arg:NULL\n");
		return NULL;
	}

	/* NOTE: we are not cloning RFC4975 attributes */
	len = sizeof(sdp_stream_cell_t) +
			stream->ip_addr.len +
			stream->media.len +
			stream->port.len +
			stream->transport.len +
			stream->sendrecv_mode.len +
			stream->ptime.len +
			stream->payloads.len +
			stream->bw_type.len +
			stream->bw_width.len +
			stream->rtcp_port.len;
	clone_stream = (sdp_stream_cell_t*)shm_malloc(len);
	if (clone_stream == NULL) {
		LM_ERR("no more shm mem (%d)\n",len);
		return NULL;
	}
	memset( clone_stream, 0, len);
	p = (char*)(clone_stream+1);

	payload_attr = NULL;
	for (i=0;i<stream->payloads_num;i++) {
		clone_payload_attr = clone_sdp_payload_attr(stream->p_payload_attr[i]);
		if (clone_payload_attr == NULL) {
			LM_ERR("unable to clone attributes for payload[%d]\n", i);
			goto error;
		}
		clone_payload_attr->next = payload_attr;
		payload_attr = clone_payload_attr;
	}
	clone_stream->payload_attr = payload_attr;

	clone_stream->payloads_num = stream->payloads_num;
	if (clone_stream->payloads_num) {
		if (NULL == init_p_payload_attr(clone_stream, USE_SHM_MEM)) {
			goto error;
		}
	}

	clone_stream->stream_num = stream->stream_num;
	clone_stream->pf = stream->pf;

	if (stream->ip_addr.len) {
		clone_stream->ip_addr.s = p;
		clone_stream->ip_addr.len = stream->ip_addr.len;
		memcpy( p, stream->ip_addr.s, stream->ip_addr.len);
		p += stream->ip_addr.len;
	}

	clone_stream->is_rtp = stream->is_rtp;

	if (stream->media.len) {
		clone_stream->media.s = p;
		clone_stream->media.len = stream->media.len;
		memcpy( p, stream->media.s, stream->media.len);
		p += stream->media.len;
	}

	if (stream->port.len) {
		clone_stream->port.s = p;
		clone_stream->port.len = stream->port.len;
		memcpy( p, stream->port.s, stream->port.len);
		p += stream->port.len;
	}

	if (stream->transport.len) {
		clone_stream->transport.s = p;
		clone_stream->transport.len = stream->transport.len;
		memcpy( p, stream->transport.s, stream->transport.len);
		p += stream->transport.len;
	}

	if (stream->sendrecv_mode.len) {
		clone_stream->sendrecv_mode.s = p;
		clone_stream->sendrecv_mode.len = stream->sendrecv_mode.len;
		memcpy( p, stream->sendrecv_mode.s, stream->sendrecv_mode.len);
		p += stream->sendrecv_mode.len;
	}

	if (stream->ptime.len) {
		clone_stream->ptime.s = p;
		clone_stream->ptime.len = stream->ptime.len;
		memcpy( p, stream->ptime.s, stream->ptime.len);
		p += stream->ptime.len;
	}

	if (stream->payloads.len) {
		clone_stream->payloads.s = p;
		clone_stream->payloads.len = stream->payloads.len;
		memcpy( p, stream->payloads.s, stream->payloads.len);
		p += stream->payloads.len;
	}

	if (stream->bw_type.len) {
		clone_stream->bw_type.s = p;
		clone_stream->bw_type.len = stream->bw_type.len;
		p += stream->bw_type.len;
	}

	if (stream->bw_width.len) {
		clone_stream->bw_width.s = p;
		clone_stream->bw_width.len = stream->bw_width.len;
		p += stream->bw_width.len;
	}

	if (stream->rtcp_port.len) {
		clone_stream->rtcp_port.s = p;
		clone_stream->rtcp_port.len = stream->rtcp_port.len;
		memcpy( p, stream->rtcp_port.s, stream->rtcp_port.len);
		p += stream->rtcp_port.len;
	}

	/* NOTE: we are not cloning RFC4975 attributes:
	 * - path
	 * - max_size
	 * - accept_types
	 * - accept_wrapped_types
	 */

	return clone_stream;
error:
	free_cloned_sdp_stream(clone_stream);
	return NULL;
}

sdp_session_cell_t * clone_sdp_session_cell(sdp_session_cell_t *session)
{
	sdp_session_cell_t *clone_session;
	sdp_stream_cell_t *clone_stream, *prev_clone_stream, *stream;
	int len, i;
	char *p;

	if (session == NULL) {
		LM_ERR("arg:NULL\n");
		return NULL;
	}
	len = sizeof(sdp_session_cell_t) +
		session->cnt_disp.len +
		session->ip_addr.len +
		session->o_ip_addr.len +
		session->bw_type.len +
		session->bw_width.len;
	clone_session = (sdp_session_cell_t*)shm_malloc(len);
	if (clone_session == NULL) {
		LM_ERR("no more shm mem (%d)\n",len);
		return NULL;
	}
	memset( clone_session, 0, len);
	p = (char*)(clone_session+1);

	if (session->streams_num) {
		stream=session->streams;
		clone_stream=clone_sdp_stream_cell(stream);
		if (clone_stream==NULL) {
			goto error;
		}
		clone_session->streams=clone_stream;
		prev_clone_stream=clone_stream;
		stream=stream->next;
		for (i=1;i<session->streams_num;i++) {
			clone_stream=clone_sdp_stream_cell(stream);
			if (clone_stream==NULL) {
				goto error;
			}
			prev_clone_stream->next=clone_stream;
			prev_clone_stream=clone_stream;
			stream=stream->next;
		}
	}

	clone_session->session_num = session->session_num;
	clone_session->pf = session->pf;
	clone_session->o_pf = session->o_pf;
	clone_session->streams_num = session->streams_num;

	if (session->cnt_disp.len) {
		clone_session->cnt_disp.s = p;
		clone_session->cnt_disp.len = session->cnt_disp.len;
		memcpy( p, session->cnt_disp.s, session->cnt_disp.len);
		p += session->cnt_disp.len;
	}

	if (session->ip_addr.len) {
		clone_session->ip_addr.s = p;
		clone_session->ip_addr.len = session->ip_addr.len;
		memcpy( p, session->ip_addr.s, session->ip_addr.len);
		p += session->ip_addr.len;
	}

	if (session->o_ip_addr.len) {
		clone_session->o_ip_addr.s = p;
		clone_session->o_ip_addr.len = session->o_ip_addr.len;
		memcpy( p, session->o_ip_addr.s, session->o_ip_addr.len);
		p += session->o_ip_addr.len;
	}

	if (session->bw_type.len) {
		clone_session->bw_type.s = p;
		clone_session->bw_type.len = session->bw_type.len;
		memcpy( p, session->bw_type.s, session->bw_type.len);
		p += session->bw_type.len;
	}

	if (session->bw_width.len) {
		clone_session->bw_width.s = p;
		clone_session->bw_width.len = session->bw_width.len;
		memcpy( p, session->bw_width.s, session->bw_width.len);
		p += session->bw_width.len;
	}

	return clone_session;
error:
	free_cloned_sdp_session(clone_session);
	return NULL;
}

sdp_info_t * clone_sdp_info(struct sip_msg* _m)
{
	sdp_info_t *clone_sdp_info, *sdp_info=(sdp_info_t*)_m->body;
	sdp_session_cell_t *clone_session, *prev_clone_session, *session;
	int i, len;

	if (sdp_info==NULL) {
		LM_ERR("no sdp to clone\n");
		return NULL;
	}
	if (sdp_info->type != MSG_BODY_SDP) {
		LM_ERR("Unsupported body type\n");
		return NULL;
	}
	if (sdp_info->sessions_num == 0) {
		LM_ERR("no sessions to clone\n");
		return NULL;
	}

	len = sizeof(sdp_info_t);
	clone_sdp_info = (sdp_info_t*)shm_malloc(len);
	if (clone_sdp_info == NULL) {
		LM_ERR("no more shm mem (%d)\n",len);
		return NULL;
	}
	LM_DBG("clone_sdp_info: %p\n", clone_sdp_info);
	memset( clone_sdp_info, 0, len);
	LM_DBG("we have %d sessions\n", sdp_info->sessions_num);
	clone_sdp_info->sessions_num = sdp_info->sessions_num;
	clone_sdp_info->streams_num = sdp_info->streams_num;

	session=sdp_info->sessions;
	clone_session=clone_sdp_session_cell(session);
	if (clone_session==NULL) {
		goto error;
	}
	clone_sdp_info->sessions=clone_session;
	prev_clone_session=clone_session;
	session=session->next;
	for (i=1;i<sdp_info->sessions_num;i++) {
		clone_session=clone_sdp_session_cell(session);
		if (clone_session==NULL) {
			goto error;
		}
		prev_clone_session->next=clone_session;
		prev_clone_session=clone_session;
		session=session->next;
	}

	return clone_sdp_info;
error:
	free_cloned_sdp(clone_sdp_info);
	return NULL;
}

