#ifndef TLS_TRACKER_OPS_API_H_
#define TLS_TRACKER_OPS_API_H_
#include "../../core/str.h"
#include "../../core/sr_module.h"

typedef int (*add_new_connection_t)(int, str *, str *, int, int);
typedef int (*add_session_key_t)(int, const char *);
typedef int (*handle_ssl_connection_ended_t)(int);
typedef int (*handle_tcp_connection_ended_t)(int);

/*
 * Struct with the tls_tracker_ops api.
 */
typedef struct tls_tracker_ops_binds
{
	add_new_connection_t add_new_connection;
	add_session_key_t add_session_key;
	handle_ssl_connection_ended_t handle_ssl_connection_ended;
	handle_tcp_connection_ended_t handle_tcp_connection_ended;
} tls_tracker_ops_api_t;

typedef int (*bind_tls_tracker_ops_f)(tls_tracker_ops_api_t *);

/*
 * function exported by module - it will load the other functions
 */
int bind_tls_tracker_ops(tls_tracker_ops_api_t *);

/*
 * Function to be called direclty from other modules to load
 * the tls_tracker_ops API.
 */
inline static int load_tls_tracker_ops_api(tls_tracker_ops_api_t *tob)
{
	bind_tls_tracker_ops_f bind_tls_tracker_ops_exports;
	if(!(bind_tls_tracker_ops_exports = (bind_tls_tracker_ops_f)find_export(
				 "bind_tls_tracker_ops", 0, 0))) {
		return -1;
	}
	return bind_tls_tracker_ops_exports(tob);
}

#endif /*TLS_TRACKER_OPS_API_H_*/
