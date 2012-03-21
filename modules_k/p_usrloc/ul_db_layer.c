#include "../../lib/srdb1/db.h"
#include "ul_db.h"
#include "p_usrloc_mod.h"
#include "ul_db_layer.h"
#include "ul_db_api.h"

typedef struct res_list {
	db1_con_t ** h;
	db1_res_t * r;
	struct res_list * next;
}
res_list_t;

static void add_res(db1_res_t * _r, db1_con_t ** _h);
static db1_con_t ** get_handle_by_res(db1_res_t * _r);
static void drop_res(db1_res_t * _r);

static ul_db_api_t p_ul_dbf;
static db_func_t dbf;

static res_list_t * used = NULL;
static res_list_t * unused = NULL;

static ul_domain_db_list_t * domain_db_list = NULL;

int ul_db_layer_init(void) {
	if(bind_ul_db(&p_ul_dbf) < 0) {
		LM_ERR("could not bind ul_db_api.\n");
		return -1;
	}
	if(db_bind_mod(&default_db_url, &dbf) < 0) {
		LM_ERR("could not bind db.\n");
		return -1;
	}
	return 0;
}

void ul_db_layer_destroy(void) {
	res_list_t * tmp, * del;
	tmp = used;
	while(tmp) {
		del = tmp;
		tmp = tmp->next;
		pkg_free(del);
	}
	tmp = unused;
	while(tmp) {
		del = tmp;
		tmp = tmp->next;
		pkg_free(del);
	}
	return;
}

int ul_db_layer_single_connect(udomain_t * domain, str * url) {
	if((domain->dbh = dbf.init(url)) == NULL){
		return -1;
	}
	return 1;
}

int ul_db_layer_insert(udomain_t * domain, str * user, str * sipdomain,
                       db_key_t* _k, db_val_t* _v, int _n) {
	ul_domain_db_t * d;
	switch(domain->dbt) {
			case DB_TYPE_CLUSTER: return p_ul_dbf.insert(domain->name, user, sipdomain, _k, _v, _n);
			case DB_TYPE_SINGLE: if(!domain->dbh){
				if((d = ul_find_domain(domain->name->s)) == 0){
					return -1;
				}
				if(ul_db_layer_single_connect(domain, &d->url) < 0){
					return -1;
				}
			}
			if(dbf.use_table(domain->dbh, domain->name) < 0) {
				return -1;
			}
			return dbf.insert(domain->dbh, _k, _v, _n);
			default: return -1;
	}
}

int ul_db_layer_update(udomain_t * domain, str * user, str * sipdomain, db_key_t* _k, db_op_t* _o, db_val_t* _v,
	         db_key_t* _uk, db_val_t* _uv, int _n, int _un) {
	ul_domain_db_t * d;
	switch(domain->dbt) {
			case DB_TYPE_CLUSTER: return p_ul_dbf.update(domain->name, user, sipdomain, _k, _o, _v, _uk, _uv, _n, _un);
			case DB_TYPE_SINGLE: if(!domain->dbh){
				if((d = ul_find_domain(domain->name->s)) == 0){
					return -1;
				}
				if(ul_db_layer_single_connect(domain, &d->url) < 0){
					return -1;
				}
			}
			if(dbf.use_table(domain->dbh, domain->name) < 0) {
				return -1;
			}
			return dbf.update(domain->dbh, _k, _o, _v, _uk, _uv, _n, _un);
			default: return -1;
	}
}

int ul_db_layer_replace (udomain_t * domain, str * user, str * sipdomain,
                              db_key_t* _k,	db_val_t* _v, int _n, int _un) {
	ul_domain_db_t * d;
	switch(domain->dbt) {
			case DB_TYPE_CLUSTER: { return p_ul_dbf.replace(domain->name, user, sipdomain, _k, _v, _n, _un);;}
			case DB_TYPE_SINGLE: if(!domain->dbh){
				if((d = ul_find_domain(domain->name->s)) == 0){
					return -1;
				}
				if(ul_db_layer_single_connect(domain, &d->url) < 0){
					return -1;
				}
			}
				if(dbf.use_table(domain->dbh, domain->name) < 0) {
				return -1;
			}
			return dbf.replace(domain->dbh, _k, _v, _n, _un, 0);
			default: return -1;
	}
}

int ul_db_layer_delete(udomain_t * domain, str * user, str * sipdomain,
                       db_key_t* _k, db_op_t* _o, db_val_t* _v, int _n) {
	ul_domain_db_t * d;
	switch(domain->dbt) {
			case DB_TYPE_CLUSTER: return p_ul_dbf.delete(domain->name, user, sipdomain, _k, _o, _v, _n);
			case DB_TYPE_SINGLE: if(!domain->dbh){
				if((d = ul_find_domain(domain->name->s)) == 0){
					return -1;
				}
				if(ul_db_layer_single_connect(domain, &d->url) < 0){
					return -1;
				}
			}
				if(dbf.use_table(domain->dbh, domain->name) < 0) {
				return -1;
			}
			return dbf.delete(domain->dbh, _k, _o, _v, _n);
			default: return -1;
	}
}

