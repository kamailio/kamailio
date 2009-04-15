#include <time.h>
#include <cds/logger.h>

#include "rl_subscription.h"
#include "rls_mod.h"

char *rls_table = "rls_subscription";
char *vs_table = "rls_vs";
char *vs_names_table = "rls_vs_names";

/* generate ID for given data */
void generate_db_id(db_id_t *id, void *data)
{
	if (id) {
		snprintf(*id, sizeof(*id), "%px%xx%x", 
				data, (int)time(NULL), rand());
		/* DEBUG_LOG("generated DB ID = %s\n", *id); */
	}
}

#define string_val(v,s)	(v).type = DB_STR; \
	(v).val.str_val=s; \
	(v).nul=(s.len == 0); 

#define int_val(v,i)	(v).type = DB_INT; \
	(v).val.int_val=i;\
	(v).nul=0; 

#define time_val(v,t)	(v).type = DB_DATETIME; \
	(v).val.time_val=t;\
	(v).nul=0; 

#define string_val_ex(v,str,l)	(v).type = DB_STR; \
	(v).val.str_val.s=str; \
	(v).val.str_val.len=l; \
	(v).nul=0; 

#define blob_val(v,str)	(v).type = DB_BLOB; \
	(v).val.blob_val=str; \
	(v).nul=0; 


/* ------------- virtual subscriptions ------------- */

static int virtual_subscription_db_add(virtual_subscription_t *vs, rl_subscription_t *s)
{
	db_key_t cols[20];
	db_val_t vals[20];
	int n = -1;
	int i, cnt;
	vs_display_name_t *dn;

	DEBUG_LOG("storing into database\n");
	
	if (rls_dbf.use_table(rls_db, vs_table) < 0) {
		LOG(L_ERR, "vsub_db_add: Error in use_table\n");
		return -1;
	}
	
	cols[++n] = "uri";
	string_val(vals[n], vs->uri);
	
	cols[++n] = "id";
	string_val_ex(vals[n], vs->dbid, strlen(vs->dbid));
	
	cols[++n] = "rls_id";
	string_val_ex(vals[n], s->dbid, strlen(s->dbid));
	
	/* insert new record into database */
	if (rls_dbf.insert(rls_db, cols, vals, n + 1) < 0) {
		LOG(L_ERR, "vsub_db_add: Error while inserting virtual subscription\n");
		return -1;
	}

	/* store display names */
	cnt = vector_size(&vs->display_names);
	for (i = 0; i < cnt; i++) {
		if (rls_dbf.use_table(rls_db, vs_names_table) < 0) {
			LOG(L_ERR, "vsub_db_add (names): Error in use_table\n");
			return -1;
		}
		dn = vector_get_ptr(&vs->display_names, i);
		if (!dn) continue;

		n = -1;
		
		cols[++n] = "id";
		string_val_ex(vals[n], vs->dbid, strlen(vs->dbid));
		
		cols[++n] = "name";
		string_val(vals[n], dn->name);
		
		cols[++n] = "lang";
		string_val(vals[n], dn->lang);
	
		if (rls_dbf.insert(rls_db, cols, vals, n + 1) < 0) {
			LOG(L_ERR, "vsub_db_add: Error while inserting name\n");
			return -1;
		}
	}
	
	return 0;
}

static int vs_db_add(rl_subscription_t *s)
{
	int i, cnt;
	int res = 0;
	virtual_subscription_t *vs;
	
	cnt = ptr_vector_size(&s->vs);
	for (i = 0; i < cnt; i++) {
		vs = ptr_vector_get(&s->vs, i);
		if (!vs) continue;

		res = virtual_subscription_db_add(vs, s);
		if (res != 0) break;
	}
	return res;
}

static int vs_db_update(rl_subscription_t *s)
{
	/* There is nothing to be updated now - may be dialogs for
	 * external subscriptions and their expirations 
	 * in the future ! 
	 *
	 * Status is newly generated on the other side (in PA)! */
	return 0;
}

static int vs_db_remove(rl_subscription_t *s)
{
	db_key_t keys[] = { "id" };
	db_op_t ops[] = { OP_EQ };
	db_val_t k_vals[1];
	int i, cnt;
	int res = 0;
	virtual_subscription_t *vs;
	
	cnt = ptr_vector_size(&s->vs);
	for (i = 0; i < cnt; i++) {
		vs = ptr_vector_get(&s->vs, i);
		if (!vs) continue;
		
		string_val_ex(k_vals[0], vs->dbid, strlen(vs->dbid));
		
		/* remove virtual subscription */
		if (rls_dbf.use_table(rls_db, vs_table) < 0) {
			LOG(L_ERR, "db_remove_presence_tuple: Error in use_table\n");
			res = -1;
		}
		if (rls_dbf.delete(rls_db, keys, ops, k_vals, 1) < 0) {
			LOG(L_ERR, "db_remove_presence_tuple: Can't delete record\n");
			res = -1;
		}
		
		/* remove display names */
		if (rls_dbf.use_table(rls_db, vs_names_table) < 0) {
			LOG(L_ERR, "db_remove_presence_tuple: Error in use_table\n");
			res = -1;
		}
		if (rls_dbf.delete(rls_db, keys, ops, k_vals, 1) < 0) {
			LOG(L_ERR, "db_remove_presence_tuple: Can't delete record\n");
			res = -1;
		}
	}
	
	return res;
}

