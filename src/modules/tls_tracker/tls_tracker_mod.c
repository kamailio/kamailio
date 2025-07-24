#include "api.h"
#include "tls_tracker_mod.h"
#include "tls_tracker_rpc.h"
#include "../../lib/srdb1/db_ut.h"
#include "../../core/rpc_lookup.h"
#include "../../core/events.h"
#include "../../core/tcp_conn.h"
#include "../../modules/tls/tls_server.h"

/*
 * Module management function prototypes
 */
static int mod_init(void);
static int mod_child(int rank);
static void mod_destroy(void);

MODULE_VERSION

#ifndef MOD_NAME
#define MOD_NAME "tls_tracker"
#endif

#define EXTRACT_STRING(strng, chars)                       \
	do {                                                   \
		strng.s = (char *)chars;                           \
		strng.len = strng.s == NULL ? 0 : strlen(strng.s); \
	} while(0);


#define MAX_DATETIME_LEN 26

static str tt_db_url = str_init("mysql://kamailio:pass@127.0.0.1/kamailio");
static unsigned int default_query_timeout = 20; //milliseconds

static db_func_t tt_dbf;
static db1_con_t *tt_db_handle = NULL;

static str str_ssl_conn_id_col = str_init("ssl_conn_id");
static str str_start_timestamp_col = str_init("start_timestamp");
static str str_finish_timestamp_col = str_init("finish_timestamp");
static str str_local_ip_col = str_init("local_ip");
static str str_local_port_col = str_init("local_port");
static str str_remote_ip_col = str_init("remote_ip");
static str str_remote_port_col = str_init("remote_port");

static str str_cid_col = str_init("cid");
static str str_key_generation_timestamp_col =
		str_init("key_generation_timestamp");
static str str_key_expiration_timestamp_col =
		str_init("key_expiration_timestamp");
static str str_session_key_col = str_init("session_key");

static str tt_tcp_connection_table = str_init("tcp_connection");
static str tt_session_key_table = str_init("session_key");

static db_locking_t tt_locking_type = DB_LOCKING_NONE;

int tt_db_init_child(const str *db_url)
{
	tt_db_handle = tt_dbf.init(db_url);
	if(NULL == tt_db_handle) {
		LM_ERR("unable to connect to the database\n");
		return -1;
	}
	return 0;
}

/*
 * Exported functions
 */
static cmd_export_t cmds[] = {
		{"bind_tls_tracker_ops", (cmd_function)bind_tls_tracker_ops, 0, 0, 0,
				0},
		{0, 0, 0, 0, 0, 0}};

/*
 * Exported parameters
 */
static param_export_t params[] = {{"db_url", PARAM_STR, &tt_db_url},
		{"query_timeout", PARAM_INT, &default_query_timeout}, {0, 0, 0}};

/*
 * Module interface
 */
struct module_exports exports = {
		MOD_NAME,		 /* module name */
		DEFAULT_DLFLAGS, /* dlopen flags */
		cmds,			 /* exported functions */
		params,			 /* exported parameters */
		0,				 /* exported rpc command */
		0,				 /* exported pseudo-variables */
		0,				 /* response handling function */
		mod_init,		 /* module init function */
		mod_child,		 /* child init function */
		mod_destroy		 /* destroy function */
};

int tls_tracker_handle_tcp_closed(sr_event_param_t *evp)
{
	tcp_closed_event_info_t *tev = (tcp_closed_event_info_t *)evp->data;

	if(tev == NULL || tev->con == NULL) {
		LM_ERR("received bad TCP closed event\n");
		return -1;
	}

	tls_extra_data_t *data = (tls_extra_data_t *)tev->con->extra_data;
	if(data == NULL) {
		//TODO: to add this logging for TLS connection only
		//LM_ERR("no TLS extra_data in TCP connection found\n");
		return -1;
	}

	LM_DBG("Received TCP closed event tcp_connection_id=%d; database_id=%d\n",
			tev->con->id, data->db_session_id);

	if(handle_tcp_connection_ended(data->db_session_id) < 0) {
		LM_ERR("error handle TCP connection\n");
		return -1;
	}

	return 0;
}

static int mod_init(void)
{
	LM_INFO("Initializing tls_tracker module\n");

	if(db_bind_mod(&tt_db_url, &tt_dbf)) {
		LM_ERR("Database module not found\n");
		return -1;
	}
	LM_DBG("loaded DB api\n");

	/* register the rpc interface */
	if(rpc_register_array(tls_tracker_rpc) != 0) {
		LM_ERR("failed to register RPC commands\n");
		return -1;
	}

	if(sr_event_register_cb(SREV_TCP_CLOSED, tls_tracker_handle_tcp_closed)
			!= 0) {
		LM_ERR("problem registering 'tls_tracker_handle_tcp_closed' "
			   "callback\n");
		return -1;
	}

	return 0;
}

static void mod_destroy(void)
{
	if(tt_db_handle && tt_dbf.close) {
		tt_dbf.close(tt_db_handle);
	}
}

