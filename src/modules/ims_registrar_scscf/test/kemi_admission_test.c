/* SPDX-License-Identifier: BSD-3-Clause
 * Test the production KEMI wrapper and registration table with adapters. */
typedef struct
{
	char *s;
	int len;
} str;
typedef struct
{
	int marker;
} sip_msg_t;
typedef struct
{
	int marker;
} udomain_t;
#define NULL ((void *)0)
#define PKG_MEM_ERROR ((void)0)
#define LM_ERR(...) ((void)0)
#define SR_KEMIP_NONE 0
#define SR_KEMIP_INT 1
#define SR_KEMIP_STR 2
#define str_init(value) {value, sizeof(value) - 1}
typedef struct
{
	str mname, fname;
	int rtype;
	void *func;
	int ptypes[6];
} sr_kemi_t;
static char buffer[128];
static udomain_t domain;
static int allocations, releases, lookups, delegates, oom, result, null_domain;
static sip_msg_t *last_msg;
static char *last_domain;
static sr_kemi_t *registered;
static int registration_result;
static void *pkg_malloc(unsigned int n)
{
	allocations++;
	return !oom && n <= sizeof(buffer) ? buffer : NULL;
}
static void pkg_free(void *p)
{
	(void)p;
	releases++;
}
static void *memcpy(void *dest, const void *src, __SIZE_TYPE__ n)
{
	char *d = dest;
	const char *s = src;
	while(n--)
		*d++ = *s++;
	return dest;
}
static void *memchr(const void *src, int byte, __SIZE_TYPE__ n)
{
	const unsigned char *s = src;
	while(n--) {
		if(*s == (unsigned char)byte)
			return (void *)s;
		s++;
	}
	return NULL;
}
static int same(const char *a, const char *b)
{
	while(*a && *b && *a == *b) {
		a++;
		b++;
	}
	return *a == *b;
}
static int get_domain(const char *name, udomain_t **d)
{
	lookups++;
	*d = null_domain ? NULL : &domain;
	return same(name, "location") ? 0 : -1;
}
static struct
{
	int (*get_udomain)(const char *, udomain_t **);
} ul = {get_domain};
static int orig_impu_has_contact(sip_msg_t *msg, char *d, char *unused)
{
	(void)unused;
	last_msg = msg;
	last_domain = d;
	delegates++;
	return result;
}
static int sr_kemi_modules_add(sr_kemi_t *exports)
{
	registered = exports;
	return registration_result;
}
#include "kemi_wrapper.inc"
#include "kemi_registration.inc"
#define CHECK(test)          \
	do {                     \
		if(!(test))          \
			return __LINE__; \
	} while(0)
static void reset(void)
{
	allocations = releases = lookups = delegates = oom = null_domain = 0;
	result = 1;
}
static int run_tests(void)
{
	sip_msg_t msg;
	char runtime_name[] = {'l', 'o', 'c', 'a', 't', 'i', 'o', 'n', 'X'};
	char embedded[] = {'l', 'o', 'c', 0, 't', 'i', 'o', 'n'};
	str name = {runtime_name, 8}, unknown = {"unknown", 7}, bad = {embedded, 8},
		empty = {runtime_name, 0};
	reset();
	CHECK(ki_orig_impu_has_contact(&msg, &name) == 1);
	CHECK(allocations == 1 && releases == 1 && lookups == 1 && delegates == 1);
	CHECK(last_msg == &msg && last_domain == (char *)&domain
			&& runtime_name[8] == 'X');
	reset();
	result = -1;
	CHECK(ki_orig_impu_has_contact(&msg, &name) == -1 && delegates == 1
			&& releases == 1);
	reset();
	CHECK(ki_orig_impu_has_contact(&msg, &unknown) == -1 && !delegates
			&& releases == 1);
	reset();
	null_domain = 1;
	CHECK(ki_orig_impu_has_contact(&msg, &name) == -1 && !delegates
			&& releases == 1);
	reset();
	CHECK(ki_orig_impu_has_contact(&msg, NULL) == -1 && !allocations);
	CHECK(ki_orig_impu_has_contact(NULL, &name) == -1 && !allocations);
	CHECK(ki_orig_impu_has_contact(&msg, &empty) == -1 && !allocations);
	empty.len = -1;
	CHECK(ki_orig_impu_has_contact(&msg, &empty) == -1 && !allocations);
	empty.s = NULL;
	empty.len = 8;
	CHECK(ki_orig_impu_has_contact(&msg, &empty) == -1 && !allocations);
	CHECK(ki_orig_impu_has_contact(&msg, &bad) == -1 && !allocations);
	reset();
	oom = 1;
	CHECK(ki_orig_impu_has_contact(&msg, &name) == -1 && allocations == 1
			&& !lookups && !releases);
	registration_result = 0;
	CHECK(mod_register(NULL, NULL, NULL, NULL) == 0 && registered);
	CHECK(same(registered[0].mname.s, "ims_registrar_scscf"));
	CHECK(same(registered[0].fname.s, "orig_impu_has_contact"));
	CHECK(registered[0].rtype == SR_KEMIP_INT
			&& registered[0].ptypes[0] == SR_KEMIP_STR);
	CHECK(registered[0].func == (void *)ki_orig_impu_has_contact
			&& registered[1].func == NULL);
	registration_result = -1;
	CHECK(mod_register(NULL, NULL, NULL, NULL) == -1);
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
