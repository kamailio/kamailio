/*
 * usrloc_db.c
 *
 *  Created on: Nov 11, 2013
 *      Author: carlos
 */

#include "../../lib/srdb1/db.h"
#include "usrloc.h"
#include "usrloc_db.h"

str id_col	        	= str_init(ID_COL);
str domain_col			= str_init(DOMAIN_COL);
str aor_col		    	= str_init(AOR_COL);
str contact_col		    = str_init(CONTACT_COL);
str received_col    	= str_init(RECEIVED_COL);
str received_port_col	= str_init(RECEIVED_PORT_COL);
str received_proto_col	= str_init(RECEIVED_PROTO_COL);
str path_col			= str_init(PATH_COL);
str rx_session_id_col	= str_init(RX_SESSION_ID_COL);
str reg_state_col		= str_init(REG_STATE_COL);
str expires_col			= str_init(EXPIRES_COL);
str service_routes_col	= str_init(SERVICE_ROUTES_COL);
str socket_col			= str_init(SOCKET_COL);
str public_ids_col		= str_init(PUBLIC_IDS_COL);
str security_type_col 	= str_init(SECURITY_TYPE_COL);
str protocol_col 		= str_init(PROTOCOL_COL);
str mode_col			= str_init(MODE_COL);
str ck_col				= str_init(CK_COL);
str ik_col 				= str_init(IK_COL);
str ealg_col			= str_init(EALG_COL);
str ialg_col 			= str_init(IALG_COL);
str port_uc_col			= str_init(PORTUC_COL);
str port_us_col 		= str_init(PORTUS_COL);
str spi_pc_col 			= str_init(SPIPC_COL);
str spi_ps_col 			= str_init(SPIPS_COL);
str spi_uc_col 			= str_init(SPIUC_COL);
str spi_us_col 			= str_init(SPIUS_COL);
str t_security_type_col	= str_init(T_SECURITY_TYPE_COL);
str t_protocol_col 		= str_init(T_PROTOCOL_COL);
str t_mode_col			= str_init(T_MODE_COL);
str t_ck_col			= str_init(T_CK_COL);
str t_ik_col 			= str_init(T_IK_COL);
str t_ealg_col			= str_init(T_EALG_COL);
str t_ialg_col 			= str_init(T_IALG_COL);
str t_port_uc_col		= str_init(T_PORTUC_COL);
str t_port_us_col 		= str_init(T_PORTUS_COL);
str t_spi_pc_col 		= str_init(T_SPIPC_COL);
str t_spi_ps_col 		= str_init(T_SPIPS_COL);
str t_spi_uc_col 		= str_init(T_SPIUC_COL);
str t_spi_us_col 		= str_init(T_SPIUS_COL);

t_reusable_buffer service_route_buffer = {0,0,0};
t_reusable_buffer impu_buffer = {0,0,0};

int connect_db(const str *db_url)
{
	if (ul_dbh) {	/* we've obviously already connected... */
		LM_WARN("DB connection already open... continuing\n");
		return 0;
	}

	if ((ul_dbh = ul_dbf.init(db_url)) == 0)
		return -1;

	LM_DBG("Successfully connected to DB and returned DB handle ptr %p\n", ul_dbh);
	return 0;
}

int init_db(const str *db_url, int db_update_period, int fetch_num_rows)
{
	/* Find a database module */
	if (db_bind_mod(db_url, &ul_dbf) < 0){
		LM_ERR("Unable to bind to a database driver\n");
		return -1;
	}

	if (connect_db(db_url)!=0){
		LM_ERR("unable to connect to the database\n");
		return -1;
	}

	if (!DB_CAPABILITY(ul_dbf, DB_CAP_ALL)) {
		LM_ERR("database module does not implement all functions needed by the module\n");
		return -1;
	}

	ul_dbf.close(ul_dbh);
	ul_dbh = 0;

	return 0;
}