static int mod_child(int rank)
{
	if(rank == PROC_INIT || rank == PROC_MAIN || rank == PROC_TCP_MAIN) {
		return 0; /* do nothing for the main process */
	}

	if(tt_db_url.s && tt_db_init_child(&tt_db_url) < 0) {
		LM_ERR("could not open database connection");
		return -1;
	}

	return 0;
}

int mod_register(char *path, int *dlflags, void *p1, void *p2)
{
	return 0;
}

int add_new_connection(int ssl_conn_id, str *remote_ip, str *local_ip,
		int remote_port, int local_port)
{
	db_key_t query_cols[7];
	db_val_t query_vals[7];
	int n_query_cols = 0;
	int ret = -1;

	time_t now;
	char start_time[MAX_DATETIME_LEN] = {0};
	int start_time_len = MAX_DATETIME_LEN;
	time(&now);
	db_time2str_ex(now, start_time, &start_time_len, 0);
	str str_start_time = {start_time, start_time_len};

	query_cols[n_query_cols] = &str_ssl_conn_id_col;
	query_vals[n_query_cols].type = DB1_INT;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].val.int_val = ssl_conn_id;
	++n_query_cols;

	query_cols[n_query_cols] = &str_start_timestamp_col;
	query_vals[n_query_cols].type = DB1_STR;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].val.str_val = str_start_time;
	++n_query_cols;

	query_cols[n_query_cols] = &str_remote_ip_col;
	query_vals[n_query_cols].type = DB1_STR;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].val.str_val = *remote_ip;
	++n_query_cols;

	query_cols[n_query_cols] = &str_remote_port_col;
	query_vals[n_query_cols].type = DB1_INT;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].val.int_val = remote_port;
	++n_query_cols;

	query_cols[n_query_cols] = &str_local_ip_col;
	query_vals[n_query_cols].type = DB1_STR;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].val.str_val = *local_ip;
	++n_query_cols;

	query_cols[n_query_cols] = &str_local_port_col;
	query_vals[n_query_cols].type = DB1_INT;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].val.int_val = local_port;
	++n_query_cols;

	if(tt_dbf.use_table(tt_db_handle, &tt_tcp_connection_table) < 0) {
		LM_ERR("unsuccessful use_table for '%.*s' table\n",
				tt_tcp_connection_table.len, tt_tcp_connection_table.s);
		return -1;
	}

	if(tt_dbf.start_transaction) {
		if(tt_dbf.start_transaction(tt_db_handle, tt_locking_type) < 0) {
			LM_ERR("Error in start_transaction\n");
			return -1;
		}
	}

	if(tt_dbf.insert(tt_db_handle, query_cols, query_vals, n_query_cols) < 0) {
		LM_DBG("inserting record in database\n");
		if(tt_dbf.abort_transaction) {
			if(tt_dbf.abort_transaction(tt_db_handle) < 0) {
				LM_ERR("Error in abort_transaction\n");
			}
		}
		return -1;
	}

	if(tt_dbf.last_inserted_id) {
		ret = tt_dbf.last_inserted_id(tt_db_handle);
	} else {
		LM_ERR("DB last_inserted_id error\n");
	}

	if(tt_dbf.end_transaction) {
		if(tt_dbf.end_transaction(tt_db_handle) < 0) {
			LM_ERR("Error in end_transaction\n");
			return -1;
		}
	}

	return ret;
}

