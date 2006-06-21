#ifdef SER

#include <cds/sip_utils.h>
#include <cds/sstr.h>
#include <parser/parse_expires.h>

int get_expiration_value(struct sip_msg *m, int *value)
{
	exp_body_t *expires = NULL;
	int res = 1;

	if (parse_headers(m, HDR_EXPIRES_F, 0) == -1) {
		/* can't parse expires header */
		return -1;
	}
	if (m->expires) {
		if (parse_expires(m->expires) < 0) {
			return -1;
		}

		res = 0;
		expires = (exp_body_t *)m->expires->parsed;
		if (expires) if (expires->valid && value) *value = expires->val;
	}
	/* ERR("subscription will expire in %d secs\n", e); */
	return res;
}

int is_terminating_notify(struct sip_msg *m)
{
	int res = 0;
	struct hdr_field *h;
	static str ss = STR_STATIC_INIT("Subscription-State");
	static str terminated = STR_STATIC_INIT("terminated");

	if (parse_headers(m, HDR_EOH_F, 0) == -1) {
		ERR("can't parse NOTIFY message\n");
		return 0;
	}
	h = m->headers;
	while (h) {
		/* try to find Subscription-Status with "terminated" */
		if (str_nocase_equals(&h->name, &ss) == 0) {
			if (str_str(&h->body, &terminated)) return 1;
			else return 0;
		}
		h = h->next;
	}

	return res;
}

#endif
