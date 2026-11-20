/*
 * Copyright (C) 2026 kamailio.org
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
 *
 */

/*
 * Re-Auth-Request from the PCRF (TS 29.214 4.4.6.2, 4.4.6.3).
 *
 * The RAR is answered at once. When it reports the loss or release of the
 * bearer of some media components of a session, those components are
 * remembered; if no SIP message removes them within bearer_release_wait
 * seconds, the Rx session is terminated and, through the same path as an
 * ASR, the dialog is released (TS 24.229 5.2.8.1.1, 5.2.8.1.2). For the
 * signalling path session the Rx session is terminated without waiting.
 */

#include <time.h>

#include "../../core/mem/shm_mem.h"
#include "../../core/locking.h"
#include "../../core/timer.h"
#include "../cdp/cdp_load.h"
#include "../cdp_avp/cdp_avp_mod.h"
#include "rx_rar.h"
#include "rx_str.h"
#include "rx_avp.h"

extern struct cdp_binds cdpb;
extern int terminate_dialog_on_rx_failure;
extern int bearer_release_wait;

/* every media component, for a release that names none */
#define ALL_MEDIA ((unsigned int)-1)

/** Rx session with media whose bearer is gone, waiting for its deadline */
typedef struct bearer_release
{
	str id; /**< Rx session id */
	struct bearer_release *next;
} bearer_release_t;

static bearer_release_t **releases = 0;
static gen_lock_t *releases_lock = 0;

static void release_timer(unsigned int ticks, void *param);

int rx_rar_init(void)
{
	releases = shm_malloc(sizeof(bearer_release_t *));
	if(releases)
		*releases = 0;
	releases_lock = lock_alloc();
	if(releases_lock)
		lock_init(releases_lock);
	if(!releases || !releases_lock) {
		LM_ERR("no more shm memory\n");
		return -1;
	}
	if(register_timer(release_timer, 0, 1) < 0) {
		LM_ERR("failed to register the bearer release timer\n");
		return -1;
	}
	return 0;
}

void rx_rar_destroy(void)
{
	bearer_release_t *r;

	if(releases) {
		while(*releases) {
			r = *releases;
			*releases = r->next;
			shm_free(r);
		}
		shm_free(releases);
		releases = 0;
	}
	if(releases_lock) {
		lock_destroy(releases_lock);
		lock_dealloc(releases_lock);
		releases_lock = 0;
	}
}

/**
 * Queues an Rx session for the timer, once.
 */
static void queue_release(str *id)
{
	bearer_release_t *r;

	lock_get(releases_lock);
	for(r = *releases; r; r = r->next)
		if(r->id.len == id->len && memcmp(r->id.s, id->s, id->len) == 0)
			break;
	if(!r) {
		r = shm_malloc(sizeof(bearer_release_t) + id->len);
		if(!r) {
			LM_ERR("no more shm memory\n");
		} else {
			r->id.s = (char *)(r + 1);
			r->id.len = id->len;
			memcpy(r->id.s, id->s, id->len);
			r->next = *releases;
			*releases = r;
		}
	}
	lock_release(releases_lock);
}

/**
 * Terminates the Rx sessions whose released media are still there at their
 * deadline. Runs without the list lock while it takes session locks: the
 * RAR handler takes them the other way round.
 */
static void release_timer(unsigned int ticks, void *param)
{
	bearer_release_t *list, *r, *keep = 0;
	AAASession *auth;
	rx_authsessiondata_t *data;
	int due;

	lock_get(releases_lock);
	list = *releases;
	*releases = 0;
	lock_release(releases_lock);

	while(list) {
		r = list;
		list = list->next;
		due = 0;
		auth = cdpb.AAAGetAuthSession(r->id);
		if(auth) {
			data = (rx_authsessiondata_t *)auth->u.auth.generic_data;
			if(data && data->released_media) {
				if(auth->u.auth.state == AUTH_ST_DISCON) {
					/* an STR is already out: the session ends anyway and
					 * its dialog must not be terminated a second time */
					data->released_media = 0;
				} else if(data->release_deadline > time(NULL)) {
					r->next = keep;
					keep = r;
					r = 0;
				} else {
					due = 1;
					data->released_media = 0;
					if(!data->subscribed_to_signaling_path_status
							&& terminate_dialog_on_rx_failure)
						data->must_terminate_dialog = 1;
				}
			}
			cdpb.AAASessionsUnlock(auth->hash);
		}
		if(due) {
			LM_INFO("bearer of Rx session [%.*s] released, terminating it\n",
					r->id.len, r->id.s);
			rx_send_str(&r->id);
		}
		if(r)
			shm_free(r);
	}

	if(keep) {
		lock_get(releases_lock);
		for(r = keep; r->next; r = r->next)
			;
		r->next = *releases;
		*releases = keep;
		lock_release(releases_lock);
	}
}

/**
 * The media components named in the Flows AVPs of a RAR, as a mask of bit
 * (Media-Component-Number - 1); all of them when it has no Flows AVP. A
 * Media-Component-Number of 0 (the AF signalling) or above 32 names none.
 * The Flows are the same for every Specific-Action of the RAR, so they are
 * parsed once: the mask is kept in *cached, *parsed tells it is there.
 */
