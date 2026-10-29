/* SPDX-License-Identifier: BSD-3-Clause
 * Execute the production subscription-error branch with deterministic adapters. */
typedef struct
{
	char *s;
	int len;
} str;
typedef struct
{
	str *pres_uri, *watcher_uri, contact, remote_contact, to_tag, id;
	int desired_expires, event;
	str *outbound_proxy, *extra_headers;
	void *cb_param;
} ua_pres_t;
typedef struct
{
	str *pres_uri, *watcher_uri, *contact, *remote_target;
	int expires, flag, source_flag, event;
	str id;
	str *outbound_proxy, *extra_headers;
	void *cb_param;
} subs_info_t;
#define NULL ((void *)0)
#define PUA_DB_ONLY 1
#define INSERT_TYPE 2
#define LM_ERR(...) ((void)0)
static int fake_now, sends, deletes, commits, retry_expires;
static long long time(void *unused)
{
	(void)unused;
	return fake_now;
}
static void *memset(void *target, int value, __SIZE_TYPE__ size)
{
	unsigned char *p = target;
	while(size--)
		*p++ = (unsigned char)value;
	return target;
}
static void find_and_delete_dialog(ua_pres_t *hentity, int hash_code)
{
	(void)hentity;
	(void)hash_code;
	deletes++;
}
static int end_db_transaction(void *connection)
{
	(void)connection;
	commits++;
	return 0;
}
static int send_subscribe(subs_info_t *subs)
{
	sends++;
	retry_expires = subs->expires;
	return 0;
}
static int exercise(int status, int desired, int now, int dbmode)
{
	ua_pres_t entity, *hentity = &entity;
	struct
	{
		int code;
	} response = {status}, *ps = &response;
	struct
	{
		int (*end_transaction)(void *);
	} pua_dbf = {end_db_transaction};
	void *pua_db = NULL;
	int hash_code = 0, flag = 1, end_transaction = 1;
	memset(&entity, 0, sizeof(entity));
	entity.desired_expires = desired;
	fake_now = now;
	sends = deletes = commits = retry_expires = 0;
#include "subscribe_error_branch.inc"
done:
	(void)end_transaction;
	return 0;
error:
	return -1;
}
#define CHECK(test)          \
	do {                     \
		if(!(test))          \
			return __LINE__; \
	} while(0)
static int run_tests(void)
{
	int codes[] = {404, 481, 500, 408};
	int i;
	for(i = 0; i < 4; i++) {
		/* Unsubscribe created and answered in the same second (the capture). */
		CHECK(exercise(codes[i], 100, 100, 0) == 0);
		CHECK(deletes == 1 && sends == 0);
		/* A later response also must not revive the cancelled dialog. */
		CHECK(exercise(codes[i], 100, 102, 0) == 0);
		CHECK(deletes == 1 && sends == 0);
		/* Preserve retry of a still-wanted active subscription. */
		CHECK(exercise(codes[i], 130, 100, 0) == 0);
		CHECK(deletes == 1 && sends == 1 && retry_expires == 33);
		/* Zero desired_expires means an indefinite subscription, not cancel. */
		CHECK(exercise(codes[i], 0, 100, 0) == 0);
		CHECK(sends == 1 && retry_expires == -1);
		/* The database-only cleanup must still commit and stop. */
		CHECK(exercise(codes[i], 100, 100, PUA_DB_ONLY) == 0);
		CHECK(deletes == 1 && commits == 1 && sends == 0);
	}
	CHECK(exercise(404, 99, 100, 0) == 0 && sends == 0);
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
