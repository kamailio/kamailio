/* Minimal cache adapters; tests execute cx_profile.c itself.
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * As a special exception, the copyright holders of the new contributions
 * permit linking those contributions with the OpenSSL library and
 * distributing the resulting combined work. The GNU General Public
 * License applies to all other code.
 */
typedef struct
{
	char *s;
	int len;
} str;
typedef struct
{
	int held;
} gen_lock_t;
typedef struct
{
	gen_lock_t locks[2];
} gen_lock_set_t;
typedef struct
{
	str public_identity, wildcarded_psi;
	int barring;
} ims_public_identity;
typedef struct
{
	ims_public_identity *public_identities;
	int public_identities_cnt;
} ims_service_profile;
typedef struct ims_subscription_s
{
	str private_identity;
	int wpsi, ref_count, sl, freed;
	ims_service_profile *service_profiles;
	int service_profiles_cnt;
	struct hslot_sp *slot;
	struct ims_subscription_s *prev, *next;
} ims_subscription;
typedef struct hslot_sp
{
	ims_subscription *first, *last;
	int n;
} hslot_sp_t;
typedef struct
{
	hslot_sp_t slot[1];
} ims_subscription_list_t;
typedef struct ucontact
{
	str c;
	int sl, state, expires;
} ucontact_t;
typedef struct impu_contact
{
	ucontact_t *contact;
	struct impu_contact *next;
} impu_contact_t;
typedef struct impurecord
{
	str public_identity, private_identity, ccf1, ccf2, ecf1, ecf2;
	ims_subscription *s;
	int reg_state, barring, send_sar_on_delete;
	struct
	{
		impu_contact_t *head;
	} linked_contacts;
	void *shead;
	struct impurecord *next;
} impurecord_t;
typedef struct
{
	int locker_pid, recursive_lock_level;
	impurecord_t *first;
} hslot_t;
typedef struct udomain
{
	int size;
	hslot_t table[4];
} udomain_t;
typedef void (*cx_notify_contact_f)(impurecord_t *, ucontact_t *, int);
enum
{
	NO_DB = 0,
	IMPU_NOT_REGISTERED = 0,
	IMPU_REGISTERED = 1,
	IMPU_UNREGISTERED = -1,
	CONTACT_VALID = 0,
	CONTACT_DELETE_PENDING = 1,
	CONTACT_EXPIRE_PENDING_NOTIFY = 2,
	CONTACT_DELETED = 3,
	CONTACT_DELAYED_DELETE = 4,
	CONTACT_NOTIFY_READY = 5
};
static int errors, subs_held, notifications, last_reason;
static int memcmp(const void *a, const void *b, unsigned long n)
{
	const unsigned char *x = a, *y = b;
	unsigned long i;
	for(i = 0; i < n; i++)
		if(x[i] != y[i])
			return x[i] - y[i];
	return 0;
}
static void zero(void *p, unsigned long n)
{
	unsigned char *x = p;
	while(n--)
		*x++ = 0;
}
static int lock_set_try(gen_lock_set_t *s, int i)
{
	if(s->locks[i].held)
		return 1;
	s->locks[i].held = 1;
	return 0;
}
static void lock_set_release(gen_lock_set_t *s, int i)
{
	if(!s->locks[i].held)
		errors++;
	s->locks[i].held = 0;
}
static void atomic_set(int *p, int v)
{
	*p = v;
}
static int my_pid(void)
{
	return 7;
}
static int core_hash(str *a, void *b, int size)
{
	(void)a;
	(void)b;
	(void)size;
	return 0;
}
extern ims_subscription_list_t *ims_subscription_list;
static int get_impurecord(udomain_t *d, str *id, impurecord_t **r)
{
	impurecord_t *p;
	int i;
	for(i = 0; i < d->size; i++)
		for(p = d->table[i].first; p; p = p->next)
			if(p->public_identity.len == id->len
					&& !memcmp(p->public_identity.s, id->s, id->len)) {
				*r = p;
				return 0;
			}
	return 1;
}
static void lock_subscription_slot(int i)
{
	(void)i;
	if(subs_held)
		errors++;
	subs_held = 1;
}
static void unlock_subscription_slot(int i)
{
	(void)i;
	if(!subs_held)
		errors++;
	subs_held = 0;
}
static void lock_subscription(ims_subscription *s)
{
	(void)s;
}
static void unlock_subscription(ims_subscription *s)
{
	(void)s;
}
static void ref_subscription_unsafe(ims_subscription *s)
{
	if(s->freed)
		errors++;
	s->ref_count++;
}
static void add_subscription_unsafe(ims_subscription *s)
{
	hslot_sp_t *slot = &ims_subscription_list->slot[0];
	if(slot->last) {
		s->prev = slot->last;
		slot->last->next = s;
	} else
		slot->first = s;
	slot->last = s;
	slot->n++;
	s->slot = slot;
	s->sl = 0;
}
static void unref_subscription(ims_subscription *s)
{
	hslot_sp_t *slot = s->slot;
	if(s->freed)
		errors++;
	if(--s->ref_count == 0) {
		if(s->sl >= 0) {
			if(!subs_held)
				errors++;
			if(s->prev)
				s->prev->next = s->next;
			else
				slot->first = s->next;
			if(s->next)
				s->next->prev = s->prev;
			else
				slot->last = s->prev;
			slot->n--;
		}
		s->freed = 1;
	}
}
static void lock_contact_slot_i(int i)
{
	(void)i;
}
static void unlock_contact_slot_i(int i)
{
	(void)i;
}
static void shm_free(void *p)
{
	(void)p;
}

/* Adapters for the real originating admission function extracted by the runner. */
struct sip_msg
{
	str asserted, to;
};
static str cscf_get_asserted_identity(struct sip_msg *m, int shared)
{
	(void)shared;
	return m->asserted;
}
static int act_time = 100;
static void get_act_time(void)
{
}
static void lock_udomain(udomain_t *domain, str *id)
{
	(void)domain;
	(void)id;
}
static void unlock_udomain(udomain_t *domain, str *id)
{
	(void)domain;
	(void)id;
}
static struct
{
	void (*lock_udomain)(udomain_t *, str *);
	void (*unlock_udomain)(udomain_t *, str *);
	int (*get_impurecord)(udomain_t *, str *, impurecord_t **);
	void (*lock_contact_slot_i)(int);
	void (*unlock_contact_slot_i)(int);
} ul = {lock_udomain, unlock_udomain, get_impurecord, lock_contact_slot_i,
		unlock_contact_slot_i};
#include "cx_valid_contact.inc"
