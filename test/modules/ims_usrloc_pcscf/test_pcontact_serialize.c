#include "test_common.h"
#include "pcontact_serialize.h"
#include "../../core/str.h"

static int test_serialize_roundtrip(void)
{
	str impus[] = {
			str_init("sip:user@ims.example.com"), str_init("tel:+15551234567")};
	char buf[512];
	str out = {buf, 0};
	str parsed[4];
	int n;

	TEST_ASSERT(pcscf_serialize_impus(impus, 2, &out, sizeof(buf)) == 0);
	TEST_ASSERT(out.len > 0);
	TEST_ASSERT(memcmp(out.s, "<sip:user@ims.example.com><tel:+15551234567>",
						out.len)
				== 0);

	n = pcscf_parse_impus(&out, parsed, 4);
	TEST_ASSERT(n == 2);
	TEST_ASSERT(parsed[0].len == impus[0].len);
	TEST_ASSERT(memcmp(parsed[0].s, impus[0].s, impus[0].len) == 0);
	TEST_ASSERT(parsed[1].len == impus[1].len);
	return 0;
}

static int test_barred_crossref(void)
{
	str impus[] = {str_init("sip:a@d.com"), str_init("tel:+1")};
	str barred[] = {str_init("tel:+1")};
	char buf[256];
	str out = {buf, 0};
	char flags[4];

	TEST_ASSERT(pcscf_apply_barred_flags(impus, 2, barred, 1, flags) == 0);
	TEST_ASSERT(flags[0] == 0);
	TEST_ASSERT(flags[1] == 1);
	TEST_ASSERT(
			pcscf_serialize_impus_barred(barred, 1, &out, sizeof(buf)) == 0);
	TEST_ASSERT(out.len == strlen("<tel:+1>"));
	return 0;
}

static int test_serialize_empty(void)
{
	str impus[1];
	char buf[64];
	str out = {buf, 0};

	TEST_ASSERT(pcscf_serialize_impus(impus, 0, &out, sizeof(buf)) == 0);
	TEST_ASSERT(out.len == 0);
	return 0;
}

static int test_parse_single_impu(void)
{
	str in = str_init("<sip:only@example.com>");
	str parsed[2];
	int n;

	n = pcscf_parse_impus(&in, parsed, 2);
	TEST_ASSERT(n == 1);
	TEST_ASSERT(parsed[0].len == strlen("sip:only@example.com"));
	TEST_ASSERT(
			memcmp(parsed[0].s, "sip:only@example.com", parsed[0].len) == 0);
	return 0;
}

static int test_barred_case_insensitive(void)
{
	str impus[] = {str_init("sip:a@d.com"), str_init("TEL:+1")};
	str barred[] = {str_init("tel:+1")};
	char flags[4];

	TEST_ASSERT(pcscf_apply_barred_flags(impus, 2, barred, 1, flags) == 0);
	TEST_ASSERT(flags[1] == 1);
	return 0;
}

int main(void)
{
	int failures = 0;
	RUN_TEST(test_serialize_roundtrip);
	RUN_TEST(test_barred_crossref);
	RUN_TEST(test_serialize_empty);
	RUN_TEST(test_parse_single_impu);
	RUN_TEST(test_barred_case_insensitive);
	return failures > 0 ? 1 : 0;
}
