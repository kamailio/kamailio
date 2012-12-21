#include "extra_attrs.h"
#include "uid_avp_db.h"
#include "../../usr_avp.h"
#include "../../sr_module.h"
#include "../../ut.h"
#include "../../mem/mem.h"
#include "../../mem/shm_mem.h"
#include "../../lock_ops.h"
#include "../../script_cb.h"
#include "../../hashes.h"

#define set_str_val(f,s)	(f).v.lstr=(s); \
	(f).flags = 0;

#define set_str_val_ex(f,s)	if ((s).len) { (f).v.lstr=(s); (f).flags = 0; }  \
	else (f).flags|=DB_NULL; 

#define set_int_val(f,t)	(f).v.int4=t;\
	(f).flags=0; 

#define get_str_val(rvi,dst)	do{if(!(rvi.flags&DB_NULL)){dst=rvi.v.lstr;} else dst.len = 0;}while(0)
#define get_int_val(rvi,dst)	do{if(!(rvi.flags&DB_NULL)){dst=rvi.v.int4;} else dst = 0;}while(0)

typedef struct _registered_table_t {
	char *id;
	char *table_name;
	
	/* column names */
	char *key_column;
	char *name_column;
	char *type_column;
	char *value_column;
	char *flags_column;

	char *flag_name;

	/* pregenerated queries */
	db_cmd_t *query;
	db_cmd_t *remove;
	db_cmd_t *add;

	avp_flags_t flag;

	int group_mutex_idx;

	struct _registered_table_t *next;

	char buf[1]; /* buffer for strings allocated with the structure */
} registered_table_t;

static registered_table_t *tables = NULL;

registered_table_t *find_registered_table(const char *id)
{
	registered_table_t *t = tables;
	while (t) {
		if (strcmp(t->id, id) == 0) return t;
		t = t->next;
	}
	return NULL;
}

char *get_token(char *s, str *name, str *value)
{
	enum { reading_name, reading_value } state = reading_name;
	/* returns 'token' which has the form name[=value][,]
	 * replaces separators ,= by binary 0 to allow char* strings */

	name->s = s;
	name->len = 0;
	value->s = NULL;
	value->len = 0;

	while (*s) {
		switch (state) {
			case reading_name: 
				switch (*s) {
					case '=':
					case ':': 
						state = reading_value;
						value->s = s + 1;
						*s = 0; /* replace separator */
						break;
					case ',': 
						*s = 0; /* replace separator */
						return s + 1;
					default: name->len++;
				}
				break;
			case reading_value:
				if (*s == ',') {
					*s = 0; /* replace separator */
					return s + 1;
				}
				else value->len++;
				break;
		}
		s++;
	}
	return NULL; /* everything read */
}

static int cmp_s(str *a, str *b)
{
	int i;
	/* Warning: none string can be NULL! */
	if (a->len != b->len) return -1;
	if (!a->len) return 0; /* equal - empty */
	for (i = 0; i < a->len; i++) 
		if (a->s[i] != b->s[i]) return 1;
	return 0;
}

static inline void cpy(char *dst, str *s)
{
	memcpy(dst, s->s, s->len);
	dst[s->len] = 0;
}

