#include "db.h"
#include "route_data.h"

db1_con_t * interconnectroute_dbh = NULL;
db_func_t interconnectroute_dbf;

static char *orig_route_data_query = "select TFROM.INTERNAL_ID as FROM_TRUNK_ID, TTO.INTERNAL_ID as TO_TRUNK_ID, RT.ROUTE_ID as ROUTE_ID, TTO.EXTERNAL_ID as EXTERNAL_PARTNER_ID, "
"if (PORT.NUMBER IS NULL,'N','Y') AS PORTED from "
"service_rate SR "
"join interconnect_trunk TFROM on TFROM.INTERCONNECT_PARTNER_ID = SR.FROM_INTERCONNECT_PARTNER_ID "
"join interconnect_trunk TTO on TTO.INTERCONNECT_PARTNER_ID = SR.TO_INTERCONNECT_PARTNER_ID "
"join interconnect_route RT on TTO.EXTERNAL_ID = RT.EXTERNAL_ID "
"LEFT JOIN ported_number PORT on PORT.NUMBER = '%.*s' "
"where "
"FROM_INTERCONNECT_PARTNER_ID = "
"( "
"SELECT IPID "
"FROM "
"( "
"( "
"SELECT INTERCONNECT_PARTNER_ID AS IPID, 1000000 AS PRIORITY "
"FROM ported_number "
"WHERE NUMBER = %.*s "
") "
"UNION "
"( "
"SELECT FROM_INTERCONNECT_PARTNER_ID AS IPID, PRIORITY "
"FROM service_rate "
"WHERE '%.*s' like concat(FROM_PREFIX,'%') "
"ORDER BY length(FROM_PREFIX) desc limit 1 "
") "
") "
"AS tmp "
"ORDER BY tmp.PRIORITY DESC "
"LIMIT 1 "
") "
"and TO_INTERCONNECT_PARTNER_ID = "
"( "
"SELECT IPID "
"FROM "
"( "
"( "
"SELECT INTERCONNECT_PARTNER_ID AS IPID, 1000000 AS PRIORITY "
"FROM ported_number "
"WHERE NUMBER = %.*s "
") "
"UNION "
"( "
"SELECT TO_INTERCONNECT_PARTNER_ID AS IPID, PRIORITY "
"FROM service_rate "
"WHERE '%.*s' like concat(TO_PREFIX,'%') "
"ORDER BY length(TO_PREFIX) desc limit 1 "
") "
") "
"AS tmp "
"ORDER BY tmp.PRIORITY DESC "
"LIMIT 1 "
") "
"and SR.SERVICE_CODE = '%.*s' "
"and SR.LEG = '%.*s' "
"order by SR.PRIORITY DESC, RT.PRIORITY DESC LIMIT 1;";

static char *term_route_data_query = "select TFROM.INTERNAL_ID as FROM_TRUNK_ID, TTO.INTERNAL_ID as TO_TRUNK_ID, TFROM.EXTERNAL_ID as EXTERNAL_PARTNER_ID from "
"service_rate SR "
"join interconnect_trunk TFROM on TFROM.INTERCONNECT_PARTNER_ID = SR.FROM_INTERCONNECT_PARTNER_ID "
"join interconnect_trunk TTO on TTO.INTERCONNECT_PARTNER_ID = SR.TO_INTERCONNECT_PARTNER_ID "
"where "
"FROM_INTERCONNECT_PARTNER_ID = "
"( "
"SELECT IPID "
"FROM "
"( "
"( "
"SELECT INTERCONNECT_PARTNER_ID AS IPID, 1000000 AS PRIORITY "
"FROM ported_number "
"WHERE NUMBER = %.*s "
") "
"UNION "
"( "
"SELECT FROM_INTERCONNECT_PARTNER_ID AS IPID, PRIORITY "
"FROM service_rate "
"WHERE '%.*s' like concat(FROM_PREFIX,'%') "
"ORDER BY length(FROM_PREFIX) desc limit 1 "
") "
") "
"AS tmp "
"ORDER BY tmp.PRIORITY DESC "
"LIMIT 1 "
") "
"and TO_INTERCONNECT_PARTNER_ID = "
"( "
"SELECT IPID "
"FROM "
"( "
"( "
"SELECT INTERCONNECT_PARTNER_ID AS IPID, 1000000 AS PRIORITY "
"FROM ported_number "
"WHERE NUMBER = %.*s "
") "
"UNION "
"( "
"SELECT TO_INTERCONNECT_PARTNER_ID AS IPID, PRIORITY "
"FROM service_rate "
"WHERE '%.*s' like concat(TO_PREFIX,'%') "
"ORDER BY length(TO_PREFIX) desc limit 1 "
") "
") "
"AS tmp "
"ORDER BY tmp.PRIORITY DESC "
"LIMIT 1 "
") "
"and SR.SERVICE_CODE = '%.*s' "
"and SR.LEG = '%.*s' "
"and TFROM.EXTERNAL_ID = '%.*s' LIMIT 1;";

