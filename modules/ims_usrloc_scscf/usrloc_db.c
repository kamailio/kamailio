#include "../../lib/srdb1/db.h"
#include "usrloc.h"
#include "usrloc_db.h"
#include "bin_utils.h"
#include "udomain.h"
#include "math.h"
#include "subscribe.h"
#include "../../lib/ims/useful_defs.h"
#include "../../parser/parse_param.h"

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
str params_col = str_init(PARAMS_COL); /*!< Name of column containing contact addresses */
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

/*impu table*/
str impu_table = str_init(IMPU_TABLE);

/*contact table*/
str contact_table = str_init(CONTACT_TABLE);

/*impu contact table*/
str impu_contact_table = str_init(IMPU_CONTACT_TABLE);

/*subscriber table*/
str subscriber_table = str_init(SUBSCRIBER_TABLE);
str sub_event_col = str_init(SUB_EVENT_COL);
str sub_expires_col = str_init(SUB_EXPIRES_COL);
str sub_version_col = str_init(SUB_VERSION_COL);
str sub_watcher_uri_col = str_init(SUB_WATCHER_URI_COL);
str sub_watcher_contact_col = str_init(SUB_WATCHER_CONTACT_COL);
str sub_presentity_uri_col = str_init(SUB_PRESENTITY_URI_COL);
str sub_local_cseq_col = str_init(SUB_LOCAL_CSEQ_COL);
str sub_call_id_col = str_init(SUB_CALL_ID_COL);
str sub_from_tag_col = str_init(SUB_FROM_TAG_COL);
str sub_to_tag_col = str_init(SUB_TO_TAG_COL);
str sub_record_route_col = str_init(SUB_RECORD_ROUTE_COL);
str sub_sockinfo_str_col = str_init(SUB_SOCKINFO_STR_COL);

/*impu_subscriber table*/
str impu_subscriber_table = str_init(IMPU_SUBSCRIBER_TABLE);
str subscriber_id_col = str_init(SUBSCRIBER_ID_COL);

str query_buffer 		= { 0, 0 };
int query_buffer_len		= 0;

char* impu_contact_insert_query = "INSERT INTO impu_contact (impu_id, contact_id) (SELECT I.id, C.id FROM impu I, contact C WHERE I.impu='%.*s' and C.contact='%.*s')";
int impu_contact_insert_query_len;
char* impu_contact_delete_query = "DELETE FROM impu_contact WHERE impu_id in (select impu.id from impu where impu.impu='%.*s') AND contact_id in (select contact.id from contact where contact.contact='%.*s')";
int impu_contact_delete_query_len;

char* impu_subscriber_insert_query = "INSERT INTO impu_subscriber (impu_id, subscriber_id) (SELECT I.id, S.id FROM impu I, subscriber S WHERE I.impu='%.*s' and S.event='%.*s' and S.watcher_contact='%.*s' and S.presentity_uri='%.*s')";
int impu_subscriber_insert_query_len;
char* impu_subscriber_delete_query = "DELETE FROM impu_subscriber WHERE impu_id in (select impu.id from impu where impu.impu='%.*s') AND subscriber_id in (select subscriber.id from subscriber where subscriber.event='%.*s' and subscriber.watcher_contact='%.*s' and subscriber.presentity_uri='%.*s')";
int impu_subscriber_delete_query_len;



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
	
	LM_DBG("DB: Inserting/Updating IMPU [%.*s]\n", public_identity->len, public_identity->s);

	//serialise ims_subscription
	if (s) {
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
	}

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
	if (s) {
	    val[i].nul = 0;
	    val[i].val.blob_val = bin_str;
	} else {
	    val[i].nul = 1;
	}
	
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
	
	if (s)
	    bin_free(&x);

	return 0;
}

int db_delete_impurecord(udomain_t* _d, struct impurecord* _r) {
	db_key_t key[1];
	db_val_t val[1];
	
	LM_DBG("DB: deleting IMPU [%.*s]\n", _r->public_identity.len, _r->public_identity.s);

	key[0] = &impu_col;
	val[0].type = DB1_STR;
	val[0].nul = 0;
	val[0].val.str_val = _r->public_identity;

	if (ul_dbf.use_table(ul_dbh, &impu_table) != 0) {
		LM_ERR("Unable to use table [%.*s]\n", impu_table.len, impu_table.s);
		return -1;
	}
	if (ul_dbf.delete(ul_dbh, key, 0, val, 1) != 0) {
		LM_ERR("Unable to delete impu [%.*s] from DB\n", _r->public_identity.len, _r->public_identity.s);
		return -1;
	}
	
	return 0;
}

