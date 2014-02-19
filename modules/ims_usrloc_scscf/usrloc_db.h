#ifndef USRLOC_DB_H_
#define USRLOC_DB_H_

extern db1_con_t* ul_dbh;
extern db_func_t ul_dbf;

#define ID_COL 			"id"
#define CONTACT_ID_COL "contact_id"
#define IMPU_ID_COL    "impu_id"
#define IMPU_COL       "impu"
#define USER_COL       "username"
#define DOMAIN_COL     "domain"
#define CONTACT_COL    "contact"
#define EXPIRES_COL    "expires"
#define Q_COL          "q"
#define CALLID_COL     "callid"
#define CSEQ_COL       "cseq"
#define FLAGS_COL      "flags"
#define CFLAGS_COL     "cflags"
#define USER_AGENT_COL "user_agent"
#define RECEIVED_COL   "received"
#define PATH_COL       "path"
#define SOCK_COL       "socket"
#define METHODS_COL    "methods"
#define LAST_MOD_COL   "last_modified"
#define REGSTATE_COL   "reg_state"
#define BARRING_COL    "barring"
#define CCF1_COL       "ccf1"
#define CCF2_COL       "ccf2"
#define ECF1_COL       "ecf1"
#define ECF2_COL       "ecf2"
#define IMS_SUB_COL    "ims_subscription_data"

int init_db(const str *db_url, int db_update_period, int fetch_num_rows);
int connect_db(const str *db_url);
void destroy_db();
int use_location_pcscf_table(str* domain);

int db_insert_impurecord(struct udomain* _d, str* public_identity, int reg_state, int barring,
		ims_subscription** s, str* ccf1, str* ccf2, str* ecf1, str* ecf2,
		struct impurecord** _r);
int db_delete_impurecord(udomain_t* _d, struct impurecord* _r);
int db_insert_ucontact(impurecord_t* _r, ucontact_t* _c);
int db_delete_ucontact(impurecord_t* _r, ucontact_t* _c);

int preload_udomain(db1_con_t* _c, udomain_t* _d);

#endif /* USRLOC_DB_H_ */
