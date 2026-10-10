/*
 * Cx profile replacement and network deregistration.
 *
 * Written by Abdul Karim Barbour <a(dot)barbour(at)telekom(punkt)de>
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Kamailio is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Kamailio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "cx_profile.h"
#include "hslot_sp.h"
#include "ims_usrloc_scscf_mod.h"
#include "../../core/dprint.h"
#include "../../core/hashes.h"
#include "../../core/mem/shm_mem.h"
#include "../../core/ut.h"
#include <string.h>

extern gen_lock_set_t *ul_locks;
extern int ul_locks_no;
extern int db_mode;
extern int subs_hash_size;
extern ims_subscription_list_t *ims_subscription_list;

#define CX_MAX_PUBLIC_IDS 256

static int cx_str_equal(const str *a, const str *b)
{
	/* Core compares text with strncmp; reject NULs within counted identities. */
	return a->len > 0 && a->len == b->len && a->s && b->s
		   && !memchr(a->s, '\0', a->len) && !memchr(b->s, '\0', b->len)
		   && str_strcmp(a, b) == 0;
}

/* Domain slots share a small physical lock pool. Never acquire the same
 * physical lock twice, or wait for a second lock while holding the first.
 * A busy operation fails without mutations and can be retried by the HSS.
 * Mark slot ownership so existing NOTIFY code can recursively lock slots. */
static int cx_lock_cache(udomain_t *d)
{
	int i, j;
	for(i = 0; i < ul_locks_no; i++) {
		if(lock_set_try(ul_locks, i) != 0) {
			for(j = i - 1; j >= 0; j--)
				lock_set_release(ul_locks, j);
			return -2;
		}
	}
	for(i = 0; i < d->size; i++) {
		atomic_set(&d->table[i].locker_pid, my_pid());
		d->table[i].recursive_lock_level = 0;
	}
	return 0;
}

static void cx_unlock_cache(udomain_t *d)
{
	int i;
	for(i = 0; i < d->size; i++)
		atomic_set(&d->table[i].locker_pid, 0);
	for(i = ul_locks_no - 1; i >= 0; i--)
		lock_set_release(ul_locks, i);
}

/* Make the newly installed immutable subscription the first IMPI lookup.
 * Old objects stay referenced by unaffected records/readers until released. */
static void cx_index_profile(ims_subscription *s)
{
	hslot_sp_t *slot;
	add_subscription_unsafe(s);
	slot = s->slot;
	if(slot->first != s) {
		slot->last = s->prev;
		s->prev->next = 0;
		s->prev = 0;
		s->next = slot->first;
		slot->first->prev = s;
		slot->first = s;
	}
}

int cx_replace_profile(udomain_t *d, ims_subscription *s)
{
	impurecord_t *records[CX_MAX_PUBLIC_IDS];
	ims_subscription *old[CX_MAX_PUBLIC_IDS];
	ims_public_identity *identities[CX_MAX_PUBLIC_IDS];
	int i, j, k, n = 0, sl, result;

	/* Cx profile replacement supports only in-memory user location. */
	if(db_mode != NO_DB) {
		LM_WARN("Cx PPR profile replacement requires db_mode=0\n");
		return -3;
	}
	if(!s || s->sl >= 0 || s->wpsi || !s->private_identity.len) {
		LM_WARN("Cx PPR rejected missing, indexed or wildcard profile\n");
		return -3;
	}
	for(i = 0; i < s->service_profiles_cnt; i++) {
		for(j = 0; j < s->service_profiles[i].public_identities_cnt; j++) {
			ims_public_identity *pi =
					&s->service_profiles[i].public_identities[j];
			if(n == CX_MAX_PUBLIC_IDS || !pi->public_identity.len
					|| pi->wildcarded_psi.len) {
				LM_WARN("Cx PPR rejected oversized, empty or wildcard identity "
						"list\n");
				return -3;
			}
			for(k = 0; k < n; k++)
				if(cx_str_equal(&identities[k]->public_identity,
						   &pi->public_identity)) {
					LM_WARN("Cx PPR rejected duplicate public identity\n");
					return -3;
				}
			identities[n++] = pi;
		}
	}
	if(!n) {
		LM_WARN("Cx PPR profile contains no public identities\n");
		return -3;
	}
	LM_DBG("Cx PPR validating %d public identities in usrloc\n", n);
	result = cx_lock_cache(d);
	if(result) {
		LM_DBG("Cx PPR cache busy; profile not changed\n");
		return result;
	}
	/* Validate the entire replacement before publishing anything. This first
 * implementation updates existing distinct identities, not IRS membership. */
	for(i = 0; i < n; i++) {
		if(get_impurecord(d, &identities[i]->public_identity, &records[i]) != 0
				|| !records[i]->s
				|| records[i]->reg_state == IMPU_NOT_REGISTERED
				|| !cx_str_equal(
						&records[i]->private_identity, &s->private_identity)
				|| !cx_str_equal(&records[i]->s->private_identity,
						&s->private_identity)) {
			cx_unlock_cache(d);
			LM_WARN("Cx PPR public identity %d is not registered for this "
					"profile\n",
					i + 1);
			return -1;
		}
		old[i] = records[i]->s;
	}
	sl = core_hash(&s->private_identity, 0, subs_hash_size);
	lock_subscription_slot(sl);
	cx_index_profile(s);
	lock_subscription(s);
	for(i = 0; i < n; i++) {
		ref_subscription_unsafe(s);
		records[i]->s = s;
		records[i]->barring = identities[i]->barring;
	}
	unlock_subscription(s);
	/* One reference per record, including when multiple records shared old. */
	for(i = 0; i < n; i++)
		unref_subscription(old[i]);
	unlock_subscription_slot(sl);
	cx_unlock_cache(d);
	LM_DBG("Cx PPR installed profile for %d public identities\n", n);
	return 0;
}