/* ------------- rls subscriptions ------------- */

int rls_db_add(rl_subscription_t *s)
{
	db_key_t cols[20];
	db_val_t vals[20];
	str_t dialog = STR_NULL;
	str_t str_xcap_params = STR_NULL;
	int n = -1;
	int res = 0;
	time_t t;

	if (!use_db) return 0;

	/* store only external subscriptions */
	if (s->type != rls_external_subscription) return 0;
	
	DEBUG_LOG("storing into database\n");
	
	if (rls_dbf.use_table(rls_db, rls_table) < 0) {
		LOG(L_ERR, "rls_db_add: Error in use_table\n");
		return -1;
	}
	
	cols[++n] = "doc_version";
	int_val(vals[n], s->doc_version);
	
	cols[++n] = "status";
	int_val(vals[n], s->u.external.status);
	
	t = time(NULL);
	t += rls_subscription_expires_in(s);
	cols[++n] = "expires";
	time_val(vals[n], t);
	
	if (dlg_func.dlg2str(s->u.external.dialog, &dialog) != 0) {	
		LOG(L_ERR, "Error while serializing dialog\n");
		return -1;
	}
	cols[++n] = "dialog";
	blob_val(vals[n], dialog);
	
	cols[++n] = "contact";
	string_val(vals[n], s->u.external.contact);
	
	cols[++n] = "uri";
	string_val(vals[n], s->u.external.record_id);
	
	cols[++n] = "package";
	string_val(vals[n], s->u.external.package);
	
	cols[++n] = "w_uri";
	string_val(vals[n], s->u.external.subscriber);

	if (xcap_params2str(&str_xcap_params, &s->xcap_params) != 0) {
		LOG(L_ERR, "Error while serializing xcap params\n");
		str_free_content(&dialog);
		return -1;
	}
	cols[++n] = "xcap_params";
	blob_val(vals[n], str_xcap_params);
	
	cols[++n] = "id";
	string_val_ex(vals[n], s->dbid, strlen(s->dbid));
	
	/* insert new record into database */
	if (rls_dbf.insert(rls_db, cols, vals, n + 1) < 0) {
		LOG(L_ERR, "rls_db_add: Error while inserting subscription\n");
		res = -1;
	}
	
	str_free_content(&dialog);
	str_free_content(&str_xcap_params);

	if (res == 0) res = vs_db_add(s);
	
	return res;
}

int rls_db_remove(rl_subscription_t *s)
{
	db_key_t keys[] = { "id" };
	db_op_t ops[] = { OP_EQ };
	db_val_t k_vals[] = { 
		{ DB_STR, 0, 
			{ .str_val = { s: s->dbid, len: strlen(s->dbid) } 
			}
		}
	};
	
	if (!use_db) return 0;
	
	/* only external subscriptions are stored */
	if (s->type != rls_external_subscription) return 0;

	if (rls_dbf.use_table(rls_db, rls_table) < 0) {
		LOG(L_ERR, "db_remove_presence_tuple: Error in use_table\n");
		return -1;
	}

	if (rls_dbf.delete(rls_db, keys, ops, k_vals, 1) < 0) {
		LOG(L_ERR, "db_remove_presence_tuple: Can't delete record\n");
		return -1;
	}
	
	return vs_db_remove(s);
}