static int MAX_PARAMS_SIZE = 1000;
static str param_name_and_nody = {"%.*s=%.*s;", 1};
static str param_name_no_body = {"%.*s;", 1};

int db_insert_ucontact(impurecord_t* _r, ucontact_t* _c) {
	
	str param_buf, param_pad;
	param_t * tmp;
	char param_bufc[MAX_PARAMS_SIZE], param_padc[MAX_PARAMS_SIZE];
	param_buf.s = param_bufc;
	param_buf.len = 0;
	param_pad.s = param_padc;
	param_pad.len = 0;
	
	db_key_t key[7];
	db_val_t val[7];
	
	LM_DBG("DB: inserting ucontact [%.*s]\n", _c->c.len, _c->c.s);
	
	tmp = _c->params;
	while (tmp) {
	    if(tmp->body.len > 0) {
		sprintf(param_pad.s, param_name_and_nody.s, tmp->name.len, tmp->name.s, tmp->body.len, tmp->body.s);
	    } else {
		sprintf(param_pad.s, param_name_no_body.s, tmp->name.len, tmp->name.s);
	    }
	    param_pad.len = strlen(param_pad.s);
	    STR_APPEND(param_buf, param_pad);
	    tmp = tmp->next;
	}
	LM_DBG("Converted params to string to insert into db: [%.*s]\n", param_buf.len, param_buf.s);
	

	key[0] = &contact_col;
	key[1] = &params_col;
	key[2] = &path_col;
	key[3] = &user_agent_col;
	key[4] = &received_col;
	key[5] = &expires_col;
	key[6] = &callid_col;

	val[0].type = DB1_STR;
	val[0].nul = 0;
	val[0].val.str_val = _c->c;

	val[1].type = DB1_STR;
	val[1].nul = 0;
	val[1].val.str_val = param_buf;
	
	val[2].type = DB1_STR;
	val[2].nul = 0;
	val[2].val.str_val = _c->path;

	val[3].type = DB1_STR;
	val[3].nul = 0;
	val[3].val.str_val = _c->user_agent;

	val[4].type = DB1_STR;
	val[4].nul = 0;
	val[4].val.str_val = _c->received;

	val[5].type = DB1_DATETIME;
	val[5].nul = 0;
	val[5].val.time_val = _c->expires;

	val[6].type = DB1_STR;
	val[6].nul = 0;
	val[6].val.str_val = _c->callid;

	if (ul_dbf.use_table(ul_dbh, &contact_table) != 0) {
		LM_ERR("Unable to use table [%.*s]\n", contact_table.len, contact_table.s);
		return -1;
	}
	if (ul_dbf.insert_update(ul_dbh, key, val, 7) != 0) {
		LM_ERR("Failed to insert/update contact record for [%.*s]\n", _c->c.len, _c->c.s);
		return -1;
	}

	return 0;
}

int db_delete_ucontact(ucontact_t* _c) {
	db_key_t key[1];
	db_val_t val[1];

	LM_DBG("Deleting ucontact [%.*s]\n",_c->c.len, _c->c.s);

	/* get contact id from DB */
	if (ul_dbf.use_table(ul_dbh, &contact_table) != 0) {
		LM_ERR("Unable to use table [%.*s]\n", contact_table.len, contact_table.s);
		return -1;
	}
	key[0] = &contact_col;
	val[0].type = DB1_STR;
	val[0].nul = 0;
	val[0].val.str_val = _c->c;
	if (ul_dbf.delete(ul_dbh, key, 0, val, 1) != 0) {
		LM_ERR("Unable to delete contact [%.*s] from DB\n", _c->c.len, _c->c.s);
		return -1;
	}

	return 0;
}