void destroy_db()
{
	/* close the DB connection */
	if (ul_dbh) {
		ul_dbf.close(ul_dbh);
		ul_dbh = 0;
	}
}

int use_location_pcscf_table(str* domain)
{
	if(!ul_dbh){
		LM_ERR("invalid database handle\n");
		return -1;
	}

	if (ul_dbf.use_table(ul_dbh, domain) < 0) {
		LM_ERR("Error in use_table\n");
		return -1;
	}

	return 0;
}

int db_update_pcontact(pcontact_t* _c)
{
	str impus, service_routes;

	db_val_t match_values[1];
	db_key_t match_keys[1] = { &aor_col };
        db_op_t op[1];
	db_key_t update_keys[8] = { &expires_col, &reg_state_col,
								&service_routes_col, &received_col,
								&received_port_col, &received_proto_col,
								&rx_session_id_col, &public_ids_col };
	db_val_t values[8];
        
        LM_DBG("updating pcontact: %.*s\n", _c->aor.len, _c->aor.s);

	VAL_TYPE(match_values) = DB1_STR;

	VAL_NULL(match_values) = 0;
	VAL_STR(match_values) = _c->aor;
        op[0]=OP_EQ;

	if (use_location_pcscf_table(_c->domain) < 0) {
		LM_ERR("Error trying to use table %.*s\n", _c->domain->len, _c->domain->s);
		return -1;
	}

	VAL_TYPE(values) 	= DB1_DATETIME;
	VAL_TIME(values)	= _c->expires;
	VAL_NULL(values) 	= 0;

	VAL_TYPE(values + 1)= DB1_INT;
	VAL_NULL(values + 1)= 0;
	VAL_INT(values + 1)	= _c->reg_state;

	str empty_str = str_init("");
	if (_c->service_routes) {
		service_routes.len = service_routes_as_string(_c, &service_route_buffer);
		service_routes.s = service_route_buffer.buf;
	}
	SET_STR_VALUE(values + 2, (_c->service_routes)?service_routes:empty_str);
	VAL_TYPE(values + 2) = DB1_STR;
	VAL_NULL(values + 2) = 0;

	SET_STR_VALUE(values + 3, _c->received_host);
	VAL_TYPE(values + 3) = DB1_STR;
	VAL_NULL(values + 3) = 0;

	VAL_TYPE(values + 4)= DB1_INT;
	VAL_NULL(values + 4)= 0;
	VAL_INT(values + 4)	= _c->received_port;

	VAL_TYPE(values + 5)= DB1_INT;
	VAL_NULL(values + 5)= 0;
	VAL_INT(values + 5)	= _c->received_proto;

	VAL_TYPE(values + 6) = DB1_STR;
	SET_PROPER_NULL_FLAG(_c->rx_session_id, values, 6);
	LM_DBG("Trying to set rx session id: %.*s\n", _c->rx_session_id.len, _c->rx_session_id.s);
	SET_STR_VALUE(values + 6, _c->rx_session_id);

	/* add the public identities */
	impus.len = impus_as_string(_c, &impu_buffer);
	impus.s = impu_buffer.buf;
	VAL_TYPE(values + 7) = DB1_STR;
	SET_PROPER_NULL_FLAG(impus, values, 7);
	SET_STR_VALUE(values + 7, impus);

	if((ul_dbf.update(ul_dbh, match_keys, op, match_values, update_keys,values, 1, 8)) !=0){
		LM_ERR("could not update database info\n");
	    return -1;
	}

	if (ul_dbf.affected_rows && ul_dbf.affected_rows(ul_dbh) == 0) {
		LM_DBG("no existing rows for an update... doing insert\n");
		if (db_insert_pcontact(_c) != 0) {
			LM_ERR("Failed to insert a pcontact on update\n");
		}
	}

	return 0;
}