int ul_db_layer_query(udomain_t * domain, str * user, str * sipdomain,
                      db_key_t* _k, db_op_t* _op,	db_val_t* _v, db_key_t* _c,
                      int _n, int _nc, db_key_t _o, db1_res_t** _r) {
	ul_domain_db_t * d;
	db1_con_t ** h;
	int ret;
	switch(domain->dbt) {
			case DB_TYPE_CLUSTER:{ 
			if((ret = p_ul_dbf.query(domain->name, user, sipdomain, &h, _k, _op, _v, _c, _n, _nc, _o, _r)) < 0 || (!_r)) {
				return -1;
			}
			add_res(*_r, h);
			return ret;
			}
			case DB_TYPE_SINGLE: if(!domain->dbh){
				if((d = ul_find_domain(domain->name->s)) == 0){
					return -1;
				}
				if(ul_db_layer_single_connect(domain, &d->url) < 0){
					return -1;
				}
			}
				if(dbf.use_table(domain->dbh, domain->name) < 0) {
				return -1;
			}
			return dbf.query(domain->dbh, _k, _op, _v, _c, _n, _nc, _o, _r);
			default: return -1;
	}
}

int ul_db_layer_raw_query(udomain_t * domain, str* _s, db1_res_t** _r) {
	ul_domain_db_t * d;
	switch(domain->dbt) {
			case DB_TYPE_CLUSTER: {
				return -1; // not useful in this mode
			}
			case DB_TYPE_SINGLE: if(!domain->dbh){
				if((d = ul_find_domain(domain->name->s)) == 0){
					return -1;
				}
				if(ul_db_layer_single_connect(domain, &d->url) < 0){
					return -1;
				}
			}
			if(dbf.use_table(domain->dbh, domain->name) < 0) {
				return -1;
			}
			return dbf.raw_query(domain->dbh, _s, _r);
			default: return -1;
	}
}

int ul_db_layer_free_result(udomain_t * domain, db1_res_t * res) {
	db1_con_t ** h;
	int ret;
	switch(domain->dbt) {
			case DB_TYPE_CLUSTER: if((h = get_handle_by_res(res)) == NULL) {
				return -1;
			}
			ret = p_ul_dbf.free_result(h, res);
			drop_res(res);
			return ret;
			case DB_TYPE_SINGLE: return dbf.free_result(domain->dbh, res);
			default: return -1;
	}
}

int ul_add_domain_db(str * d, int t, str * url) {
	ul_domain_db_list_t * new_d = NULL;
	LM_DBG("%.*s, type: %s\n", d->len, d->s,
			t==DB_TYPE_SINGLE ? "SINGLE" : "CLUSTER");
	if((new_d = pkg_malloc(sizeof(ul_domain_db_list_t))) == NULL) {
		return -1;
	}
	memset(new_d, 0, sizeof(ul_domain_db_list_t));
	if(!d){
		return -1;
	}
	if(!d->s){
		return -1;		
	}
	if((new_d->domain.name.s = pkg_malloc(d->len + 1)) == NULL) {
		return -1;
	}
	if(t == DB_TYPE_SINGLE) {
		if(url) {
			LM_DBG("url: %.*s", url->len, url->s);
			if((new_d->domain.url.s = pkg_malloc(url->len + 1)) == NULL) {
				return -1;
			}
			strncpy(new_d->domain.url.s, url->s, url->len);
                       new_d->domain.url.s[url->len] = '\0';
			new_d->domain.url.len = url->len;
		} else {
			if((new_d->domain.url.s = pkg_malloc(default_db_url.len + 1)) == NULL) {
				return -1;
			}
			strcpy(new_d->domain.url.s, default_db_url.s);
			new_d->domain.url.len = default_db_url.len;
		}
	}
	strncpy(new_d->domain.name.s, d->s, d->len);
	new_d->domain.name.len = d->len;
	new_d->domain.dbt = t;
	new_d->next = domain_db_list;
	domain_db_list = new_d;
	return 1;
}

ul_domain_db_t * ul_find_domain(const char * s) {
	ul_domain_db_list_t * tmp;
	str d;
	if(!domain_db_list){
		if(parse_domain_db(&domain_db) < 0){
			LM_ERR("could not parse domain parameter.\n");
			return NULL;
		}
	}
	tmp = domain_db_list;
	while(tmp){
		LM_DBG("searched domain: %s, actual domain: %.*s, length: %i, type: %s\n", 
			s, tmp->domain.name.len, tmp->domain.name.s, tmp->domain.name.len, 
			tmp->domain.dbt==DB_TYPE_SINGLE ? "SINGLE" : "CLUSTER");
		if((strlen(s) == tmp->domain.name.len) && (memcmp(s, tmp->domain.name.s, tmp->domain.name.len) == 0)){
			return &tmp->domain;
		}
		tmp = tmp->next;
	}
	if((d.s = pkg_malloc(strlen(s) + 1)) == NULL){
		return NULL;
	}
	strcpy(d.s, s);
	d.len = strlen(s);
	if(ul_add_domain_db(&d, default_dbt, &default_db_url)){
		pkg_free(d.s);
		return ul_find_domain(s);
	}
	pkg_free(d.s);
	return NULL;
}

