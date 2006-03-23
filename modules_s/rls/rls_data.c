#include "rls_data.h"
#include "rls_auth.h"

#include <cds/logger.h>
#include <time.h>

rls_data_t *rls = NULL;

static gen_lock_t *rls_mutex = NULL;

static int send_notify_cb(struct _subscription_data_t *s)
{
	if (s) 
		rls_generate_notify((rl_subscription_t *)s->usr_data, 1);
	return 0;
}

static int terminate_subscription_cb(struct _subscription_data_t *s)
{
	if (s) rls_remove((rl_subscription_t*)s->usr_data);
	return 0;
}

static void do_external_notifications()
{
	subscription_data_t *s = NULL;
	rl_subscription_t *rs;
	
	if (rls_manager) s = rls_manager->first;
	/* this goes through all EXTERNAL subscriptions only !!!
	 * but internal subscriptions are notified immediately, thus this is what
	 * we want */
	
	/* there can be some logic to handle at most xxx subscriptions ... */
	while (s) {
		rs = (rl_subscription_t*)(s->usr_data);
		if (rs->changed) rls_generate_notify(rs, 0);
		s = s->next;
	}
		
	rls->changed_subscriptions = 0;
}

static void rls_timer_cb(unsigned int ticks, void *param)
{
	virtual_subscription_t *vs;
	int cnt = 0;
	time_t start, stop;
	mq_message_t *msg;
	client_notify_info_t *info;

	start = time(NULL);
	rls_lock();

	/* process all messages for virtual subscriptions */
	while (!is_msg_queue_empty(&rls->notify_mq)) {
		msg = pop_message(&rls->notify_mq);
		if (!msg) continue;
		info = (client_notify_info_t *)msg->data;
		if (info) {
			vs = (virtual_subscription_t *)info->subscription->subscriber_data;
			process_rls_notification(vs, info);
			cnt++;
		}
		free_message(msg);
	}

	/* experimental optimization:
	 *   if priority (change count) is high, it is processed
	 *   otherwise is the priority incremented => changes are 
	 *   cumulated together */
	if (rls->changed_subscriptions > 5) {
		do_external_notifications();
		/* other logic may be used to generate at most some number of
		 * subscriptions -> rls->changed_subscriptions is reset from
		 * do_external_notifications() */
	}
	else {
		if (rls->changed_subscriptions > 0) 
			rls->changed_subscriptions++;
	}
	
	rls_unlock();
	stop = time(NULL);

	if (stop - start > 1) WARN("rls_timer_cb took %d secs\n", (int) (stop - start));
}


void rls_lock()
{
	/* FIXME: solve locking more efficiently - locking whole RLS in 
	 * all cases of manipulating internal structures is not good
	 * solution */
	lock_get(rls_mutex);
}

void rls_unlock()
{
	lock_release(rls_mutex);
}

int rls_init()
{
	rls = (rls_data_t*)mem_alloc(sizeof(rls_data_t));
	if (!rls) {
		LOG(L_ERR, "rls_init(): memory allocation error\n");
		return -1;
	}
/*	rls->first = NULL;
	rls->last = NULL;*/
	rls->changed_subscriptions = 0;

	if (msg_queue_init(&rls->notify_mq) != 0) {
		ERR("can't initialize message queue for RLS notifications!\n");
		return -1;
	}
	
	rls_mutex = lock_alloc();
	if (!rls_mutex) {
		LOG(L_ERR, "rls_init(): Can't initialize mutex\n");
		return -1;
	}
	lock_init(rls_mutex);

	rls_manager = sm_create(send_notify_cb, 
			terminate_subscription_cb, 
			rls_authorize_subscription,
			rls_mutex,
			rls_min_expiration,	/* min expiration time in seconds */
			rls_max_expiration, /* max expiration time in seconds */
			rls_default_expiration /* default expiration time in seconds */
			);
	
	/* register timer for handling notify messages */
	if (register_timer(rls_timer_cb, NULL, 10) < 0) {
		LOG(L_ERR, "vs_init(): can't register timer\n");
		return -1;
	}
	
	return 0;
}

int rls_destroy()
{
	DEBUG_LOG("rls_destroy() called\n");
	/* FIXME: destroy the whole rl_subscription list */
	/* sm_destroy(rls_manager); */

	if (rls_mutex) {
		lock_destroy(rls_mutex);
		lock_dealloc(rls_mutex);
	}
	if (rls) {
		mem_free(rls);
		rls = NULL;
	}
	return 0;
}

/*
static int process_rls_messages()
{
	int cnt = 0;
	client_notify_info_t *info;
	mq_message_t *msg;

	while (!is_msg_queue_empty(&rls->notify_mq)) {
		msg = pop_message(&rls->notify_mq);
		if (!msg) continue;
		info = (client_notify_info_t *)msg->data;
		if (info) {
			process_notify_info(vs, info);
			cnt++;
		}
		free_message(msg);
	}
	return cnt;
}*/

void destroy_vs_notifications(virtual_subscription_t *vs)
{
	/* removes all notifications for given VS from message queue 
	 * and discards them */
	int cnt = 0;
	int other_cnt = 0;
	mq_message_t *msg;
	msg_queue_t tmp;
	client_notify_info_t *info;

	msg_queue_init(&tmp);
	
	/* process all messages for virtual subscriptions */
	while (!is_msg_queue_empty(&rls->notify_mq)) {
		msg = pop_message(&rls->notify_mq);
		if (!msg) continue;
		info = (client_notify_info_t *)msg->data;
		if (info) {
			if (vs == info->subscription->subscriber_data) {
				cnt++;
				free_message(msg);
			}
			else {
				push_message(&tmp, msg);
				other_cnt++;
			}
		}
		else free_message(msg); /* broken message */
	}

	/* move messages back to main queue */
	while (!is_msg_queue_empty(&tmp)) {
		msg = pop_message(&tmp);
		push_message(&rls->notify_mq, msg);
	}
}

