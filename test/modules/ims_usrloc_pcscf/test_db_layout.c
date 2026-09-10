#include "test_common.h"
#include "pcscf_db_layout.h"

void test_load_ncols(void)
{
	TEST_ASSERT(PCSCF_LOAD_NCOLS == 22);
}

void test_row_offsets(void)
{
	TEST_ASSERT(PCSCF_ROW_ID_OFF == 0);
	TEST_ASSERT(PCSCF_ROW_DOMAIN_OFF == 1);
	TEST_ASSERT(PCSCF_ROW_AOR_OFF == 2);
	TEST_ASSERT(PCSCF_ROW_BARRED_OFF == 20);
}

int main(void)
{
	RUN_TEST(test_load_ncols);
	RUN_TEST(test_row_offsets);
	printf("ALL TESTS PASSED\n");
	return 0;
}
