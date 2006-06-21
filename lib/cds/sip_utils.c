#ifdef SER

#include <cds/sip_utils.h>
#include <cds/sstr.h>
#include <parser/parse_expires.h>
#include <parser/parse_subscription_state.h>

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

#endif
