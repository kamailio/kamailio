/* Cx profile replacement and network deregistration.
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * As a special exception, the copyright holders of the new contributions
 * permit linking those contributions with the OpenSSL library and
 * distributing the resulting combined work. The GNU General Public
 * License applies to all other code.
 */
#ifdef IMS_CX_PROFILE_TEST
#include "test/cx_profile_mocks.h"
#else
#include "cx_profile.h"
#include "hslot_sp.h"
#include "ims_usrloc_scscf_mod.h"
#include "../../core/hashes.h"
#include "../../core/mem/shm_mem.h"
#include "../../core/ut.h"
#include <string.h>
#endif

extern gen_lock_set_t *ul_locks;
extern int ul_locks_no;
extern int db_mode;
extern int subs_hash_size;
extern ims_subscription_list_t *ims_subscription_list;

#define CX_MAX_PUBLIC_IDS 256

static int cx_str_equal(const str *a, const str *b)
{
	return a->len == b->len && a->len > 0 && memcmp(a->s, b->s, a->len) == 0;
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

	/* Atomic database persistence is intentionally not simulated. */
	if(db_mode != NO_DB || !s || s->sl >= 0 || s->wpsi
			|| !s->private_identity.len)
		return -3;
	for(i = 0; i < s->service_profiles_cnt; i++) {
		for(j = 0; j < s->service_profiles[i].public_identities_cnt; j++) {
			ims_public_identity *pi =
					&s->service_profiles[i].public_identities[j];
			if(n == CX_MAX_PUBLIC_IDS || !pi->public_identity.len
					|| pi->wildcarded_psi.len)
				return -3;
			for(k = 0; k < n; k++)
				if(cx_str_equal(&identities[k]->public_identity,
						   &pi->public_identity))
					return -3;
			identities[n++] = pi;
		}
	}
	if(!n)
		return -3;
	result = cx_lock_cache(d);
	if(result)
		return result;
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
	return 0;
}

static int cx_emergency_contact(ucontact_t *c)
{
	int i;
	/* The baseline does not retain a dedicated emergency ownership flag. */
	for(i = 0; i + 4 <= c->c.len; i++)
		if(c->c.s[i] == ';' && (c->c.s[i + 1] | 32) == 's'
				&& (c->c.s[i + 2] | 32) == 'o' && (c->c.s[i + 3] | 32) == 's'
				&& (i + 4 == c->c.len || c->c.s[i + 4] == ';'
						|| c->c.s[i + 4] == '=' || c->c.s[i + 4] == '?'))
			return 1;
	return 0;
}

int cx_deregister(udomain_t *d, str *private_id, str *ids, int count,
		int reason, cx_notify_contact_f notify)
{
	impurecord_t *records[CX_MAX_PUBLIC_IDS];
	impurecord_t *r, *other;
	impu_contact_t *link, *other_link;
	ims_subscription *old;
	int i, j, slot, n = 0, result, sl;

	if(db_mode != NO_DB || count <= 0 || count > CX_MAX_PUBLIC_IDS || reason < 0
			|| reason > 3)
		return -3;
	result = cx_lock_cache(d);
	if(result)
		return result;
	/* Preflight all identities, preserving unrelated/ambiguous ownership. */
	for(i = 0; i < count; i++) {
		if(get_impurecord(d, &ids[i], &r) != 0)
			continue; /* an already absent alias does not abort the request */
		if(!cx_str_equal(&r->private_identity, private_id)) {
			result = -1;
			goto done;
		}
		for(j = 0; j < n && records[j] != r; j++) {
		}
		if(j == n)
			records[n++] = r;
		if(reason == 3 && r->reg_state == IMPU_REGISTERED) {
			result = -3;
			goto done;
		}
		for(link = r->linked_contacts.head; link; link = link->next) {
			if(cx_emergency_contact(link->contact)) {
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
	return result;
}