int add_session_key(int cid, const char *session_key)
{
	if(!session_key) {
		LM_ERR("Error session_key is empty\n");
		return -1;
	}

	db_key_t query_cols[1], result_cols[1];
	db_val_t query_vals[1], *values;
	db_row_t *rows;
	db1_res_t *result = NULL;

	int n_query_cols = 0, n_result_cols = 0;
	int session_key_col;

	query_cols[n_query_cols] = &str_cid_col;
	query_vals[n_query_cols].type = DB1_INT;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].val.int_val = cid;
	++n_query_cols;

	result_cols[session_key_col = n_result_cols++] = &str_session_key_col;

	if(tt_dbf.use_table(tt_db_handle, &tt_session_key_table) < 0) {
		LM_ERR("use table failed\n");
		return -1;
	}

	if(tt_dbf.query(tt_db_handle, query_cols, 0, query_vals, result_cols,
			   n_query_cols, n_result_cols, 0, &result)
			< 0) {
		LM_ERR("sql query error\n");
		return -1;
	}

	if(result == NULL) {
		LM_ERR("bad result\n");
		return -1;
	}

	if(RES_ROW_N(result) < 0) {
		LM_ERR("bad result\n");
		return -1;
	} else if(RES_ROW_N(result) == 0) {
		db_key_t insert_query_cols[4];
		db_val_t insert_query_vals[4];
		int n_insert_query_cols = 0;

		time_t now;
		char start_time[MAX_DATETIME_LEN] = {0};
		int start_time_len = MAX_DATETIME_LEN;
		time(&now);
		db_time2str_ex(now, start_time, &start_time_len, 0);
		str str_start_time = {start_time, start_time_len};

		str str_session_key = {(char *)session_key, strlen(session_key)};

		insert_query_cols[n_insert_query_cols] = &str_cid_col;
		insert_query_vals[n_insert_query_cols].type = DB1_INT;
		insert_query_vals[n_insert_query_cols].nul = 0;
		insert_query_vals[n_insert_query_cols].val.int_val = cid;
		++n_insert_query_cols;

		insert_query_cols[n_insert_query_cols] =
				&str_key_generation_timestamp_col;
		insert_query_vals[n_insert_query_cols].type = DB1_STR;
		insert_query_vals[n_insert_query_cols].nul = 0;
		insert_query_vals[n_insert_query_cols].val.str_val = str_start_time;
		++n_insert_query_cols;

		insert_query_cols[n_insert_query_cols] = &str_session_key_col;
		insert_query_vals[n_insert_query_cols].type = DB1_STR;
		insert_query_vals[n_insert_query_cols].nul = 0;
		insert_query_vals[n_insert_query_cols].val.str_val = str_session_key;
		++n_insert_query_cols;

		if(tt_dbf.use_table(tt_db_handle, &tt_session_key_table) < 0) {
			LM_ERR("unsuccessful use_table for '%.*s' table\n",
					tt_session_key_table.len, tt_session_key_table.s);
			return -1;
		}

		if(tt_dbf.start_transaction) {
			if(tt_dbf.start_transaction(tt_db_handle, tt_locking_type) < 0) {
				LM_ERR("Error in start_transaction\n");
				return -1;
			}
		}

		LM_DBG("Insert into the '%.*s' table with the following id: cid=%d\n",
				tt_session_key_table.len, tt_session_key_table.s, cid);
		if(tt_dbf.insert(tt_db_handle, insert_query_cols, insert_query_vals,
				   n_insert_query_cols)
				< 0) {
			LM_ERR("Error inserting record in database\n");
			if(tt_dbf.abort_transaction) {
				if(tt_dbf.abort_transaction(tt_db_handle) < 0) {
					LM_ERR("Error in abort_transaction\n");
				}
			}
			return -1;
		}

		if(tt_dbf.end_transaction) {
			if(tt_dbf.end_transaction(tt_db_handle) < 0) {
				LM_ERR("Error in end_transaction\n");
				return -1;
			}
		}
	} else {
		str session_key_line = STR_NULL;

		rows = RES_ROWS(result);

		values = ROW_VALUES(&rows[0]);
		EXTRACT_STRING(session_key_line, VAL_STRING(&values[session_key_col]));

		int updated_session_key_len =
				strlen(session_key) + session_key_line.len + 2;
		char *_session_key_buff = (char *)pkg_mallocxz(updated_session_key_len);
		if(!_session_key_buff) {
			PKG_MEM_ERROR;
			tt_dbf.free_result(tt_db_handle, result);
			return -1;
		}
		int key_len =
				snprintf(_session_key_buff, updated_session_key_len, "%.*s\n%s",
						session_key_line.len, session_key_line.s, session_key);
		tt_dbf.free_result(tt_db_handle, result);

		str value_to_update = {_session_key_buff, key_len};

		db_key_t query_cols_query[1];
		db_val_t query_vals_query[1];
		int n_query_cols_query = 0;

		db_key_t query_cols_update[1];
		db_val_t query_vals_update[1];
		int n_query_cols_update = 0;

		query_cols_query[n_query_cols_query] = &str_cid_col;
		query_vals_query[n_query_cols_query].type = DB1_INT;
		query_vals_query[n_query_cols_query].nul = 0;
		query_vals_query[n_query_cols_query].val.int_val = cid;
		++n_query_cols_query;

		query_cols_update[n_query_cols_update] = &str_session_key_col;
		query_vals_update[n_query_cols_update].type = DB1_STR;
		query_vals_update[n_query_cols_update].nul = 0;
		query_vals_update[n_query_cols_update].val.str_val = value_to_update;
		++n_query_cols_update;

		if(tt_dbf.use_table(tt_db_handle, &tt_session_key_table) < 0) {
			LM_ERR("unsuccessful use_table for '%.*s' table\n",
					tt_session_key_table.len, tt_session_key_table.s);
			return -1;
		}

		if(tt_dbf.start_transaction) {
			if(tt_dbf.start_transaction(tt_db_handle, tt_locking_type) < 0) {
				LM_ERR("Error in start_transaction\n");
				return -1;
			}
		}

		LM_DBG("Update '%.*s' table with the following id: cid=%d\n",
				tt_session_key_table.len, tt_session_key_table.s, cid);
		int res = tt_dbf.update(tt_db_handle, query_cols_query, 0,
				query_vals_query, query_cols_update, query_vals_update,
				n_query_cols_query, n_query_cols_update);
		pkg_free(_session_key_buff);
		if(res < 0) {
			LM_ERR("Error updating record in database\n");
			if(tt_dbf.abort_transaction) {
				if(tt_dbf.abort_transaction(tt_db_handle) < 0) {
					LM_ERR("Error in abort_transaction\n");
				}
			}
			return -1;
		}

		if(tt_dbf.end_transaction) {
			if(tt_dbf.end_transaction(tt_db_handle) < 0) {
				LM_ERR("Error in end_transaction\n");
				return -1;
			}
		}
	}

	return 0;
}