static char query[QUERY_LEN];


str interconnect_trunk_table = str_init("interconnect_trunk");
str interconnect_route_table = str_init("interconnect_route");
str service_rate_table = str_init("service_rate");


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


int get_orig_route_data(str* a_number, str* b_number, str* leg, str* sc, ix_route_list_t** ix_route_list) {
    int i,n;
    db1_res_t* route_rs;
    db_row_t* route_row;
    db_val_t* route_vals;
    str query_s, incoming_trunk_id, outgoing_trunk_id, route_id, external_trunk_id, is_ported;
    route_data_t* new_route;
    ix_route_list_t* route_list = new_route_list();
    int num_rows;
    

    if (strlen(orig_route_data_query) + a_number->len + a_number->len + b_number->len + b_number->len + leg->len + sc->len > QUERY_LEN) {
	LM_ERR("query too big\n");
	return -1;
    }
    
    snprintf(query, QUERY_LEN, orig_route_data_query, b_number->len, b_number->s, a_number->len, a_number->s, a_number->len, a_number->s, b_number->len, b_number->s,
	    b_number->len, b_number->s, sc->len, sc->s, leg->len, leg->s);
    query_s.s = query;
    query_s.len = strlen(query);

    LM_DBG("get_orig_route_data query is: [%s]\n", query);
    if (interconnectroute_dbf.raw_query(interconnectroute_dbh, &query_s, &route_rs) != 0) {
	LM_ERR("Unable to query DB for interconnect routes with a_number [%.*s] b_number [%.*s]\n", a_number->len, a_number->s, b_number->len, b_number->s);
	interconnectroute_dbf.free_result(interconnectroute_dbh, route_rs);
    }

    LM_DBG("Received route results [%d]\n", RES_ROW_N(route_rs));
    if (RES_ROW_N(route_rs) <= 0) {
	LM_DBG("No routes found for a_number [%.*s] b_number [%.*s]\n", a_number->len, a_number->s, b_number->len, b_number->s);
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
		    incoming_trunk_id.s = (char*) VAL_STRING(route_vals);
		    incoming_trunk_id.len = strlen(incoming_trunk_id.s);
		    LM_DBG("incoming_trunk_id: [%.*s]\n", incoming_trunk_id.len, incoming_trunk_id.s);
		}
		if (!VAL_NULL(route_vals+1)) {
		    outgoing_trunk_id.s = (char*) VAL_STRING(route_vals+1);
		    outgoing_trunk_id.len = strlen(outgoing_trunk_id.s);
		    LM_DBG("outgoing_trunk_id: [%.*s]\n", outgoing_trunk_id.len, outgoing_trunk_id.s);
		}
		if (!VAL_NULL(route_vals+2)) {
		    route_id.s = (char*) VAL_STRING(route_vals+2);
		    route_id.len = strlen(route_id.s);
		    LM_DBG("route_id: [%.*s]\n", route_id.len, route_id.s);
		}
		if (!VAL_NULL(route_vals+3)) {
		    external_trunk_id.s = (char*) VAL_STRING(route_vals+3);
		    external_trunk_id.len = strlen(external_trunk_id.s);
		    LM_DBG("external_trunk_id: [%.*s]\n", external_trunk_id.len, external_trunk_id.s);
		}
                if (!VAL_NULL(route_vals+4)) {
		    is_ported.s = (char*) VAL_STRING(route_vals+4);
                    is_ported.len = strlen(is_ported.s);
		    LM_DBG("ported is: [%.*s]\n", is_ported.len, is_ported.s);
		}
                
		new_route = new_route_data(&incoming_trunk_id, &outgoing_trunk_id, &route_id, &external_trunk_id, &is_ported);
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
    
    //*route_data = new_route;
    *ix_route_list = route_list;
    num_rows = route_list->count;//RES_ROW_N(route_rs);
    interconnectroute_dbf.free_result(interconnectroute_dbh, route_rs);
    LM_DBG("Returning %d rows\n", num_rows);
    return num_rows;
}