/* adds new 'extra attribute group' (it adds new table for it) */
int declare_attr_group(modparam_t type, char* _param)
{
	registered_table_t *rt;
	str name, value;
	str param;
	char *p;
	
	static str table = STR_STATIC_INIT("table");
	static str flag = STR_STATIC_INIT("flag");
	static str id = STR_STATIC_INIT("id");
	static str key_column = STR_STATIC_INIT("key_column");
	static str name_column = STR_STATIC_INIT("name_column");
	static str value_column = STR_STATIC_INIT("value_column");
	static str type_column = STR_STATIC_INIT("type_column");
	static str flags_column = STR_STATIC_INIT("flags_column");
	
	if (!(type & PARAM_STR)) {
		ERR("Invalid parameter type\n");
		return -1;
	}
	if (!_param) {
		ERR("invalid parameter value\n");
		return -1;
	}
	param = *((str*)_param);
    DBG("group def: %.*s\n", param.len, param.s);
	
	rt = pkg_malloc(param.len + sizeof(*rt) + 1);
	if (!rt) {
		ERR("can't allocate PKG memory\n");
		return -1;
	}
	memset(rt, 0, sizeof(*rt));
	cpy(rt->buf, &param);

	/* default column names */
	rt->key_column = "id";
	rt->name_column = "name";
	rt->type_column = "type";
	rt->value_column = "value";
	rt->flags_column = "flags";

	/* parse the string */
	p = rt->buf;
	do {
		p = get_token(p, &name, &value);
		if (cmp_s(&name, &table) == 0) rt->table_name = value.s;
		else if (cmp_s(&name, &flag) == 0) rt->flag_name = value.s;
		else if (cmp_s(&name, &id) == 0) rt->id = value.s;
		else if (cmp_s(&name, &key_column) == 0) rt->key_column = value.s;
		else if (cmp_s(&name, &name_column) == 0) rt->name_column = value.s;
		else if (cmp_s(&name, &type_column) == 0) rt->type_column = value.s;
		else if (cmp_s(&name, &value_column) == 0) rt->value_column = value.s;
		else if (cmp_s(&name, &flags_column) == 0) rt->flags_column = value.s;
	} while (p);
	
	if ((!rt->id) || (!rt->flag_name)) {
		ERR("at least attribute group ID and flags must ve given\n");
		return -1;
	}
	/* insert new element into registered tables */

	rt->flag = register_avpflag(rt->flag_name);
	if (!rt->flag) {
		ERR("can't register AVP flag: %s\n", rt->flag_name);
		pkg_free(rt);
		return -1;
	}
	
	/* append to the beggining - it doesn't depend on the order */
	rt->next = tables;
	tables = rt;

	return 0;
}

/** Initialize all queries needed by 'extra attributes' for given table. 
 * Variable default_res holds columns which can used by read_attrs. */
static int init_queries(db_ctx_t *ctx, registered_table_t *t)
{
	db_fld_t match[] = {
		{ .name = t->key_column, .type = DB_STR, .op = DB_EQ },
		{ .name = NULL }
	};
	db_fld_t query_res[] = {
		/* Warning: be careful here - the query must have the same result
		 * as query for user/uri AVPs to be readable by read_attrs */
		{ .name = t->name_column, .type = DB_STR, .op = DB_EQ },
		{ .name = t->type_column, .type = DB_INT, .op = DB_EQ },
		{ .name = t->value_column, .type = DB_STR, .op = DB_EQ },
		{ .name = t->flags_column, .type = DB_BITMAP, .op = DB_EQ },
		{ .name = NULL }
	};
	db_fld_t add_values[] = {
		{ .name = t->key_column, .type = DB_STR, .op = DB_EQ },
		{ .name = t->name_column, .type = DB_STR, .op = DB_EQ },
		{ .name = t->type_column, .type = DB_INT, .op = DB_EQ },
		{ .name = t->value_column, .type = DB_STR, .op = DB_EQ },
		{ .name = t->flags_column, .type = DB_BITMAP, .op = DB_EQ },
		{ .name = NULL }
	};

	t->query = db_cmd(DB_GET, ctx, t->table_name, query_res, match, NULL);
	t->remove = db_cmd(DB_DEL, ctx, t->table_name, NULL, match, NULL);
	t->add = db_cmd(DB_PUT, ctx, t->table_name, NULL, NULL, add_values);

	if (t->query && t->remove && t->add) return 0;
	else return -1; /* not all queries were initialized */
}

int init_extra_avp_queries(db_ctx_t *ctx)
{
	registered_table_t *t = tables;
	while (t) {
		if (init_queries(ctx, t) < 0)  return -1;
		t = t->next;
	}
	return 0;
}

static void get_avp_value_ex(avp_t *avp, str *dst, int *type) {
	avp_value_t val;

	/* Warning! it uses static buffer from int2str !!! */

	get_avp_val(avp, &val);
	if (avp->flags & AVP_VAL_STR) {
		*dst = val.s;
		*type = AVP_VAL_STR;
	}
	else { /* probably (!) number */
		dst->s = int2str(val.n, &dst->len);
		*type = 0;
	}
}

/** Saves attribute into DB with given ID.  The ID must not be NULL. The
 * use_table must be called outside of this function (from interface
 * functions) */
