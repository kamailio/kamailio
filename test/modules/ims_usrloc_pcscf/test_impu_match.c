#include "test_common.h"
#include "impu_match.h"
#include "../../core/str.h"

static int test_impu_exact_match_case_insensitive(void)
{
	str aor = str_init("sip:User@IMS.example.com");
	str impu = str_init("sip:user@ims.example.com");

	TEST_ASSERT(pcscf_impu_matches_aor(&aor, &impu) == 1);
	return 0;
}

static int test_impu_no_match(void)
{
	str aor = str_init("sip:alice@ims.example.com");
	str impu = str_init("sip:bob@ims.example.com");

	TEST_ASSERT(pcscf_impu_matches_aor(&aor, &impu) == 0);
	return 0;
}

static int test_contact_primary_aor_match(void)
{
	pcontact_t c;
	str search = str_init("sip:user@ims.example.com");

	memset(&c, 0, sizeof(c));
	STR_STATIC_SET(c.aor, "sip:user@ims.example.com");
	c.head = c.tail = NULL;

	TEST_ASSERT(pcscf_contact_has_impu(&c, &search) == 1);
	return 0;
}

static int test_contact_implicit_tel_impu(void)
{
	static ppublic_t pub1, pub2;
	pcontact_t c;
	str search = str_init("tel:+15551234567");

	memset(&c, 0, sizeof(c));
	STR_STATIC_SET(c.aor, "sip:user@ims.example.com");
	STR_STATIC_SET(pub1.public_identity, "sip:user@ims.example.com");
	pub1.barred = 0;
	pub1.next = &pub2;
	STR_STATIC_SET(pub2.public_identity, "tel:+15551234567");
	pub2.barred = 0;
	pub2.next = NULL;
	c.head = &pub1;
	c.tail = &pub2;

	TEST_ASSERT(pcscf_contact_has_impu(&c, &search) == 1);
	return 0;
}

static int test_contact_missing_impu(void)
{
	static ppublic_t pub1;
	pcontact_t c;
	str search = str_init("tel:+19998887777");

	memset(&c, 0, sizeof(c));
	STR_STATIC_SET(c.aor, "sip:user@ims.example.com");
	STR_STATIC_SET(pub1.public_identity, "sip:user@ims.example.com");
	pub1.next = NULL;
	c.head = &pub1;

	TEST_ASSERT(pcscf_contact_has_impu(&c, &search) == 0);
	return 0;
}

static int test_impu_barred_flag(void)
{
	static ppublic_t pub1, pub2;
	pcontact_t c;
	str barred_impu = str_init("tel:+15551234567");
	str open_impu = str_init("sip:user@ims.example.com");

	memset(&c, 0, sizeof(c));
	STR_STATIC_SET(pub1.public_identity, "sip:user@ims.example.com");
	pub1.barred = 0;
	pub1.next = &pub2;
	STR_STATIC_SET(pub2.public_identity, "tel:+15551234567");
	pub2.barred = 1;
	pub2.next = NULL;
	c.head = &pub1;

	TEST_ASSERT(is_impu_barred(&c, &barred_impu) == 1);
	TEST_ASSERT(is_impu_barred(&c, &open_impu) == 0);
	return 0;
}

static int test_impu_user_part_sip_tel(void)
{
	str aor = str_init("tel:+15551234567");
	str impu = str_init("sip:+15551234567@ims.example.com");

	TEST_ASSERT(pcscf_impu_matches_aor(&aor, &impu) == 1);
	return 0;
}

int main(void)
{
	int failures = 0;
	RUN_TEST(test_impu_exact_match_case_insensitive);
	RUN_TEST(test_impu_no_match);
	RUN_TEST(test_contact_primary_aor_match);
	RUN_TEST(test_contact_implicit_tel_impu);
	RUN_TEST(test_contact_missing_impu);
	RUN_TEST(test_impu_barred_flag);
	RUN_TEST(test_impu_user_part_sip_tel);
	return failures > 0 ? 1 : 0;
}