int db_delete_pcontact(pcontact_t* _c)
{
	LM_DBG("Trying to delete contact: %.*s\n", _c->aor.len, _c->aor.s);
	db_val_t values[1];
	db_key_t match_keys[1] = { &aor_col};

	VAL_TYPE(values) = DB1_STR;
	VAL_NULL(values) = 0;
	SET_STR_VALUE(values, _c->aor);

	if (use_location_pcscf_table(_c->domain) < 0) {
		LM_ERR("Error trying to use table %.*s\n", _c->domain->len, _c->domain->s);
		return -1;
	}

    if(ul_dbf.delete(ul_dbh, match_keys, 0, values, 1) < 0) {
    	LM_ERR("Failed to delete database information: aor[%.*s], rx_session_id=[%.*s]\n",
    													_c->aor.len, _c->aor.s,
    													_c->rx_session_id.len, _c->rx_session_id.s);
        return -1;
    }

    return 0;
}

int db_insert_pcontact(struct pcontact* _c)
{
	str empty_str = str_init("");
	str impus, service_routes;

	db_key_t keys[13] = {
							&domain_col,
				&aor_col, 			&contact_col,
				&received_col,
				&received_port_col,	&received_proto_col,
				&path_col,			&rx_session_id_col,
				&reg_state_col,
				&expires_col,		&service_routes_col,
				&socket_col,		&public_ids_col
	};
	db_val_t values[13];

	VAL_TYPE(GET_FIELD_IDX(values, LP_DOMAIN_IDX)) = DB1_STR;
	VAL_TYPE(GET_FIELD_IDX(values, LP_AOR_IDX)) = DB1_STR;
	VAL_TYPE(GET_FIELD_IDX(values, LP_CONTACT_IDX)) = DB1_STR;
	VAL_TYPE(GET_FIELD_IDX(values, LP_RECEIVED_IDX)) = DB1_STR;
	VAL_TYPE(GET_FIELD_IDX(values, LP_RECEIVED_PORT_IDX)) = DB1_INT;
	VAL_TYPE(GET_FIELD_IDX(values, LP_RECEIVED_PROTO_IDX)) = DB1_INT;
	VAL_TYPE(GET_FIELD_IDX(values, LP_PATH_IDX)) = DB1_STR;
	VAL_TYPE(GET_FIELD_IDX(values, LP_RX_SESSION_ID_IDX)) = DB1_STR;
	VAL_TYPE(GET_FIELD_IDX(values, LP_REG_STATE_IDX)) = DB1_INT;
	VAL_TYPE(GET_FIELD_IDX(values, LP_EXPIRES_IDX)) = DB1_DATETIME;
	VAL_TYPE(GET_FIELD_IDX(values, LP_SERVICE_ROUTES_IDX)) = DB1_STR;
	VAL_TYPE(GET_FIELD_IDX(values, LP_SOCKET_IDX)) = DB1_STR;
	VAL_TYPE(GET_FIELD_IDX(values, LP_PUBLIC_IPS_IDX)) = DB1_STR;

	SET_STR_VALUE(GET_FIELD_IDX(values, LP_DOMAIN_IDX), (*_c->domain));
	SET_STR_VALUE(GET_FIELD_IDX(values, LP_AOR_IDX), _c->aor);	//TODO: need to clean AOR
	SET_STR_VALUE(GET_FIELD_IDX(values, LP_CONTACT_IDX), _c->aor);
	SET_STR_VALUE(GET_FIELD_IDX(values, LP_RECEIVED_IDX), _c->received_host);

	SET_PROPER_NULL_FLAG((*_c->domain), values, LP_DOMAIN_IDX);
	SET_PROPER_NULL_FLAG(_c->aor, values, LP_AOR_IDX);
	SET_PROPER_NULL_FLAG(_c->aor, values, LP_CONTACT_IDX);
	SET_PROPER_NULL_FLAG(_c->received_host, values, LP_RECEIVED_IDX);

	VAL_INT(GET_FIELD_IDX(values, LP_RECEIVED_PORT_IDX)) = _c->received_port;
	VAL_INT(GET_FIELD_IDX(values, LP_RECEIVED_PROTO_IDX)) = _c->received_proto;
	VAL_NULL(GET_FIELD_IDX(values, LP_RECEIVED_PORT_IDX)) = 0;
	VAL_NULL(GET_FIELD_IDX(values, LP_RECEIVED_PROTO_IDX)) = 0;

	SET_STR_VALUE(GET_FIELD_IDX(values, LP_PATH_IDX), _c->path);
	SET_STR_VALUE(GET_FIELD_IDX(values, LP_RX_SESSION_ID_IDX), _c->rx_session_id);

	SET_PROPER_NULL_FLAG(_c->path, values, LP_PATH_IDX);
	SET_PROPER_NULL_FLAG(_c->rx_session_id, values, LP_RX_SESSION_ID_IDX);

	VAL_DOUBLE(GET_FIELD_IDX(values, LP_REG_STATE_IDX)) = _c->reg_state;
	VAL_TIME(GET_FIELD_IDX(values, LP_EXPIRES_IDX)) = _c->expires;
	VAL_NULL(GET_FIELD_IDX(values, LP_REG_STATE_IDX)) = 0;
	VAL_NULL(GET_FIELD_IDX(values, LP_EXPIRES_IDX)) = 0;

	SET_STR_VALUE(GET_FIELD_IDX(values, LP_SERVICE_ROUTES_IDX), _c->service_routes?(*_c->service_routes):empty_str);
	VAL_NULL(GET_FIELD_IDX(values, LP_SERVICE_ROUTES_IDX)) = 1;
	SET_STR_VALUE(GET_FIELD_IDX(values, LP_SOCKET_IDX), _c->sock?_c->sock->sock_str:empty_str);
	VAL_NULL(GET_FIELD_IDX(values, LP_SOCKET_IDX)) = 1;

	if (_c->service_routes) {
		SET_PROPER_NULL_FLAG((*_c->service_routes), values, LP_SERVICE_ROUTES_IDX);
	}
	else {
		VAL_NULL(GET_FIELD_IDX(values, LP_SERVICE_ROUTES_IDX)) = 1;
	}

	if (_c->sock) {
		SET_PROPER_NULL_FLAG(_c->sock->sock_str, values, LP_SOCKET_IDX);
	} else {
		VAL_NULL(GET_FIELD_IDX(values, LP_SOCKET_IDX)) = 1;
	}

	/* add the public identities */
	impus.len = impus_as_string(_c, &impu_buffer);
	impus.s = impu_buffer.buf;
	SET_PROPER_NULL_FLAG(impus, values, LP_PUBLIC_IPS_IDX);
	SET_STR_VALUE(GET_FIELD_IDX(values, LP_PUBLIC_IPS_IDX), impus);

	/* add service routes */
	service_routes.len = service_routes_as_string(_c, &service_route_buffer);
	service_routes.s = service_route_buffer.buf;
	SET_PROPER_NULL_FLAG(service_routes, values, LP_SERVICE_ROUTES_IDX);
	SET_STR_VALUE(GET_FIELD_IDX(values, LP_SERVICE_ROUTES_IDX), service_routes);

	if (use_location_pcscf_table(_c->domain) < 0) {
		LM_ERR("Error trying to use table %.*s\n", _c->domain->len, _c->domain->s);
		return -1;
	}

	if (ul_dbf.insert(ul_dbh, keys, values, 13) < 0) {
		LM_ERR("inserting contact in db failed\n");
		return -1;
	}

	return 0;
}