int get_term_route_data(str* a_number, str* b_number, str* leg, str* sc, str* ext_trunk_id, ix_route_list_t** ix_route_list) {
    
    int i,n;
    db1_res_t* route_rs;
    db_row_t* route_row;
    db_val_t* route_vals;
    str query_s, incoming_trunk_id, outgoing_trunk_id, external_trunk_id;
    route_data_t* new_route;
    ix_route_list_t* route_list = new_route_list();
    int num_rows;
    

    if (strlen(term_route_data_query) + a_number->len + a_number->len + b_number->len + a_number->len + leg->len + sc->len + ext_trunk_id->len > QUERY_LEN) {
	LM_ERR("query too big\n");
	return -1;
    }
    
    snprintf(query, QUERY_LEN, term_route_data_query, a_number->len, a_number->s, a_number->len, a_number->s, b_number->len, b_number->s,
	    b_number->len, b_number->s, sc->len, sc->s, leg->len, leg->s, ext_trunk_id->len, ext_trunk_id->s);
    query_s.s = query;
    query_s.len = strlen(query);

    LM_DBG("get_term_route_data query is: [%s]\n", query);

    if (interconnectroute_dbf.raw_query(interconnectroute_dbh, &query_s, &route_rs) != 0) {
	LM_ERR("Unable to query DB for interconnect routes with ext trunk id [%.*s]\n", ext_trunk_id->len, ext_trunk_id->s);
	interconnectroute_dbf.free_result(interconnectroute_dbh, route_rs);
    }

    LM_DBG("Received route results [%d]\n", RES_ROW_N(route_rs));
    if (RES_ROW_N(route_rs) <= 0) {
	LM_DBG("No routes found for ext trunk id [%.*s]\n", ext_trunk_id->len, ext_trunk_id->s);
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
		    incoming_trunk_id.s = (char*) VAL_STRING(route_vals);
		    incoming_trunk_id.len = strlen(incoming_trunk_id.s);
		    LM_DBG("incoming_trunk_id: [%.*s]\n", incoming_trunk_id.len, incoming_trunk_id.s);
		}
		if (!VAL_NULL(route_vals+1)) {
		    outgoing_trunk_id.s = (char*) VAL_STRING(route_vals+1);
		    outgoing_trunk_id.len = strlen(outgoing_trunk_id.s);
		    LM_DBG("outgoing_trunk_id: [%.*s]\n", outgoing_trunk_id.len, outgoing_trunk_id.s);
		}
                if (!VAL_NULL(route_vals+2)) {
		    external_trunk_id.s = (char*) VAL_STRING(route_vals+1);
		    external_trunk_id.len = strlen(external_trunk_id.s);
		    LM_DBG("external_trunk_id: [%.*s]\n", external_trunk_id.len, external_trunk_id.s);
		}
		
		new_route = new_route_data(&incoming_trunk_id, &outgoing_trunk_id, 0, &external_trunk_id, 0);
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
    
    //*route_data = new_route;
    *ix_route_list = route_list;
    num_rows = route_list->count;//RES_ROW_N(route_rs);
    interconnectroute_dbf.free_result(interconnectroute_dbh, route_rs);
    LM_DBG("Returning %d rows\n", num_rows);
    return num_rows;
}


