#ifdef SER

#include <cds/sip_utils.h>
#include <cds/sstr.h>
#include <parser/parse_expires.h>
#include <parser/parse_subscription_state.h>
#include <parser/parser_f.h>
#include <parser/parse_to.h>
#include <trim.h>


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
	subscription_state_t *ss;

	if (parse_headers(m, HDR_SUBSCRIPTION_STATE_F, 0) == -1) {
		ERR("Error while parsing headers\n");
		return 0; /* ignore */
	}
	if (!m->subscription_state) {
		ERR("Invalid NOTIFY request (without Subscription-State)\n");
		return 0; /* ignore */
	}
	if (parse_subscription_state(m->subscription_state) < 0) {
		ERR("can't parse Subscription-State\n");
		return 0; /* ignore */
	}
	ss = (subscription_state_t*)m->subscription_state->parsed;
	if (!ss) {
		ERR("invalid Subscription-State\n");
		return 0; /* ignore */
	}
	
	if (ss->value == ss_terminated) return 1;

	return 0;
}

static inline int contains_extension_support(struct hdr_field *h, 
		str *extension)
{
	/* "parses" Supported header and looks for extension */
	str s, val;
	char *c;
	
	if (!h) return 0;

	s = h->body;
	while (s.len > 0) {
		c = find_not_quoted(&s, ',');
		if (c) {
			val.s = s.s;
			val.len = c - val.s;
			trim(&val);
			if (str_case_equals(&val, extension) == 0) return 1;
			s.len = s.len - (c - s.s) - 1;
			s.s = c + 1;
		}
		else {
			trim(&s);
			if (str_case_equals(&s, extension) == 0) return 1;
			break;
		}
	}
	return 0;
}

int supports_extension(struct sip_msg *m, str *extension)
{
	/* walk through all Supported headers */
	struct hdr_field *h;
	int res;

	/* we need all Supported headers */
	res = parse_headers(m, HDR_EOH_F, 0);
	if (res == -1) {
		ERR("Error while parsing headers (%d)\n", res);
		return 0; /* what to return here ? */
	}
	
	h = m->supported;
	while (h) {
		if (h->type == HDR_SUPPORTED_T) {
			if (contains_extension_support(h, extension)) return 1;
		}
		h = h->next;
	}
	return 0;
}

int requires_extension(struct sip_msg *m, str *extension)
{
	/* walk through all Require headers */
	struct hdr_field *h;
	int res;

	/* we need all Require headers */
	res = parse_headers(m, HDR_EOH_F, 0);
	if (res == -1) {
		ERR("Error while parsing headers (%d)\n", res);
		return 0; /* what to return here ? */
	}
	
	h = m->require;
	while (h) {
		if (h->type == HDR_REQUIRE_T) {
			if (contains_extension_support(h, extension)) return 1;
		}
		h = h->next;
	}
	return 0;
}

/**
 * Verifies presence of the To-tag in message. Returns 1 if
 * the tag is present, 0 if not, -1 on error.
 */
int has_to_tag(struct sip_msg *_m)
{
	struct to_body *to = (struct to_body*)_m->to->parsed;
	if (!to) return -1;
	if (to->tag_value.len > 0) return 1;
	return 0;
}


#endif
