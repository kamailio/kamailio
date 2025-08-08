#ifndef _TLS_TRACKER_MOD_H
#define _TLS_TRACKER_MOD_H

#include "../../core/str.h"
#include "../../core/sr_module.h"

int add_new_connection(int ssl_conn_id, str *remote_ip, str *local_ip,
		int remote_port, int local_port);
int add_session_key(int cid, const char *session_key);
int handle_ssl_connection_ended(int cid);
int handle_tcp_connection_ended(int cid);

int get_session_key(str *local_ip, str *remote_ip, int src_port, int dst_port,
		str *date_start, str *date_end, str *time_start, str *time_end,
		int connection_id, char **session_key);

int get_conn_id_by_filter(str *local_ip, str *remote_ip, int src_port,
		int dst_port, str *date_start, str *time_start, int connection_id,
		int **conn_id_array, int *conn_id_array_size);

#endif /* _TLS_TRACKER_MOD_H */
