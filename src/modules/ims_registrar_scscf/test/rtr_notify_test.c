/* SPDX-License-Identifier: BSD-3-Clause
 * Exercise the RTR callback and the production deregistration XML builder. */
typedef struct
{
	char *s;
	int len;
} str;
typedef struct
{
	int unused;
} impurecord_t;
typedef struct
{
	str c;
} ucontact_t;
#define IMS_REGISTRAR_CONTACT_DEACTIVATED 21
#define IMS_REGISTRAR_CONTACT_DEREGISTERED 22
#define LM_DBG(...) ((void)0)
#ifdef _WIN32
int _fltused;
__declspec(dllimport) int sprintf(char *, const char *, ...);
#else
extern int sprintf(char *, const char *, ...);
#endif
static __SIZE_TYPE__ strlen(const char *s)
{
	__SIZE_TYPE__ n = 0;
	while(s[n])
		n++;
	return n;
}
static int memcmp(const void *a, const void *b, __SIZE_TYPE__ n)
{
	const unsigned char *x = a, *y = b;
	while(n--) {
		if(*x != *y)
			return *x - *y;
		x++;
		y++;
	}
	return 0;
}
#define STR_APPEND(target, value)                      \
	do {                                               \
		int j;                                         \
		for(j = 0; j < (value).len; j++)               \
			(target).s[(target).len++] = (value).s[j]; \
	} while(0)
#include "rtr_xml_constants.inc"
#include "rtr_xml_builder.inc"
static int count, event, calls;
static impurecord_t *notified_record;
static ucontact_t *notified_contact;
static str *notified_uris;
static int notify_subscribers(
		impurecord_t *r, ucontact_t *c, str *uris, int n, int e)
{
	notified_record = r;
	notified_contact = c;
	notified_uris = uris;
	count = n;
	event = e;
	calls++;
	return 0;
}
#include "rtr_notify_callback.inc"
#define CHECK(test)          \
	do {                     \
		if(!(test))          \
			return __LINE__; \
	} while(0)
static int contains(str *buf, const char *s)
{
	int i, len = (int)strlen(s);
	for(i = 0; i + len <= buf->len; i++)
		if(!memcmp(buf->s + i, s, len))
			return 1;
	return 0;
}
static int run_tests(void)
{
	impurecord_t record;
	ucontact_t contact = {{"sip:user@192.0.2.1:5070;transport=tcp", 37}};
	char xml[1024], pad_storage[512];
	str body = {xml, 0}, pad = {pad_storage, 0};
	int reason;
	for(reason = 0; reason <= 3; reason++) {
		calls = 0;
		cx_notify(&record, &contact, reason);
		CHECK(calls == 1 && count == 1);
		CHECK(notified_record == &record && notified_contact == &contact);
		CHECK(notified_uris == &contact.c);
		CHECK(event
				== (reason == 2 ? IMS_REGISTRAR_CONTACT_DEACTIVATED
								: IMS_REGISTRAR_CONTACT_DEREGISTERED));
	}
	process_xml_for_explit_dereg_contact(&body, &pad, contact.c);
	CHECK(contains(&body, "state=\"terminated\""));
	CHECK(contains(&body, "expires=\"0\""));
	CHECK(contains(&body, "q=\"0.000\""));
	CHECK(contains(&body, "<uri>sip:user@192.0.2.1:5070;transport=tcp</uri>"));
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
