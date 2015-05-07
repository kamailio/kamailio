#include "query.h"
#include "db.h"
#include "../../parser/parse_uri.h"
#include "../../sr_module.h"
#include "../../lib/ims/ims_getters.h"
#include "interconnectroute.h"
#include "../../parser/sdp/sdp.h"

extern str voice_service_code, video_service_code;

/**We need to get the service code
 currently we hard code this to voice service context id _ voice service id
 This is Smile specific - think about making this more generic*/
inline int get_service_code(str *sc, struct sip_msg* msg) {
    int sdp_stream_num = 0;
    sdp_session_cell_t* msg_sdp_session;
    sdp_stream_cell_t* msg_sdp_stream;
    int intportA;
    
    //check SDP - if there is video then use video service code otherwise use voice service code
    if (parse_sdp(msg) < 0) {
	LM_ERR("Unable to parse req SDP\n");
	return -1;
    }

    msg_sdp_session = get_sdp_session(msg, 0);
    if (!msg_sdp_session ) {
	LM_ERR("Missing SDP session information from rpl\n");
    } else {
	for (;;) {
	    msg_sdp_stream = get_sdp_stream(msg, 0, sdp_stream_num);
	    if (!msg_sdp_stream) {
		break;
	    }

	    intportA = atoi(msg_sdp_stream->port.s);
	    if(intportA != 0 && strncasecmp(msg_sdp_stream->media.s,"video",5)==0){
		LM_DBG("This SDP has a video component and src ports not equal to 0 - so we use video service code: [%.*s]", 
			video_service_code.len, video_service_code.s);
		sc->s = video_service_code.s;
		sc->len = video_service_code.len;
		break;
	    }

	    sdp_stream_num++;
	}
    }

    free_sdp((sdp_info_t**) (void*) &msg->body);
    
    if(sc->len == 0) {
	LM_DBG("We use default voice service code: [%.*s]", 
			voice_service_code.len, voice_service_code.s);
		sc->s = voice_service_code.s;
		sc->len = voice_service_code.len;
    }
    return 1;
    
}

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

int create_orig_avps(route_data_t* route_data) {
    create_response_avp_string("ix_incoming_trunk_id", &route_data->incoming_trunk_id);
    create_response_avp_string("ix_outgoing_trunk_id", &route_data->outgoing_trunk_id);
    create_response_avp_string("ix_route_id", &route_data->route_id);
    create_response_avp_string("ix_external_trunk_id", &route_data->external_trunk_id);
    create_response_avp_string("ix_is_ported", &route_data->is_ported);
    
    return 1;
}

int create_term_avps(route_data_t* route_data) {
    create_response_avp_string("ix_incoming_trunk_id", &route_data->incoming_trunk_id);
    create_response_avp_string("ix_outgoing_trunk_id", &route_data->outgoing_trunk_id);
    create_response_avp_string("ix_external_trunk_id", &route_data->external_trunk_id);
    return 1;
}

int isonlydigits(str* s) {
    int i;

    for (i = 0; i < s->len; i++) {
	if (!isdigit(s->s[i]))
	    return 0;
    }

    return 1;
}