int get_session_key(str *local_ip, str *remote_ip, int src_port, int dst_port,
		str *date_start, str *date_end, str *time_start, str *time_end,
		int connection_id, char **session_key)
{
	db_key_t query_cols[7], result_cols[1];
	db_val_t query_vals[7], *values;
	db_op_t op[7];
	db_row_t *rows;
	db1_res_t *result = NULL;

	int tc_query_cols = 0;
	int tc_result_cols = 0;
	int cid_col;

	if(connection_id > 0) {
		query_cols[tc_query_cols] = &str_ssl_conn_id_col;
		op[tc_query_cols] = OP_EQ;
		query_vals[tc_query_cols].type = DB1_INT;
		query_vals[tc_query_cols].nul = 0;
		query_vals[tc_query_cols].val.int_val = connection_id;
		++tc_query_cols;
	}

	if(src_port > 0) {
		query_cols[tc_query_cols] = &str_local_port_col;
		op[tc_query_cols] = OP_EQ;
		query_vals[tc_query_cols].type = DB1_INT;
		query_vals[tc_query_cols].nul = 0;
		query_vals[tc_query_cols].val.int_val = src_port;
		++tc_query_cols;
	}

	if(dst_port > 0) {
		query_cols[tc_query_cols] = &str_remote_port_col;
		op[tc_query_cols] = OP_EQ;
		query_vals[tc_query_cols].type = DB1_INT;
		query_vals[tc_query_cols].nul = 0;
		query_vals[tc_query_cols].val.int_val = dst_port;
		++tc_query_cols;
	}

	if(local_ip->len && local_ip->s) {
		query_cols[tc_query_cols] = &str_local_ip_col;
		op[tc_query_cols] = OP_EQ;
		query_vals[tc_query_cols].type = DB1_STR;
		query_vals[tc_query_cols].nul = 0;
		query_vals[tc_query_cols].val.str_val = *local_ip;
		++tc_query_cols;
	}

	if(remote_ip->len && remote_ip->s) {
		query_cols[tc_query_cols] = &str_remote_ip_col;
		op[tc_query_cols] = OP_EQ;
		query_vals[tc_query_cols].type = DB1_STR;
		query_vals[tc_query_cols].nul = 0;
		query_vals[tc_query_cols].val.str_val = *remote_ip;
		++tc_query_cols;
	}

	char start_datetime[MAX_DATETIME_LEN] = {0};
	if(date_start->s || time_start->s) {
		if(date_start->s && !time_start->s) {
			strncpy(start_datetime, date_start->s, date_start->len);
			start_datetime[strlen(start_datetime)] = ' ';
			strcpy(&start_datetime[strlen(start_datetime)], "00:00:00");
		} else if(!date_start->s && time_start->s) {
			size_t len;
			time_t now = time(0);
			struct tm tm_time;

			if(gmtime_r(&now, &tm_time) == NULL) {
				LM_ERR("gmtime failed\n");
				return -1;
			}

			len = strftime(
					start_datetime, MAX_DATETIME_LEN, "%Y-%m-%d ", &tm_time);
			if(!len) {
				LM_ERR("unexpected time length\n");
				return -1;
			}

			strncpy(&start_datetime[strlen(start_datetime)], time_start->s,
					time_start->len);
		} else {
			strncpy(start_datetime, date_start->s, date_start->len);
			start_datetime[strlen(start_datetime)] = ' ';
			strncpy(&start_datetime[strlen(start_datetime)], time_start->s,
					time_start->len);
		}

		time_t tm;
		if(db_str2time(start_datetime, &tm) < 0) {
			LM_ERR("error conversion string to datetime: '%s'\n",
					start_datetime);
			return -1;
		}

		query_cols[tc_query_cols] = &str_start_timestamp_col;
		op[tc_query_cols] = OP_GEQ;
		query_vals[tc_query_cols].type = DB1_DATETIME;
		query_vals[tc_query_cols].nul = 0;
		query_vals[tc_query_cols].val.time_val = tm;
		++tc_query_cols;
	}


	char end_datetime[MAX_DATETIME_LEN] = {0};
	if(date_end->s || time_end->s) {
		if(date_end->s && !time_end->s) {
			strncpy(end_datetime, date_end->s, date_end->len);
			end_datetime[strlen(end_datetime)] = ' ';
			strcpy(&end_datetime[strlen(end_datetime)], "23:59:59");
		} else if(!date_end->s && time_end->s) {
			size_t len;
			time_t now = time(0);
			struct tm tm_time;

			if(gmtime_r(&now, &tm_time) == NULL) {
				LM_ERR("gmtime failed\n");
				return -1;
			}

			len = strftime(
					end_datetime, MAX_DATETIME_LEN, "%Y-%m-%d ", &tm_time);
			if(!len) {
				LM_ERR("unexpected time length\n");
				return -1;
			}

			strncpy(&end_datetime[strlen(end_datetime)], time_end->s,
					time_end->len);
		} else {
			strncpy(end_datetime, date_end->s, date_end->len);
			end_datetime[strlen(end_datetime)] = ' ';
			strncpy(&end_datetime[strlen(end_datetime)], time_end->s,
					time_end->len);
		}

		time_t tm;
		if(db_str2time(end_datetime, &tm) < 0) {
			LM_ERR("error conversion string to datetime: '%s'\n", end_datetime);
			return -1;
		}

		query_cols[tc_query_cols] = &str_start_timestamp_col;
		op[tc_query_cols] = OP_LEQ;
		query_vals[tc_query_cols].type = DB1_DATETIME;
		query_vals[tc_query_cols].nul = 0;
		query_vals[tc_query_cols].val.time_val = tm;
		++tc_query_cols;
	}

	result_cols[cid_col = tc_result_cols++] = &str_cid_col;

	if(tt_dbf.use_table(tt_db_handle, &tt_tcp_connection_table) < 0) {
		LM_ERR("use table failed\n");
		return -1;
	}

	if(tt_dbf.query(tt_db_handle, query_cols, op, query_vals, result_cols,
			   tc_query_cols, tc_result_cols, 0, &result)
			< 0) {
		LM_ERR("sql query error\n");
		return -1;
	}

	if(result == NULL) {
		LM_ERR("bad result\n");
		return -1;
	}

	int num_rows = RES_ROW_N(result);
	if(num_rows < 0) {
		LM_ERR("bad result\n");
		return -1;
	} else if(num_rows == 0) {
		*session_key = NULL;
		tt_dbf.free_result(tt_db_handle, result);
		LM_WARN("the records in '%.*s' table  are not found for the requested "
				"filter\n",
				tt_tcp_connection_table.len, tt_tcp_connection_table.s);
	} else {
		LM_DBG("query result rows number=%d\n", num_rows);

		int *cid_list = pkg_mallocxz(num_rows * sizeof(int));
		if(!cid_list) {
			PKG_MEM_ERROR;
			tt_dbf.free_result(tt_db_handle, result);
			return -1;
		}
		rows = RES_ROWS(result);
		int loop = 0;
		for(; loop < num_rows; ++loop) {
			values = ROW_VALUES(&rows[loop]);
			cid_list[loop] = VAL_INT(values);
		}
		tt_dbf.free_result(tt_db_handle, result);
		result = NULL;

		if(tt_dbf.use_table(tt_db_handle, &tt_session_key_table) < 0) {
			LM_ERR("use table failed\n");
			return -1;
		}

		size_t session_key_len = 0;
		char *session_key_result = NULL;
		for(loop = 0; loop < num_rows; ++loop) {
			db_key_t sk_query_cols[1];
			db_key_t sk_result_cols[1];
			db_val_t sk_query_vals[1];

			db_row_t *sk_rows = NULL;

			sk_query_cols[0] = &str_cid_col;
			sk_query_vals[0].type = DB1_INT;
			sk_query_vals[0].nul = 0;
			sk_query_vals[0].val.int_val = cid_list[loop];

			sk_result_cols[0] = &str_session_key_col;

			if(tt_dbf.query(tt_db_handle, sk_query_cols, NULL, sk_query_vals,
					   sk_result_cols, 1, 1, 0, &result)
					< 0) {
				LM_ERR("sql query error\n");
				return -1;
			}

			if(result == NULL) {
				LM_ERR("bad result\n");
				return -1;
			}

			int sk_num_rows = RES_ROW_N(result);
			if(sk_num_rows < 0) {
				LM_ERR("bad result\n");
				return -1;
			} else if(sk_num_rows == 0) {
				LM_WARN("the records in '%.*s' table  are not found for the "
						"requested filter\n",
						tt_session_key_table.len, tt_session_key_table.s);
			} else {
				sk_rows = RES_ROWS(result);
				values = ROW_VALUES(&sk_rows[0]);

				char *item = VAL_STR(values).s;
				if(item) {
					session_key_result = (char *)pkg_realloc(session_key_result,
							session_key_len + strlen(item) + 2);
					if(!session_key_result) {
						PKG_MEM_ERROR;
						return -1;
					}
					strncpy(session_key_result + session_key_len, item,
							strlen(item));
					session_key_len += strlen(item);

					session_key_result[session_key_len++] = '\n';
					session_key_result[session_key_len] = '\0';
				}
			}
			tt_dbf.free_result(tt_db_handle, result);
			result = NULL;
			usleep(default_query_timeout * 1000ll);
		}

		pkg_free(cid_list);
		if(session_key_result) {
			*session_key = session_key_result;
		}
	}

	return 0;
}

