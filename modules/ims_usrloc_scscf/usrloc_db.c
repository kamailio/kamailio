#include "../../lib/srdb1/db.h"
#include "usrloc.h"
#include "usrloc_db.h"
#include "bin_utils.h"
#include "udomain.h"
#include "math.h"

str id_col = str_init(ID_COL); /*!< Name of column containing ID (gen. auto_increment field */
str impu_id_col = str_init(IMPU_ID_COL); /*!< Name of column containing impu ID in mapping table */
str contact_id_col = str_init(CONTACT_ID_COL); /*!< Name of column containing contact ID in mapping table */
str impu_col = str_init(IMPU_COL); /*!< Name of column containing impu in impu table */
str reg_state_col = str_init(REGSTATE_COL); /*!< Name of column containing reg state for aor */
str barring_col = str_init(BARRING_COL); /*!< Name of column containing aor barring */
str ccf1_col = str_init(CCF1_COL); /*!< Name of column containing ccf1 */
str ccf2_col = str_init(CCF2_COL); /*!< Name of column containing ccf2 */
str ecf1_col = str_init(ECF1_COL); /*!< Name of column containing ecf1 */
str ecf2_col = str_init(ECF2_COL); /*!< Name of column containing ecf2 */
str ims_sub_data_col = str_init(IMS_SUB_COL); /*!< Name of column containing ims_subscription data */
str contact_col = str_init(CONTACT_COL); /*!< Name of column containing contact addresses */
str expires_col = str_init(EXPIRES_COL); /*!< Name of column containing expires values */
str q_col = str_init(Q_COL); /*!< Name of column containing q values */
str callid_col = str_init(CALLID_COL); /*!< Name of column containing callid string */
str cseq_col = str_init(CSEQ_COL); /*!< Name of column containing cseq values */
str flags_col = str_init(FLAGS_COL); /*!< Name of column containing internal flags */
str cflags_col = str_init(CFLAGS_COL); /*!< Name of column containing contact flags */
str user_agent_col = str_init(USER_AGENT_COL); /*!< Name of column containing user agent string */
str received_col = str_init(RECEIVED_COL); /*!< Name of column containing transport info of REGISTER */
str path_col = str_init(PATH_COL); /*!< Name of column containing the Path header */
str sock_col = str_init(SOCK_COL); /*!< Name of column containing the received socket */
str methods_col = str_init(METHODS_COL); /*!< Name of column containing the supported methods */
str last_mod_col = str_init(LAST_MOD_COL); /*!< Name of column containing the last modified date */

str id_column 			= { "id", 2 };
str impu_table 			= { "impu", 4 };
str contact_table 		= { "contact", 7 };
str impu_contact_table 	= { "impu_contact", 12 };
str query_buffer 		= { 0, 0 };
int query_buffer_len 	= 0;

extern db1_con_t* ul_dbh;
extern db_func_t ul_dbf;
extern int ul_fetch_rows;