static inline int save_avp(registered_table_t *t, avp_t *avp, str *id) /* id MUST NOT be NULL */
{
	str *s, v;
	int type;
	static str empty = STR_STATIC_INIT("");
	
	set_str_val(t->add->vals[0], *id);
	
	s = get_avp_name(avp);
	if (!s) s = &empty;
	set_str_val(t->add->vals[1], *s);
	
	get_avp_value_ex(avp, &v, &type);
	set_int_val(t->add->vals[2], type);
	set_str_val(t->add->vals[3], v);
	set_int_val(t->add->vals[4], avp->flags & (AVP_CLASS_ALL | AVP_TRACK_ALL | AVP_NAME_STR | AVP_VAL_STR));
	
	if (db_exec(NULL, t->add) < 0) {
		ERR("Can't insert record into DB\n");
		return -1;
	}
	return 0;
}

/** Loads all attributes with given ID.  The ID must not be NULL. */
static int read_avps(db_res_t *res, avp_flags_t flag) /* id must not be NULL */
{
	db_rec_t *row;

	row = db_first(res);
	while (row) {
		int flags = 0;
		int type = 0;
		str value = STR_NULL;
		avp_value_t val;
		avp_name_t name;
		
		get_str_val(row->fld[0], name.s);
		get_int_val(row->fld[1], type);
		get_str_val(row->fld[2], value);
		get_int_val(row->fld[3], flags);

		if (flags & SRDB_LOAD_SER) {
			if (type == AVP_VAL_STR) val.s = value;
			else str2int(&value, (unsigned int *)&val.n); /* FIXME */

			flags |= flag;

			/* FIXME: avps probably should be removed before they are added,
			 * but this should be done in add_avp and not here! */
			add_avp(flags, name, val);
		}
		row = db_next(res);
	}
	return 0;
}

/** Removes all attributes with given ID.  The ID must not be NULL.  */
static inline int remove_all_avps(registered_table_t *t, str *id)
{
	set_str_val(t->remove->match[0], *id);
	if (db_exec(NULL, t->remove) < 0) {
		ERR("can't remove attrs\n");
		return -1;
	}
	return 0;
}

/* ----- interface functions ----- */

int load_extra_attrs(struct sip_msg* msg, char* _table, char* _id)
{
	registered_table_t *t;
	db_res_t *res = NULL;
	str id;

	t = (registered_table_t *)_table;
	if ((!t) || (get_str_fparam(&id, msg, (fparam_t*)_id) < 0)) {
		ERR("invalid parameter value\n");
		return -1;
	}
	
	set_str_val(t->query->match[0], id);
	if (db_exec(&res, t->query) < 0) {
		ERR("DB query failed\n");
		return -1;
	}
	if (res) {
		read_avps(res, t->flag);
		db_res_free(res);
	}

	return 1;
}

int remove_extra_attrs(struct sip_msg* msg, char *_table, char* _id)
{
	str id;
	registered_table_t *t;

	t = (registered_table_t *)_table;

	if ((!t) || (get_str_fparam(&id, msg, (fparam_t*)_id) < 0)) {
		ERR("invalid parameter value\n");
		return -1;
	}
	remove_all_avps(t, &id);

	return 1;
}

int save_extra_attrs(struct sip_msg* msg, char* _table, char *_id)
{
	str id;
	int i;
	struct usr_avp *avp;
	static unsigned short lists[] = {
		AVP_CLASS_USER | AVP_TRACK_FROM, 
		AVP_CLASS_USER | AVP_TRACK_TO, 
		AVP_CLASS_URI | AVP_TRACK_FROM, 
		AVP_CLASS_URI | AVP_TRACK_TO, 
		0
	};
	registered_table_t *t;

	t = (registered_table_t *)_table;

	if ((!t) || (get_str_fparam(&id, msg, (fparam_t*)_id) < 0)) {
		ERR("invalid parameter value\n");
		return -1;
	}

	/* delete all attrs under given id */
	remove_all_avps(t, &id);

	/* save all attrs flagged with flag under id */	
	for (i = 0; lists[i]; i++) {
		for (avp = get_avp_list(lists[i]); avp; avp = avp->next) {
			if ((avp->flags & t->flag) != 0) save_avp(t, avp, &id);
		}
	}
	return 1;
}