int handle_ssl_connection_ended(int cid)
{
	db_key_t query_cols[1];
	db_key_t result_cols[2];
	db_val_t query_vals[1];

	db1_res_t *result = NULL;
	int key_generation_col, key_expiration_col;
	int query_cols_count = 0;
	int result_cols_count = 0;

	query_cols[query_cols_count] = &str_cid_col;
	query_vals[query_cols_count].type = DB1_INT;
	query_vals[query_cols_count].nul = 0;
	query_vals[query_cols_count].val.int_val = cid;
	++query_cols_count;

	result_cols[key_generation_col = result_cols_count++] =
			&str_key_generation_timestamp_col;
	result_cols[key_expiration_col = result_cols_count++] =
			&str_key_expiration_timestamp_col;

	if(tt_dbf.query(tt_db_handle, query_cols, NULL, query_vals, result_cols,
			   query_cols_count, result_cols_count, 0, &result)
			< 0) {
		LM_ERR("sql query error\n");
		return -1;
	}

	if(result == NULL) {
		LM_ERR("bad result\n");
		return -1;
	}

	int num_rows = RES_ROW_N(result);
	if(num_rows < 0) {
		LM_ERR("bad result\n");
		return -1;
	} else if(num_rows == 0) {
		LM_WARN("the record in '%.*s' table with cid=%d is not found\n",
				tt_session_key_table.len, tt_session_key_table.s, cid);
	} else {
		int row_num = RES_ROW_N(result);

		db_row_t *row = NULL;
		db_val_t *row_vals = NULL;
		time_t generation_time = 0;
		int found = 0;

		for(int index = 0; index < row_num; ++index) {
			row = &result->rows[index];
			row_vals = ROW_VALUES(row);

			if(!row_vals[key_generation_col].nul) {
				generation_time = row_vals[key_generation_col].val.time_val;
			}
			if(row_vals[key_expiration_col].nul) {
				found = 1;
				break;
			}
		}

		if(found) {
			db_key_t query_cols_query[2];
			db_val_t query_vals_query[2];
			int n_query_cols_query = 0;

			db_key_t query_cols_update[1];
			db_val_t query_vals_update[1];
			int n_query_cols_update = 0;

			query_cols_query[n_query_cols_query] = &str_cid_col;
			query_vals_query[n_query_cols_query].type = DB1_INT;
			query_vals_query[n_query_cols_query].nul = 0;
			query_vals_query[n_query_cols_query].val.int_val = cid;
			++n_query_cols_query;

			query_cols_query[n_query_cols_query] =
					&str_key_generation_timestamp_col;
			query_vals_query[n_query_cols_query].type = DB1_DATETIME;
			query_vals_query[n_query_cols_query].nul = 0;
			query_vals_query[n_query_cols_query].val.time_val = generation_time;
			++n_query_cols_query;

			query_cols_update[n_query_cols_update] =
					&str_key_expiration_timestamp_col;
			query_vals_update[n_query_cols_update].type = DB1_DATETIME;
			query_vals_update[n_query_cols_update].nul = 0;
			query_vals_update[n_query_cols_update].val.time_val = time(NULL);
			++n_query_cols_update;

			if(tt_dbf.use_table(tt_db_handle, &tt_session_key_table) < 0) {
				LM_ERR("unsuccessful use_table for '%.*s' table\n",
						tt_session_key_table.len, tt_session_key_table.s);
				return -1;
			}

			if(tt_dbf.start_transaction) {
				if(tt_dbf.start_transaction(tt_db_handle, tt_locking_type)
						< 0) {
					LM_ERR("Error in start_transaction\n");
					return -1;
				}
			}

			LM_DBG("update '%.*s' table with the following id: cid=%d\n",
					tt_session_key_table.len, tt_session_key_table.s, cid);
			int res = tt_dbf.update(tt_db_handle, query_cols_query, 0,
					query_vals_query, query_cols_update, query_vals_update,
					n_query_cols_query, n_query_cols_update);

			if(res < 0) {
				LM_ERR("Error updating record in database\n");
				if(tt_dbf.abort_transaction) {
					if(tt_dbf.abort_transaction(tt_db_handle) < 0) {
						LM_ERR("Error in abort_transaction\n");
					}
				}
				return -1;
			}

			if(tt_dbf.end_transaction) {
				if(tt_dbf.end_transaction(tt_db_handle) < 0) {
					LM_ERR("Error in end_transaction\n");
					return -1;
				}
			}

			LM_DBG("session key expiration time has been updated for cid=%d\n",
					cid);
		} else {
			LM_WARN("the record in '%.*s' table with empty expiration time is "
					"not found\n",
					tt_session_key_table.len, tt_session_key_table.s);
		}
	}
	tt_dbf.free_result(tt_db_handle, result);
	return 0;
}