int db_update_pcontact_security_temp(struct pcontact* _c, security_type _t, security_t* _s) {
	db_val_t match_values[1];
	db_key_t match_keys[1] = { &aor_col };
        db_op_t op[1];
	db_key_t update_keys[13] = { &t_security_type_col, &t_protocol_col,
			&t_mode_col, &t_ck_col, &t_ik_col, &t_ealg_col, &t_ialg_col, &t_port_uc_col,
			&t_port_us_col, &t_spi_pc_col, &t_spi_ps_col, &t_spi_uc_col, &t_spi_us_col };
	db_val_t values[13];

	LM_DBG("updating temp security for pcontact: %.*s\n", _c->aor.len, _c->aor.s);

	VAL_TYPE(match_values) = DB1_STR;
	VAL_NULL(match_values) = 0;
	VAL_STR(match_values) = _c->aor;
        op[0]=OP_EQ;

	if (use_location_pcscf_table(_c->domain) < 0) {
		LM_ERR("Error trying to use table %.*s\n", _c->domain->len, _c->domain->s);
		return -1;
	}

	VAL_TYPE(values) = DB1_INT;
	VAL_TIME(values) = _s?_s->type:0;
	VAL_NULL(values) = 0;

	switch (_t) {
	case SECURITY_IPSEC: {
		ipsec_t* ipsec = _s?_s->data.ipsec:0;
		str s_empty = {0,0};
		int i = 1;
		VAL_TYPE(values + i) = DB1_STR;
		VAL_NULL(values + i) = ipsec?0:1;
		VAL_STR(values + i) = ipsec?ipsec->prot:s_empty;

		VAL_TYPE(values + ++i) = DB1_STR;
		VAL_NULL(values + i) = ipsec?0:1;
		VAL_STR(values + i) = ipsec?ipsec->mod:s_empty;

		VAL_TYPE(values + ++i) = DB1_STR;
		VAL_NULL(values + i) = ipsec?0:1;
		VAL_STR(values + i) = ipsec?ipsec->ck:s_empty;
		VAL_TYPE(values + ++i) = DB1_STR;
		VAL_NULL(values + i) = ipsec?0:1;
		VAL_STR(values + i) = ipsec?ipsec->ik:s_empty;
		VAL_TYPE(values + ++i) = DB1_STR;
		VAL_NULL(values + i) = ipsec?0:1;
		VAL_STR(values + i) = ipsec?ipsec->ealg:s_empty;
		VAL_TYPE(values + ++i) = DB1_STR;
		VAL_NULL(values + i) = ipsec?0:1;
		VAL_STR(values + i) = ipsec?ipsec->alg:s_empty;
		VAL_TYPE(values + ++i) = DB1_INT;
		VAL_NULL(values + i) = ipsec?0:1;
		VAL_INT(values + i) = ipsec?ipsec->port_uc:0;
		VAL_TYPE(values + ++i) = DB1_INT;
		VAL_NULL(values + i) = ipsec?0:1;
		VAL_INT(values + i) = ipsec?ipsec->port_us:0;
		VAL_TYPE(values + ++i) = DB1_BIGINT;
		VAL_NULL(values + i) = ipsec?0:1;
		VAL_BIGINT(values + i) = ipsec?ipsec->spi_pc:0;
		VAL_TYPE(values + ++i) = DB1_BIGINT;
		VAL_NULL(values + i) = ipsec?0:1;
		VAL_BIGINT(values + i) = ipsec?ipsec->spi_ps:0;
		VAL_TYPE(values + ++i) = DB1_BIGINT;
		VAL_NULL(values + i) = ipsec?0:1;
		VAL_BIGINT(values + i) = ipsec?ipsec->spi_uc:0;
		VAL_TYPE(values + ++i) = DB1_BIGINT;
		VAL_NULL(values + i) = ipsec?0:1;
		VAL_BIGINT(values + i) = ipsec?ipsec->spi_us:0;

		if ((ul_dbf.update(ul_dbh, match_keys, op, match_values, update_keys,
				values, 1, 13)) != 0) {
			LM_ERR("could not update database info\n");
			return -1;
		}

		if (ul_dbf.affected_rows && ul_dbf.affected_rows(ul_dbh) == 0) {
			LM_DBG("no existing rows for an update... doing insert\n");
			if (db_insert_pcontact(_c) != 0) {
				LM_ERR("Failed to insert a pcontact on update\n");
			}
		}
		break;
	}
	default:
		LM_WARN("not yet implemented or unknown security type\n");
		return -1;
	}

	return 0;
}