int ix_orig_trunk_query(struct sip_msg* msg) {
    str sc = {0, 0};
    sip_uri_t calling_party_sip_uri, called_party_sip_uri;
    ix_route_list_t* ix_route_list;
    str called_asserted_identity = {0 , 0 },
	asserted_identity = {0 , 0 },
	    a_number = {0 , 0 },
	b_number = {0 , 0 };
    int free_called_asserted_identity = 0;
    struct hdr_field *h=0;
    str orig_leg = {"O", 1};
    
    //getting asserted identity
    if ((asserted_identity = cscf_get_asserted_identity(msg, 0)).len == 0) {
	    LM_DBG("No P-Asserted-Identity hdr found. Using From hdr for asserted_identity");
	    if (!cscf_get_from_uri(msg, &asserted_identity)) {
    		LM_ERR("Error assigning P-Asserted-Identity using From hdr");
    		goto error;
    	}
    }
    
    
    //getting called asserted identity
    if ((called_asserted_identity = cscf_get_public_identity_from_called_party_id(msg, &h)).len == 0) {
	    LM_DBG("No P-Called-Identity hdr found. Using request URI for called_asserted_identity");
	    called_asserted_identity = cscf_get_public_identity_from_requri(msg);
	    free_called_asserted_identity = 1;
    }
    
    
    
    LM_DBG("IX Route: Calling party [%.*s] Called party [%.*s]\n", asserted_identity.len, asserted_identity.s, called_asserted_identity.len, called_asserted_identity.s);
    
    /*Cleaning asserted identity*/
    if (asserted_identity.len < 4 || (strncmp(asserted_identity.s, "sip:", 4) && strncmp(asserted_identity.s, "tel:", 4))) {
	LM_DBG("no leading tel: or sip: [%.*s] - assuming number passed in... will check => \n", asserted_identity.len, asserted_identity.s);
    } else {
	//need to clean uri to extract "user" portion - which should be a number...
	if (parse_uri(asserted_identity.s, asserted_identity.len, &calling_party_sip_uri) != 0) {
	    LM_ERR("Failed to parse URI [%.*s]\n", asserted_identity.len, asserted_identity.s);
	    goto error;
	}
	a_number = calling_party_sip_uri.user;
    }
    if (a_number.len > 0 && a_number.s[0]=='+') {
	LM_DBG("stripping off leading +\n");
	a_number.s = a_number.s + 1;
	a_number.len -= 1;
    }
    /* check this is a number? */
    if (!isonlydigits(&a_number)) {
	LM_WARN("not a number and not a URI... aborting\n");
	goto error;
    }
    
    /*Cleaning asserted identity*/
    if (called_asserted_identity.len < 4 || (strncmp(called_asserted_identity.s, "sip:", 4) && strncmp(called_asserted_identity.s, "tel:", 4))) {
	LM_DBG("no leading tel: or sip: [%.*s] - assuming number passed in... will check => \n", called_asserted_identity.len, called_asserted_identity.s);
    } else {
	//need to clean uri to extract "user" portion - which should be a number...
	if (parse_uri(called_asserted_identity.s, called_asserted_identity.len, &called_party_sip_uri) != 0) {
	    LM_ERR("Failed to parse URI [%.*s]\n", called_asserted_identity.len, called_asserted_identity.s);
	    goto error;
	}
	b_number = called_party_sip_uri.user;
    }
    if (b_number.len > 0 && b_number.s[0]=='+') {
	LM_DBG("stripping off leading +\n");
	b_number.s = b_number.s + 1;
	b_number.len -= 1;
    }
    /* check this is a number? */
    if (!isonlydigits(&b_number)) {
	LM_WARN("not a number and not a URI... aborting\n");
	goto error;
    }
    
    if(!get_service_code(&sc, msg)){
	LM_ERR("Could not get service code\n");
	goto error;
    }

    LM_DBG("a_number to be searched: [%.*s], b_number to be searched: [%.*s], with service code: [%.*s] and leg [%.*s]\n", 
	    a_number.len, a_number.s, b_number.len, b_number.s, sc.len, sc.s, orig_leg.len, orig_leg.s);
    
    
    //GET ORIG ROUTE DATA 
    //you have A number, B number, service code and direction  
    //you want incoming trunk_id, outgoing trunk_id, route_id
    //look up A number, B number, service code and direction in service_rate table return single highest priority entry to get from_interconnect_partner_id and to_interconnect_partner_id
    //Join from_interconnect_partner_id with interconnect_partner table to get incoming_trunk_id
    //Join to_interconnect_partner_id with interconnect_partner table to get outgoing_trunk_id
    //Join outgoing_trunk_id with interconnect_trunk table to get external_trunk_id
    //Join external_trunk_id with interconnect_route table to get route_id
    //pass back incoming trunk_id, outgoing trunk_id, route_id
    //S-CSCF/IPSMGW/etc. passes incoming_trunk_id, outgoing_trunk_id, service_code and direction to BM for charging and uses route-ID to route the request (use dispatcher or something similar)
    
    int res = get_orig_route_data(&a_number, &b_number, &orig_leg, &sc, &ix_route_list);
    
    if (res <= 0) {
	goto error;
    }
    
    LM_DBG("Received list of routes - currently we just use first in list incoming_trunk_id: [%.*s] outgoing_trunk_id [%.*s]\n", 
	    ix_route_list->first->incoming_trunk_id.len, ix_route_list->first->incoming_trunk_id.s, ix_route_list->first->outgoing_trunk_id.len, ix_route_list->first->outgoing_trunk_id.s);
    create_orig_avps(ix_route_list->first);
    
    free_route_list(ix_route_list);
    
    if(free_called_asserted_identity) {
	if(called_asserted_identity.s) shm_free(called_asserted_identity.s);// shm_malloc in cscf_get_public_identity_from_requri
    }  

    return 1;
    
error:
    if(free_called_asserted_identity) {
	if(called_asserted_identity.s) shm_free(called_asserted_identity.s);// shm_malloc in cscf_get_public_identity_from_requri
    }    

    return -1;
}


