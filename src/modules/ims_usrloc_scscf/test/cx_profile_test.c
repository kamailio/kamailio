/* Run actual cache operations with deterministic cache/lock adapters.
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * As a special exception, the copyright holders of the new contributions
 * permit linking those contributions with the OpenSSL library and
 * distributing the resulting combined work. The GNU General Public
 * License applies to all other code.
 */
#define IMS_CX_PROFILE_TEST
#include "../cx_profile.c"
#include "cx_admission.inc"

static gen_lock_set_t lockset;
gen_lock_set_t *ul_locks = &lockset;
int ul_locks_no = 2, db_mode = 0, subs_hash_size = 1;
static ims_subscription_list_t registry;
ims_subscription_list_t *ims_subscription_list = &registry;
static udomain_t d;
static impurecord_t r[3];
static ucontact_t c[2];
static impu_contact_t links[3];
static ims_subscription old, other, fresh;
static ims_public_identity pi[2];
static ims_service_profile sp;
static str ids[3];
static void init(void)
{
	int i;
	zero(&d, sizeof(d));
	zero(r, sizeof(r));
	zero(c, sizeof(c));
	zero(links, sizeof(links));
	zero(&old, sizeof(old));
	zero(&other, sizeof(other));
	zero(&fresh, sizeof(fresh));
	zero(&registry, sizeof(registry));
	zero(&lockset, sizeof(lockset));
	zero(pi, sizeof(pi));
	errors = subs_held = notifications = last_reason = 0;
	db_mode = 0;
	d.size = 4;
	ids[0] = (str){"sip:a", 5};
	ids[1] = (str){"tel:a", 5};
	ids[2] = (str){"sip:b", 5};
	old.private_identity = (str){"impi-a", 6};
	other.private_identity = (str){"impi-b", 6};
	old.ref_count = 2;
	other.ref_count = 1;
	old.sl = other.sl = fresh.sl = -1;
	add_subscription_unsafe(&old);
	add_subscription_unsafe(&other);
	fresh.private_identity = old.private_identity;
	fresh.ref_count = 1;
	pi[0].public_identity = ids[0];
	pi[1].public_identity = ids[1];
	pi[1].barring = 1;
	sp.public_identities = pi;
	sp.public_identities_cnt = 2;
	fresh.service_profiles = &sp;
	fresh.service_profiles_cnt = 1;
	c[0].c = (str){"sip:device-a", 12};
	c[1].c = (str){"sip:device-b", 12};
	c[0].expires = c[1].expires = 200;
	for(i = 0; i < 3; i++) {
		r[i].public_identity = ids[i];
		r[i].private_identity =
				i == 2 ? other.private_identity : old.private_identity;
		r[i].s = i == 2 ? &other : &old;
		r[i].reg_state = IMPU_REGISTERED;
		r[i].send_sar_on_delete = 1;
		r[i].shead = &d;
		r[i].linked_contacts.head = &links[i];
		links[i].contact = &c[i == 2 ? 1 : 0];
		if(i < 2)
			r[i].next = &r[i + 1];
	}
	d.table[0].first = &r[0];
}
static void notify(impurecord_t *record, ucontact_t *contact, int reason)
{
	if(!r[0].s || !r[1].s || record->reg_state != IMPU_NOT_REGISTERED
			|| contact->state != CONTACT_DELETED)
		errors++;
	notifications++;
	last_reason = reason;
}
#define CHECK(x)             \
	do {                     \
		if(!(x))             \
			return __LINE__; \
	} while(0)