void free_all_udomains(void) {
	ul_domain_db_list_t * tmp, * del;
	tmp = domain_db_list;
	while(tmp){
		del = tmp;
		tmp = tmp->next;
		pkg_free(del->domain.name.s);
		if(del->domain.dbt == DB_TYPE_SINGLE){
			pkg_free(del->domain.url.s);
		}
		pkg_free(del);
	}
	return;
}

int parse_domain_db(str * _d) {
#define STATE_START 0
#define STATE_DOMAIN 1
#define STATE_TYPE 2
#define STATE_URL 3
	char * domains;
	char * end;
	str d = {NULL, 0}, u = {NULL, 0}, t = {NULL, 0};
	int type = 0;
	int state;

	if (_d->len==0) {
		return -1;
	}
	domains = _d->s;
	end = domains + _d->len;

	state = STATE_START;
	while(domains <= end) {
		switch(*domains) {
			case '=':
				switch(state) {
					case STATE_START: return -1;
					case STATE_DOMAIN: 
						LM_DBG("found domain %.*s\n", d.len, d.s);
						state = STATE_TYPE;
						t.s = domains + 1;
						t.len = 0;
						break;
						case STATE_TYPE: return -1;
						case STATE_URL: return -1;
						default: return -1;
				}
				break;
			case ';':
				switch(state) {
					case STATE_START: return 1;
					case STATE_DOMAIN: return -1;
					case STATE_TYPE: 
						LM_DBG("found type %.*s\n", t.len, t.s);
						if(strncmp(t.s, DB_TYPE_CLUSTER_STR, strlen(DB_TYPE_CLUSTER_STR)) == 0) {
							type = DB_TYPE_CLUSTER;
						} else {
							type = DB_TYPE_SINGLE;
						}
						state = STATE_URL;
						u.s = domains + 1;
						u.len = 0;
						break;
						case STATE_URL: return -1;
						break;
						default: break;
				}
				break;
			case ',':
				switch(state) {
					case STATE_START: return -1;
					case STATE_DOMAIN: return -1;
					case STATE_TYPE: 
						LM_DBG("found type %.*s\n", t.len, t.s);
						if(strncmp(t.s, DB_TYPE_CLUSTER_STR, strlen(DB_TYPE_CLUSTER_STR)) == 0) {
							type = DB_TYPE_CLUSTER;
						} else {
							type = DB_TYPE_SINGLE;
						}
						ul_add_domain_db(&d, type, NULL);
						state = STATE_START;
						break;
					case STATE_URL: 
						LM_DBG("found url %.*s\n", u.len, u.s);
						state = STATE_START;
						ul_add_domain_db(&d, type, &u);	
						break;
						default: return -1;
				}
				break;
			case '\0':
				switch(state) {
					case STATE_START:
						return 1;
					case STATE_DOMAIN:
						return -1;
					case STATE_TYPE:
						LM_DBG("found type %.*s\n", t.len, t.s);
						if(strncmp(t.s, DB_TYPE_CLUSTER_STR, strlen(DB_TYPE_CLUSTER_STR)) == 0) {
							type = DB_TYPE_CLUSTER;
						} else {
							type = DB_TYPE_SINGLE;
						}
						ul_add_domain_db(&d, type, NULL);
						return 1;
					case STATE_URL:
						LM_DBG("found url %.*s\n", u.len, u.s);
						ul_add_domain_db(&d, type, &u);
						return 1;
						default: return -1;
				}
				break;
			default:
				switch(state) {
					case STATE_START:
						state = STATE_DOMAIN;
						d.s = domains;
						d.len = 1;
						break;
					case STATE_DOMAIN:
						d.len++;
						break;
					case STATE_TYPE:
						t.len++;
						break;
					case STATE_URL:
						u.len++;
						break;
						default: break;
				}
				break;
		}
		domains++;
	}
	return 1;
}

static void add_res(db1_res_t * _r, db1_con_t ** _h) {
	res_list_t * new_res;
	if(!unused) {
		if((new_res = pkg_malloc(sizeof(res_list_t))) == NULL) {
			return;
		}
		memset(new_res, 0, sizeof(res_list_t));
	} else {
		new_res = unused;
		unused = unused->next;
	}
	new_res->h = _h;
	new_res->r = _r;
	new_res->next = used;
	used = new_res;
	return;
}

static db1_con_t ** get_handle_by_res(db1_res_t * _r) {
	res_list_t * tmp = used;
	while(tmp) {
		if(_r == tmp->r) {
			return tmp->h;
		}
		tmp = tmp->next;
	}
	return NULL;
}

static void drop_res(db1_res_t * _r) {
	res_list_t * prev = NULL;
	res_list_t * tmp;
	tmp = used;
	while(tmp) {
		if(_r == tmp->r) {
			if(prev==NULL) {
				used = tmp->next;
			} else {
				prev->next = tmp->next;
			}
			tmp->next = unused;
			unused = tmp;
			return;
		}
		prev = tmp;
		tmp = tmp->next;
	}
	return;
}