static int cx_emergency_contact(ucontact_t *c)
{
	const str marker = str_init(";sos");
	const str *uri = &c->c;
	int i;
	for(i = 0; uri->len - i >= marker.len; i++) {
		const str token = {uri->s + i, marker.len};
		int end = i + marker.len;
		if((end == uri->len || uri->s[end] == ';' || uri->s[end] == '='
				   || uri->s[end] == '?')
				&& str_strcasecmp(&token, &marker) == 0)
			return 1;
	}
	return 0;
}

int cx_deregister(udomain_t *d, str *private_id, str *ids, int count,
		int reason, cx_notify_contact_f notify)
{
	impurecord_t *records[CX_MAX_PUBLIC_IDS];
	impurecord_t *r, *other;
	impu_contact_t *link, *other_link;
	ims_subscription *old;
	const char *rejection = NULL;
	int i, j, slot, n = 0, result, sl;

	if(db_mode != NO_DB) {
		LM_WARN("Cx RTR cleanup requires db_mode=0\n");
		return -3;
	}
	if(count <= 0 || count > CX_MAX_PUBLIC_IDS || reason < 0 || reason > 3) {
		LM_WARN("Cx RTR rejected invalid identity count or reason\n");
		return -3;
	}
	result = cx_lock_cache(d);
	if(result) {
		LM_DBG("Cx RTR cache busy; registrations not changed\n");
		return result;
	}
	/* Preflight all identities, preserving unrelated/ambiguous ownership. */
	for(i = 0; i < count; i++) {
		LM_DBG("Cx RTR processing public identity %d/%d [%.*s]\n", i + 1, count,
				ids[i].len, ids[i].s);
		if(get_impurecord(d, &ids[i], &r) != 0) {
			LM_WARN("Cx RTR public identity %d/%d [%.*s] not found in usrloc; "
					"skipping\n",
					i + 1, count, ids[i].len, ZSW(ids[i].s));
			continue;
		}
		if(!cx_str_equal(&r->private_identity, private_id)) {
			rejection = "public identity does not belong to the requested "
						"private identity";
			result = -1;
			goto done;
		}
		for(j = 0; j < n && records[j] != r; j++) {
		}
		if(j == n)
			records[n++] = r;
		if(reason == 3 && r->reg_state == IMPU_REGISTERED) {
			rejection = "reason 3 cannot remove a registered identity";
			result = -3;
			goto done;
		}
		for(link = r->linked_contacts.head; link; link = link->next) {
			if(cx_emergency_contact(link->contact)) {
				rejection = "target has an emergency contact";
				result = -3; /* never deregister an emergency binding blindly */
				goto done;
			}
		}
	}
	/* A contact shared with an untargeted IMPU cannot be deleted safely.
 * Reject rather than invalidate another set/private identity. */
	for(slot = 0; slot < d->size; slot++) {
		for(other = d->table[slot].first; other; other = other->next) {
			for(j = 0; j < n && records[j] != other; j++) {
			}
			if(j < n)
				continue;
			for(other_link = other->linked_contacts.head; other_link;
					other_link = other_link->next) {
				for(i = 0; i < n; i++) {
					for(link = records[i]->linked_contacts.head; link;
							link = link->next)
						if(link->contact == other_link->contact) {
							rejection = "contact is shared with an untargeted "
										"identity";
							result = -3;
							goto done;
						}
				}
			}
		}
	}
	for(i = 0; i < n; i++) {
		r = records[i];
		r->reg_state = IMPU_NOT_REGISTERED;
		r->send_sar_on_delete = 0;
		for(link = r->linked_contacts.head; link; link = link->next) {
			LM_DBG("Cx RTR marking contact [%.*s] for AOR [%.*s] deleted\n",
					link->contact->c.len, ZSW(link->contact->c.s),
					link->contact->aor.len, ZSW(link->contact->aor.s));
			lock_contact_slot_i(link->contact->sl);
			link->contact->state = CONTACT_DELETED;
			unlock_contact_slot_i(link->contact->sl);
		}
	}
	/* Prepare every alias notification before removing any profile; NOTIFY
 * generation uses the subscription's implicit-set identities. */
	for(i = 0; i < n; i++) {
		r = records[i];
		for(link = r->linked_contacts.head; link; link = link->next)
			if(r->shead && r->s && notify)
				notify(r, link->contact, reason);
	}
	for(i = 0; i < n; i++) {
		r = records[i];
		old = r->s;
		r->s = 0;
		if(old) {
			sl = old->sl;
			if(sl >= 0)
				lock_subscription_slot(sl);
			unref_subscription(old);
			if(sl >= 0)
				unlock_subscription_slot(sl);
		}
		if(r->ccf1.s)
			shm_free(r->ccf1.s);
		if(r->ccf2.s)
			shm_free(r->ccf2.s);
		if(r->ecf1.s)
			shm_free(r->ecf1.s);
		if(r->ecf2.s)
			shm_free(r->ecf2.s);
		r->ccf1 = r->ccf2 = r->ecf1 = r->ecf2 = (str){0, 0};
	}
	result = 0;
done:
	cx_unlock_cache(d);
	if(rejection)
		LM_WARN("Cx RTR cleanup refused: %s\n", rejection);
	else
		LM_DBG("Cx RTR cleanup complete: reason=%d requested=%d "
			   "processed=%d\n",
				reason, count, n);
	return result;
}
