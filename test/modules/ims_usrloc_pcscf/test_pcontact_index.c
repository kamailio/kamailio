#include "test_common.h"
#include "pcontact_index.h"
#include "../../core/str.h"

static int test_index_add_get_remove(void)
{
	pcscf_index_t idx;
	pcscf_index_entry_t *e;
	str key = str_init("sip:user@domain.com");
	pcontact_t fake;

	memset(&fake, 0, sizeof(fake));
	TEST_ASSERT(pcscf_index_init(&idx) == 0);
	TEST_ASSERT(pcscf_index_add(&idx, &key, &fake) == 0);
	e = pcscf_index_get(&idx, &key);
	TEST_ASSERT(e != NULL);
	TEST_ASSERT(e->contact == &fake);
	TEST_ASSERT(pcscf_index_remove_contact(&idx, &fake) == 0);
	TEST_ASSERT(pcscf_index_get(&idx, &key) == NULL);
	pcscf_index_destroy(&idx);
	return 0;
}

static int test_index_multiple_keys(void)
{
	pcscf_index_t idx;
	str key1 = str_init("sip:a@d.com");
	str key2 = str_init("sip:b@d.com");
	pcontact_t c1, c2;

	memset(&c1, 0, sizeof(c1));
	memset(&c2, 0, sizeof(c2));
	TEST_ASSERT(pcscf_index_init(&idx) == 0);
	TEST_ASSERT(pcscf_index_add(&idx, &key1, &c1) == 0);
	TEST_ASSERT(pcscf_index_add(&idx, &key2, &c2) == 0);
	TEST_ASSERT(pcscf_index_get(&idx, &key1)->contact == &c1);
	TEST_ASSERT(pcscf_index_get(&idx, &key2)->contact == &c2);
	TEST_ASSERT(pcscf_index_remove_contact(&idx, &c1) == 0);
	TEST_ASSERT(pcscf_index_get(&idx, &key1) == NULL);
	TEST_ASSERT(pcscf_index_get(&idx, &key2) != NULL);
	pcscf_index_destroy(&idx);
	return 0;
}

static int test_index_miss(void)
{
	pcscf_index_t idx;
	str key = str_init("sip:missing@d.com");

	TEST_ASSERT(pcscf_index_init(&idx) == 0);
	TEST_ASSERT(pcscf_index_get(&idx, &key) == NULL);
	pcscf_index_destroy(&idx);
	return 0;
}

int main(void)
{
	int failures = 0;
	RUN_TEST(test_index_add_get_remove);
	RUN_TEST(test_index_multiple_keys);
	RUN_TEST(test_index_miss);
	return failures > 0 ? 1 : 0;
}
