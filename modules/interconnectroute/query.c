#include "query.h"
#include "db.h"
#include "../../parser/parse_uri.h"
#include "../../sr_module.h"

int create_response_avp_string(char* name, str* val) {
    int rc;
    int_str avp_val, avp_name;
    avp_name.s.s = name;
    avp_name.s.len = strlen(name);

    avp_val.s = *val;

    rc = add_avp(AVP_NAME_STR|AVP_VAL_STR, avp_name, avp_val);

    if (rc < 0)
        LM_ERR("couldnt create AVP\n");
    else
        LM_INFO("created AVP successfully : [%.*s] - [%.*s]\n", avp_name.s.len, avp_name.s.s, val->len, val->s);

    return 1;
}

int create_avps(route_data_t* route_data) {
    create_response_avp_string("ix_trunk_id", &route_data->trunk_id);
    create_response_avp_string("ix_trunk_gw", &route_data->ipv4);
    create_response_avp_string("ix_external_trunk_id", &route_data->external_trunk_id);
}

int isonlydigits(str* s) {
    int i;

    for (i = 0; i < s->len; i++) {
	if (!isdigit(s->s[i]))
	    return 0;
    }

    return 1;
}

int ix_trunk_query(struct sip_msg* msg, char* uri) {
    str uri_number_s, number;
    sip_uri_t* sip_uri;
    route_data_t* route_data;
    
    if (get_str_fparam(&uri_number_s, msg, (fparam_t*) uri) < 0) {
	    LM_ERR("failed to get URI or number\n");
	    return -1;
    }
    number.s = uri_number_s.s;
    number.len = uri_number_s.len;
    
    LM_DBG("IX Route: searching for URI: <%.*s>\n", number.len, number.s);
    
    if (number.len < 4 || (strncmp(number.s, "sip:", 4) && strncmp(number.s, "tel:", 4))) {
	LM_DBG("no leading tel: or sip: [%.*s] - assuming number passed in... will check => \n", number.len, number.s);
    } else {
	//need to clean uri to extract "user" portion - which should be a number...
	if (parse_uri(uri_number_s.s, number.len, sip_uri) != 0) {
	    LM_ERR("Failed to parse URI [%.*s]\n", number.len, number.s);
	    return -1;
	}
	number = sip_uri->user;
    }
    
    if (number.len > 0 && number.s[0]=='+') {
	LM_DBG("stripping off leading +\n");
	number.s = number.s + 1;
	number.len -= 1;
    }

    /* check this is a number? */
    if (!isonlydigits(&number)) {
	LM_WARN("not a number and not a URI... aborting\n");
	return -1;
    } 

    LM_DBG("number to be searched: [%.*s]\n", number.len, number.s);
    int res = get_routes(&number, &route_data);
    
    if (res <= 0) {
	return -1;
    }
    
    LM_DBG("Received route for operator key: [%.*s]\n", route_data->operator_key.len, route_data->operator_key.s);
    create_avps(route_data);

    return 1;
}