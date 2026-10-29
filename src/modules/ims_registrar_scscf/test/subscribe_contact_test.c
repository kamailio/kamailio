/* SPDX-License-Identifier: BSD-3-Clause
 * Exercise the real reply builder and subscription reply call sites. */
typedef struct
{
	char *s;
	int len;
} str;
struct sip_msg
{
	str to;
};
static char allocation[512], reply[1024];
static int reply_len, reply_code;
void *memset(void *target, int value, __SIZE_TYPE__ size)
{
	unsigned char *p = target;
	while(size--)
		*p++ = (unsigned char)value;
	return target;
}
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
static void *pkg_malloc(int size)
{
	return size <= 512 ? allocation : (void *)0;
}
static void pkg_free(void *p)
{
	(void)p;
}
static int sprintf(char *target, const char *format, int value)
{
	char digits[16];
	int n = 0, len = 0;
	unsigned int v = value;
	(void)format;
	if(value < 0) {
		target[len++] = '-';
		v = (unsigned int)-value;
	}
	do {
		digits[n++] = (char)('0' + v % 10);
		v /= 10;
	} while(v);
	while(n)
		target[len++] = digits[--n];
	target[len] = 0;
	return len;
}
#define LM_ERR(...) ((void)0)
#define STR_APPEND(target, value)                             \
	do {                                                      \
		int append_i;                                         \
		for(append_i = 0; append_i < (value).len; append_i++) \
			(target).s[(target).len++] = (value).s[append_i]; \
	} while(0)
static int cscf_add_header_rpl(struct sip_msg *msg, str *hdr)
{
	int i;
	(void)msg;
	for(i = 0; i < hdr->len; i++)
		reply[reply_len++] = hdr->s[i];
	reply[reply_len] = 0;
	return 0;
}
static int send_reply(struct sip_msg *msg, int code, char *text)
{
	(void)msg;
	(void)text;
	reply_code = code;
	return 1;
}
static struct
{
	int (*t_reply)(struct sip_msg *, int, char *);
} tmb = {send_reply};
static str expires_hdr1 = {"Expires: ", 9}, expires_hdr2 = {"\r\n", 2};
static str contact_hdr1 = {"Contact: <", 10}, contact_hdr2 = {">\r\n", 3};
static str scscf_name_str = {"sip:scscf.example.net", 21};
#define MSG_REG_SUBSCRIBE_OK "Subscription to REG saved"
#define MSG_REG_UNSUBSCRIBE_OK "Subscription to REG removed"
#include "subscribe_reply_body.inc"
static int exercise(int unsubscribe)
{
	struct sip_msg message, *msg = &message;
	str presentity_uri = {"sip:user@example.net", 20};
	int expires = unsubscribe ? 0 : 3600;
	message.to = presentity_uri;
	reply_len = reply_code = 0;
	if(unsubscribe) {
#include "unsubscribe_reply_call.inc"
	} else {
#include "subscribe_reply_call.inc"
	}
	return reply_code == 200 && message.to.s == presentity_uri.s;
}
#define CHECK(test)          \
	do {                     \
		if(!(test))          \
			return __LINE__; \
	} while(0)
static int run_tests(void)
{
	int i, found;
	const char *expected = "Contact: <sip:scscf.example.net>\r\n";
	CHECK(exercise(0));
	found = 0;
	for(i = 0; i + (int)strlen(expected) <= reply_len; i++)
		if(!memcmp(reply + i, expected, strlen(expected)))
			found = 1;
	CHECK(found);
	CHECK(exercise(1));
	found = 0;
	for(i = 0; i + (int)strlen(expected) <= reply_len; i++)
		if(!memcmp(reply + i, expected, strlen(expected)))
			found = 1;
	CHECK(found);
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