static unsigned int rar_media(
		AAAMessage *request, unsigned int *cached, int *parsed)
{
	AAA_AVP *avp, *mcn;
	AAA_AVP_LIST flows;
	unsigned int mask = 0, n;
	int has_flows = 0;

	if(*parsed)
		return *cached;

	for(avp = request->avpList.head; avp; avp = avp->next) {
		if(avp->code != AVP_IMS_Flows || avp->vendorId != IMS_vendor_id_3GPP)
			continue;
		has_flows = 1;
		flows = cdpb.AAAUngroupAVPS(avp->data);
		mcn = cdpb.AAAFindMatchingAVPList(flows, flows.head,
				AVP_IMS_Media_Component_Number, IMS_vendor_id_3GPP,
				AAA_FORWARD_SEARCH);
		if(mcn && mcn->data.len == 4) {
			n = get_4bytes(mcn->data.s);
			if(n >= 1 && n <= 32)
				mask |= 1u << (n - 1);
			else if(n != 0)
				LM_WARN("Media-Component-Number %u out of range\n", n);
		} else {
			LM_WARN("Flows AVP without a valid Media-Component-Number\n");
		}
		cdpb.AAAFreeAVPList(&flows);
	}
	*cached = has_flows ? mask : ALL_MEDIA;
	*parsed = 1;
	return *cached;
}

/**
 * Handles a RAR from the PCRF and returns the RAA (TS 29.214 4.4.6.2:
 * the AF "shall acknowledge the command by sending an RAA").
 * @param request - the RAR
 * @returns the RAA, NULL on error
 */
AAAMessage *rx_process_rar(AAAMessage *request)
{
	AAAMessage *raa;
	AAASession *auth = 0;
	rx_authsessiondata_t *data = 0;
	AAA_AVP *avp;
	unsigned int rc = DIAMETER_SUCCESS, action, media, flows = 0;
	int flows_parsed = 0;
	char x[4];

	raa = cdpb.AAACreateResponse(request);
	if(!raa)
		return 0;

	if(request->sessionId)
		auth = cdpb.AAAGetAuthSession(request->sessionId->data);
	if(auth)
		data = (rx_authsessiondata_t *)auth->u.auth.generic_data;
	if(!data) {
		LM_INFO("RAR for an unknown Rx session\n");
		rc = DIAMETER_UNKNOWN_SESSION_ID;
		goto send;
	}

	for(avp = request->avpList.head; avp; avp = avp->next) {
		if(avp->code != AVP_IMS_Specific_Action
				|| avp->vendorId != IMS_vendor_id_3GPP || avp->data.len != 4)
			continue;
		action = get_4bytes(avp->data.s);
		switch(action) {
			case AVP_IMS_Specific_Action_Indication_Of_Loss_Of_Bearer:
			case AVP_IMS_Specific_Action_Indication_Of_Release_Of_Bearer:
				if(data->subscribed_to_signaling_path_status) {
					/* TS 29.214 4.4.6.3: the AF may terminate the session */
					LM_INFO("signalling bearer of Rx session [%.*s] lost\n",
							auth->id.len, auth->id.s);
					data->released_media = ALL_MEDIA;
					data->release_deadline = time(NULL);
				} else {
					/* media SIP already removed are not waited for */
					media = rar_media(request, &flows, &flows_parsed)
							& data->kept_media;
					if(!media) {
						LM_INFO("lost bearer of Rx session [%.*s] is of no "
								"media it still has, ignored\n",
								auth->id.len, auth->id.s);
						break;
					}
					/* the wait runs from the first loss still pending: a
					 * later loss RAR must not push the release back */
					if(!data->released_media)
						data->release_deadline =
								time(NULL) + bearer_release_wait;
					data->released_media |= media;
					LM_INFO("bearer of media 0x%x of Rx session [%.*s] lost, "
							"waiting %ld s for SIP to remove them\n",
							media, auth->id.len, auth->id.s,
							(long)(data->release_deadline - time(NULL)));
				}
				queue_release(&auth->id);
				break;
			case AVP_IMS_Specific_Action_Indication_Of_Recovery_Of_Bearer:
			case AVP_IMS_Specific_Action_Indication_Of_Establishment_Of_Bearer:
				if(!data->subscribed_to_signaling_path_status)
					data->released_media &=
							~rar_media(request, &flows, &flows_parsed);
				break;
			default:
				LM_DBG("ignoring Specific-Action %u\n", action);
		}
	}

send:
	if(auth)
		cdpb.AAASessionsUnlock(auth->hash);
	set_4bytes(x, rc);
	if(!rx_add_avp(raa, x, 4, AVP_Result_Code, AAA_AVP_FLAG_MANDATORY, 0,
			   AVP_DUPLICATE_DATA, __FUNCTION__)) {
		cdpb.AAAFreeMessage(&raa);
		return 0;
	}
	return raa;
}

/**
 * Called when the PCRF accepts an AAR with the media components its SDP
 * offer/answer keeps: released ones it does not keep were removed by SIP, so
 * their release is done, and later RARs about them are ignored.
 * @param session_data - the Rx session data, with the session locked
 * @param kept - mask of bit (Media-Component-Number - 1) of the kept media
 */
void rx_media_kept(rx_authsessiondata_t *session_data, unsigned int kept)
{
	if(session_data->subscribed_to_signaling_path_status)
		return;
	session_data->kept_media = kept;
	if(!session_data->released_media)
		return;
	session_data->released_media &= kept;
	if(!session_data->released_media)
		LM_INFO("media with a released bearer removed by SIP\n");
}