int extra_attrs_fixup(void** param, int param_no)
{
	registered_table_t *t;

	switch (param_no) {
		case 1: /* try to find registered table, error if not found */
				t = find_registered_table(*param);
				if (!t) {
					ERR("can't find attribute group with id: %s\n", (char*)*param);
					return -1;
				}
				*param = (void*)t;
				break;
		case 2: return fixup_var_str_2(param, param_no);
	}
	return 0;
}

/******* locking *******/ 

#define LOCK_CNT	32

gen_lock_t *locks = NULL; /* set of mutexes allocated in shared memory */
int lock_counters[LOCK_CNT]; /* set of counters (each proces has its own counters) */

static int avpdb_post_script_cb(struct sip_msg *msg, unsigned int flags, void *param) {
	int i;

	for (i=0; i<LOCK_CNT; i++) {
		if (lock_counters[i] > 0) {
			if (auto_unlock) {
				DEBUG("post script auto unlock extra attrs <%d>\n", i);
				lock_release(&locks[i]);
				lock_counters[i]=0;
			} else {
				BUG("script writer didn't unlock extra attrs !!!\n");
				return 1;
			}
		}
	}
	return 1;
}

int init_extra_avp_locks()
{
	int i;
	registered_table_t *t = tables;

	/* zero all 'lock counters' */
	memset(lock_counters, 0, sizeof(lock_counters));

	locks = shm_malloc(sizeof(gen_lock_t) * LOCK_CNT);
	if (!locks) {
		ERR("can't allocate mutexes\n");
		return -1;
	}
	for (i = 0; i < LOCK_CNT; i++) {
		lock_init(&locks[i]);
	}

	/* initializes mutexes for extra AVPs */
	i = 0;
	while (t) {
		t->group_mutex_idx = get_hash1_raw(t->table_name, strlen(t->table_name)) % LOCK_CNT;
		t = t->next;
	}

	register_script_cb(avpdb_post_script_cb, REQUEST_CB | ONREPLY_CB | POST_SCRIPT_CB, 0);

	return 0;
}

static inline int find_mutex(registered_table_t *t, str *id)
{
	/* hash(table_name) + hash(id) */
	return ((t->group_mutex_idx + get_hash1_raw(id->s, id->len)) % LOCK_CNT);
}

int lock_extra_attrs(struct sip_msg* msg, char *_table, char* _id)
{
	str id;
	registered_table_t *t;
	int mutex_idx;

	t = (registered_table_t *)_table;
	if ((!t) || (get_str_fparam(&id, msg, (fparam_t*)_id) < 0)) {
		ERR("invalid parameter value\n");
		return -1;
	}

	/* find right mutex according to id/table */
	mutex_idx = find_mutex(t, &id);

	if (lock_counters[mutex_idx] > 0) {
		/* mutex is already locked by this process */
		lock_counters[mutex_idx]++;
	}
	else {
		/* the mutex was not locked => lock it and set counter */
		lock_get(&locks[mutex_idx]);
		lock_counters[mutex_idx] = 1;
	}

	return 1;
}

int unlock_extra_attrs(struct sip_msg* msg, char *_table, char* _id)
{
	str id;
	registered_table_t *t;
	int mutex_idx;

	t = (registered_table_t *)_table;
	if ((!t) || (get_str_fparam(&id, msg, (fparam_t*)_id) < 0)) {
		ERR("invalid parameter value\n");
		return -1;
	}

	/* find right mutex according to id/table */
	mutex_idx = find_mutex(t, &id);

	if (lock_counters[mutex_idx] > 1) {
		/* mutex is locked more times by this process */
		lock_counters[mutex_idx]--;
	}
	else if (lock_counters[mutex_idx] == 1) {
		/* the mutex is locked once => unlock it and reset counter */
		lock_release(&locks[mutex_idx]);
		lock_counters[mutex_idx] = 0;
	}
	else {
		BUG("trying to unlock without lock group=\"%s\" id=\"%.*s\"\n", t->id, id.len, id.s);
	}

	return 1;
}

