/*
 * ims_qos_n5 - P-CSCF media authorisation over N5 (Npcf_PolicyAuthorization)
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

/*
 * Media components per TS 29.513 clause 7.3 (SDP to N5 mapping), mirroring
 * what ims_qos puts into the Rx Media-Component-Description AVPs:
 *  - one MediaComponent per RTP stream with a non-zero port on both sides;
 *  - medType from the m= line, marBwDl/marBwUl from b=AS (else defaults);
 *  - one sub-component with an uplink and a downlink IPFilterRule. As on Rx
 *    (TS 29.214 clause 5.3.8), "out" is the downlink (far end -> UE) and
 *    "in" the uplink, written with the UE as source.
 */

#include <stdio.h>
#include <string.h>
#include <strings.h>

#include "../../core/dprint.h"
#include "../../core/parser/sdp/sdp.h"
#include "n5_media.h"

extern int n5_audio_default_bw;
extern int n5_video_default_bw;
extern int n5_authorize_video;
extern int n5_include_rtcp;

static const char *n5_med_type(str *media)
{
	if(media->len == 5 && strncasecmp(media->s, "audio", 5) == 0)
		return "AUDIO";
	if(media->len == 5 && strncasecmp(media->s, "video", 5) == 0)
		return "VIDEO";
	if(media->len == 4 && strncasecmp(media->s, "text", 4) == 0)
		return "TEXT";
	if(media->len == 11 && strncasecmp(media->s, "application", 11) == 0)
		return "APPLICATION";
	if(media->len == 7 && strncasecmp(media->s, "message", 7) == 0)
		return "MESSAGE";
	return "OTHER";
}

/* b=AS in kbit/s, else the default for the media type */
static int n5_stream_bw(sdp_stream_cell_t *st, const char *med_type)
{
	if(st->bw_type.len == 2 && strncasecmp(st->bw_type.s, "AS", 2) == 0
			&& st->bw_width.len > 0) {
		int v = 0, i;
		for(i = 0; i < st->bw_width.len && st->bw_width.s[i] >= '0'
				   && st->bw_width.s[i] <= '9';
				i++)
			v = v * 10 + (st->bw_width.s[i] - '0');
		if(v > 0)
			return v;
	}
	return strcmp(med_type, "VIDEO") == 0 ? n5_video_default_bw
										  : n5_audio_default_bw;
}

static str n5_conn_ip(sdp_session_cell_t *sess, sdp_stream_cell_t *st)
{
	return (st->ip_addr.len > 0) ? st->ip_addr : sess->ip_addr;
}

static void n5_add_flows(json_t *fdescs, const char *proto, str *ue_ip,
		int ue_port, str *peer_ip, int peer_port)
{
	char rule[256];

	snprintf(rule, sizeof(rule), "permit out %s from %.*s %d to %.*s %d", proto,
			peer_ip->len, peer_ip->s, peer_port, ue_ip->len, ue_ip->s, ue_port);
	json_array_append_new(fdescs, json_string(rule));
	snprintf(rule, sizeof(rule), "permit in %s from %.*s %d to %.*s %d", proto,
			ue_ip->len, ue_ip->s, ue_port, peer_ip->len, peer_ip->s, peer_port);
	json_array_append_new(fdescs, json_string(rule));
}

json_t *n5_build_med_components(struct sip_msg *ue_sdp,
		struct sip_msg *peer_sdp, char *ue_ip, int ue_ip_size)
{
	sdp_session_cell_t *us, *ps;
	sdp_stream_cell_t *ust, *pst;
	json_t *comps;
	int n, comp_n = 0;
	char key[16];

	if(parse_sdp(ue_sdp) < 0 || parse_sdp(peer_sdp) < 0) {
		LM_ERR("unable to parse SDP\n");
		return NULL;
	}
	/* one SDP session, as ims_qos */
	us = get_sdp_session(ue_sdp, 0);
	ps = get_sdp_session(peer_sdp, 0);
	if(us == NULL || ps == NULL) {
		LM_ERR("missing SDP session in %s\n", us ? "far-end SDP" : "UE SDP");
		return NULL;
	}

	comps = json_object();
	for(n = 0;; n++) {
		json_t *comp, *subs, *sub, *fdescs;
		const char *med_type, *proto;
		str uip, pip;
		int uport, pport, ubw, pbw;

		ust = get_sdp_stream(ue_sdp, 0, n);
		pst = get_sdp_stream(peer_sdp, 0, n);
		if(ust == NULL || pst == NULL)
			break;
		if(!ust->is_rtp)
			continue;
		uport = atoi(ust->port.s);
		pport = atoi(pst->port.s);
		if(uport == 0 || pport == 0)
			continue; /* rejected or disabled stream */
		med_type = n5_med_type(&ust->media);
		if(!n5_authorize_video && strcmp(med_type, "VIDEO") == 0)
			continue;
		uip = n5_conn_ip(us, ust);
		pip = n5_conn_ip(ps, pst);
		if(uip.len <= 0 || pip.len <= 0) {
			LM_ERR("no connection address for stream %d\n", n);
			continue;
		}
		if(ue_ip && ue_ip[0] == '\0' && uip.len < ue_ip_size) {
			memcpy(ue_ip, uip.s, uip.len);
			ue_ip[uip.len] = '\0';
		}
		/* The UE's b=AS is what it will receive (downlink), the far end's
		 * what the UE will send (uplink), as for Rx Max-Requested-Bandwidth. */
		ubw = n5_stream_bw(ust, med_type);
		pbw = n5_stream_bw(pst, med_type);
		proto = (ust->transport.len >= 3
						&& strncasecmp(ust->transport.s, "TCP", 3) == 0)
						? "6"
						: "17";

		fdescs = json_array();
		n5_add_flows(fdescs, proto, &uip, uport, &pip, pport);
		sub = json_object();
		json_object_set_new(sub, "fNum", json_integer(1));
		json_object_set_new(sub, "fDescs", fdescs);
		json_object_set_new(sub, "fStatus", json_string("ENABLED"));
		subs = json_object();
		json_object_set_new(subs, "1", sub);
		if(n5_include_rtcp) {
			json_t *rfd = json_array(), *rsub = json_object();
			n5_add_flows(rfd, proto, &uip, uport + 1, &pip, pport + 1);
			json_object_set_new(rsub, "fNum", json_integer(2));
			json_object_set_new(rsub, "fDescs", rfd);
			json_object_set_new(rsub, "flowUsage", json_string("RTCP"));
			json_object_set_new(subs, "2", rsub);
		}

		comp = json_object();
		comp_n++;
		json_object_set_new(comp, "medCompN", json_integer(comp_n));
		json_object_set_new(comp, "medType", json_string(med_type));
		{
			char bw[32];
			snprintf(bw, sizeof(bw), "%d Kbps", ubw);
			json_object_set_new(comp, "marBwDl", json_string(bw));
			snprintf(bw, sizeof(bw), "%d Kbps", pbw);
			json_object_set_new(comp, "marBwUl", json_string(bw));
		}
		json_object_set_new(comp, "fStatus", json_string("ENABLED"));
		json_object_set_new(comp, "medSubComps", subs);
		snprintf(key, sizeof(key), "%d", comp_n);
		json_object_set_new(comps, key, comp);
	}

	free_sdp((sdp_info_t **)(void *)&ue_sdp->body);
	free_sdp((sdp_info_t **)(void *)&peer_sdp->body);

	if(comp_n == 0) {
		json_decref(comps);
		return NULL;
	}
	return comps;
}
