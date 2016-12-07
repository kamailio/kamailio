#include "pa_mod.h"
#include "async_auth.h"
#include "../../timer.h"

int async_timer_interval = 1;
int max_auth_requests_per_tick = 50;
int async_auth_queries = 0;

static msg_queue_t *async_mq = NULL;

typedef struct {
	str uid;
	struct pdomain *d;
	char buf[1];
} async_auth_query_t;

int xcap_get_pres_rules(str *uid, cp_ruleset_t **dst, xcap_query_params_t *xcap)
{
	int res;
	str *filename = NULL;
	/* str u; */
	
	if (!is_str_empty(&pres_rules_file)) filename = &pres_rules_file;
	
	res = get_pres_rules(uid, filename, xcap, dst);
	return res;
}

static inline void query_auth_rules(async_auth_query_t *params)
{
	presence_rules_t *rules;
	presentity_t *p;

	lock_pdomain(params->d);
	if (find_presentity_uid(params->d, &params->uid, &p) == 0) {
		p->ref_cnt++;
	}
	else p = NULL;
	unlock_pdomain(params->d);
	
	if (p) {
		rules = NULL;

		/* we can use p->xcap_params because it doesn't change
		 * till the presentity dies */
		xcap_get_pres_rules(&params->uid, &rules, &p->xcap_params);

		lock_pdomain(params->d);
		/*if (rules)*/ set_auth_rules(p, rules);
		p->ref_cnt--;
		unlock_pdomain(params->d);
	}
}

static void async_timer_cb(unsigned int ticks, void *param)
{
	mq_message_t *msg;
	async_auth_query_t *params;
	int cnt = 0;
	/* TODO: dynamicaly set max_xcap_requests according to
	 * load, free mem, ... */
	
	/* process queries in message queue */
	/* the number of processed queries may be limited for one step */
	
	msg = pop_message(async_mq);
	while (msg) {
		/* INFO("processing authorization rules query\n"); */
		
		params = (async_auth_query_t*)get_message_data(msg);
		if (params) query_auth_rules(params);

		free_message(msg);
		if (++cnt > max_auth_requests_per_tick) break;
		msg = pop_message(async_mq);
	}
}

int async_auth_timer_init()
{
	if (register_timer(async_timer_cb, NULL, async_timer_interval) < 0) {
		LOG(L_ERR, "vs_init(): can't register timer\n");
		return -1;
	}
	async_mq = shm_malloc(sizeof(*async_mq));
	if (!async_mq) {
		ERR("can't allocate memory\n");
		return -1;
	}
	msg_queue_init(async_mq);
	return 0;
}

int ask_auth_rules(presentity_t *p)
{
	int len, res;
	mq_message_t *msg;
	async_auth_query_t *params;
	presence_rules_t *rules = NULL;

	/* ! call from critical section locked by pdomain mutex ! */
	
	if (pa_auth_params.type != auth_xcap) {
		return 0; /* don't load anything */
	}

	if (!async_auth_queries) {
		/* do synchronous query - it is called from locked section,
		 * thus it is not good for performance! */
		res = xcap_get_pres_rules(&p->uuid, &rules, &p->xcap_params);
		if (res == RES_OK)
			set_auth_rules(p, rules);
		return res;
	}
	
	/* Ask for authorization rules (only XCAP now). This should be done 
	 * asynchronously, at least for time consuming operations like XCAP
	 * queries */
	
	len = sizeof(async_auth_query_t) + p->uuid.len;	
	msg = create_message_ex(len);
	if (!msg) {
		ERR("can't allocate memory (%d bytes)\n", len);
		return -1;
	}
	params = (async_auth_query_t*)get_message_data(msg);
	params->uid.s = params->buf;
	params->d = p->pdomain;
	if (!is_str_empty(&p->uuid)) {
		params->uid.len = p->uuid.len;
		memcpy(params->uid.s, p->uuid.s, p->uuid.len);
	}
	else params->uid.len = 0;
	push_message(async_mq, msg);

	/* INFO("asking authorization rules\n"); */
	return 0;
}