static int set_session_key_expiration_timestamp(int cid, time_t tm)
{
	db_key_t query_cols_query[1];
	db_val_t query_vals_query[1];
	int n_query_cols_query = 0;

	db_key_t query_cols_update[1];
	db_val_t query_vals_update[1];
	int n_query_cols_update = 0;

	query_cols_query[n_query_cols_query] = &str_cid_col;
	query_vals_query[n_query_cols_query].type = DB1_INT;
	query_vals_query[n_query_cols_query].nul = 0;
	query_vals_query[n_query_cols_query].val.int_val = cid;
	++n_query_cols_query;

	query_cols_update[n_query_cols_update] = &str_key_expiration_timestamp_col;
	query_vals_update[n_query_cols_update].type = DB1_DATETIME;
	query_vals_update[n_query_cols_update].nul = 0;
	query_vals_update[n_query_cols_update].val.time_val = tm;
	++n_query_cols_update;

	if(tt_dbf.use_table(tt_db_handle, &tt_session_key_table) < 0) {
		LM_ERR("unsuccessful use_table for '%.*s' table\n",
				tt_session_key_table.len, tt_session_key_table.s);
		return -1;
	}

	if(tt_dbf.start_transaction) {
		if(tt_dbf.start_transaction(tt_db_handle, tt_locking_type) < 0) {
			LM_ERR("Error in start_transaction\n");
			return -1;
		}
	}

	LM_DBG("Update '%.*s' table with the following id: cid=%d\n",
			tt_session_key_table.len, tt_session_key_table.s, cid);
	int res = tt_dbf.update(tt_db_handle, query_cols_query, 0, query_vals_query,
			query_cols_update, query_vals_update, n_query_cols_query,
			n_query_cols_update);

	if(res < 0) {
		LM_ERR("Error updating record in database\n");
		if(tt_dbf.abort_transaction) {
			if(tt_dbf.abort_transaction(tt_db_handle) < 0) {
				LM_ERR("Error in abort_transaction\n");
			}
		}
		return -1;
	}

	if(tt_dbf.end_transaction) {
		if(tt_dbf.end_transaction(tt_db_handle) < 0) {
			LM_ERR("Error in end_transaction\n");
			return -1;
		}
	}

	return 0;
}