static int run_tests(void)
{
	str absent = {"absent", 6};
	int result;
	struct sip_msg msg = {{"sip:a;user=phone", 16}, {"sip:b", 5}};
	init();
	CHECK(cx_replace_profile(&d, &fresh) == 0);
	CHECK(r[0].s == &fresh && r[1].s == &fresh && r[2].s == &other);
	CHECK(r[0].reg_state == IMPU_REGISTERED && c[0].state == 0);
	CHECK(r[1].barring == 1 && registry.slot[0].first == &fresh);
	CHECK(old.freed && fresh.ref_count == 3 && !errors);
	init();
	pi[1].public_identity = absent;
	CHECK(cx_replace_profile(&d, &fresh) == -1);
	CHECK(r[0].s == &old && old.ref_count == 2 && fresh.ref_count == 1
			&& !errors);
	init();
	r[1].private_identity = other.private_identity;
	CHECK(cx_replace_profile(&d, &fresh) == -1);
	CHECK(r[0].s == &old && registry.slot[0].first == &old);
	init();
	pi[1].public_identity = pi[0].public_identity;
	CHECK(cx_replace_profile(&d, &fresh) == -3);
	init();
	lockset.locks[1].held = 1;
	CHECK(cx_replace_profile(&d, &fresh) == -2);
	CHECK(!lockset.locks[0].held && r[0].s == &old && !errors);
	init();
	db_mode = 1;
	CHECK(cx_replace_profile(&d, &fresh) == -3);
	init();
	sp.public_identities_cnt = 0;
	CHECK(cx_replace_profile(&d, &fresh) == -3);
	init();
	fresh.wpsi = 1;
	CHECK(cx_replace_profile(&d, &fresh) == -3);
	init();
	old.ref_count++;
	CHECK(cx_replace_profile(&d, &fresh) == 0);
	CHECK(!old.freed && old.ref_count == 1 && !errors);
	init();
	ids[2] = absent;
	CHECK(cx_deregister(&d, &old.private_identity, ids, 3, 0, notify) == 0);
	CHECK(!r[0].s && !r[1].s && r[2].s == &other && old.freed);
	CHECK(r[0].reg_state == IMPU_NOT_REGISTERED
			&& c[0].state == CONTACT_DELETED);
	CHECK(c[1].state == 0 && notifications == 2 && !r[0].send_sar_on_delete
			&& !errors);
	CHECK(cx_deregister(&d, &old.private_identity, ids, 3, 0, notify) == 0
			&& !errors);
	init();
	CHECK(cx_deregister(&d, &other.private_identity, ids, 2, 0, notify) == -1);
	CHECK(r[0].s == &old && c[0].state == 0 && notifications == 0);
	init();
	CHECK(cx_deregister(&d, &old.private_identity, ids, 2, 3, notify) == -3);
	init();
	r[0].reg_state = r[1].reg_state = IMPU_UNREGISTERED;
	r[0].linked_contacts.head = r[1].linked_contacts.head = 0;
	CHECK(cx_deregister(&d, &old.private_identity, ids, 2, 3, notify) == 0);
	CHECK(!r[0].s && !r[1].s && old.freed && !errors);
	init();
	links[2].contact = &c[0];
	CHECK(cx_deregister(&d, &old.private_identity, ids, 2, 0, notify) == -3);
	CHECK(r[0].s == &old && c[0].state == 0 && !notifications);
	init();
	c[0].c = (str){"sip:device-a;sos", 16};
	CHECK(cx_deregister(&d, &old.private_identity, ids, 2, 0, notify) == -3);
	CHECK(r[0].s == &old && c[0].state == 0);
	init();
	CHECK(cx_deregister(&d, &old.private_identity, &absent, 1, 0, notify) == 0);
	CHECK(r[0].s == &old && !notifications);
	init();
	CHECK(cx_deregister(&d, &old.private_identity, ids, 257, 0, notify) == -3);
	init();
	result = cx_deregister(&d, &old.private_identity, ids, 2, 2, notify);
	CHECK(result == 0 && last_reason == 2 && !errors);
	init();
	CHECK(orig_impu_has_contact(&msg, (char *)&d, 0) == 1);
	r[0].reg_state = IMPU_NOT_REGISTERED;
	CHECK(orig_impu_has_contact(&msg, (char *)&d, 0)
			== -1); /* active callee cannot authorize caller */
	init();
	c[0].state = CONTACT_DELETED;
	CHECK(orig_impu_has_contact(&msg, (char *)&d, 0) == -1);
	init();
	c[0].expires = 99;
	CHECK(orig_impu_has_contact(&msg, (char *)&d, 0) == -1);
	init();
	r[0].barring = 1;
	CHECK(orig_impu_has_contact(&msg, (char *)&d, 0) == -1);
	init();
	r[0].s = 0;
	CHECK(orig_impu_has_contact(&msg, (char *)&d, 0) == -1);
	init();
	c[0].expires = 0;
	CHECK(orig_impu_has_contact(&msg, (char *)&d, 0) == 1);
	init();
	msg.asserted = (str){0, 0};
	CHECK(orig_impu_has_contact(&msg, (char *)&d, 0) == -1);
	init();
	msg.asserted = absent;
	CHECK(orig_impu_has_contact(&msg, (char *)&d, 0) == -1);
	return 0;
}
#ifdef _WIN32
__declspec(dllimport) void __stdcall ExitProcess(unsigned int);
void mainCRTStartup(void)
{
	ExitProcess((unsigned int)run_tests());
}
#else
int main(void)
{
	return run_tests();
}
#endif
