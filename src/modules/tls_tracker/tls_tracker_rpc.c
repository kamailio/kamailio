#include <stdlib.h>
#include "tls_tracker_rpc.h"
#include "tls_tracker_mod.h"
#include "../../core/str.h"
#include "../../core/dprint.h"
#include "../../core/mem/pkg.h"

#include "../../core/tcp_conn.h"
#include "../../modules/tls/tls_server.h"

#define MAX_FILE_PATH_LEN 2048

static const char *tls_tracker_export_keys_doc[2] = {
		"Export session keys for TLS connections.", 0};

static int parse_file_name(str file_name_str, char *file_name)
{
	const int prefix_len = strlen("file_name=");

	if(!file_name_str.len || !file_name_str.s) {
		LM_ERR("file_name parameter is empty\n");
		return -1;
	}
	if(file_name_str.len <= prefix_len) {
		LM_ERR("file_name parameter contains incorrect value: %.*s\n",
				file_name_str.len, file_name_str.s);
		return -1;
	}
	if(!file_name) {
		LM_ERR("file_name parameter is NULL\n");
		return -1;
	}

	char _file_name[MAX_FILE_PATH_LEN] = {0};
	strncpy(_file_name, file_name_str.s, file_name_str.len);
	char *start = _strnistr(_file_name, "file_name=", prefix_len);

	if(start == NULL) {
		LM_ERR("no file name in the file_name input parameter\n");
		return -1;
	}
	start = start + prefix_len;

	if(!strchr(_file_name, '/')) {
		static const char *default_path = "/var/lib/kamailio/";
		strncpy(file_name, default_path, strlen(default_path));
	}

	strncpy(file_name + strlen(file_name), start,
			strlen(_file_name) - prefix_len);

	return 0;
}