int rls_db_update(rl_subscription_t *s)
{
	db_key_t cols[20];
	db_val_t vals[20];
	str_t dialog = STR_NULL;
	str_t str_xcap_params = STR_NULL;
	int n = -1;
	int res = 0;
	time_t t;
	db_key_t keys[] = { "id" };
	db_op_t ops[] = { OP_EQ };
	db_val_t k_vals[] = { 
		{ DB_STR, 0, 
			{ .str_val = { s: s->dbid, len: strlen(s->dbid) } 
			}
		}
	};

	if (!use_db) return 0;
	
	/* only external subscriptions are stored */
	if (s->type != rls_external_subscription) return 0;

	if (rls_dbf.use_table(rls_db, rls_table) < 0) {
		LOG(L_ERR, "rls_db_add: Error in use_table\n");
		return -1;
	}
	
	cols[++n] = "doc_version";
	int_val(vals[n], s->doc_version);
	
	cols[++n] = "status";
	int_val(vals[n], s->u.external.status);
	
	t = time(NULL);
	t += rls_subscription_expires_in(s);
	cols[++n] = "expires";
	time_val(vals[n], t);
	
	if (dlg_func.dlg2str(s->u.external.dialog, &dialog) != 0) {	
		LOG(L_ERR, "Error while serializing dialog\n");
		return -1;
	}
	cols[++n] = "dialog";
	blob_val(vals[n], dialog);
	
	cols[++n] = "contact";
	string_val(vals[n], s->u.external.contact);
	
	cols[++n] = "uri";
	string_val(vals[n], s->u.external.record_id);
	
	cols[++n] = "package";
	string_val(vals[n], s->u.external.package);
	
	cols[++n] = "w_uri";
	string_val(vals[n], s->u.external.subscriber);
	
	if (xcap_params2str(&str_xcap_params, &s->xcap_params) != 0) {
		LOG(L_ERR, "Error while serializing xcap params\n");
		str_free_content(&dialog);
		return -1;
	}
	cols[++n] = "xcap_params";
	blob_val(vals[n], str_xcap_params);
	
	if (rls_dbf.update(rls_db, keys, ops, k_vals, 
				cols, vals, 1, n + 1) < 0) {
		LOG(L_ERR, "rls_db_add: Error while inserting subscription\n");
		res = -1;
	}
	
	str_free_content(&dialog);
	str_free_content(&str_xcap_params);

	return vs_db_update(s);
}

/* ------------- Loading ------------- */

#define get_str_val(rvi,dst)	do{if(!rvi.nul){dst.s=(char*)rvi.val.string_val;dst.len=strlen(dst.s);}}while(0)
#define get_blob_val(rvi,dst)	do{if(!rvi.nul){dst=rvi.val.blob_val;}else dst.len=0;}while(0)
#define get_time_val(rvi,dst)	do{if(!rvi.nul){dst=rvi.val.time_val;}}while(0)
#define get_int_val(rvi,dst)	do{if(!rvi.nul){dst=rvi.val.int_val;}else dst=0;}while(0)

static dlg_t *dlg2str(str_t *s)
{
	dlg_t *dlg = (dlg_t*)mem_alloc(sizeof(*dlg));
	if (!dlg) LOG(L_ERR, "Can't allocate dialog\n");
	else {
		if (dlg_func.str2dlg(s, dlg) != 0) {	
			LOG(L_ERR, "Error while deserializing dialog\n");
			mem_free(dlg);
			dlg = NULL;
		}
	}
	return dlg;
}

int db_load_vs_names(db_con_t *rls_db, virtual_subscription_t *vs)
{
	int i, r = 0;
	db_res_t *res = NULL;
	db_key_t result_cols[] = { 
		"name", "lang"
	};
	db_key_t keys[] = { "id" };
	db_op_t ops[] = { OP_EQ };
	db_val_t k_vals[] = { 
		{ DB_STR, 0, 
			{ .str_val = { s: vs->dbid, len: strlen(vs->dbid) } } 
		} 
	};

	if (rls_dbf.use_table(rls_db, vs_names_table) < 0) {
		LOG(L_ERR, "vs_load_vs_names: Error in use_table\n");
		return -1;
	}

	if (rls_dbf.query (rls_db, keys,ops, k_vals,
			result_cols, 1, sizeof(result_cols) / sizeof(db_key_t), 
			0, &res) < 0) {
		LOG(L_ERR, "db_load_vs_names: Error while querying vs names\n");
		r = -1;
		res = NULL;
	}
	if (res) {
		for (i = 0; i < res->n; i++) {
			db_row_t *row = &res->rows[i];
			db_val_t *row_vals = ROW_VALUES(row);
			str name = STR_NULL;
			str lang = STR_NULL;

			get_str_val(row_vals[0], name);
			get_str_val(row_vals[1], lang);
	
			DEBUG_LOG("     adding name %.*s\n", FMT_STR(name));
			vs_add_display_name(vs, name.s, lang.s);
		}

		rls_dbf.free_result(rls_db, res);
	}
	
	return r;
}

