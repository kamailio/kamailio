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
#include "../../core/mem/mem.h"
#include "ul_callback.h"
#include "../../core/ut.h"
#include "../../core/parser/parse_uri.h"
#include <string.h>

extern gen_lock_set_t *ul_locks;
extern int ul_locks_no;
extern int db_mode;
extern int subs_hash_size;
extern ims_subscription_list_t *ims_subscription_list;


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

static int cx_profile_has(ims_subscription *s, const str *id)
{
	int i, j;
	if(!s)
		return 0;
	for(i = 0; i < s->service_profiles_cnt; i++)
		for(j = 0; j < s->service_profiles[i].public_identities_cnt; j++)
			if(cx_str_equal(id, &s->service_profiles[i]
										 .public_identities[j]
										 .public_identity))
				return 1;
	return 0;
}

static int cx_private_selected(const str *id, str *ids, int count)
{
	int i;
	for(i = 0; i < count; i++)
		if(cx_str_equal(id, &ids[i]))
			return i + 1;
	return 0;
}

static int cx_record_private(impurecord_t *r, str *ids, int count)
{
	impu_contact_t *link;
	int found, known = 0, unknown = 0;
	for(link = r->linked_contacts.head; link; link = link->next) {
		if(!VALID_CONTACT(link->contact, time(NULL)))
			continue;
		if(!link->contact->private_identity.len) {
			unknown = 1;
			continue;
		}
		known = 1;
		found = cx_private_selected(
				&link->contact->private_identity, ids, count);
		if(found)
			return found;
	}
	/* Once all live bindings have owners, a stale record-level IMPI must
	 * not authorize a profile update for an already terminated private ID. */
	return known && !unknown
				   ? 0
				   : cx_private_selected(&r->private_identity, ids, count);
}

