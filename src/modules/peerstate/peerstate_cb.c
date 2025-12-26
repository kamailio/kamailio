#include <string.h>

#include "../../core/mem/shm_mem.h"
#include "../../core/locking.h"
#include "../../core/dprint.h"
#include "peerstate_cb.h"

/**
 * @brief Callback list node
 */
struct peerstate_cb_node
{
	int event_types;		 /* Bitmask of subscribed event types */
	peerstate_cb_f callback; /* Callback function */
	void *param;			 /* User parameter */
	struct peerstate_cb_node *next;
};

static struct peerstate_cb_node *callback_list = NULL;
static gen_lock_t *callback_lock = NULL;

/**
 * @brief Initialize callback system
 */
int init_peerstate_callbacks(void)
{
	callback_lock = lock_alloc();
	if(!callback_lock) {
		LM_ERR("failed to allocate callback lock\n");
		return -1;
	}

	if(!lock_init(callback_lock)) {
		LM_ERR("failed to initialize callback lock\n");
		lock_dealloc(callback_lock);
		return -1;
	}

	callback_list = NULL;
	LM_DBG("Callback system initialized\n");
	return 0;
}

/**
 * @brief Register a callback
 */
int register_peerstate_callback(
		int event_types, peerstate_cb_f callback, void *param)
{
	struct peerstate_cb_node *node;

	if(!callback) {
		LM_ERR("callback function is NULL\n");
		return -1;
	}

	if(event_types == 0) {
		LM_ERR("event_types cannot be 0\n");
		return -1;
	}

	if(!callback_lock) {
		LM_ERR("callback system not initialized - load peerstate module "
			   "first!\n");
		return -1;
	}

	node = (struct peerstate_cb_node *)shm_malloc(
			sizeof(struct peerstate_cb_node));
	if(!node) {
		SHM_MEM_ERROR;
		return -1;
	}

	node->event_types = event_types;
	node->callback = callback;
	node->param = param;

	lock_get(callback_lock);
	node->next = callback_list;
	callback_list = node;
	lock_release(callback_lock);

	LM_INFO("Peerstate callback registered (event_types=0x%x)\n", event_types);
	return 0;
}

/**
 * @brief Trigger callbacks for an event
 */
void trigger_peerstate_callbacks(peerstate_cb_ctx_t *ctx)
{
	struct peerstate_cb_node *node;

	if(!ctx || !callback_list)
		return;

	lock_get(callback_lock);
	node = callback_list;

	while(node) {
		/* Check if callback is interested in this event type */
		if(node->event_types & ctx->event_type) {
			/* Release lock before callback to avoid deadlocks */
			lock_release(callback_lock);

			/* Call the callback */
			node->callback(ctx, node->param);

			/* Re-acquire lock for next iteration */
			lock_get(callback_lock);
			node = node->next;
		} else {
			node = node->next;
		}
	}

	lock_release(callback_lock);
}

/**
 * @brief Destroy callback system
 */
void destroy_peerstate_callbacks(void)
{
	struct peerstate_cb_node *node, *next;

	if(!callback_lock)
		return;

	lock_get(callback_lock);

	node = callback_list;
	while(node) {
		next = node->next;
		shm_free(node);
		node = next;
	}
	callback_list = NULL;

	lock_release(callback_lock);
	lock_destroy(callback_lock);
	lock_dealloc(callback_lock);
	callback_lock = NULL;

	LM_DBG("Callback system destroyed\n");
}

/**
 * @brief Bind function for other modules
 */
int bind_peerstate(peerstate_api_t *api)
{
	if(!api) {
		LM_ERR("invalid parameter\n");
		return -1;
	}

	api->register_callback = register_peerstate_callback;
	return 0;
}