int db_insert_subscriber(impurecord_t* _r, reg_subscriber* _reg_subscriber) {
	int col_num = 12;
	db_key_t key[col_num];
	db_val_t val[col_num];
	
	LM_DBG("DB: inserting subscriber [%.*s]\n", _reg_subscriber->presentity_uri.len, _reg_subscriber->presentity_uri.s);
	
	key[0] = &sub_watcher_uri_col;
	key[1] = &sub_watcher_contact_col;
	key[2] = &sub_presentity_uri_col;
	key[3] = &sub_event_col;
	key[4] = &sub_expires_col;
	key[5] = &sub_version_col;
	key[6] = &sub_local_cseq_col;
	key[7] = &sub_call_id_col;
	key[8] = &sub_from_tag_col;
	key[9] = &sub_to_tag_col;
	key[10] = &sub_record_route_col;
	key[11] = &sub_sockinfo_str_col;

	val[0].type = DB1_STR;
	val[0].nul = 0;
	val[0].val.str_val = _reg_subscriber->watcher_uri;

	val[1].type = DB1_STR;
	val[1].nul = 0;
	val[1].val.str_val = _reg_subscriber->watcher_contact;

	val[2].type = DB1_STR;
	val[2].nul = 0;
	val[2].val.str_val = _reg_subscriber->presentity_uri;

	val[3].type = DB1_INT;
	val[3].nul = 0;
	val[3].val.int_val = _reg_subscriber->event;

	val[4].type = DB1_DATETIME;
	val[4].nul = 0;
	val[4].val.time_val = _reg_subscriber->expires;

	val[5].type = DB1_INT;
	val[5].nul = 0;
	val[5].val.int_val = _reg_subscriber->version;
	
	val[6].type = DB1_INT;
	val[6].nul = 0;
	val[6].val.int_val = _reg_subscriber->local_cseq;
	
	val[7].type = DB1_STR;
	val[7].nul = 0;
	val[7].val.str_val = _reg_subscriber->call_id;
	
	val[8].type = DB1_STR;
	val[8].nul = 0;
	val[8].val.str_val = _reg_subscriber->from_tag;
	
	val[9].type = DB1_STR;
	val[9].nul = 0;
	val[9].val.str_val = _reg_subscriber->to_tag;
	
	val[10].type = DB1_STR;
	val[10].nul = 0;
	val[10].val.str_val = _reg_subscriber->record_route;
	
	val[11].type = DB1_STR;
	val[11].nul = 0;
	val[11].val.str_val = _reg_subscriber->sockinfo_str;
	
	if (ul_dbf.use_table(ul_dbh, &subscriber_table) != 0) {
		LM_ERR("Unable to use table [%.*s]\n", subscriber_table.len, subscriber_table.s);
		return -1;
	}
	if (ul_dbf.insert_update(ul_dbh, key, val, col_num) != 0) {
		LM_ERR("Failed to insert/update subscriber record for [%.*s]\n", _reg_subscriber->presentity_uri.len, _reg_subscriber->presentity_uri.s);
		return -1;
	}
	
	return 0;
}


