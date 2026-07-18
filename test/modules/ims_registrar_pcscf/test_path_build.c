#include "test_common.h"
#include <string.h>

typedef struct
{
	char *s;
	int len;
} str;

static inline str str_init(const char *s)
{
	str r;
	r.s = (char *)s;
	r.len = (int)strlen(s);
	return r;
}

int pcscf_build_path_uri(str *uri, str *out, char *buf, int buf_len);

static int test_build_path(void)
{
	str uri = str_init("sip:pcscf.ims.local:4060");
	str out = {0, 0};
	char buf[256];

	TEST_ASSERT(pcscf_build_path_uri(&uri, &out, buf, sizeof(buf)) == 0);
	TEST_ASSERT(out.len > 0);
	TEST_ASSERT(strstr(out.s, "sip:pcscf.ims.local:4060;lr") != NULL);
	TEST_ASSERT(strstr(out.s, "sip:sip:") == NULL);
	return 0;
}

int main(void)
{
	int failures = 0;
	RUN_TEST(test_build_path);
	return failures ? 1 : 0;
}