int db_load_vs(db_con_t *rls_db, rl_subscription_t *s)
{
	int i, r = 0;
	db_res_t *res = NULL;
	virtual_subscription_t *vs;
	db_key_t result_cols[] = { 
		"id", "uri"
	};
	db_key_t keys[] = { "rls_id" };
	db_op_t ops[] = { OP_EQ };
	db_val_t k_vals[] = { 
		{ DB_STR, 0, 
			{ .str_val = { s: s->dbid, len: strlen(s->dbid) } } 
		} 
	};

	if (rls_dbf.use_table(rls_db, vs_table) < 0) {
		LOG(L_ERR, "vs_load_vs: Error in use_table\n");
		return -1;
	}

	if (rls_dbf.query (rls_db, keys,ops, k_vals,
			result_cols, 1, sizeof(result_cols) / sizeof(db_key_t), 
			0, &res) < 0) {
		LOG(L_ERR, "db_load_vs: Error while querying presentity\n");
		r = -1;
		res = NULL;
	}
	if (res) {
		for (i = 0; i < res->n; i++) {
			db_row_t *row = &res->rows[i];
			db_val_t *row_vals = ROW_VALUES(row);
			str id = STR_NULL;
			str uri = STR_NULL;

			get_str_val(row_vals[0], id);
			get_str_val(row_vals[1], uri);
			
			r = vs_create(&uri, &vs, NULL, s, max_list_nesting_level) | r;
			if ((r != 0) || (!vs)) { r = -1; break; }
			
			strcpy(vs->dbid, id.s);
			DEBUG_LOG("  created VS to %.*s\n", FMT_STR(uri));
		
			ptr_vector_add(&s->vs, vs);
			
			db_load_vs_names(rls_db, vs);	
		}

		rls_dbf.free_result(rls_db, res);
	}
	
	return r;
}

int db_load_rls()
{
	/* this function may be called from mod_init, thus can not work
	 * with DB connection opened from child_init */
	db_con_t* rls_db = NULL; /* own database connection handle */
	int i, r = 0;
	rl_subscription_t *s;
	db_res_t *res = NULL;
	db_key_t result_cols[] = { 
		"id", "doc_version", "dialog",
		"expires", "status", "contact",
		"uri", "package", "w_uri",
		"xcap_params"
	};

	if (!use_db) return 0;

	DEBUG_LOG("loading rls from db\n");

	/* open own database connection */
	if (rls_dbf.init) rls_db = rls_dbf.init(db_url);
	if (!rls_db) {
		LOG(L_ERR, "db_load_rls: Error while connecting database\n");
		return -1;
	}

	if (rls_dbf.use_table(rls_db, rls_table) < 0) {
		LOG(L_ERR, "rls_load_rls: Error in use_table\n");
		return -1;
	}

	if (rls_dbf.query (rls_db, NULL, NULL, NULL,
			result_cols, 0, sizeof(result_cols) / sizeof(db_key_t), 
			0, &res) < 0) {
		LOG(L_ERR, "db_load_rls: Error while querying presentity\n");
		r = -1;
		res = NULL;
	}
	if (res) {
		for (i = 0; i < res->n; i++) {
			db_row_t *row = &res->rows[i];
			db_val_t *row_vals = ROW_VALUES(row);
			str id = STR_NULL;
			str contact = STR_NULL;
			str dialog = STR_NULL;
			str xcap_params = STR_NULL;
			str uri = STR_NULL;
			str package = STR_NULL;
			str w_uri = STR_NULL;
			subscription_status_t status;
			time_t expires = 0;
			int expires_after;
			dlg_t *dlg = NULL;

			s = rls_alloc_subscription(rls_external_subscription);
			if (!s) { r = -1; break; }
		
			get_str_val(row_vals[0], id);
			strcpy(s->dbid, id.s);
			get_int_val(row_vals[1], s->doc_version);
			get_blob_val(row_vals[2], dialog);
			get_time_val(row_vals[3], expires);
			get_int_val(row_vals[4], status);
			get_str_val(row_vals[5], contact);
			get_str_val(row_vals[6], uri);
			get_str_val(row_vals[7], package);
			get_str_val(row_vals[8], w_uri);
			get_blob_val(row_vals[9], xcap_params);
			if (expires != 0) expires_after = expires - time(NULL);
			else expires_after = 0;
			dlg = dlg2str(&dialog);
			sm_init_subscription_nolock_ex(rls_manager, &s->u.external, 
					dlg,
					status,
					&contact,
					&uri,
					&package,
					&w_uri,
					expires_after,
					s);
			DEBUG_LOG("  created RLS to %.*s from %.*s\n", 
					FMT_STR(uri), FMT_STR(w_uri));

			if (str2xcap_params(&s->xcap_params, &xcap_params) < 0) {
				ERR("can't set xcap params\n");
				rls_free(s);
				s = 0;
				r = -1;
				break;
			}
		
			/* load virtual subscriptions */
			db_load_vs(rls_db, s);	
		}

		rls_dbf.free_result(rls_db, res);
	}
	
	/* close db connection */
	if (rls_dbf.close) rls_dbf.close(rls_db);

	DEBUG_LOG("rls loaded\n");

	return r;
}