static int set_tcp_conn_finish_timestamp(int cid, time_t tm)
{
	db_key_t query_cols_query[1];
	db_val_t query_vals_query[1];
	int n_query_cols_query = 0;

	db_key_t query_cols_update[1];
	db_val_t query_vals_update[1];
	int n_query_cols_update = 0;

	query_cols_query[n_query_cols_query] = &str_cid_col;
	query_vals_query[n_query_cols_query].type = DB1_INT;
	query_vals_query[n_query_cols_query].nul = 0;
	query_vals_query[n_query_cols_query].val.int_val = cid;
	++n_query_cols_query;

	query_cols_update[n_query_cols_update] = &str_finish_timestamp_col;
	query_vals_update[n_query_cols_update].type = DB1_DATETIME;
	query_vals_update[n_query_cols_update].nul = 0;
	query_vals_update[n_query_cols_update].val.time_val = tm;
	++n_query_cols_update;

	if(tt_dbf.use_table(tt_db_handle, &tt_tcp_connection_table) < 0) {
		LM_ERR("unsuccessful use_table for '%.*s' table\n",
				tt_tcp_connection_table.len, tt_tcp_connection_table.s);
		return -1;
	}

	if(tt_dbf.start_transaction) {
		if(tt_dbf.start_transaction(tt_db_handle, tt_locking_type) < 0) {
			LM_ERR("Error in start_transaction\n");
			return -1;
		}
	}

	LM_DBG("Update session_key table with the following id: cid=%d\n", cid);
	int res = tt_dbf.update(tt_db_handle, query_cols_query, 0, query_vals_query,
			query_cols_update, query_vals_update, n_query_cols_query,
			n_query_cols_update);

	if(res < 0) {
		LM_ERR("Error updating record in database\n");
		if(tt_dbf.abort_transaction) {
			if(tt_dbf.abort_transaction(tt_db_handle) < 0) {
				LM_ERR("Error in abort_transaction\n");
			}
		}
		return -1;
	}

	if(tt_dbf.end_transaction) {
		if(tt_dbf.end_transaction(tt_db_handle) < 0) {
			LM_ERR("Error in end_transaction\n");
			return -1;
		}
	}

	return 0;
}

int handle_tcp_connection_ended(int cid)
{

	if(!tt_db_handle) {
		if(tt_db_url.s && tt_db_init_child(&tt_db_url) < 0) {
			LM_ERR("could not open database connection\n");
			return -1;
		}
		LM_DBG("initialized database connection\n");
	} else {
		LM_DBG("database connection is already initialized\n");
	}

	time_t timestamp = time(NULL);
	if(set_session_key_expiration_timestamp(cid, timestamp) < 0) {
		LM_ERR("Error update 'session_key' table\n");
		return -1;
	}

	if(set_tcp_conn_finish_timestamp(cid, timestamp) < 0) {
		LM_ERR("Error update 'tcp_connection' table\n");
		return -1;
	}

	return 0;
}