int db_update_pcontact_security(struct pcontact* _c, security_type _t, security_t* _s) {
	db_val_t match_values[1];
	db_key_t match_keys[1] = { &aor_col };
        db_op_t op[1];
        
	db_key_t update_keys[13] = { &security_type_col, &protocol_col,
			&mode_col, &ck_col, &ik_col, &ealg_col, &ialg_col, &port_uc_col,
			&port_us_col, &spi_pc_col, &spi_ps_col, &spi_uc_col, &spi_us_col };
	db_val_t values[13];

	LM_DBG("updating security for pcontact: %.*s\n", _c->aor.len, _c->aor.s);

	VAL_TYPE(match_values) = DB1_STR;
	VAL_NULL(match_values) = 0;
	VAL_STR(match_values) = _c->aor;
        op[0]=OP_EQ;

	if (use_location_pcscf_table(_c->domain) < 0) {
		LM_ERR("Error trying to use table %.*s\n", _c->domain->len, _c->domain->s);
		return -1;
	}

	VAL_TYPE(values) = DB1_INT;
	VAL_TIME(values) = _s?_s->type:0;
	VAL_NULL(values) = 0;
        

	switch (_t) {
	case SECURITY_IPSEC: {
		ipsec_t* ipsec = _s?_s->data.ipsec:0;
		int i = 1;
		str s_empty = {0,0};
		VAL_TYPE(values + i) = DB1_STR;
		VAL_NULL(values + i) = ipsec?0:1;
		VAL_STR(values + i) = ipsec?ipsec->prot:s_empty;

		VAL_TYPE(values + ++i) = DB1_STR;
		VAL_NULL(values + i) = ipsec?0:1;
		VAL_STR(values + i) = ipsec?ipsec->mod:s_empty;

		VAL_TYPE(values + ++i) = DB1_STR;
		VAL_NULL(values + i) = ipsec?0:1;
		VAL_STR(values + i) = ipsec?ipsec->ck:s_empty;

		VAL_TYPE(values + ++i) = DB1_STR;
		VAL_NULL(values + i) = ipsec?0:1;
		VAL_STR(values + i) = ipsec?ipsec->ik:s_empty;

		VAL_TYPE(values + ++i) = DB1_STR;
		VAL_NULL(values + i) = ipsec?0:1;
		VAL_STR(values + i) = ipsec?ipsec->ealg:s_empty;

		VAL_TYPE(values + ++i) = DB1_STR;
		VAL_NULL(values + i) = ipsec?0:1;
		VAL_STR(values + i) = ipsec?ipsec->alg:s_empty;

		VAL_TYPE(values + ++i) = DB1_INT;
		VAL_NULL(values + i) = ipsec?0:1;
		VAL_INT(values + i) = ipsec?ipsec->port_uc:0;

		VAL_TYPE(values + ++i) = DB1_INT;
		VAL_NULL(values + i) = ipsec?0:1;
		VAL_INT(values + i) = ipsec?ipsec->port_us:0;

		VAL_TYPE(values + ++i) = DB1_BIGINT;
		VAL_NULL(values + i) = ipsec?0:1;
		VAL_BIGINT(values + i) = ipsec?ipsec->spi_pc:0;

		VAL_TYPE(values + ++i) = DB1_BIGINT;
		VAL_NULL(values + i) = ipsec?0:1;
		VAL_BIGINT(values + i) = ipsec?ipsec->spi_ps:0;

		VAL_TYPE(values + ++i) = DB1_BIGINT;
		VAL_NULL(values + i) = ipsec?0:1;
		VAL_BIGINT(values + i) = ipsec?ipsec->spi_uc:0;

		VAL_TYPE(values + ++i) = DB1_BIGINT;
		VAL_NULL(values + i) = ipsec?0:1;
		VAL_BIGINT(values + i) = ipsec?ipsec->spi_us:0;

		if ((ul_dbf.update(ul_dbh, match_keys, op, match_values, update_keys,
				values, 1, 13)) != 0) {
			LM_ERR("could not update database info\n");
			return -1;
		}

		if (ul_dbf.affected_rows && ul_dbf.affected_rows(ul_dbh) == 0) {
			LM_DBG("no existing rows for an update... doing insert\n");
			if (db_insert_pcontact(_c) != 0) {
				LM_ERR("Failed to insert a pcontact on update\n");
			}
		}
		break;
	}
	default:
		LM_WARN("not yet implemented or unknown security type\n");
		return -1;
	}

	return 0;
}