/*
 This takes in an external trunk id - gets the operator the number then gets the operator then gets the trunk details
 */
int ix_term_trunk_query(struct sip_msg* msg, char* ext_trunk_id) {
    str external_trunk_id = {0, 0};
    ix_route_list_t* ix_route_list;
    str sc = {0, 0};
    sip_uri_t calling_party_sip_uri, called_party_sip_uri;
    str called_asserted_identity = {0 , 0 },
	asserted_identity = {0 , 0 },
	    a_number = {0 , 0 },
	b_number = {0 , 0 };
    int free_called_asserted_identity = 0;
    struct hdr_field *h=0;
    str term_leg = {"T", 1};
    
    
    if (get_str_fparam(&external_trunk_id, msg, (fparam_t*) ext_trunk_id) < 0) {
	    LM_ERR("failed to get external_trunk_id\n");
	    goto error;
    }
    
    //getting asserted identity
    if ((asserted_identity = cscf_get_asserted_identity(msg, 0)).len == 0) {
	    LM_DBG("No P-Asserted-Identity hdr found. Using From hdr for asserted_identity");
	    if (!cscf_get_from_uri(msg, &asserted_identity)) {
    		LM_ERR("Error assigning P-Asserted-Identity using From hdr");
    		goto error;
    	}
    }
    
    //getting called asserted identity
    if ((called_asserted_identity = cscf_get_public_identity_from_called_party_id(msg, &h)).len == 0) {
	    LM_DBG("No P-Called-Identity hdr found. Using request URI for called_asserted_identity");
	    called_asserted_identity = cscf_get_public_identity_from_requri(msg);
	    free_called_asserted_identity = 1;
    }
    
    LM_DBG("IX Route: Calling party [%.*s] Called party [%.*s]\n", asserted_identity.len, asserted_identity.s, called_asserted_identity.len, called_asserted_identity.s);
    
    /*Cleaning asserted identity*/
    if (asserted_identity.len < 4 || (strncmp(asserted_identity.s, "sip:", 4) && strncmp(asserted_identity.s, "tel:", 4))) {
	LM_DBG("no leading tel: or sip: [%.*s] - assuming number passed in... will check => \n", asserted_identity.len, asserted_identity.s);
    } else {
	//need to clean uri to extract "user" portion - which should be a number...
	if (parse_uri(asserted_identity.s, asserted_identity.len, &calling_party_sip_uri) != 0) {
	    LM_ERR("Failed to parse URI [%.*s]\n", asserted_identity.len, asserted_identity.s);
	    goto error;
	}
	a_number = calling_party_sip_uri.user;
    }
    if (a_number.len > 0 && a_number.s[0]=='+') {
	LM_DBG("stripping off leading +\n");
	a_number.s = a_number.s + 1;
	a_number.len -= 1;
    }
    /* check this is a number? */
    if (!isonlydigits(&a_number)) {
	LM_WARN("not a number and not a URI... aborting\n");
	goto error;
    }
    
    /*Cleaning asserted identity*/
    if (called_asserted_identity.len < 4 || (strncmp(called_asserted_identity.s, "sip:", 4) && strncmp(called_asserted_identity.s, "tel:", 4))) {
	LM_DBG("no leading tel: or sip: [%.*s] - assuming number passed in... will check => \n", called_asserted_identity.len, called_asserted_identity.s);
    } else {
	//need to clean uri to extract "user" portion - which should be a number...
	if (parse_uri(called_asserted_identity.s, called_asserted_identity.len, &called_party_sip_uri) != 0) {
	    LM_ERR("Failed to parse URI [%.*s]\n", called_asserted_identity.len, called_asserted_identity.s);
	    goto error;
	}
	b_number = called_party_sip_uri.user;
    }
    if (b_number.len > 0 && b_number.s[0]=='+') {
	LM_DBG("stripping off leading +\n");
	b_number.s = b_number.s + 1;
	b_number.len -= 1;
    }
    /* check this is a number? */
    if (!isonlydigits(&b_number)) {
	LM_WARN("not a number and not a URI... aborting\n");
	goto error;
    }
    
    if(!get_service_code(&sc, msg)){
	LM_ERR("Could not get service code\n");
	goto error;
    }

    LM_DBG("a_number to be searched: [%.*s], b_number to be searched: [%.*s], with service code: [%.*s] and leg [%.*s] and external_trunk_id [%.*s]\n", 
	    a_number.len, a_number.s, b_number.len, b_number.s, sc.len, sc.s, term_leg.len, term_leg.s, external_trunk_id.len, external_trunk_id.s);
    
    
    //GET TERM ROUTE DATA 
    //you have A number, B number, external trunk id and direction
    //you want incoming_trunk_id and outgoing_trunk_id
    //pass in A number, B number, external trunk id and direction
    //look up interconnect_route table based on external_trunk_id
    //join with interconnect_trunk to get incoming_trunk_id
    //join with service_rate table passing A number, B number, direction and incoming_trunk_id to get outgoing_trunk_id
    //Pass back incoming_trunk_id and outgoing_trunk_id
    //S-CSCF/IPSMGW/etc. passes incoming_trunk_id, outgoing_trunk_id, service_code and direction to BM for charging 
    
    int res = get_term_route_data(&a_number, &b_number, &term_leg, &sc, &external_trunk_id, &ix_route_list);
    
    if (res <= 0) {
	goto error;
    }
    
    LM_DBG("Received list of routes - currently we just use first in list incoming_trunk_id: [%.*s] outgoing_trunk_id [%.*s]\n", 
	    ix_route_list->first->incoming_trunk_id.len, ix_route_list->first->incoming_trunk_id.s, ix_route_list->first->outgoing_trunk_id.len, ix_route_list->first->outgoing_trunk_id.s);
    create_term_avps(ix_route_list->first);
    
    free_route_list(ix_route_list);

    if(free_called_asserted_identity) {
	if(called_asserted_identity.s) shm_free(called_asserted_identity.s);// shm_malloc in cscf_get_public_identity_from_requri
    } 
    
    return 1;
    
        
error:
    if(free_called_asserted_identity) {
	if(called_asserted_identity.s) shm_free(called_asserted_identity.s);// shm_malloc in cscf_get_public_identity_from_requri
    }    

    return -1;
}