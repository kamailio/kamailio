/*
 * usrloc_db.h
 *
 *  Created on: Nov 11, 2013
 *      Author: carlos
 */

#ifndef USRLOC_DB_H_
#define USRLOC_DB_H_

typedef enum location_pcscf_fields_idx {
//	LP_ID_IDX = 0,
	LP_DOMAIN_IDX = 0,
	LP_AOR_IDX,
	LP_CONTACT_IDX,
	LP_RECEIVED_IDX,
	LP_RECEIVED_PORT_IDX,
	LP_RECEIVED_PROTO_IDX,
	LP_PATH_IDX,
	LP_RX_SESSION_ID_IDX,
	LP_REG_STATE_IDX,
	LP_EXPIRES_IDX,
	LP_SERVICE_ROUTES_IDX,
	LP_SOCKET_IDX,
	LP_PUBLIC_IPS_IDX,

} location_pcscf_fields_idx_t;

#define GET_FIELD_IDX(_val, _idx)\
						(_val + _idx)

#define SET_PROPER_NULL_FLAG(_str, _vals, _index)\
	do{\
		if( (_str).len == 0)\
			VAL_NULL( (_vals)+(_index) ) = 1;\
		else\
			VAL_NULL( (_vals)+(_index) ) = 0;\
	}while(0);

#define SET_STR_VALUE(_val, _str)\
	do{\
			VAL_STR((_val)).s 		= (_str).s;\
			VAL_STR((_val)).len 	= (_str).len;\
	}while(0);

#define SET_NULL_FLAG(_vals, _i, _max, _flag)\
	do{\
		for((_i) = 0;(_i)<(_max); (_i)++)\
			VAL_NULL((_vals)+(_i)) = (_flag);\
	}while(0);


#define ID_COL				"id"
#define DOMAIN_COL			"domain"
#define AOR_COL				"aor"
#define CONTACT_COL			"contact"
#define RECEIVED_COL		"received"
#define RECEIVED_PORT_COL	"received_port"
#define RECEIVED_PROTO_COL	"received_proto"
#define PATH_COL			"path"
#define RX_SESSION_ID_COL	"rx_session_id"
#define REG_STATE_COL		"reg_state"
#define EXPIRES_COL			"expires"
#define SERVICE_ROUTES_COL	"service_routes"
#define SOCKET_COL			"socket"
#define PUBLIC_IDS_COL		"public_ids"
#define SECURITY_TYPE_COL	"security_type"
#define PROTOCOL_COL		"protocol"
#define MODE_COL			"mode"
#define CK_COL				"ck"
#define IK_COL				"ik"
#define EALG_COL			"ealg"
#define IALG_COL			"ialg"
#define PORTUC_COL			"port_uc"
#define PORTUS_COL			"port_us"
#define SPIPC_COL			"spi_pc"
#define SPIPS_COL			"spi_ps"
#define SPIUC_COL			"spi_uc"
#define SPIUS_COL			"spi_us"
#define T_SECURITY_TYPE_COL	"t_security_type"
#define T_PROTOCOL_COL		"t_protocol"
#define T_MODE_COL			"t_mode"
#define T_CK_COL			"t_ck"
#define T_IK_COL			"t_ik"
#define T_EALG_COL			"t_ealg"
#define T_IALG_COL			"t_ialg"
#define T_PORTUC_COL		"t_port_uc"
#define T_PORTUS_COL		"t_port_us"
#define T_SPIPC_COL			"t_spi_pc"
#define T_SPIPS_COL			"t_spi_ps"
#define T_SPIUC_COL			"t_spi_uc"
#define T_SPIUS_COL			"t_spi_us"

extern db1_con_t* ul_dbh;
extern db_func_t ul_dbf;

extern str id_col;
extern str domain_col;
extern str aor_col;
extern str contact_col;
extern str received_col;
extern str received_port_col;
extern str received_proto_col;
extern str path_col;
extern str rx_session_id_col;
extern str reg_state_col;
extern str expires_col;
extern str service_routes_col;
extern str socket_col;
extern str public_ids_col;

typedef struct reusable_buffer{
	char* buf;
	int buf_len;
	int data_len;
} t_reusable_buffer;

int use_location_pcscf_table();
void destroy_db();
int init_db(const str *db_url, int db_update_period, int fetch_num_rows);
int connect_db(const str *db_url);

int impus_as_string(struct pcontact* _c, t_reusable_buffer* buffer);
int service_routes_as_string(struct pcontact* _c, t_reusable_buffer *buffer);
void free_service_route_buf(void);
void free_impu_buf(void);

int db_insert_pcontact(pcontact_t* _c);
int db_delete_pcontact(pcontact_t* _c);
int db_update_pcontact(pcontact_t* _c);
int db_update_pcontact_security_temp(struct pcontact* _c, security_type _t, security_t* _s);
int db_update_pcontact_security(struct pcontact* _c, security_type _t, security_t* _s);

#endif /* USRLOC_DB_H_ */
