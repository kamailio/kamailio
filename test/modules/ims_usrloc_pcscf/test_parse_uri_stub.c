#ifdef UNIT_TEST
#include "../../core/str.h"
#include <string.h>

#define SIP_URI_T 1
#define TEL_URI_T 2

struct sip_uri
{
	int type;
	str user;
	str host;
};

int parse_uri(char *buf, int len, struct sip_uri *uri)
{
	char *at;

	if(!buf || len <= 0 || !uri)
		return -1;
	memset(uri, 0, sizeof(*uri));
	if(len >= 4 && strncasecmp(buf, "sip:", 4) == 0) {
		uri->type = SIP_URI_T;
		uri->user.s = buf + 4;
		at = memchr(uri->user.s, '@', len - 4);
		if(at) {
			uri->user.len = at - uri->user.s;
			uri->host.s = at + 1;
			uri->host.len = len - 4 - uri->user.len - 1;
		} else {
			uri->user.len = len - 4;
		}
		return 0;
	}
	if(len >= 4 && strncasecmp(buf, "tel:", 4) == 0) {
		uri->type = TEL_URI_T;
		uri->user.s = buf + 4;
		uri->user.len = len - 4;
		return 0;
	}
	return -1;
}
#endif
