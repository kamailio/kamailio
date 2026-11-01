#include "test_common.h"
#include "pcontact_index.h"
#include "usrloc.h"
#include <time.h>

static int test_lru_add_get(void)
{
	pcontact_t contact;
	str old_gruu = str_init("sip:old@ims.local;gr=urn:uuid:aaaa");
	time_t expires;

	memset(&contact, 0, sizeof(contact));
	TEST_ASSERT(pcscf_temp_gruu_lru_init(8) == 0);
	expires = time(NULL) + 3600;
	TEST_ASSERT(pcscf_temp_gruu_lru_add(&old_gruu, &contact, expires) == 0);
	TEST_ASSERT(pcscf_temp_gruu_lru_get(&old_gruu) == &contact);
	pcscf_temp_gruu_lru_destroy();
	return 0;
}

static int test_lru_expired_miss(void)
{
	pcontact_t contact;
	str old_gruu = str_init("sip:expired@ims.local;gr=urn:uuid:bbbb");
	time_t expires;

	memset(&contact, 0, sizeof(contact));
	TEST_ASSERT(pcscf_temp_gruu_lru_init(8) == 0);
	expires = time(NULL) - 60;
	TEST_ASSERT(pcscf_temp_gruu_lru_add(&old_gruu, &contact, expires) == 0);
	TEST_ASSERT(pcscf_temp_gruu_lru_get(&old_gruu) == NULL);
	pcscf_temp_gruu_lru_destroy();
	return 0;
}

static int test_lru_ring_wrap(void)
{
	pcontact_t contacts[3];
	str key1 = str_init("sip:gruu1@ims.local;gr=u1");
	str key2 = str_init("sip:gruu2@ims.local;gr=u2");
	str key3 = str_init("sip:gruu3@ims.local;gr=u3");
	time_t expires;

	TEST_ASSERT(pcscf_temp_gruu_lru_init(2) == 0);
	expires = time(NULL) + 3600;
	memset(&contacts[0], 0, sizeof(contacts[0]));
	memset(&contacts[1], 0, sizeof(contacts[1]));
	memset(&contacts[2], 0, sizeof(contacts[2]));
	TEST_ASSERT(pcscf_temp_gruu_lru_add(&key1, &contacts[0], expires) == 0);
	TEST_ASSERT(pcscf_temp_gruu_lru_add(&key2, &contacts[1], expires) == 0);
	TEST_ASSERT(pcscf_temp_gruu_lru_add(&key3, &contacts[2], expires) == 0);
	TEST_ASSERT(pcscf_temp_gruu_lru_get(&key1) == NULL);
	TEST_ASSERT(pcscf_temp_gruu_lru_get(&key2) == &contacts[1]);
	TEST_ASSERT(pcscf_temp_gruu_lru_get(&key3) == &contacts[2]);
	pcscf_temp_gruu_lru_destroy();
	return 0;
}

int main(void)
{
	int failures = 0;

	RUN_TEST(test_lru_add_get);
	RUN_TEST(test_lru_expired_miss);
	RUN_TEST(test_lru_ring_wrap);
	return failures > 0 ? 1 : 0;
}
