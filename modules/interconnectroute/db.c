#include "db.h"
#include "route_data.h"

db1_con_t * interconnectroute_dbh = NULL;
db_func_t interconnectroute_dbf;

static char *query_fmt = "select N.OPERATOR_KEY, T.TRUNK_ID, T.PRIORITY, T.IPV4, T.EXTERNAL_TRUNK_ID "
	"from operator_number N left join operator_trunk T on N.OPERATOR_KEY=T.OPERATOR_KEY "
	"where %.*s >= CAST(NUMBER_START AS UNSIGNED) and %.*s <= CAST(NUMBER_END AS UNSIGNED) ORDER BY T.PRIORITY DESC";
static char query[QUERY_LEN];

str interconnectnumber_table = str_init("operator_number");

/* table version */
const unsigned int interconnectroute_version = 1;

str interconnecttrunk_table = str_init("operator_trunk");

/*
 * Closes the DB connection.
 */
void interconnectroute_db_close(void) {
    if (interconnectroute_dbh) {
	interconnectroute_dbf.close(interconnectroute_dbh);
	interconnectroute_dbh = NULL;
    }
}


int interconnectroute_db_init(void) {
    if (!interconnectroute_db_url.s || !interconnectroute_db_url.len) {
	LM_ERR("you have to set the db_url module parameter.\n");
	return -1;
    }
    if (db_bind_mod(&interconnectroute_db_url, &interconnectroute_dbf) < 0) {
	LM_ERR("can't bind database module.\n");
	return -1;
    }
    if ((interconnectroute_dbh = interconnectroute_dbf.init(&interconnectroute_db_url)) == NULL) {
	LM_ERR("can't connect to database using [%.*s]\n", interconnectroute_db_url.len, interconnectroute_db_url.s);
	return -1;
    }
    if (
	    (db_check_table_version(&interconnectroute_dbf, interconnectroute_dbh, &interconnectnumber_table, interconnectroute_version) < 0) ||
	    (db_check_table_version(&interconnectroute_dbf, interconnectroute_dbh, &interconnecttrunk_table, interconnectroute_version) < 0)) {
	LM_ERR("during table version check.\n");
	interconnectroute_db_close();
	return -1;
    }
    interconnectroute_db_close();
    return 0;
}


int interconnectroute_db_open(void) {
    if (interconnectroute_dbh) {
	interconnectroute_dbf.close(interconnectroute_dbh);
    }
    if ((interconnectroute_dbh = interconnectroute_dbf.init(&interconnectroute_db_url)) == NULL) {
	LM_ERR("can't connect to database.\n");
	return -1;
    }
    return 0;
}

int get_routes(str* dst_number, route_data_t** route_data) {
    int i,n, priority;
    db1_res_t* route_rs;
    db_row_t* route_row;
    db_val_t* route_vals;
    str query_s, operator_key, trunk_id, external_trunk_id, gateway_ipv4;
    route_data_t* new_route;
    ix_route_list_t* route_list = new_route_list();
    int num_rows;
    

    if (strlen(query_fmt) + dst_number->len > QUERY_LEN) {
	LM_ERR("query too big\n");
	return -1;
    }
    snprintf(query, QUERY_LEN, query_fmt, dst_number->len, dst_number->s, dst_number->len, dst_number->s);
    query_s.s = query;
    query_s.len = strlen(query);

    LM_DBG("QUERY IS: [%s]\n", query);

    if (interconnectroute_dbf.raw_query(interconnectroute_dbh, &query_s, &route_rs) != 0) {
	LM_ERR("Unable to query DB for interconnect routes with number [%.*s]\n", dst_number->len, dst_number->s);
	interconnectroute_dbf.free_result(interconnectroute_dbh, route_rs);
    }

    LM_DBG("Received route results [%d]\n", RES_ROW_N(route_rs));
    if (RES_ROW_N(route_rs) <= 0) {
	LM_DBG("No routes found for number [%.*s]\n", dst_number->len, dst_number->s);
	return -1;
    } else {
	n = 0;
	do {
	    n++;
	    LM_DBG("looping through route recordset [%d]\n", n);
	    for (i = 0; i < RES_ROW_N(route_rs); i++) {
		route_row = RES_ROWS(route_rs) + i;
		route_vals = ROW_VALUES(route_row);

		if (!VAL_NULL(route_vals)) {
		    operator_key.s = (char*) VAL_STRING(route_vals);
		    operator_key.len = strlen(operator_key.s);
		    LM_DBG("Received route for operator key: [%.*s]\n", operator_key.len, operator_key.s);
		}
		if (!VAL_NULL(route_vals+1)) {
		    trunk_id.s = (char*) VAL_STRING(route_vals+1);
		    trunk_id.len = strlen(trunk_id.s);
		    LM_DBG("Trunk is: [%.*s]\n", trunk_id.len, trunk_id.s);
		}
		if (!VAL_NULL(route_vals+2)) {
		    priority = VAL_INT(route_vals+2);
		    LM_DBG("Priority is: [%d]\n", priority);
		}
		if (!VAL_NULL(route_vals+3)) {
		    gateway_ipv4.s = (char*) VAL_STRING(route_vals+3);
		    gateway_ipv4.len = strlen(gateway_ipv4.s);
		    LM_DBG("Gateway IP for trunk to this operator is: [%.*s]\n", gateway_ipv4.len, gateway_ipv4.s);
		}
		if (!VAL_NULL(route_vals+4)) {
		    external_trunk_id.s = (char*) VAL_STRING(route_vals+4);
		    external_trunk_id.len = strlen(external_trunk_id.s);
		    LM_DBG("External trunk ID: [%.*s]\n", external_trunk_id.len, external_trunk_id.s);
		}
		
		new_route = new_route_data(&operator_key, &trunk_id, priority, &gateway_ipv4, &external_trunk_id);
		if (!new_route) {
		    LM_DBG("Could not get new route... continuing\n");
		    continue;
		}
		
		if (!add_route(route_list, new_route)) {
		    LM_DBG("unable to add route.....\n");
		    continue;
		}
		
		LM_DBG("route list now has %d elements\n", route_list->count);
		
		
	    }
	    if (DB_CAPABILITY(interconnectroute_dbf, DB_CAP_FETCH)) {
		if (interconnectroute_dbf.fetch_result(interconnectroute_dbh, &route_rs, 2000/*ul_fetch_rows*/) < 0) {
		    LM_ERR("fetching rows failed\n");
		    interconnectroute_dbf.free_result(interconnectroute_dbh, route_rs);
		    return -1;
		}
	    } else {
		break;
	    }
	} while (RES_ROW_N(route_rs) > 0);
	interconnectroute_dbf.free_result(interconnectroute_dbh, route_rs);
    }
    
    *route_data = new_route;
    num_rows = route_list->count;//RES_ROW_N(route_rs);
    interconnectroute_dbf.free_result(interconnectroute_dbh, route_rs);
    LM_DBG("Returning %d rows\n", num_rows);
    return num_rows;
}