int init_db(const str *db_url, int db_update_period, int fetch_num_rows) {
	/* Find a database module */
	if (db_bind_mod(db_url, &ul_dbf) < 0) {
		LM_ERR("Unable to bind to a database driver\n");
		return -1;
	}

	if (connect_db(db_url) != 0) {
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

int connect_db(const str *db_url) {
	if (ul_dbh) { /* we've obviously already connected... */
		LM_WARN("DB connection already open... continuing\n");
		return 0;
	}

	if ((ul_dbh = ul_dbf.init(db_url)) == 0)
		return -1;

	LM_DBG("Successfully connected to DB and returned DB handle ptr %p\n", ul_dbh);
	return 0;
}

void destroy_db() {
	/* close the DB connection */
	if (ul_dbh) {
		ul_dbf.close(ul_dbh);
		ul_dbh = 0;
	}
}

int use_location_scscf_table(str* domain) {
	if (!ul_dbh) {
		LM_ERR("invalid database handle\n");
		return -1;
	}

	if (ul_dbf.use_table(ul_dbh, domain) < 0) {
		LM_ERR("Error in use_table\n");
		return -1;
	}

	return 0;
}

int db_insert_impurecord(struct udomain* _d, str* public_identity,
		int reg_state, int barring, ims_subscription** s, str* ccf1, str* ccf2,
		str* ecf1, str* ecf2, struct impurecord** _r) {
	int i;
	bin_data x;
	db_key_t key[8];
	db_val_t val[8];
	str bin_str;

	//serialise ims_subscription
	if (!bin_alloc(&x, 256)) {
		LM_DBG("unable to allocate buffer for binary serialisation\n");
		return -1;
	}
	if (!bin_encode_ims_subscription(&x, (*s))) {
		LM_DBG("Unable to serialise ims_subscription data\n");
		bin_free(&x);
		return -1;
	}
	bin_str.s = x.s;
	bin_str.len = x.len;

	key[0] = &impu_col;
	key[1] = &barring_col;
	key[2] = &reg_state_col;

	val[0].type = DB1_STR;
	val[0].nul = 0;
	val[0].val.str_val = *public_identity;
	val[1].type = DB1_INT;
	val[1].nul = 0;
	val[1].val.int_val = barring;
	val[2].type = DB1_INT;
	val[2].nul = 0;
	val[2].val.int_val = reg_state;

	i = 3;

	if (ccf1 && ccf1->s && ccf1->len >= 0) {
		key[i] = &ccf1_col;
		val[i].type = DB1_STR;
		val[i].nul = 0;
		val[i].val.str_val = *ccf1;
		i++;
	}
	if (ecf1 && ecf1->s && ecf1->len >= 0) {
		key[i] = &ecf1_col;
		val[i].type = DB1_STR;
		val[i].nul = 0;
		val[i].val.str_val = *ecf1;
		i++;
	}
	if (ccf2 && ccf2->s && ccf2->len >= 0) {
		key[i] = &ccf2_col;
		val[i].type = DB1_STR;
		val[i].nul = 0;
		val[i].val.str_val = *ccf2;
		i++;
	}
	if (ecf2 && ecf2->s && ecf2->len >= 0) {
		key[i] = &ecf2_col;
		val[i].type = DB1_STR;
		val[i].nul = 0;
		val[i].val.str_val = *ecf2;
		i++;
	}
	key[i] = &ims_sub_data_col;
	val[i].type = DB1_BLOB;
	val[i].nul = 0;
	val[i].val.blob_val = bin_str;
	i++;

	if (ul_dbf.use_table(ul_dbh, &impu_table) != 0) {
		LM_ERR("Unable to use table [%.*s]\n", impu_table.len, impu_table.s);
		bin_free(&x);
		return -1;
	}
	if (ul_dbf.insert_update(ul_dbh, key, val, i) != 0) {
		LM_ERR("Unable to insert impu into table [%.*s]\n", public_identity->len, public_identity->s);
		bin_free(&x);
		return -1;
	}
	bin_free(&x);

	return 0;
}

int db_delete_impurecord(udomain_t* _d, struct impurecord* _r) {
	db_key_t key[1];
	db_val_t val[1];

	key[0] = &impu_col;
	val[0].type = DB1_STR;
	val[0].nul = 0;
	val[0].val.str_val = _r->public_identity;

	ul_dbf.use_table(ul_dbh, &impu_table);
	ul_dbf.delete(ul_dbh, key, 0, val, 1);

	return 0;
}

int db_insert_ucontact(impurecord_t* _r, ucontact_t* _c) {
	db1_res_t* _rs;
	int contact_id;
	int impu_id;
	db_key_t key[6];
	db_val_t val[6];
	db_key_t key_return[1];
	db_val_t* ret_val;

	key_return[0] = &id_column;

	key[0] = &contact_col;
	key[1] = &path_col;
	key[2] = &user_agent_col;
	key[3] = &received_col;
	key[4] = &expires_col;
	key[5] = &callid_col;

	val[0].type = DB1_STR;
	val[0].nul = 0;
	val[0].val.str_val = _c->c;

	val[1].type = DB1_STR;
	val[1].nul = 0;
	val[1].val.str_val = _c->path;

	val[2].type = DB1_STR;
	val[2].nul = 0;
	val[2].val.str_val = _c->user_agent;

	val[3].type = DB1_STR;
	val[3].nul = 0;
	val[3].val.str_val = _c->received;

	val[4].type = DB1_DATETIME;
	val[4].nul = 0;
	val[4].val.time_val = _c->expires;

	val[5].type = DB1_STR;
	val[5].nul = 0;
	val[5].val.str_val = _c->callid;

	if (ul_dbf.use_table(ul_dbh, &contact_table) != 0) {
		LM_ERR("Unable to use table [%.*s]\n", contact_table.len, contact_table.s);
		return -1;
	}
	if (ul_dbf.insert_update(ul_dbh, key, val, 6) != 0) {
		LM_ERR("Failed to insert/update contact record for [%.*s]\n", _c->c.len, _c->c.s);
		return -1;
	}
	contact_id = ul_dbf.last_inserted_id(ul_dbh);
	LM_DBG("contact ID is %d\n", contact_id);

	/* search for ID of the associated IMPU */
	key[0] = &impu_col;
	val[0].nul = 0;
	val[0].type = DB1_STR;
	val[0].val.str_val = _r->public_identity;

	if (ul_dbf.use_table(ul_dbh, &impu_table) != 0) {
		LM_ERR("Unable to use table [%.*s]\n", impu_table.len, impu_table.s);
		return -1;
	}
	if (ul_dbf.query(ul_dbh, key, 0, val, key_return, 1, 1, NULL, &_rs) != 0) {
		LM_ERR("Unable to find IMPU [%.*s] in DB to complete IMPU-contact mapping\n", _r->public_identity.len, _r->public_identity.s);
		return -1;
	}
	if (RES_ROW_N(_rs) == 0) {
		LM_DBG("IMPU %.*s not found in DB\n", _r->public_identity.len, _r->public_identity.s);
		ul_dbf.free_result(ul_dbh, _rs);
		return -1;
	}

	if (RES_ROW_N(_rs) > 1) {
		LM_WARN("more than one IMPU found in DB for contact [%.*s] - this should not happen... proceeding with first entry\n",
				_r->public_identity.len, _r->public_identity.s);
	}
	ret_val = ROW_VALUES(RES_ROWS(_rs));
	impu_id = ret_val[0].val.int_val;

	ul_dbf.free_result(ul_dbh, _rs);
	LM_DBG("IMPU ID is %d\n", impu_id);

	/* update mapping table between contact and IMPU */
	key[0] = &impu_id_col;
	key[1] = &contact_id_col;
	val[0].nul = 0;
	val[0].type = DB1_INT;
	val[0].val.int_val = impu_id;
	val[1].nul = 0;
	val[1].type = DB1_INT;
	val[1].val.int_val = contact_id;

	if (ul_dbf.use_table(ul_dbh, &impu_contact_table) != 0) {
		LM_ERR("Unable to use table [%.*s]\n", impu_table.len, impu_table.s);
		return -1;
	}

	if (ul_dbf.insert_update(ul_dbh, key, val, 2) != 0) {
		LM_ERR("Failed to insert/update impu-contact mapping record for contact [%.*s] and impu [%.*s]\n",
				_c->c.len, _c->c.s,
				_r->public_identity.len, _r->public_identity.s);
		return -1;
	}

	return 0;
}

int db_delete_ucontact(impurecord_t* _r, ucontact_t* _c) {
	db_key_t key[2];
	db_val_t val[2];
	db_key_t key_return[1];
	db_val_t* ret_val;
	db1_res_t* _rs;
	int impu_id, contact_id;

	LM_DBG("Deleting contact binding [%.*s] on impu [%.*s]\n",
			_c->c.len, _c->c.s,
			_r->public_identity.len, _r->public_identity.s);

	/* get id of IMPU entry */
	key[0] = &impu_col;
	val[0].type = DB1_STR;
	val[0].nul = 0;
	val[0].val.str_val = _r->public_identity;
	key_return[0] = &id_column;

	if (ul_dbf.use_table(ul_dbh, &impu_table) != 0) {
		LM_ERR("Unable to use table [%.*s]\n", impu_table.len, impu_table.s);
		return -1;
	}
	if (ul_dbf.query(ul_dbh, key, 0, val, key_return, 1, 1, NULL, &_rs) != 0) {
		LM_ERR("Unable to find IMPU [%.*s] in DB to complete IMPU-contact mapping\n", _r->public_identity.len, _r->public_identity.s);
		return -1;
	}
	if (RES_ROW_N(_rs) == 0) {
		LM_DBG("IMPU %.*s not found in DB\n", _r->public_identity.len, _r->public_identity.s);
		ul_dbf.free_result(ul_dbh, _rs);
		return -1;
	}
	if (RES_ROW_N(_rs) > 1) {
		LM_WARN("more than one IMPU found in DB for contact [%.*s] - this should not happen... proceeding with first entry\n",
				_r->public_identity.len, _r->public_identity.s);
	}
	ret_val = ROW_VALUES(RES_ROWS(_rs));
	impu_id = ret_val[0].val.int_val;

	ul_dbf.free_result(ul_dbh, _rs);
	LM_DBG("IMPU ID is %d\n", impu_id);

	/* get contact id from DB */
	if (ul_dbf.use_table(ul_dbh, &contact_table) != 0) {
		LM_ERR("Unable to use table [%.*s]\n", contact_table.len, contact_table.s);
		return -1;
	}
	key[0] = &contact_col;
	val[0].type = DB1_STR;
	val[0].nul = 0;
	val[0].val.str_val = _c->c;
	if (ul_dbf.query(ul_dbh, key, 0, val, key_return, 1, 1, NULL, &_rs) != 0) {
		LM_ERR("Unable to find contact [%.*s] in DB to complete IMPU-contact mapping removal\n", _c->c.len, _c->c.s);
		return -1;
	}
	if (RES_ROW_N(_rs) == 0) {
		LM_DBG("Contact %.*s not found in DB\n",_c->c.len, _c->c.s);
		ul_dbf.free_result(ul_dbh, _rs);
		return -1;
	}
	if (RES_ROW_N(_rs) > 1) {
		LM_WARN("more than one contact found in DB for contact [%.*s] - this should not happen... proceeding with first entry\n",
				_c->c.len, _c->c.s);
	}
	ret_val = ROW_VALUES(RES_ROWS(_rs));
	contact_id = ret_val[0].val.int_val;
	ul_dbf.free_result(ul_dbh, _rs);
	LM_DBG("contact ID is %d\n", contact_id);

	LM_DBG("need to remove contact-impu mapping %d:%d\n", impu_id, contact_id);

	/* update impu-contact mapping table */
	if (ul_dbf.use_table(ul_dbh, &impu_contact_table) != 0) {
		LM_ERR("Unable to use table [%.*s]\n", impu_contact_table.len, impu_contact_table.s);
		return -1;
	}
	key[0] = &contact_id_col;
	key[1] = &impu_id_col;
	val[0].type = DB1_INT;
	val[0].nul = 0;
	val[0].val.int_val = contact_id;
	val[1].type = DB1_INT;
	val[1].nul = 0;
	val[1].val.int_val = impu_id;

	if (ul_dbf.delete(ul_dbh, key, 0, val, 2) != 0) {
		LM_ERR("unable to remove impu-contact mapping from DB for contact [%.*s], impu [%.*s]  ..... continuing\n",
				_c->c.len, _c->c.s,
				_r->public_identity.len, _r->public_identity.s);
	}

	/* delete contact from contact table - IFF there are no more mappings for it to impus */
	if (ul_dbf.query(ul_dbh, key, 0, val, key_return, 1, 1, NULL, &_rs) != 0) {
		LM_WARN("error searching for impu-contact mappings in DB\n");
	}
	if (RES_ROW_N(_rs) > 0) {
		ul_dbf.free_result(ul_dbh, _rs);
		LM_DBG("impu-contact mappings still exist, not removing contact from DB\n");
		return 0;
	}
	ul_dbf.free_result(ul_dbh, _rs);

	key[0] = &contact_col;
	val[0].type = DB1_STR;
	val[0].nul = 0;
	val[0].val.str_val = _c->c;

	if (ul_dbf.use_table(ul_dbh, &contact_table) != 0) {
		LM_ERR("Unable to use table [%.*s]\n", contact_table.len, contact_table.s);
		return -1;
	}

	if (ul_dbf.delete(ul_dbh, key, 0, val, 1) != 0) {
		LM_ERR("unable to remove contact from DB [%.*s]\n", _c->c.len, _c->c.s);
	}

	return 0;
}

int inline int_to_str_len(int i) {
	if (i < 0)
		i = -i;
	if (i < 10)
		return 1;
	if (i < 100)
		return 2;
	if (i < 1000)
		return 3;
	if (i < 10000)
		return 4;
	if (i < 100000)
		return 5;
	if (i < 1000000)
		return 6;
	if (i < 10000000)
		return 7;
	if (i < 100000000)
		return 8;
	if (i < 1000000000)
		return 9;
	return 10;
}

static inline int dbrow2contact(db_val_t* val, ucontact_info_t* ci) {
	static str path, user_agent, callid;

	/* path */
	if (!VAL_NULL(val + 1)) {
		path.s = (char*)VAL_STRING(val + 1);
		path.len = strlen(path.s);
	}
	ci->path = &path;

	/* user-agent */
	if (!VAL_NULL(val + 2)) {
		user_agent.s = (char*)VAL_STRING(val + 2);
		user_agent.len = strlen(user_agent.s);
	}
	ci->user_agent = &user_agent;

	/* received */
	if (!VAL_NULL(val + 3)) {
		ci->received.s = (char*)VAL_STRING(val + 3);
		ci->received.len = strlen(ci->received.s);
	}

	/* expires */
	if (!VAL_NULL(val + 4)) {
		ci->expires = VAL_TIME(val + 4);
	}
	/* callid */
	if (!VAL_NULL(val + 5)) {
		callid.s = (char*) VAL_STRING(val + 5);
		callid.len = strlen(callid.s);
	}
	ci->callid = &callid;

	return 0;
}

int preload_udomain(db1_con_t* _c, udomain_t* _d) {
	db_key_t col[9];
	db_row_t* row;
	db_row_t* contact_row;
	db1_res_t* rs;
	db1_res_t* contact_rs;
	db_val_t* vals;
	db_val_t* contact_vals;
	int barring = 0, reg_state = 0, impu_id, n, nn, i, j, len;
	str query, impu, ccf1 = { 0, 0 }, ecf1 = { 0, 0 }, ccf2 = { 0, 0 }, ecf2 = {
			0, 0 }, blob = { 0, 0 }, contact={0,0};
	bin_data x;
	ims_subscription* subscription = 0;
	impurecord_t* impurecord;
	int impu_id_len;
	ucontact_t* c;
	ucontact_info_t contact_data;

	/*
	 * the two queries - get the IMPUs, then get associated contacts for each IMPU:
	 * SELECT impu.impu,impu.barring,impu.reg_state,impu.ccf1,impu.ccf2,impu.ecf1,impu.ecf2,impu.ims_subscription_data FROM impu;
	 * SELECT c.contact,c.path,c.user_agent,c.received,c.expires FROM impu_contact m LEFT JOIN contact c ON c.id=m.contact_id WHERE m.impu_id=20;
	 */

	char *p =
			"SELECT c.contact,c.path,c.user_agent,c.received,c.expires,c.callid FROM impu_contact m LEFT JOIN contact c ON c.id=m.contact_id WHERE m.impu_id=";

	query.s = p;
	query.len = strlen(query.s);

	col[0] = &impu_col;
	col[1] = &barring_col;
	col[2] = &reg_state_col;
	col[3] = &ccf1_col;
	col[4] = &ecf1_col;
	col[5] = &ccf2_col;
	col[6] = &ecf2_col;
	col[7] = &ims_sub_data_col;
	col[8] = &id_col;

	if (ul_dbf.use_table(_c, &impu_table) != 0) {
		LM_ERR("SQL use table failed\n");
		return -1;
	}
	if (ul_dbf.query(_c, NULL, 0, NULL, col, 0, 9, NULL, &rs) != 0) {
		LM_ERR("Unable to query DB to preload S-CSCF usrloc\n");
		return -1;
	}

	if (RES_ROW_N(rs) == 0) {
		LM_DBG("table is empty\n");
		ul_dbf.free_result(_c, rs);
		return 0;
	}

	LM_DBG("preloading S-CSCF usrloc...\n");
	LM_DBG("%d rows returned in preload\n", RES_ROW_N(rs));

	n = 0;
	do {
		LM_DBG("loading S-CSCF usrloc records - cycle [%d]\n", ++n);
		for (i = 0; i < RES_ROW_N(rs); i++) {
			impu_id = -1;

			row = RES_ROWS(rs) + i;
			LM_DBG("Fetching IMPU row %d\n", i+1);
			vals = ROW_VALUES(row);

			impu.s = (char*) VAL_STRING(vals);
			if (VAL_NULL(vals) || !impu.s || !impu.s[0]) {
				impu.len = 0;
				impu.s = 0;
			} else {
				impu.len = strlen(impu.s);
			}
			LM_DBG("IMPU from DB is [%.*s]\n", impu.len, impu.s);
			if (!VAL_NULL(vals + 1)) {
				barring = VAL_INT(vals + 1);
			}
			if (!VAL_NULL(vals + 2)) {
				reg_state = VAL_INT(vals + 2);
			}
			if (!VAL_NULL(vals + 3)) {
				ccf1.s = (char*) VAL_STRING(vals + 3);
				ccf1.len = strlen(ccf1.s);
			}
			LM_DBG("CCF1 from DB is [%.*s]\n", ccf1.len, ccf1.s);
			if (!VAL_NULL(vals + 4)) {
				ecf1.s = (char*) VAL_STRING(vals + 3);
				ecf1.len = strlen(ecf1.s);
			}
			LM_DBG("ECF1 from DB is [%.*s]\n", ecf1.len, ecf1.s);
			if (!VAL_NULL(vals + 5)) {
				ccf2.s = (char*) VAL_STRING(vals + 5);
				ccf2.len = strlen(ccf2.s);
			}
			LM_DBG("CCF2 from DB is [%.*s]\n", ccf2.len, ccf2.s);
			if (!VAL_NULL(vals + 6)) {
				ecf2.s = (char*) VAL_STRING(vals + 6);
				ecf2.len = strlen(ecf2.s);
			}
			LM_DBG("ECF2 from DB is [%.*s]\n", ecf2.len, ecf2.s);

			if (!VAL_NULL(vals + 7)) {
				blob = VAL_BLOB(vals + 7);
				bin_alloc(&x, blob.len);
				memcpy(x.s, blob.s, blob.len);
				x.len = blob.len;
				x.max = 0;
				subscription = bin_decode_ims_subscription(&x);
				bin_free(&x);
			}
			if (!VAL_NULL(vals + 8)) {
				impu_id = VAL_INT(vals + 8);
			}

			/* insert impu into memory */
			lock_udomain(_d, &impu);
			if (mem_insert_impurecord(_d, &impu, reg_state, barring,
					&subscription, &ccf1, &ccf2, &ecf1, &ecf2, &impurecord)
					!= 0) {
				LM_ERR("Unable to insert IMPU into memory [%.*s]\n", impu.len, impu.s);
			}

			/* add contacts */
			if (impu_id < 0) {
				LM_ERR("impu_id has not been set [%.*s] - we cannot read contacts from DB....aborting preload\n", impu.len, impu.s);
				//TODO: check frees
				unlock_udomain(_d, &impu);
				continue;
			}
			impu_id_len = int_to_str_len(impu_id);
			len = query.len + impu_id_len + 1/*nul*/;
			if (!query_buffer_len || query_buffer_len < len) {
				if (query_buffer.s) {
					pkg_free(query_buffer.s);
				}
				query_buffer.s = (char*) pkg_malloc(len);
				if (!query_buffer.s) {
					LM_ERR("mo more pkg mem\n");
					//TODO: check free
					unlock_udomain(_d, &impu);
					return -1;
				}
				query_buffer_len = len;
			}
			memcpy(query_buffer.s, query.s, query.len);
			p = query_buffer.s + query.len;
			snprintf(p, impu_id_len + 1, "%d", impu_id);
			query_buffer.len = query.len + impu_id_len;
			if (ul_dbf.raw_query(_c, &query_buffer, &contact_rs) != 0) {
				LM_ERR("Unable to query DB for contacts associated with impu [%.*s]\n",
						impu.len, impu.s);
				ul_dbf.free_result(_c, contact_rs);
				unlock_udomain(_d, &impu);
				continue;
			}
			if (RES_ROW_N(contact_rs) == 0) {
				LM_DBG("no contacts associated with impu [%.*s]\n",impu.len, impu.s);
				ul_dbf.free_result(_c, contact_rs);
				unlock_udomain(_d, &impu);
				continue;
			}

			nn = 0;
			do {
				LM_DBG("loading S-CSCF contact - cycle [%d]\n", ++nn);
				for (j = 0; j < RES_ROW_N(contact_rs); j++) {
					contact_row = RES_ROWS(contact_rs) + j;
					contact_vals = ROW_VALUES(contact_row);

					if (!VAL_NULL(contact_vals)) {
						contact.s = (char*) VAL_STRING(contact_vals);
						contact.len = strlen(contact.s);
					}
					if (dbrow2contact(contact_vals, &contact_data) != 0) {
						LM_ERR("unable to convert contact row from DB into valid data... moving on\n");
						unlock_udomain(_d, &impu);
						continue;
					}

					if ((c = mem_insert_ucontact(impurecord, &contact, &contact_data)) == 0) {
						LM_ERR("Unable to insert contact [%.*s] for IMPU [%.*s] into memory... continuing...\n",
								contact.len, contact.s,
								impu.len, impu.s);
					}
				}
				if (DB_CAPABILITY(ul_dbf, DB_CAP_FETCH)) {
					if (ul_dbf.fetch_result(_c, &contact_rs, ul_fetch_rows) < 0) {
						LM_ERR("fetching rows failed\n");
						ul_dbf.free_result(_c, contact_rs);
						unlock_udomain(_d, &impu);
						return -1;
					}
				} else {
					break;
				}
			} while (RES_ROW_N(contact_rs) > 0);

			unlock_udomain(_d, &impu);
			ul_dbf.free_result(_c, contact_rs);
		}

		if (DB_CAPABILITY(ul_dbf, DB_CAP_FETCH)) {
			if (ul_dbf.fetch_result(_c, &rs, ul_fetch_rows) < 0) {
				LM_ERR("fetching rows (1) failed\n");
				ul_dbf.free_result(_c, rs);
				return -1;
			}
		} else {
			break;
		}
	} while (RES_ROW_N(rs) > 0);

	ul_dbf.free_result(_c, rs);

	return 0;
}
