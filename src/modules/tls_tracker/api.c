#include "api.h"
#include "tls_tracker_mod.h"

int add_new_connection_api(int ssl_conn_id, str *remote_ip, str *local_ip,
		int remote_port, int local_port)
{
	return add_new_connection(
			ssl_conn_id, remote_ip, local_ip, remote_port, local_port);
}

int add_session_key_api(int cid, const char *session_key)
{
	return add_session_key(cid, session_key);
}

int handle_ssl_connection_ended_api(int cid)
{
	return handle_ssl_connection_ended(cid);
}

int handle_tcp_connection_ended_api(int cid)
{
	return handle_tcp_connection_ended(cid);
}

/*
 * Function to load the tls_tracker_ops api.
 */
int bind_tls_tracker_ops(tls_tracker_ops_api_t *ttob)
{
	if(ttob == NULL) {
		LM_WARN("tls_tracker_ops_binds: Cannot load tls_tracker_ops API into a "
				"NULL pointer\n");
		return -1;
	}
	ttob->add_new_connection = add_new_connection_api;
	ttob->add_session_key = add_session_key_api;
	ttob->handle_ssl_connection_ended = handle_ssl_connection_ended_api;
	ttob->handle_tcp_connection_ended = handle_tcp_connection_ended_api;
	return 0;
}