static int parse_filter(str filter, str *local_ip, str *remote_ip,
		int *src_port, int *dst_port, str *date_start, str *date_end,
		str *time_start, str *time_end, int *connection_id)
{
	if(!filter.len || !filter.s) {
		LM_ERR("filter parameter is empty\n");
		return -1;
	}

	if(!local_ip || !remote_ip || !src_port || !dst_port || !date_start
			|| !date_end || !time_start || !time_end || !connection_id) {
		LM_ERR("error input params\n");
		return -1;
	}

	int len = filter.len + 2;
	char *_filter = pkg_mallocxz(len);
	memcpy(_filter, filter.s, filter.len);
	_filter[filter.len] = ' ';
	char *start = _strnistr(_filter, "local_ip=", len);

	if(start != NULL) {
		do {
			start = start + strlen("local_ip=");
			char *end = strchr(start, ' ');
			if(!end) {
				LM_ERR("error 'local_ip' param extraction from the filter\n");
				break;
			}
			int value_len = end - start;
			local_ip->s = pkg_mallocxz(value_len + 1);
			if(!local_ip->s) {
				PKG_MEM_ERROR;
				return -1;
			}
			strncpy(local_ip->s, start, value_len);
			local_ip->len = value_len;
			LM_DBG("local_ip=%.*s\n", local_ip->len, local_ip->s);
		} while(0);
	}

	start = _strnistr(_filter, "remote_ip=", len);
	if(start != NULL) {
		do {
			start = start + strlen("remote_ip=");
			char *end = strchr(start, ' ');
			if(!end) {
				LM_ERR("error 'remote_ip' param extraction from the filter\n");
				break;
			}
			int value_len = end - start;
			remote_ip->s = pkg_mallocxz(value_len + 1);
			if(!remote_ip->s) {
				PKG_MEM_ERROR;
				return -1;
			}
			strncpy(remote_ip->s, start, value_len);
			remote_ip->len = value_len;
			LM_DBG("remote_ip=%.*s\n", remote_ip->len, remote_ip->s);
		} while(0);
	}

	start = _strnistr(_filter, "src_port=", len);
	if(start != NULL) {
		do {
			start = start + strlen("src_port=");
			char *end = strchr(start, ' ');
			if(!end) {
				LM_ERR("error 'src_port' param extraction from the filter\n");
				break;
			}
			int value_len = end - start;
			char value_str[255] = {0};
			strncpy(value_str, start, value_len);
			int port = atoi(value_str);
			if(!port) {
				LM_ERR("error cannot convert 'src_port' param from string to "
					   "integer\n");
			} else {
				*src_port = port;
			}
			LM_DBG("src_port=%d\n", *src_port);
		} while(0);
	}

	start = _strnistr(_filter, "dst_port=", len);
	if(start != NULL) {
		do {
			start = start + strlen("dst_port=");
			char *end = strchr(start, ' ');
			if(!end) {
				LM_ERR("error 'dst_port' param extraction from the filter\n");
				break;
			}
			int value_len = end - start;
			char value_str[255] = {0};
			strncpy(value_str, start, value_len);
			int port = atoi(value_str);
			if(!port) {
				LM_ERR("error cannot convert 'dst_port' param from string to "
					   "integer\n");
			} else {
				*dst_port = port;
			}
			LM_DBG("dst_port=%d\n", *dst_port);
		} while(0);
	}

	start = _strnistr(_filter, "date_start=", len);
	if(start != NULL) {
		do {
			start = start + strlen("date_start=");
			char *end = strchr(start, ' ');
			if(!end) {
				LM_ERR("error 'date_start' param extraction from the filter\n");
				break;
			}
			int value_len = end - start;
			date_start->s = pkg_mallocxz(value_len + 1);
			if(!date_start->s) {
				PKG_MEM_ERROR;
				return -1;
			}
			strncpy(date_start->s, start, value_len);
			date_start->len = value_len;
			LM_DBG("date_start=%.*s\n", date_start->len, date_start->s);
		} while(0);
	}

	start = _strnistr(_filter, "date_end=", len);
	if(start != NULL) {
		do {
			start = start + strlen("date_end=");
			char *end = strchr(start, ' ');
			if(!end) {
				LM_ERR("error 'date_end' param extraction from the filter\n");
				break;
			}
			int value_len = end - start;
			date_end->s = pkg_mallocxz(value_len + 1);
			if(!date_end->s) {
				PKG_MEM_ERROR;
				return -1;
			}
			strncpy(date_end->s, start, value_len);
			date_end->len = value_len;
			LM_DBG("date_end=%.*s\n", date_end->len, date_end->s);
		} while(0);
	}

	start = _strnistr(_filter, "time_start=", len);
	if(start != NULL) {
		do {
			start = start + strlen("time_start=");
			char *end = strchr(start, ' ');
			if(!end) {
				LM_ERR("error 'time_start' param extraction from the filter\n");
				break;
			}
			int value_len = end - start;
			time_start->s = pkg_mallocxz(value_len + 1);
			if(!time_start->s) {
				PKG_MEM_ERROR;
				return -1;
			}
			strncpy(time_start->s, start, value_len);
			time_start->len = value_len;
			LM_DBG("time_start=%.*s\n", time_start->len, time_start->s);
		} while(0);
	}

	start = _strnistr(_filter, "time_end=", len);
	if(start != NULL) {
		do {
			start = start + strlen("time_end=");
			char *end = strchr(start, ' ');
			if(!end) {
				LM_ERR("error 'time_end' param extraction from the filter\n");
				break;
			}
			int value_len = end - start;
			time_end->s = pkg_mallocxz(value_len + 1);
			if(!time_end->s) {
				PKG_MEM_ERROR;
				return -1;
			}
			strncpy(time_end->s, start, value_len);
			time_end->len = value_len;
			LM_DBG("time_end=%.*s\n", time_end->len, time_end->s);
		} while(0);
	}

	start = _strnistr(_filter, "connection_id=", len);
	if(start != NULL) {
		do {
			start = start + strlen("connection_id=");
			char *end = strchr(start, ' ');
			if(!end) {
				LM_ERR("error 'connection_id' param extraction from the "
					   "filter\n");
				break;
			}
			int value_len = end - start;
			char value_str[255] = {0};
			strncpy(value_str, start, value_len);
			int port = atoi(value_str);
			if(!port) {
				LM_ERR("error cannot convert 'connection_id' param from string "
					   "to integer\n");
			} else {
				*connection_id = port;
			}
			LM_DBG("connection_id=%d\n", *connection_id);
		} while(0);
	}

	pkg_free(_filter);
	return 0;
}

static void tls_tracker_export_keys(rpc_t *rpc, void *c)
{
	str filter = STR_NULL;
	str file_name = STR_NULL;
	char _file_name[MAX_FILE_PATH_LEN] = {0};

	str local_ip = STR_NULL;
	str remote_ip = STR_NULL;
	int src_port = 0;
	int dst_port = 0;
	str date_start = STR_NULL;
	str date_end = STR_NULL;
	str time_start = STR_NULL;
	str time_end = STR_NULL;
	int connection_id = 0;

	if(rpc->scan(c, "SS", &filter, &file_name) < 2) {
		rpc->fault(c, 500, "Error too less parameters");
		return;
	}

	LM_DBG("filter = %.*s\n", filter.len, filter.s);
	LM_DBG("file_name = %.*s\n", file_name.len, file_name.s);

	if(!filter.len || !filter.s) {
		rpc->fault(c, 500, "Error 'filter' param is empty");
		return;
	}

	if(!file_name.len || !file_name.s) {
		rpc->fault(c, 500, "Error 'file_name' param is empty");
		return;
	}

	if(parse_file_name(file_name, _file_name) < 0) {
		rpc->fault(c, 500, "Parse 'file_name' param error");
		return;
	}

	if(parse_filter(filter, &local_ip, &remote_ip, &src_port, &dst_port,
			   &date_start, &date_end, &time_start, &time_end, &connection_id)
			< 0) {
		rpc->fault(c, 500, "Parse 'filter' param error");
		return;
	}

	char *session_key = NULL;
	get_session_key(&local_ip, &remote_ip, src_port, dst_port, &date_start,
			&date_end, &time_start, &time_end, connection_id, &session_key);

	if(session_key) {
		do {
			FILE *file_descriptor = fopen(_file_name, "wb+");
			if(!file_descriptor) {
				rpc->fault(c, 500,
						"failed to open '%s' file for session_keys uploading; "
						"error: %s",
						_file_name, strerror(errno));
				break;
			}

			fprintf(file_descriptor,
					"# SSL/TLS secrets log file, generated by Kamailio\n%s",
					session_key);
			fclose(file_descriptor);

			rpc->rpl_printf(c,
					"TLS session keys have been successfully uploaded into the "
					"file '%s'",
					_file_name);
		} while(0);
	} else {
		rpc->rpl_printf(c, "TLS sessions were not found for the filter; "
						   "nothing to upload into the file");
	}

	if(session_key) {
		pkg_free(session_key);
	}
	if(local_ip.s && local_ip.len) {
		pkg_free(local_ip.s);
	}
	if(remote_ip.s && remote_ip.len) {
		pkg_free(remote_ip.s);
	}
	if(date_start.s && date_start.len) {
		pkg_free(date_start.s);
	}
	if(date_end.s && date_end.len) {
		pkg_free(date_end.s);
	}
	if(time_start.s && time_start.len) {
		pkg_free(time_start.s);
	}
	if(time_end.s && time_end.len) {
		pkg_free(time_end.s);
	}
}