int get_conn_id_by_filter(str *local_ip, str *remote_ip, int src_port,
		int dst_port, str *date_start, str *time_start, int connection_id,
		int **conn_id_array, int *conn_id_array_size)
{
	if(!conn_id_array || !conn_id_array_size) {
		LM_ERR("error: output parameters are null\n");
		return -1;
	}

	db_key_t query_cols[6], result_cols[2];
	db_val_t query_vals[6], *values;
	db_op_t op[6];
	db_row_t *rows;
	db1_res_t *result = NULL;

	int tc_query_cols = 0;
	int tc_result_cols = 0;
	int ssl_conn_id_col;
	int finish_timestamp_col;

	if(connection_id > 0) {
		query_cols[tc_query_cols] = &str_ssl_conn_id_col;
		op[tc_query_cols] = OP_EQ;
		query_vals[tc_query_cols].type = DB1_INT;
		query_vals[tc_query_cols].nul = 0;
		query_vals[tc_query_cols].val.int_val = connection_id;
		++tc_query_cols;
	}

	if(src_port > 0) {
		query_cols[tc_query_cols] = &str_local_port_col;
		op[tc_query_cols] = OP_EQ;
		query_vals[tc_query_cols].type = DB1_INT;
		query_vals[tc_query_cols].nul = 0;
		query_vals[tc_query_cols].val.int_val = src_port;
		++tc_query_cols;
	}

	if(dst_port > 0) {
		query_cols[tc_query_cols] = &str_remote_port_col;
		op[tc_query_cols] = OP_EQ;
		query_vals[tc_query_cols].type = DB1_INT;
		query_vals[tc_query_cols].nul = 0;
		query_vals[tc_query_cols].val.int_val = dst_port;
		++tc_query_cols;
	}

	if(local_ip->len && local_ip->s) {
		query_cols[tc_query_cols] = &str_local_ip_col;
		op[tc_query_cols] = OP_EQ;
		query_vals[tc_query_cols].type = DB1_STR;
		query_vals[tc_query_cols].nul = 0;
		query_vals[tc_query_cols].val.str_val = *local_ip;
		++tc_query_cols;
	}

	if(remote_ip->len && remote_ip->s) {
		query_cols[tc_query_cols] = &str_remote_ip_col;
		op[tc_query_cols] = OP_EQ;
		query_vals[tc_query_cols].type = DB1_STR;
		query_vals[tc_query_cols].nul = 0;
		query_vals[tc_query_cols].val.str_val = *remote_ip;
		++tc_query_cols;
	}

	char start_datetime[MAX_DATETIME_LEN] = {0};
	if(date_start->s || time_start->s) {
		if(date_start->s && !time_start->s) {
			strncpy(start_datetime, date_start->s, date_start->len);
			start_datetime[strlen(start_datetime)] = ' ';
			strcpy(&start_datetime[strlen(start_datetime)], "00:00:00");
		} else if(!date_start->s && time_start->s) {
			size_t len;
			time_t now = time(0);
			struct tm tm_time;

			if(gmtime_r(&now, &tm_time) == NULL) {
				LM_ERR("gmtime failed\n");
				return -1;
			}

			len = strftime(
					start_datetime, MAX_DATETIME_LEN, "%Y-%m-%d ", &tm_time);
			if(!len) {
				LM_ERR("unexpected time length\n");
				return -1;
			}

			strncpy(&start_datetime[strlen(start_datetime)], time_start->s,
					time_start->len);
		} else {
			strncpy(start_datetime, date_start->s, date_start->len);
			start_datetime[strlen(start_datetime)] = ' ';
			strncpy(&start_datetime[strlen(start_datetime)], time_start->s,
					time_start->len);
		}

		time_t tm;
		if(db_str2time(start_datetime, &tm) < 0) {
			LM_ERR("error conversion string to datetime: '%s'\n",
					start_datetime);
			return -1;
		}

		query_cols[tc_query_cols] = &str_start_timestamp_col;
		op[tc_query_cols] = OP_GEQ;
		query_vals[tc_query_cols].type = DB1_DATETIME;
		query_vals[tc_query_cols].nul = 0;
		query_vals[tc_query_cols].val.time_val = tm;
		++tc_query_cols;
	}

	result_cols[ssl_conn_id_col = tc_result_cols++] = &str_ssl_conn_id_col;
	result_cols[finish_timestamp_col = tc_result_cols++] =
			&str_finish_timestamp_col;

	if(tt_dbf.use_table(tt_db_handle, &tt_tcp_connection_table) < 0) {
		LM_ERR("use table failed\n");
		return -1;
	}

	if(tt_dbf.query(tt_db_handle, query_cols, op, query_vals, result_cols,
			   tc_query_cols, tc_result_cols, 0, &result)
			< 0) {
		LM_ERR("sql query error\n");
		return -1;
	}

	if(result == NULL) {
		LM_ERR("bad result\n");
		return -1;
	}

	int num_rows = RES_ROW_N(result);
	if(num_rows < 0) {
		LM_ERR("bad result\n");
		return -1;
	} else if(num_rows == 0) {
		LM_WARN("the records in '%.*s' table  are not found for the requested "
				"filter\n",
				tt_tcp_connection_table.len, tt_tcp_connection_table.s);
		*conn_id_array = NULL;
		*conn_id_array_size = 0;
	} else {
		LM_DBG("query result rows number=%d\n", num_rows);

		rows = RES_ROWS(result);

		int connection_count = 0;
		for(int index = 0; index < num_rows; ++index) {
			values = ROW_VALUES(&rows[index]);
			if(!VAL_NULL(&values[finish_timestamp_col])) {
				continue;
			}
			++connection_count;
		}

		if(connection_count) {
			*conn_id_array = pkg_mallocxz(connection_count * sizeof(int));
			if(!(*conn_id_array)) {
				PKG_MEM_ERROR;
				tt_dbf.free_result(tt_db_handle, result);
				return -1;
			}

			int count = 0;
			for(int index = 0; index < num_rows; ++index) {
				values = ROW_VALUES(&rows[index]);
				if(!VAL_NULL(&values[finish_timestamp_col])) {
					continue;
				}
				(*conn_id_array)[count++] = VAL_INT(values);
			}
		}
		*conn_id_array_size = connection_count;
	}

	tt_dbf.free_result(tt_db_handle, result);
	return 0;
}
