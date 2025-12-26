#ifndef _PEERSTATE_CB_H
#define _PEERSTATE_CB_H

#include "peerstate.h"

/**
 * @brief Callback event types
 */
typedef enum peerstate_event_type
{
	PEERSTATE_EVENT_DIALOG = 1, /* Dialog event (EARLY/CONFIRMED/ENDED) */
	PEERSTATE_EVENT_REGISTRATION =
			2 /* Registration event (REGISTER/UNREGISTER) */
} peerstate_event_type_t;

/**
 * @brief Callback context for state change events
 */
typedef struct peerstate_cb_ctx
{
	str peer;						   /* Peer identifier */
	pstate_t current_state;			   /* Current state */
	pstate_t previous_state;		   /* Previous state */
	int call_count;					   /* Active call count */
	int is_registered;				   /* Registration status */
	str uniq_id;					   /* Call-ID or RUID */
	peerstate_event_type_t event_type; /* Event source type */
} peerstate_cb_ctx_t;

/**
 * @brief Callback function signature
 * 
 * @param ctx Event context
 * @param param User-provided parameter during registration
 */
typedef void (*peerstate_cb_f)(peerstate_cb_ctx_t *ctx, void *param);

/**
 * @brief Register a callback for peerstate events
 * 
 * @param event_types Bitmask of event types (PEERSTATE_EVENT_*)
 * @param callback Callback function
 * @param param User parameter passed to callback
 * @return int 0 on success, -1 on error
 */
typedef int (*register_peerstate_cb_f)(
		int event_types, peerstate_cb_f callback, void *param);

/**
 * @brief Peerstate notify API structure
 */
typedef struct peerstate_api
{
	register_peerstate_cb_f register_callback;
} peerstate_api_t;

/**
 * @brief Bind function for other modules
 */
typedef int (*bind_peerstate_f)(peerstate_api_t *api);

/**
 * @brief Initialize callback system
 * @return int 0 on success, -1 on error
 */
int init_peerstate_callbacks(void);

/**
 * @brief Destroy callback system
 */
void destroy_peerstate_callbacks(void);

/**
 * @brief Register a callback (internal)
 * 
 * @param event_types Bitmask of event types
 * @param callback Callback function
 * @param param User parameter
 * @return int 0 on success, -1 on error
 */
int register_peerstate_callback(
		int event_types, peerstate_cb_f callback, void *param);

/**
 * @brief Trigger callbacks for an event
 * 
 * @param ctx Event context
 */
void trigger_peerstate_callbacks(peerstate_cb_ctx_t *ctx);

/**
 * @brief Bind to peerstate API
 * 
 * @param api API structure to fill
 * @return int 0 on success, -1 on error
 */
int bind_peerstate(peerstate_api_t *api);


#endif /* _PEERSTATE_CB_H */