static int remove_duplicates(int *array, int size)
{
	int index1, index2, index3;
	for(index1 = 0; index1 < size; ++index1) {
		for(index2 = index1 + 1; index2 < size; ++index2) {
			if(array[index1] == array[index2]) {
				for(index3 = index2; index3 < size; ++index3) {
					array[index3] = array[index3 + 1];
				}
				--index2;
				--size;
			}
		}
	}
	return size;
}

static const char *tls_tracker_refresh_key_doc[2] = {
		"Refresh session key for TLS connections.", 0};

static void tls_tracker_refresh_key(rpc_t *rpc, void *c)
{
	str filter = STR_NULL;

	str local_ip = STR_NULL;
	str remote_ip = STR_NULL;
	int src_port = 0;
	int dst_port = 0;
	str date_start = STR_NULL;
	str time_start = STR_NULL;
	str dummy = STR_NULL;
	int connection_id = 0;

	if(rpc->scan(c, "S", &filter) < 1) {
		rpc->fault(c, 500, "Error too less parameters");
		return;
	}

	LM_DBG("filter = %.*s\n", filter.len, filter.s);

	if(!filter.len || !filter.s) {
		rpc->fault(c, 500, "Error 'filter' param is empty");
		return;
	}

	if(parse_filter(filter, &local_ip, &remote_ip, &src_port, &dst_port,
			   &date_start, &dummy, &time_start, &dummy, &connection_id)
			< 0) {
		rpc->fault(c, 500, "Parse 'filter' param error");
		return;
	}

	int *conn_id_array = NULL;
	int conn_id_array_size = 0;

	int res = get_conn_id_by_filter(&local_ip, &remote_ip, src_port, dst_port,
			&date_start, &time_start, connection_id, &conn_id_array,
			&conn_id_array_size);

	if(res < 0) {
		rpc->fault(c, 500, "error connection id retrieving from the database");
		return;
	}

	if(!conn_id_array_size) {
		rpc->rpl_printf(c, "Active TLS sessions were not found for the filter");
		return;
	}

	conn_id_array_size = remove_duplicates(conn_id_array, conn_id_array_size);

	if(!conn_id_array_size) {
		rpc->rpl_printf(c, "Active TLS sessions were not found for the filter");
		return;
	}

	int fail_count = 0;
	int success_count = 0;

	for(int index = 0; index < conn_id_array_size; ++index) {
		struct tcp_connection *s_con;

		if(conn_id_array[index] == 0) {
			continue;
		}

		if(unlikely((s_con = tcpconn_get(conn_id_array[index], 0, 0, 0, 0))
					== NULL)) {
			++fail_count;
		} else {
			if(s_con->extra_data) {
				SSL *ssl = ((struct tls_extra_data *)s_con->extra_data)->ssl;

				if(ssl) {
					int res = SSL_renegotiate(ssl);
					LM_INFO("TLS renegotiate result for connection_id=%d is "
							"%d\n",
							conn_id_array[index], res);
					if(res) {
						++success_count;
					} else {
						++fail_count;
					}
				} else {
					++fail_count;
				}
			} else {
				++fail_count;
			}
		}
	}

	rpc->rpl_printf(c,
			"TLS connections count successfully renegotiated: %d\nTLS "
			"connections count not renegotiated: %d",
			success_count, fail_count);

	pkg_free(conn_id_array);
}

rpc_export_t tls_tracker_rpc[] = {
		{"tls_tracker.export_keys", tls_tracker_export_keys,
				tls_tracker_export_keys_doc, 0},
		{"tls_tracker.refresh_key", tls_tracker_refresh_key,
				tls_tracker_refresh_key_doc, 0},
		{0, 0, 0, 0}};