static void cx_clear_charging(impurecord_t *r)
{
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

static void cx_clear_profile(impurecord_t *r)
{
	ims_subscription *old = r->s;
	int sl;
	r->s = NULL;
	if(old) {
		sl = old->sl;
		if(sl >= 0)
			lock_subscription_slot(sl);
		unref_subscription(old);
		if(sl >= 0)
			unlock_subscription_slot(sl);
	}
	cx_clear_charging(r);
}

/* Remove only this IMPU's link. A shared contact remains usable by the
 * identities that still own it. The cache lock protects both list ends. */
static void cx_unlink_contact(impurecord_t *r, impu_contact_t *link)
{
	ucontact_t *c = link->contact;
	/* Earlier removals may have left a stale predecessor at the head. */
	impu_contact_t *prev = link == r->linked_contacts.head ? NULL : link->prev;
	if(prev)
		prev->next = link->next;
	else
		r->linked_contacts.head = link->next;
	if(link->next)
		link->next->prev = prev;
	else
		r->linked_contacts.tail = prev;
	r->linked_contacts.numcontacts--;
	if(c->is_3gpp)
		r->linked_contacts.num3gppcontacts--;
	lock_contact_slot_i(c->sl);
	lock_get(c->lock);
	unref_contact_unsafe(c);
	lock_release(c->lock);
	unlock_contact_slot_i(c->sl);
	shm_free(link);
}

static void cx_link_contact(impurecord_t *r, impu_contact_t *link)
{
	link->prev = r->linked_contacts.tail;
	link->next = NULL;
	if(link->prev)
		link->prev->next = link;
	else
		r->linked_contacts.head = link;
	r->linked_contacts.tail = link;
	r->linked_contacts.numcontacts++;
	if(link->contact->is_3gpp)
		r->linked_contacts.num3gppcontacts++;
}

static int cx_record_in(impurecord_t *r, impurecord_t **records, int count)
{
	int i;
	for(i = 0; i < count; i++)
		if(records[i] == r)
			return 1;
	return 0;
}

static int cx_contact_elsewhere(
		udomain_t *d, ucontact_t *c, impurecord_t **records, int count)
{
	int slot;
	impurecord_t *r;
	impu_contact_t *link;
	for(slot = 0; slot < d->size; slot++)
		for(r = d->table[slot].first; r; r = r->next) {
			if(cx_record_in(r, records, count))
				continue;
			for(link = r->linked_contacts.head; link; link = link->next)
				if(link->contact == c)
					return 1;
		}
	return 0;
}

static int cx_ambiguous_contact(udomain_t *d, ucontact_t *c, str *private_id)
{
	impurecord_t *r;
	impu_contact_t *link;
	int slot;
	if(c->private_identity.len)
		return 0;
	for(slot = 0; slot < d->size; slot++)
		for(r = d->table[slot].first; r; r = r->next) {
			if(cx_str_equal(&r->private_identity, private_id))
				continue;
			for(link = r->linked_contacts.head; link; link = link->next)
				if(link->contact == c)
					return 1;
		}
	return 0;
}

static int cx_emergency_contact(ucontact_t *c)
{
	struct sip_uri uri;
	const str sos = str_init("sos");
	str name;
	int i, end;
	if(parse_uri(c->c.s, c->c.len, &uri) < 0)
		return -1;
	/* Only URI parameter names count; a username or header containing
	 * ";sos" must not turn an ordinary contact into an emergency binding. */
	for(i = 0; i < uri.params.len; i = end + 1) {
		for(end = i; end < uri.params.len && uri.params.s[end] != ';'; end++) {
		}
		name = (str){uri.params.s + i, 0};
		while(i + name.len < end && name.s[name.len] != '=')
			name.len++;
		if(str_strcasecmp(&name, &sos) == 0)
			return 1;
	}
	return 0;
}

static int cx_emergency_pair(
		cx_dereg_result_t *response, str *private_id, str *public_id)
{
	cx_emergency_identity_t *pair;
	int i;
	for(i = 0; i < response->emergency_count; i++)
		if(cx_str_equal(&response->emergency[i].private_identity, private_id)
				&& cx_str_equal(
						&response->emergency[i].public_identity, public_id))
			return 0;
	if(response->emergency_count == CX_MAX_PUBLIC_IDS) {
		LM_WARN("Cx RTR emergency result exceeds the local limit of %d pairs\n",
				CX_MAX_PUBLIC_IDS);
		return -2;
	}
	pair = &response->emergency[response->emergency_count];
	if(pkg_str_dup(&pair->private_identity, private_id) < 0) {
		LM_ERR("Cx RTR cannot allocate emergency private identity\n");
		return -2;
	}
	if(pkg_str_dup(&pair->public_identity, public_id) < 0) {
		LM_ERR("Cx RTR cannot allocate emergency public identity\n");
		pkg_free(pair->private_identity.s);
		pair->private_identity = (str){0, 0};
		return -2;
	}
	response->emergency_count++;
	return 0;
}

static void cx_free_charge(cx_charging_t *c)
{
	if(c->ccf1.s)
		shm_free(c->ccf1.s);
	if(c->ccf2.s)
		shm_free(c->ccf2.s);
	if(c->ecf1.s)
		shm_free(c->ecf1.s);
	if(c->ecf2.s)
		shm_free(c->ecf2.s);
	memset(c, 0, sizeof(*c));
}

static int cx_copy_charge(cx_charging_t *to, const cx_charging_t *from)
{
	if((from->ccf1.len && shm_str_dup(&to->ccf1, &from->ccf1) < 0)
			|| (from->ccf2.len && shm_str_dup(&to->ccf2, &from->ccf2) < 0)
			|| (from->ecf1.len && shm_str_dup(&to->ecf1, &from->ecf1) < 0)
			|| (from->ecf2.len && shm_str_dup(&to->ecf2, &from->ecf2) < 0)) {
		LM_ERR("Cx PPR cannot allocate replacement charging information\n");
		cx_free_charge(to);
		return -2;
	}
	return 0;
}

static str *cx_default_identity(ims_subscription *s)
{
	int i, j;
	for(i = 0; i < s->service_profiles_cnt; i++)
		for(j = 0; j < s->service_profiles[i].public_identities_cnt; j++)
			if(!s->service_profiles[i].public_identities[j].barring)
				return &s->service_profiles[i]
								.public_identities[j]
								.public_identity;
	return NULL;
}

/* TS 29.228 6.2.2.1 requires installed data to survive a rejected update.
 * Prepare storage while holding the cache locks, then publish the new
 * subscription/charging references together. The locks and work limits are
 * implementation choices; they do not change the PPR protocol scope. */
int cx_update_profile(udomain_t *d, str *private_id, ims_subscription *s,
		cx_charging_t *charging, cx_notify_contact_f notify)
{
	impurecord_t *records[CX_MAX_PUBLIC_IDS] = {0};
	impurecord_t *removed[CX_MAX_PUBLIC_IDS],
			*charge_records[CX_MAX_PUBLIC_IDS];
	impurecord_t *anchor = NULL, *r;
	ims_public_identity *identities[CX_MAX_PUBLIC_IDS];
	ims_subscription *old[CX_MAX_PUBLIC_IDS], *old_set;
	impu_contact_t *links[CX_MAX_PUBLIC_IDS] = {0}, *link, *next;
	ucontact_t *contacts[CX_MAX_PUBLIC_IDS];
	cx_charging_t copies[CX_MAX_PUBLIC_IDS] = {0}, inherited;
	str private_copies[CX_MAX_PUBLIC_IDS] = {0};
	int added[CX_MAX_PUBLIC_IDS] = {0}, created[CX_MAX_PUBLIC_IDS] = {0};
	int i, j, k, slot, n = 0, nr = 0, nc = 0, nch = 0, result, sl;
	int private_known = 0;
	time_t now = time(NULL);
	str *old_default, *new_default;

	if(!d || db_mode != NO_DB || !private_id || !private_id->len
			|| (s
					&& (s->sl >= 0 || s->wpsi
							|| !cx_str_equal(
									&s->private_identity, private_id)))) {
		LM_WARN("Cx PPR rejected unsupported backend or wildcard profile\n");
		return -3;
	}
	if(s) {
		for(i = 0; i < s->service_profiles_cnt; i++) {
			for(j = 0; j < s->service_profiles[i].public_identities_cnt; j++) {
				ims_public_identity *pi =
						&s->service_profiles[i].public_identities[j];
				if(n == CX_MAX_PUBLIC_IDS || !pi->public_identity.len
						|| memchr(pi->public_identity.s, 0,
								pi->public_identity.len)
						|| pi->wildcarded_psi.len) {
					LM_WARN("Cx PPR rejected oversized, empty, invalid or "
							"wildcard identity list\n");
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
			LM_WARN("Cx PPR profile contains no public identities; use RTR to "
					"remove the whole set\n");
			return -3;
		}
	}
	LM_DBG("Cx PPR validating %d public identities in usrloc\n", n);
	result = cx_lock_cache(d);
	if(result) {
		LM_DBG("Cx PPR cache busy; profile not changed\n");
		return result;
	}
	for(slot = 0; slot < d->size; slot++)
		for(r = d->table[slot].first; r; r = r->next)
			if(r->s && r->reg_state != IMPU_NOT_REGISTERED
					&& cx_record_private(r, private_id, 1))
				private_known = 1;
	if(!private_known) {
		LM_WARN("Cx PPR private identity [%.*s] not found in the registration "
				"cache\n",
				private_id->len, private_id->s);
		result = -1;
		goto rollback;
	}
	for(i = 0; i < n; i++) {
		if(get_impurecord(d, &identities[i]->public_identity, &records[i]) != 0)
			records[i] = NULL;
		r = records[i];
		if(r && r->s && r->reg_state != IMPU_NOT_REGISTERED) {
			if(!cx_record_private(r, private_id, 1)) {
				LM_WARN("Cx PPR public identity [%.*s] is not associated with "
						"private identity [%.*s]\n",
						r->public_identity.len, r->public_identity.s,
						private_id->len, private_id->s);
				result = -3;
				goto rollback;
			}
			if(!anchor)
				anchor = r;
		}
	}
	if(s && !anchor) {
		LM_WARN("Cx PPR has no retained member of an existing implicit "
				"registration set\n");
		result = -3; /* No existing set member: do not invent or merge sets. */
		goto rollback;
	}
	if(s) {
		old_set = anchor->s;
		old_default = cx_default_identity(old_set);
		new_default = cx_default_identity(s);
		/* Local capability restriction: migrating the default identity of
		 * a registered set is not implemented by this cache operation. */
		if(anchor->reg_state == IMPU_REGISTERED
				&& (!old_default || !new_default
						|| !cx_str_equal(old_default, new_default))) {
			LM_WARN("Cx PPR cannot change the default identity of a registered "
					"set\n");
			result = -3;
			goto rollback;
		}
		for(slot = 0; slot < d->size; slot++) {
			for(r = d->table[slot].first; r; r = r->next) {
				if(!r->s || !cx_profile_has(old_set, &r->public_identity))
					continue;
				/* Every retained record must belong to this set, including
				 * records whose profile was last downloaded for another IMPI. */
				if(!cx_profile_has(r->s, &anchor->public_identity)) {
					LM_WARN("Cx PPR cached public identity [%.*s] has no "
							"association with retained set member [%.*s]\n",
							r->public_identity.len, r->public_identity.s,
							anchor->public_identity.len,
							anchor->public_identity.s);
					result = -3;
					goto rollback;
				}
				if(!cx_record_in(r, records, n)) {
					if(nr == CX_MAX_PUBLIC_IDS) {
						LM_WARN("Cx PPR removed identities exceed the local "
								"limit "
								"of %d\n",
								CX_MAX_PUBLIC_IDS);
						result = -2;
						goto rollback;
					}
					removed[nr++] = r;
				}
				for(link = r->linked_contacts.head; link; link = link->next) {
					if(!VALID_CONTACT(link->contact, now))
						continue;
					for(k = 0; k < nc && contacts[k] != link->contact; k++) {
					}
					if(k == nc) {
						if(nc == CX_MAX_PUBLIC_IDS) {
							LM_WARN("Cx PPR inherited contacts exceed the "
									"local "
									"limit of %d\n",
									CX_MAX_PUBLIC_IDS);
							result = -2;
							goto rollback;
						}
						contacts[nc++] = link->contact;
					}
				}
			}
		}
		for(i = 0; i < n; i++) {
			r = records[i];
			if(r && !cx_str_equal(&r->private_identity, private_id)
					&& shm_str_dup(&private_copies[i], private_id) < 0) {
				LM_ERR("Cx PPR cannot allocate replacement private identity\n");
				result = -2;
				goto rollback;
			}
			if(r && r->s && r->reg_state != IMPU_NOT_REGISTERED) {
				if(!cx_profile_has(old_set, &r->public_identity)) {
					LM_WARN("Cx PPR public identity [%.*s] is outside the "
							"retained implicit registration set\n",
							r->public_identity.len, r->public_identity.s);
					result = -3;
					goto rollback;
				}
				continue;
			}
			added[i] = 1;
			/* Allocate all contact links before publishing the new profile. */
			for(j = 0; j < nc; j++) {
				link = shm_malloc(sizeof(*link));
				if(!link) {
					LM_ERR("Cx PPR cannot allocate inherited contact link\n");
					result = -2;
					goto rollback;
				}
				memset(link, 0, sizeof(*link));
				link->contact = contacts[j];
				link->next = links[i];
				links[i] = link;
			}
			if(!r) {
				if(mem_insert_impurecord(d, &identities[i]->public_identity,
						   private_id, anchor->reg_state,
						   identities[i]->barring, NULL, NULL, NULL, NULL, NULL,
						   &records[i])
						< 0) {
					result = -2;
					goto rollback;
				}
				created[i] = 1;
			}
		}
	}
	/* Charging changes cover every cached set for this IMPI. Newly added
	 * public identities inherit charging data even in a User-Data-only PPR. */
	for(slot = 0; slot < d->size; slot++) {
		for(r = d->table[slot].first; r; r = r->next) {
			if(cx_record_in(r, removed, nr))
				continue;
			for(i = 0; i < n && records[i] != r; i++) {
			}
			if(!(charging && r->s && r->reg_state != IMPU_NOT_REGISTERED
					   && cx_record_private(r, private_id, 1))
					&& !(i < n && added[i]))
				continue;
			if(nch == CX_MAX_PUBLIC_IDS) {
				LM_WARN("Cx PPR charging records exceed the local limit of "
						"%d\n",
						CX_MAX_PUBLIC_IDS);
				result = -2;
				goto rollback;
			}
			inherited = charging ? *charging
								 : (cx_charging_t){anchor->ccf1, anchor->ccf2,
										   anchor->ecf1, anchor->ecf2};
			if(cx_copy_charge(&copies[nch], &inherited)) {
				result = -2;
				goto rollback;
			}
			charge_records[nch++] = r;
		}
	}
	/* All cache storage is prepared. Publish those references before any
	 * notification callbacks; callbacks retain their own failure handling. */
	if(s) {
		sl = core_hash(&s->private_identity, 0, subs_hash_size);
		lock_subscription_slot(sl);
		cx_index_profile(s);
		lock_subscription(s);
		for(i = 0; i < n; i++) {
			r = records[i];
			old[i] = r->s;
			ref_subscription_unsafe(s);
			r->s = s;
			r->barring = identities[i]->barring;
			if(private_copies[i].s) {
				shm_free(r->private_identity.s);
				r->private_identity = private_copies[i];
				private_copies[i] = (str){0, 0};
			}
			if(added[i]) {
				r->reg_state = anchor->reg_state;
				r->expires = anchor->expires;
				r->send_sar_on_delete = anchor->send_sar_on_delete;

				for(link = r->linked_contacts.head; link; link = next) {
					next = link->next;
					cx_unlink_contact(r, link);
				}
				for(link = links[i]; link; link = next) {
					next = link->next;
					lock_contact_slot_i(link->contact->sl);
					lock_get(link->contact->lock);
					ref_contact_unsafe(link->contact);
					lock_release(link->contact->lock);
					unlock_contact_slot_i(link->contact->sl);
					cx_link_contact(r, link);
				}
				links[i] = NULL;
			}
		}
		unlock_subscription(s);
		unlock_subscription_slot(sl);
		for(i = 0; i < n; i++) {
			if(!old[i])
				continue;
			sl = old[i]->sl;
			if(sl >= 0)
				lock_subscription_slot(sl);
			unref_subscription(old[i]);
			if(sl >= 0)
				unlock_subscription_slot(sl);
		}
		for(i = 0; i < nr; i++) {
			r = removed[i];
			r->reg_state = IMPU_NOT_REGISTERED;
			r->send_sar_on_delete = 0;
			for(link = r->linked_contacts.head; link; link = next) {
				next = link->next;
				if(notify && r->shead && VALID_CONTACT(link->contact, now))
					notify(r, link->contact,
							CX_PROFILE_IDENTITY_REMOVED); /* only the removed IMPU */
				if(!cx_contact_elsewhere(d, link->contact, removed, nr)) {
					lock_contact_slot_i(link->contact->sl);
					link->contact->state = CONTACT_DELETED;
					unlock_contact_slot_i(link->contact->sl);
				}
				cx_unlink_contact(r, link);
			}
			cx_clear_profile(r);
		}
	}
	for(i = 0; i < nch; i++) {
		r = charge_records[i];
		cx_clear_charging(r);
		r->ccf1 = copies[i].ccf1;
		r->ccf2 = copies[i].ccf2;
		r->ecf1 = copies[i].ecf1;
		r->ecf2 = copies[i].ecf2;
		memset(&copies[i], 0, sizeof(copies[i]));
	}
	for(i = 0; i < n; i++) {
		if(created[i])
			run_ul_callbacks(NULL, UL_IMPU_INSERT, records[i], NULL);
		if(notify && records[i]->shead)
			notify(records[i], NULL, CX_PROFILE_CHANGED);
	}
	cx_unlock_cache(d);
	LM_DBG("Cx PPR installed profile: identities=%d removed=%d "
		   "charging_records=%d\n",
			n, nr, nch);
	return 0;
rollback:
	for(i = 0; i < nch; i++)
		cx_free_charge(&copies[i]);
	for(i = 0; i < n; i++) {
		for(link = links[i]; link; link = next) {
			next = link->next;
			shm_free(link);
		}
		if(private_copies[i].s)
			shm_free(private_copies[i].s);
		if(created[i])
			mem_delete_impurecord(d, records[i]);
	}
	cx_unlock_cache(d);
	LM_WARN("Cx PPR rejected before publishing changes: result=%d\n", result);
	return result;
}

int cx_replace_profile(udomain_t *d, ims_subscription *s)
{
	if(!s)
		return -3;
	return cx_update_profile(d, &s->private_identity, s, NULL, NULL);
}

/* TS 29.228 6.1.3.1 and 6.5.2.2 determine the affected implicit sets and
 * private identities. Select/validate the whole scope before modifying it,
 * so an unsupported scope cannot partially deregister the subscription. */
int cx_deregister(udomain_t *d, str *private_ids, int private_count, str *ids,
		int count, int reason, unsigned int flags, cx_notify_contact_f notify,
		cx_dereg_result_t *response)
{
	impurecord_t *records[CX_MAX_PUBLIC_IDS], *r, *selected;
	str owners[CX_MAX_PUBLIC_IDS] = {0}, *survivor;
	int primary_survives;
	impu_contact_t *link, *next;
	int known[CX_MAX_PUBLIC_IDS] = {0};
	int protected[CX_MAX_PUBLIC_IDS] = {0};
	int i, j, slot, n = 0, result, owner, keep, emergency, protected_count = 0;
	time_t now = time(NULL);
	str *private_id;

	if(!response)
		return -3;
	memset(response, 0, sizeof(*response));
	if(!d || !private_ids || (count > 0 && !ids) || db_mode != NO_DB
			|| private_count <= 0 || private_count > CX_MAX_PUBLIC_IDS
			|| count < 0 || count > CX_MAX_PUBLIC_IDS || reason < 0
			|| reason > 3 || (!count && reason == 1)) {
		LM_WARN("Cx RTR rejected unsupported backend or invalid scope\n");
		return -3;
	}
	result = cx_lock_cache(d);
	if(result) {
		LM_DBG("Cx RTR cache busy; registrations not changed\n");
		return result;
	}
	/* A named IMPU selects its entire implicit set, not just its record.
	 * IMPI-only requests select every set belonging to the requested IMPIs.
	 * Untargeted private bindings survive, except for NEW_SERVER_ASSIGNED
	 * (reason 1), which terminates the public-identity assignment. */
	for(slot = 0; slot < d->size; slot++) {
		for(r = d->table[slot].first; r; r = r->next) {
			owner = cx_record_private(r, private_ids, private_count);
			if(!owner && reason != 1)
				continue;
			if(count) {
				for(i = 0; i < count; i++) {
					if(cx_str_equal(&r->public_identity, &ids[i]))
						break;
					if(get_impurecord(d, &ids[i], &selected) == 0 && selected->s
							&& cx_profile_has(selected->s, &r->public_identity))
						break;
				}
				if(i == count)
					continue;
			}
			if(n == CX_MAX_PUBLIC_IDS) {
				LM_WARN("Cx RTR selected records exceed the local limit of "
						"%d\n",
						CX_MAX_PUBLIC_IDS);
				result = -2;
				goto done;
			}
			records[n] = r;
			survivor = NULL;
			primary_survives = 0;
			if(owner)
				known[owner - 1] = 1;
			for(link = r->linked_contacts.head; link; link = link->next) {
				if(!VALID_CONTACT(link->contact, now))
					continue;
				if(cx_ambiguous_contact(
						   d, link->contact, &r->private_identity)) {
					LM_WARN("Cx RTR cannot determine the IMPI owning a shared "
							"legacy contact\n");
					result = -3;
					goto done;
				}
				private_id = link->contact->private_identity.len
									 ? &link->contact->private_identity
									 : &r->private_identity;
				owner = cx_private_selected(
						private_id, private_ids, private_count);
				if(!owner && reason != 1) {
					if(!survivor)
						survivor = private_id;
					if(cx_str_equal(private_id, &r->private_identity))
						primary_survives = 1;
					continue;
				}
				if(owner)
					known[owner - 1] = 1;
				/* TS 29.228 6.1.3.1 preserves emergency registrations for
				 * PERMANENT_TERMINATION (0) and REMOVE_S-CSCF (3). */
				emergency = (reason == 0 || reason == 3)
									? cx_emergency_contact(link->contact)
									: 0;
				if(emergency < 0) {
					LM_WARN("Cx RTR cannot classify emergency contact for "
							"public identity [%.*s]\n",
							r->public_identity.len, r->public_identity.s);
					result = -3;
					goto done;
				}
				if(emergency) {
					if(!survivor)
						survivor = private_id;
					if(cx_str_equal(private_id, &r->private_identity))
						primary_survives = 1;
					LM_DBG("Cx RTR protecting emergency identity [%.*s] for "
						   "IMPI [%.*s]\n",
							r->public_identity.len, r->public_identity.s,
							private_id->len, private_id->s);
					if(cx_emergency_pair(
							   response, private_id, &r->public_identity)) {
						result = -2;
						goto done;
					}
					protected[n] = 1;
				}
			}
			if(reason == 3 && r->reg_state == IMPU_REGISTERED
					&& !protected[n]) {
				LM_WARN("Cx RTR REMOVE_S-CSCF targets a registered identity "
						"[%.*s]\n",
						r->public_identity.len, r->public_identity.s);
				result = -3;
				goto done;
			}
			if(survivor && !primary_survives
					&& shm_str_dup(&owners[n], survivor) < 0) {
				LM_ERR("Cx RTR cannot allocate surviving private identity\n");
				result = -2;
				goto done;
			}
			protected_count += protected[n];
			n++;
		}
	}
	for(i = 0; i < count; i++)
		if(get_impurecord(d, &ids[i], &r) != 0)
			LM_WARN("Cx RTR public identity [%.*s] not found in usrloc; "
					"skipping\n",
					ids[i].len, ZSW(ids[i].s));
	/* Queue every alias notification before marking a shared contact deleted. */
	for(i = 0; i < n; i++) {
		r = records[i];
		for(link = r->linked_contacts.head; link; link = link->next) {
			if(!VALID_CONTACT(link->contact, now))
				continue;
			private_id = link->contact->private_identity.len
								 ? &link->contact->private_identity
								 : &r->private_identity;
			if(reason != 1
					&& !cx_private_selected(
							private_id, private_ids, private_count))
				continue;
			if((reason == 0 || reason == 3)
					&& cx_emergency_contact(link->contact))
				continue;
			if(r->shead && r->s && notify)
				notify(r, link->contact,
						reason == 0 && (flags & 1) ? 2 : reason);
		}
	}
	for(i = 0; i < n; i++) {
		r = records[i];
		LM_DBG("Cx RTR processing implicit identity [%.*s]\n",
				r->public_identity.len, r->public_identity.s);
		keep = 0;
		for(link = r->linked_contacts.head; link; link = next) {
			next = link->next;
			private_id = link->contact->private_identity.len
								 ? &link->contact->private_identity
								 : &r->private_identity;
			if((reason != 1
					   && !cx_private_selected(
							   private_id, private_ids, private_count))
					|| ((reason == 0 || reason == 3)
							&& cx_emergency_contact(link->contact))) {
				if(VALID_CONTACT(link->contact, now))
					keep = 1;
				continue;
			}
			LM_DBG("Cx RTR deleting contact [%.*s] for AOR [%.*s]\n",
					link->contact->c.len, ZSW(link->contact->c.s),
					link->contact->aor.len, ZSW(link->contact->aor.s));
			if(!cx_contact_elsewhere(d, link->contact, records, n)) {
				lock_contact_slot_i(link->contact->sl);
				link->contact->state = CONTACT_DELETED;
				unlock_contact_slot_i(link->contact->sl);
			}
			cx_unlink_contact(r, link);
		}
		if(keep && owners[i].s) {
			shm_free(r->private_identity.s);
			r->private_identity = owners[i];
			owners[i] = (str){0, 0};
		}
		if(!keep) {
			r->reg_state = IMPU_NOT_REGISTERED;
			r->send_sar_on_delete = 0;
		}
	}
	/* All copied notifications see the old complete implicit-set profile. */
	for(i = 0; i < n; i++)
		if(records[i]->reg_state == IMPU_NOT_REGISTERED)
			cx_clear_profile(records[i]);
	for(j = 1; j < private_count; j++)
		if(known[j])
			response->associated[response->associated_count++] = private_ids[j];
	result = !protected_count ? 0 : protected_count == n ? 2 : 1;
done:
	for(i = 0; i < CX_MAX_PUBLIC_IDS; i++)
		if(owners[i].s)
			shm_free(owners[i].s);
	cx_unlock_cache(d);
	if(result < 0)
		LM_WARN("Cx RTR cleanup refused before mutation: result=%d\n", result);
	else
		LM_DBG("Cx RTR cleanup complete: reason=%d requested=%d processed=%d "
			   "protected=%d\n",
				reason, count, n, protected_count);
	return result;
}