/* take a contact structure and a pointer to some memory and returns a list of public identities in the format
 * <impu1><impu2>....<impu(n)>
 * make sure p already has memory allocated
 * returns the length of the string (list)
 * the string list itself will be available in p
 */
int impus_as_string(struct pcontact* _c, t_reusable_buffer* buffer) {
	ppublic_t* impu;
	int len = 0;
	char *p;

	impu = _c->head;
	while (impu) {
		len += 2 + impu->public_identity.len;
		impu = impu->next;
	}

	if (!buffer->buf || buffer->buf_len == 0 || len > buffer->buf_len) {
		if (buffer->buf) {
			pkg_free(buffer->buf);
		}
		buffer->buf = (char*) pkg_malloc(len);
		if (!buffer->buf) {
			LM_CRIT("unable to allocate pkg memory\n");
			return 0;
		}
		buffer->buf_len = len;
	}

	impu = _c->head;
	p = buffer->buf;
	while (impu) {
		*p++ = '<';
		memcpy(p, impu->public_identity.s, impu->public_identity.len);
		p += impu->public_identity.len;
		*p++ = '>';
		impu = impu->next;
	}

	return len;
}

/* take a contact structure and a pointer to some memory and returns a list of public identities in the format
 * <impu1><impu2>....<impu(n)>
 * make sure p already has memory allocated
 * returns the length of the string (list)
 * the string list itself will be available in p
 */