int db_delete_subscriber(impurecord_t* _r, reg_subscriber* _reg_subscriber) {
	db_key_t key[3];
	db_val_t val[3];
	
	LM_DBG("Deleting subscriber binding [%.*s] on impu [%.*s]\n",
			_reg_subscriber->presentity_uri.len, _reg_subscriber->presentity_uri.s,
			_r->public_identity.len, _r->public_identity.s);

	/* get subscriber id from DB */
	if (ul_dbf.use_table(ul_dbh, &subscriber_table) != 0) {
		LM_ERR("Unable to use table [%.*s]\n", subscriber_table.len, subscriber_table.s);
		return -1;
	}
	key[0] = &sub_event_col;
	val[0].type = DB1_INT;
	val[0].nul = 0;
	val[0].val.int_val = _reg_subscriber->event;
		
	key[1] = &sub_watcher_contact_col;
	val[1].type = DB1_STR;
	val[1].nul = 0;
	val[1].val.str_val = _reg_subscriber->watcher_contact;
	
	key[2] = &sub_presentity_uri_col;
	val[2].type = DB1_STR;
	val[2].nul = 0;
	val[2].val.str_val = _reg_subscriber->presentity_uri;
	
	if (ul_dbf.delete(ul_dbh, key, 0, val, 3) != 0) {
		LM_ERR("Unable to delete subscriber [%.*s] from DB\n", _reg_subscriber->presentity_uri.len, _reg_subscriber->presentity_uri.s);
		return -1;
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
	static str path, user_agent, callid, params;
	param_hooks_t hooks;
	
	// Set ci to 0:
	memset( ci, 0, sizeof(ucontact_info_t));

	/* params */
	if (!VAL_NULL(val + 1)) {
		params.s = (char*)VAL_STRING(val + 1);
		params.len = strlen(params.s);
		if (parse_params(&params, CLASS_CONTACT, &hooks, &ci->params) < 0) {
			LM_WARN("Error while parsing parameters: %.*s\n", params.len, params.s);
		}
	}
	
	/* path */
	if (!VAL_NULL(val + 2)) {
		path.s = (char*)VAL_STRING(val + 2);
		path.len = strlen(path.s);
	}
	ci->path = &path;

	/* user-agent */
	if (!VAL_NULL(val + 3)) {
		user_agent.s = (char*)VAL_STRING(val + 3);
		user_agent.len = strlen(user_agent.s);
	}
	ci->user_agent = &user_agent;

	/* received */
	if (!VAL_NULL(val + 4)) {
		ci->received.s = (char*)VAL_STRING(val + 4);
		ci->received.len = strlen(ci->received.s);
	}

	/* expires */
	if (!VAL_NULL(val + 5)) {
		ci->expires = VAL_TIME(val + 5);
	}
	/* callid */
	if (!VAL_NULL(val + 6)) {
		callid.s = (char*) VAL_STRING(val + 6);
		callid.len = strlen(callid.s);
	}
	ci->callid = &callid;

	return 0;
}

static inline int dbrow2subscriber(db_val_t* val, subscriber_data_t* subscriber_data) {
	static str presentity_uri, watcher_uri, watcher_contact, call_id, from_tag, to_tag, record_route, sockinfo_str;
	
	/*presentity uri*/
	if (!VAL_NULL(val)) {
	    presentity_uri.s = (char*) VAL_STRING(val);
	    presentity_uri.len = strlen(presentity_uri.s);
	}
	subscriber_data->presentity_uri = &presentity_uri;
	LM_DBG("presentity_uri: [%.*s]", subscriber_data->presentity_uri->len, subscriber_data->presentity_uri->s);
	
	/*watcher_uri*/
	if (!VAL_NULL(val + 1)) {
	    watcher_uri.s = (char*) VAL_STRING(val + 1);
	    watcher_uri.len = strlen(watcher_uri.s);
	}
	subscriber_data->watcher_uri = &watcher_uri;
	LM_DBG("watcher_uri: [%.*s]", subscriber_data->watcher_uri->len, subscriber_data->watcher_uri->s);
	
	/*watcher_contact*/
	if (!VAL_NULL(val + 2)) {
	    watcher_contact.s = (char*) VAL_STRING(val + 2);
	    watcher_contact.len = strlen(watcher_contact.s);
	}
	subscriber_data->watcher_contact = &watcher_contact;
	LM_DBG("watcher_contact: [%.*s]", subscriber_data->watcher_contact->len, subscriber_data->watcher_contact->s);
	
	/*event*/
	if (!VAL_NULL(val + 3)) {
	    subscriber_data->event = VAL_INT(val + 3);
	}
	LM_DBG("event: [%d]", subscriber_data->event);
	
	/* expires */
	if (!VAL_NULL(val + 4)) {
		subscriber_data->expires = VAL_TIME(val + 4);
	}
	LM_DBG("expires: [%d]", subscriber_data->expires);
	
	/*event*/
	if (!VAL_NULL(val + 5)) {
	    subscriber_data->version = VAL_INT(val + 5);
	}
	LM_DBG("version: [%d]", subscriber_data->version);
	
	/*local_cseq*/
	if (!VAL_NULL(val + 6)) {
	    subscriber_data->local_cseq = VAL_INT(val + 6);
	}
	LM_DBG("local_cseq: [%d]", subscriber_data->local_cseq);
	
	/* callid */
	if (!VAL_NULL(val + 7)) {
		call_id.s = (char*) VAL_STRING(val + 7);
		call_id.len = strlen(call_id.s);
	}
	subscriber_data->callid = &call_id;
	LM_DBG("callid: [%.*s]", subscriber_data->callid->len, subscriber_data->callid->s);
	
	/* ftag */
	if (!VAL_NULL(val + 8)) {
		from_tag.s = (char*) VAL_STRING(val + 8);
		from_tag.len = strlen(from_tag.s);
	}
	subscriber_data->ftag = &from_tag;
	LM_DBG("ftag: [%.*s]", subscriber_data->ftag->len, subscriber_data->ftag->s);
	
	/* ttag */
	if (!VAL_NULL(val + 9)) {
		to_tag.s = (char*) VAL_STRING(val + 9);
		to_tag.len = strlen(to_tag.s);
	}
	subscriber_data->ttag = &to_tag;
	LM_DBG("ttag: [%.*s]", subscriber_data->ttag->len, subscriber_data->ttag->s);
	
	/* record_route */
	if (!VAL_NULL(val + 10)) {
		record_route.s = (char*) VAL_STRING(val + 10);
		record_route.len = strlen(record_route.s);
	}
	subscriber_data->record_route = &record_route;
	LM_DBG("record_route: [%.*s]", subscriber_data->record_route->len, subscriber_data->record_route->s);
	
	/* sockinfo_str */
	if (!VAL_NULL(val + 11)) {
		sockinfo_str.s = (char*) VAL_STRING(val + 11);
		sockinfo_str.len = strlen(sockinfo_str.s);
	}
	subscriber_data->sockinfo_str = &sockinfo_str;
	LM_DBG("sockinfo_str: [%.*s]", subscriber_data->sockinfo_str->len, subscriber_data->sockinfo_str->s);
	
	return 0;
}

int preload_udomain(db1_con_t* _c, udomain_t* _d) {
    db_key_t col[9];
    db_row_t* row;
    db_row_t* contact_row;
    db_row_t* subscriber_row;
    db1_res_t* rs;
    db1_res_t* contact_rs;
    db1_res_t* subscriber_rs;
    db_val_t* vals;
    db_val_t* contact_vals;
    db_val_t* subscriber_vals;
    int barring = 0, reg_state = 0, impu_id, n, nn, i, j, len;
    str query_contact, query_subscriber, impu, ccf1 = {0, 0}, ecf1 = {0, 0}, ccf2 = {0, 0}, ecf2 = {
	0, 0
    }, blob = {0, 0}, contact = {0, 0}, presentity_uri = {0, 0};
    bin_data x;
    ims_subscription* subscription = 0;
    impurecord_t* impurecord;
    int impu_id_len;
    ucontact_t* c;
    ucontact_info_t contact_data;
    subscriber_data_t subscriber_data;
    reg_subscriber *reg_subscriber;

    /*
     * the two queries - get the IMPUs, then get associated contacts for each IMPU:
     * SELECT impu.impu,impu.barring,impu.reg_state,impu.ccf1,impu.ccf2,impu.ecf1,impu.ecf2,impu.ims_subscription_data FROM impu;
     * SELECT c.contact,c.path,c.user_agent,c.received,c.expires FROM impu_contact m LEFT JOIN contact c ON c.id=m.contact_id WHERE m.impu_id=20;
     */

    char *p_contact =
	    "SELECT c.contact,c.params,c.path,c.user_agent,c.received,c.expires,c.callid FROM impu_contact m LEFT JOIN contact c ON c.id=m.contact_id WHERE m.impu_id=";

    char *p_subscriber =
	    "SELECT s.presentity_uri,s.watcher_uri,s.watcher_contact,s.event,s.expires,s.version,s.local_cseq,s.call_id,s.from_tag,"
	    "s.to_tag,s.record_route,s.sockinfo_str FROM impu_subscriber m LEFT JOIN subscriber s ON s.id=m.subscriber_id WHERE m.impu_id=";

    query_contact.s = p_contact;
    query_contact.len = strlen(query_contact.s);

    query_subscriber.s = p_subscriber;
    query_subscriber.len = strlen(query_subscriber.s);


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
	n++;
	LM_DBG("loading S-CSCF usrloc records - cycle [%d]\n", n);
	for (i = 0; i < RES_ROW_N(rs); i++) {
	    impu_id = -1;

	    row = RES_ROWS(rs) + i;
	    LM_DBG("Fetching IMPU row %d\n", i + 1);
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
	    if (get_impurecord(_d, &impu, &impurecord) != 0) {
		if (mem_insert_impurecord(_d, &impu, reg_state, barring,
			&subscription, &ccf1, &ccf2, &ecf1, &ecf2, &impurecord)
			!= 0) {
		    LM_ERR("Unable to insert IMPU into memory [%.*s]\n", impu.len, impu.s);
		}
	    }

	    /* add contacts */
	    if (impu_id < 0) {
		LM_ERR("impu_id has not been set [%.*s] - we cannot read contacts or subscribers from DB....aborting preload\n", impu.len, impu.s);
		//TODO: check frees
		unlock_udomain(_d, &impu);
		continue;
	    }
	    impu_id_len = int_to_str_len(impu_id);
	    len = query_contact.len + impu_id_len + 1/*nul*/;
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
	    memcpy(query_buffer.s, query_contact.s, query_contact.len);
	    p_contact = query_buffer.s + query_contact.len;
	    snprintf(p_contact, impu_id_len + 1, "%d", impu_id);
	    query_buffer.len = query_contact.len + impu_id_len;
	    if (ul_dbf.raw_query(_c, &query_buffer, &contact_rs) != 0) {
		LM_ERR("Unable to query DB for contacts associated with impu [%.*s]\n",
			impu.len, impu.s);
		ul_dbf.free_result(_c, contact_rs);
	    } else {
		if (RES_ROW_N(contact_rs) == 0) {
		    LM_DBG("no contacts associated with impu [%.*s]\n", impu.len, impu.s);
		    ul_dbf.free_result(_c, contact_rs);
		} else {
		    nn = 0;
		    do {
			nn++;
			LM_DBG("loading S-CSCF contact - cycle [%d]\n", nn);
			for (j = 0; j < RES_ROW_N(contact_rs); j++) {
			    contact_row = RES_ROWS(contact_rs) + j;
			    contact_vals = ROW_VALUES(contact_row);

			    if (!VAL_NULL(contact_vals)) {
				contact.s = (char*) VAL_STRING(contact_vals);
				contact.len = strlen(contact.s);
			    }
			    if (dbrow2contact(contact_vals, &contact_data) != 0) {
				LM_ERR("unable to convert contact row from DB into valid data... moving on\n");
				continue;
			    }

			    if (get_ucontact(impurecord, &contact, contact_data.callid, contact_data.path, contact_data.cseq, &c) != 0) {
				LM_DBG("Contact doesn't exist yet, creating new one [%.*s]\n", contact.len, contact.s);
				if ((c = mem_insert_ucontact(impurecord, &contact, &contact_data)) == 0) {
				    LM_ERR("Unable to insert contact [%.*s] for IMPU [%.*s] into memory... continuing...\n",
					    contact.len, contact.s,
					    impu.len, impu.s);
				    continue;
				}
			    }
			    link_contact_to_impu(impurecord, c, 0);
			    release_ucontact(c);
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
		    ul_dbf.free_result(_c, contact_rs);
		}
	    }

	    /* add subscriber */
	    impu_id_len = int_to_str_len(impu_id);
	    len = query_subscriber.len + impu_id_len + 1/*nul*/;
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
	    memcpy(query_buffer.s, query_subscriber.s, query_subscriber.len);
	    p_subscriber = query_buffer.s + query_subscriber.len;
	    snprintf(p_subscriber, impu_id_len + 1, "%d", impu_id);
	    query_buffer.len = query_subscriber.len + impu_id_len;
	    if (ul_dbf.raw_query(_c, &query_buffer, &subscriber_rs) != 0) {
		LM_ERR("Unable to query DB for subscriber associated with impu [%.*s]\n",
			impu.len, impu.s);
		ul_dbf.free_result(_c, subscriber_rs);
		unlock_udomain(_d, &impu);
		continue;
	    }
	    if (RES_ROW_N(subscriber_rs) == 0) {
		LM_DBG("no subscriber associated with impu [%.*s]\n", impu.len, impu.s);
		ul_dbf.free_result(_c, subscriber_rs);
		unlock_udomain(_d, &impu);
		continue;
	    }

	    nn = 0;
	    do {
		nn++;
		LM_DBG("loading S-CSCF subscriber - cycle [%d]\n", nn);
		for (j = 0; j < RES_ROW_N(subscriber_rs); j++) {
		    subscriber_row = RES_ROWS(subscriber_rs) + j;
		    subscriber_vals = ROW_VALUES(subscriber_row);

		    /*presentity uri*/
		    if (!VAL_NULL(subscriber_vals)) {
			presentity_uri.s = (char*) VAL_STRING(subscriber_vals);
			presentity_uri.len = strlen(presentity_uri.s);
		    }

		    if (dbrow2subscriber(subscriber_vals, &subscriber_data) != 0) {
			LM_ERR("unable to convert subscriber row from DB into valid subscriberdata... moving on\n");
			continue;
		    }

		    if (add_subscriber(impurecord, &subscriber_data, &reg_subscriber, 1 /*db_load*/) != 0) {
			LM_ERR("Unable to insert subscriber with presentity_uri [%.*s] for IMPU [%.*s] into memory... continuing...\n",
				presentity_uri.len, presentity_uri.s,
				impu.len, impu.s);
		    }
		}
		if (DB_CAPABILITY(ul_dbf, DB_CAP_FETCH)) {
		    if (ul_dbf.fetch_result(_c, &subscriber_rs, ul_fetch_rows) < 0) {
			LM_ERR("fetching rows failed\n");
			ul_dbf.free_result(_c, subscriber_rs);
			unlock_udomain(_d, &impu);
			return -1;
		    }
		} else {
		    break;
		}
	    } while (RES_ROW_N(subscriber_rs) > 0);
	    ul_dbf.free_result(_c, subscriber_rs);

	    unlock_udomain(_d, &impu);

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

    LM_DBG("Completed preload_udomain");

    return 0;
}

int db_link_contact_to_impu(impurecord_t* _r, ucontact_t* _c) {
    int len;
    db1_res_t* rs;

    LM_DBG("DB: linking contact to IMPU\n");

    len = strlen(impu_contact_insert_query) + _r->public_identity.len + _c->c.len + 1;

    if (!query_buffer_len || query_buffer_len < len) {
	if (query_buffer.s) {
	    pkg_free(query_buffer.s);
	}
	query_buffer.s = (char*) pkg_malloc(len);
	if (!query_buffer.s) {
	    LM_ERR("no more pkg mem\n");
	    return -1;
	}
	query_buffer_len = len;
	
    }

    snprintf(query_buffer.s, query_buffer_len, impu_contact_insert_query, _r->public_identity.len, _r->public_identity.s, _c->c.len, _c->c.s);
    query_buffer.len = strlen(query_buffer.s);//len;

    LM_DBG("QUERY IS [%.*s] and len is %d\n", query_buffer.len, query_buffer.s, query_buffer.len);
    if (ul_dbf.raw_query(ul_dbh, &query_buffer, &rs) != 0) {
	LM_ERR("Unable to link impu-contact in DB - impu [%.*s], contact [%.*s]\n", _r->public_identity.len, _r->public_identity.s, _c->c.len, _c->c.s);
	return -1;
    }
    ul_dbf.free_result(ul_dbh, rs);
    LM_DBG("Query success\n");

    return 0;
}

int db_unlink_contact_from_impu(impurecord_t* _r, ucontact_t* _c) {
    int len;
    db1_res_t* rs;

    LM_DBG("DB: un-linking contact to IMPU\n");

    len = strlen(impu_contact_delete_query) + _r->public_identity.len + _c->c.len + 1;

    if (!query_buffer_len || query_buffer_len < len) {
	if (query_buffer.s) {
	    pkg_free(query_buffer.s);
	}
	query_buffer.s = (char*) pkg_malloc(len);
	if (!query_buffer.s) {
	    LM_ERR("no more pkg mem\n");
	    return -1;
	}
	query_buffer_len = len;
	
    }

    snprintf(query_buffer.s, query_buffer_len, impu_contact_delete_query, _r->public_identity.len, _r->public_identity.s, _c->c.len, _c->c.s);
    query_buffer.len = strlen(query_buffer.s);//len;

    if (ul_dbf.raw_query(ul_dbh, &query_buffer, &rs) != 0) {
	LM_ERR("Unable to un-link impu-contact in DB - impu [%.*s], contact [%.*s]\n", _r->public_identity.len, _r->public_identity.s, _c->c.len, _c->c.s);
	return -1;
    }
    ul_dbf.free_result(ul_dbh, rs);
    LM_DBG("Delete query success\n");

    return 0;
}

int db_unlink_subscriber_from_impu(impurecord_t* _r, reg_subscriber* _reg_subscriber) {
    int len;
    db1_res_t* rs;
    char event[11];
    int event_len;

    LM_DBG("DB: un-linking subscriber to IMPU\n");
    
    event_len = int_to_str_len(_reg_subscriber->event);
    snprintf(event, event_len + 1, "%d", _reg_subscriber->event);

    len = strlen(impu_subscriber_delete_query) + _r->public_identity.len + _reg_subscriber->watcher_contact.len + _reg_subscriber->presentity_uri.len + strlen(event) + 1;

    if (!query_buffer_len || query_buffer_len < len) {
	if (query_buffer.s) {
	    pkg_free(query_buffer.s);
	}
	query_buffer.s = (char*) pkg_malloc(len);
	if (!query_buffer.s) {
	    LM_ERR("no more pkg mem\n");
	    return -1;
	}
	query_buffer_len = len;
	
    }

    snprintf(query_buffer.s, query_buffer_len, impu_subscriber_delete_query, _r->public_identity.len, _r->public_identity.s, strlen(event), event, 
	    _reg_subscriber->watcher_contact.len, _reg_subscriber->watcher_contact.s, _reg_subscriber->presentity_uri.len, _reg_subscriber->presentity_uri.s);
    query_buffer.len = strlen(query_buffer.s);//len;

    if (ul_dbf.raw_query(ul_dbh, &query_buffer, &rs) != 0) {
	LM_ERR("Unable to un-link impu-subscriber in DB - impu [%.*s], subscriber [%.*s]\n", _r->public_identity.len, _r->public_identity.s, _reg_subscriber->presentity_uri.len, _reg_subscriber->presentity_uri.s);
	return -1;
    }
    ul_dbf.free_result(ul_dbh, rs);
    LM_DBG("Delete query success\n");

    return 0;
}

int db_link_subscriber_to_impu(impurecord_t* _r, reg_subscriber* _reg_subscriber) {
    int len;
    db1_res_t* rs;
    char event[11];
    int event_len;
    
    LM_DBG("DB: linking subscriber to IMPU\n");
    
    event_len = int_to_str_len(_reg_subscriber->event);
    snprintf(event, event_len + 1, "%d", _reg_subscriber->event);

    len = strlen(impu_subscriber_insert_query) + _r->public_identity.len + _reg_subscriber->watcher_contact.len + _reg_subscriber->presentity_uri.len + strlen(event) + 1;

    if (!query_buffer_len || query_buffer_len < len) {
	if (query_buffer.s) {
	    pkg_free(query_buffer.s);
	}
	query_buffer.s = (char*) pkg_malloc(len);
	if (!query_buffer.s) {
	    LM_ERR("no more pkg mem\n");
	    return -1;
	}
	query_buffer_len = len;
	
    }

    snprintf(query_buffer.s, query_buffer_len, impu_subscriber_insert_query, _r->public_identity.len, _r->public_identity.s, strlen(event), event, 
	    _reg_subscriber->watcher_contact.len, _reg_subscriber->watcher_contact.s, _reg_subscriber->presentity_uri.len, _reg_subscriber->presentity_uri.s);
    query_buffer.len = strlen(query_buffer.s);//len;

    LM_DBG("QUERY IS [%.*s] and len is %d\n", query_buffer.len, query_buffer.s, query_buffer.len);
    if (ul_dbf.raw_query(ul_dbh, &query_buffer, &rs) != 0) {
	LM_ERR("Unable to link impu-subscriber in DB - impu [%.*s], subscriber [%.*s]\n", _r->public_identity.len, _r->public_identity.s, _reg_subscriber->presentity_uri.len, _reg_subscriber->presentity_uri.s);
	return -1;
    }
    LM_DBG("Query success\n");
    ul_dbf.free_result(ul_dbh, rs);

    return 0;
}