int service_routes_as_string(struct pcontact* _c, t_reusable_buffer *buffer) {
	int i;
	int len = 0;
	char *p;
	for (i=0; i<_c->num_service_routes; i++) {
		len += 2 + _c->service_routes[i].len;
	}

	if (!buffer->buf || buffer->buf_len==0 || len > buffer->buf_len) {
		if (buffer->buf) {
			pkg_free(buffer->buf);
		}
		buffer->buf = (char*)pkg_malloc(len);
		if (!buffer->buf) {
			LM_CRIT("unable to allocate pkg memory\n");
			return 0;
		}
		buffer->buf_len = len;
	}

	p = buffer->buf;
	for (i=0; i<_c->num_service_routes; i++) {
		*p = '<';
		p++;
		memcpy(p, _c->service_routes[i].s, _c->service_routes[i].len);
		p+=_c->service_routes[i].len;
		*p = '>';
		p++;
	}

	return len;
}

void free_service_route_buf(void)
{
	if (service_route_buffer.buf) {
		pkg_free(service_route_buffer.buf);
		service_route_buffer.data_len = 0;
		service_route_buffer.buf_len = 0;
		service_route_buffer.buf = 0;
	}
}

void free_impu_buf(void) {
	if (impu_buffer.buf) {
		pkg_free(impu_buffer.buf);
		impu_buffer.data_len = 0;
		impu_buffer.buf_len = 0;
		impu_buffer.buf = 0;
	}
}

