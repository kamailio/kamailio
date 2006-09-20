#include "reg_avps.h"
#include "dlist.h"
#include "ul_mod.h"
#include "../../ut.h"
#include "../../sr_module.h"

#define TYPE_STRING 's'
#define TYPE_NUM    'n'

char *reg_avp_table = "contact_attrs";
/* int use_serialized_reg_avps = 0; */

/*** table columns ***/

char *serialized_reg_avp_column = NULL;

char *regavp_uid_column = "uid";
char *regavp_contact_column = "contact";
char *regavp_name_column = "name";
char *regavp_value_column = "value";
char *regavp_type_column = "type";
char *regavp_flags_column = "flags";

/*  FIXME - ugly */
extern avp_t *create_avp (avp_flags_t flags, avp_name_t name, avp_value_t val);

static inline int num_digits(int i)
{
	int digits = 1;
	while (i > 9) {
		i = i / 10;
		digits++;
	}
	/* better will be to use logarithms, but can I use math routines 
	 * here (is it linked with libm)? */
	return digits;
}

void get_avp_value_ex(avp_t *avp, str *dst, int *type) {
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

/* returns length of serialized avp */
int serialize_avp(avp_t *avp, char *buf)
{
	int len = 0;
	str name = STR_NULL;
	str val;
	str *s;
	int type;
	char c;

	get_avp_value_ex(avp, &val, &type);
	s = get_avp_name(avp);
	if (s) name = *s;
	len = name.len + val.len + 3 /* : */ + 1 /* type */ + 
		num_digits(name.len) + num_digits(val.len) + 
		num_digits(avp->flags);
	
	if (buf) {
		/* can't use int2str because of static buf */
		if (type == AVP_VAL_STR) c = TYPE_STRING;
		else c = TYPE_NUM;
		sprintf(buf, "%c%d:%d:%d:%.*s%.*s", c, name.len, val.len, avp->flags,
				name.len, ZSW(name.s),
				val.len, ZSW(val.s)); /* not effective but simple */
	}
	return len;
}

static inline char *get_separator(char *buf) /* returns position after the number */
{
	/* find next ':' */
	int i;
	for (i = 0; buf[i] != ':'; i++);
	return buf + i;
}

char *get_nums(char *buf, int *_name_len, int *_value_len, int *_flags)
{
	str name_len, value_len, flags;
	char *last;
	
	name_len.s = buf + 1; /* skip type */
	value_len.s = get_separator(name_len.s) + 1;
	flags.s = get_separator(value_len.s) + 1;
	last = get_separator(flags.s);
	flags.len = last - flags.s;
	value_len.len = flags.s - value_len.s - 1;
	name_len.len = value_len.s - name_len.s - 1;

/*	INFO("I read: name_len=%.*s, value_len=%.*s, flags=%.*s\n",
			name_len.len, name_len.s,
			value_len.len, value_len.s,
			flags.len, flags.s);*/

	str2int(&name_len, (unsigned int*)_name_len);
	str2int(&value_len, (unsigned int*)_value_len);
	str2int(&flags, (unsigned int*)_flags);
	
	return last + 1;
}

avp_t *deserialize_avps(str *serialized_avps)
{
	avp_t *first, *last, *avp;
	int i, flags;
	str value;
	avp_value_t val;
	avp_name_t name;
	
	if (!serialized_avps) return NULL;
	if ((serialized_avps->len < 1) || (!serialized_avps->s)) return NULL;
	
	first = NULL; last = NULL;
	i = 0;
	while (i < serialized_avps->len) {
		name.s.s = get_nums(serialized_avps->s + i, &name.s.len, &value.len, &flags);
		value.s = name.s.s + name.s.len;
		switch (serialized_avps->s[i]) {
			case TYPE_NUM: str2int(&value, (unsigned int*)&val.n); break;
			case TYPE_STRING: val.s = value; break;
		}
/*		INFO("I read: name=%.*s, value=%.*s, flags=%d\n",
			name.s.len, name.s.s,
			value.len, value.s,
			flags);*/
		avp = create_avp(flags, name, val);

		if (last) last->next = avp;
		else first = avp;
		last = avp;
		
		i += value.s + value.len - serialized_avps->s;
	}

	return first;
}

int serialize_avps(avp_t *first, str *dst) {
	avp_t *avp;
	int len = 0;

	avp = first;
	while (avp) {
		len += serialize_avp(avp, NULL);
		avp = avp->next;
	}
	
	dst->len = len;

	if (len < 1) {
		dst->s = NULL;
		return 0;
	}
	
	dst->s = (char*)pkg_malloc(len + 1); /* hack: +1 due to \0 from sprintf */
	if (!dst->s) {
		dst->len = 0;
		ERR("no pkg mem (%d)\n", len);
		return -1;
	}
		
	avp = first;
	len = 0;
	while (avp) {
		len += serialize_avp(avp, dst->s + len);
		avp = avp->next;
	}
	return 0;
}

/* ************************************************************** */
/* database operations */

#define string_val(v,s)	(v).type = DB_STR; \
	if (s) { (v).val.str_val=*s; (v).nul = 0; }  \
	else (v).nul=1; 
#define int_val(v,t)	(v).type = DB_INT; \
	(v).val.int_val=t;\
	(v).nul=0; 

#define get_str_val(rvi,dst)	do{if(!rvi.nul){dst.s=(char*)rvi.val.string_val;dst.len=strlen(dst.s);}}while(0)
#define get_int_val(rvi,dst)	do{if(!rvi.nul){dst=rvi.val.int_val;}else dst=0;}while(0)

static inline int can_write_db()
{
	if (((db_mode == WRITE_THROUGH) || (db_mode == WRITE_BACK))) {
		if (ul_dbh) return 1;
	}
	return 0;
}
	
int db_read_reg_avps_et(db_con_t* con, struct ucontact *c)
{
	/* load on startup, ... */
	db_key_t keys[] = { regavp_uid_column, regavp_contact_column };
	db_op_t ops[] = { OP_EQ, OP_EQ };
	db_val_t k_vals[2];
	int i;
	db_res_t *res = NULL;
	db_key_t result_cols[] = { regavp_name_column, 
		regavp_type_column, regavp_value_column, regavp_flags_column };
	avp_t *first = NULL;
	avp_t *last = NULL;
	avp_t *avp;

	if (db_mode == NO_DB) {
		INFO("not reading attrs\n");
		return 0;
	}

/*	INFO("reading attrs for uid: %.*s, contact: %.*s\n",
			c->uid->len, c->uid->s, 
			c->c.len, c->c.s);*/
	
	string_val(k_vals[0], c->uid);
	string_val(k_vals[1], &c->c);
	
	if (ul_dbf.use_table(con, reg_avp_table) < 0) {
		ERR("Error in use_table\n");
		return -1;
	}

	if (ul_dbf.query (con, keys, ops, k_vals,
			result_cols, 2, sizeof(result_cols) / sizeof(db_key_t), 
			0, &res) < 0) {
		ERR("Error while querying contact attrs\n");
		return -1;
	}
	
	if (!res) return 0; /* ? */
	
	for (i = 0; i < res->n; i++) {
		int flags = 0;
		int type = 0;
		str value = STR_NULL;
		avp_value_t val;
		avp_name_t name;
		db_row_t *row = &res->rows[i];
		db_val_t *row_vals = ROW_VALUES(row);
		
		get_str_val(row_vals[0], name.s);
		get_int_val(row_vals[1], type);
		get_str_val(row_vals[2], value);
		get_int_val(row_vals[3], flags);

		if (type == AVP_VAL_STR) val.s = value;
		else str2int(&value, (unsigned int*)&val.n);
		avp = create_avp(flags, name, val);

		if (last) last->next = avp;
		else first = avp;
		last = avp;
	}
	ul_dbf.free_result(con, res);
	
	c->avps = first;
	return 0;
}

int db_delete_reg_avps_et(struct ucontact *c)
{
	db_key_t keys[] = { regavp_uid_column, regavp_contact_column };
	db_op_t ops[] = { OP_EQ, OP_EQ };
	db_val_t k_vals[2];

	if (!can_write_db()) return 0;
	
	string_val(k_vals[0], c->uid);
	string_val(k_vals[1], &c->c);

	if (ul_dbf.use_table(ul_dbh, reg_avp_table) < 0) {
		ERR("Error in use_table\n");
		return -1;
	}

	if (ul_dbf.delete(ul_dbh, keys, ops, k_vals, 2) < 0) {
		LOG(L_ERR, "Error while removing data\n");
		return -1;
	}
	
	return 0;
}

static inline int db_save_avp_et(avp_t *avp, str *uid, str *contact)
{
	db_key_t cols[10];
	db_val_t vals[10];
	str *s;
	str v;
	int type;
	
	int n = -1;

	cols[++n] = regavp_uid_column;
	string_val(vals[n], uid);
	
	cols[++n] = regavp_contact_column;
	string_val(vals[n], contact);
	
	s = get_avp_name(avp);
	cols[++n] = regavp_name_column;
	string_val(vals[n], s);
	
	get_avp_value_ex(avp, &v, &type);
	cols[++n] = regavp_value_column;
	string_val(vals[n], &v);
	
	cols[++n] = regavp_type_column;
	int_val(vals[n], type);
	
	cols[++n] = regavp_flags_column;
	int_val(vals[n], avp->flags);

	if (ul_dbf.insert(ul_dbh, cols, vals, n + 1) < 0) {
		ERR("Can't insert record into DB\n");
		return -1;
	}
	
	return 0;
}

int db_save_reg_avps_et(struct ucontact *c)
{
	avp_t *avp = c->avps;
	int res = 0;

	if (!can_write_db()) return 0;

	if (avp) {
		if (ul_dbf.use_table(ul_dbh, reg_avp_table) < 0) {
			ERR("Error in use_table\n");
			return -1;
		}
	}

	while (avp) {
		if (db_save_avp_et(avp, c->uid, &c->c) < 0) res = -1;
		avp = avp->next;
	}
	
	return res;
}

int db_update_reg_avps_et(struct ucontact* _c)
{
	db_delete_reg_avps(_c);
	return db_save_reg_avps(_c);
}

/* AVPs are stored into 'location' table into "serialized_avp_column" column - hack for
 * existing database */

int db_delete_reg_avps_lt(struct ucontact *c) /* !!! FIXME: hacked version !!! */
{
	db_key_t cols[] = { serialized_reg_avp_column };
	db_val_t vals[1];
	
	db_key_t keys[] = { regavp_uid_column, regavp_contact_column };
	db_op_t ops[] = { OP_EQ, OP_EQ };
	db_val_t k_vals[2];
	char b[256];
	
	if (!can_write_db()) return 0;

	memcpy(b, c->domain->s, c->domain->len);
	b[c->domain->len] = '\0';
	
	if (ul_dbf.use_table(ul_dbh, b) < 0) {
		ERR("Error in use_table\n");
		return -1;
	}

	string_val(k_vals[0], c->uid);
	string_val(k_vals[1], &c->c);
	vals[0].type = DB_STR;
	vals[0].val.str_val.s = NULL;
	vals[0].val.str_val.len = 0;
	vals[0].nul = 1;
	
	if (ul_dbf.update(ul_dbh, keys, ops, k_vals, 
				cols, vals, 2, 1) < 0) {
		ERR("Can't update record\n");
		return -1;
	}
	
	return 0;
}

int db_update_reg_avps_lt(struct ucontact *c) /* !!! FIXME: hacked version !!! */
{
	/* db_delete_reg_avps(c); not needed ! */
	return db_save_reg_avps(c);
}

int db_save_reg_avps_lt(struct ucontact *c) /* !!! FIXME: hacked version !!! */
{
	avp_t *avp = c->avps;
	db_key_t cols[] = { serialized_reg_avp_column };
	db_val_t vals[1];
	
	db_key_t keys[] = { regavp_uid_column, regavp_contact_column };
	db_op_t ops[] = { OP_EQ, OP_EQ };
	db_val_t k_vals[2];
	
	str s = STR_NULL;
	char b[256];

	if (!can_write_db()) return 0;
	
	memcpy(b, c->domain->s, c->domain->len);
	b[c->domain->len] = '\0';

	if (ul_dbf.use_table(ul_dbh, b) < 0) {
		ERR("Error in use_table\n");
		return -1;
	}

	if (serialize_avps(avp, &s) < 0) {
		ERR("can't serialize AVPs\n");
		return -1;
	}
	
	string_val(k_vals[0], c->uid);
	string_val(k_vals[1], &c->c);
	string_val(vals[0], &s);
	
	if (ul_dbf.update(ul_dbh, keys, ops, k_vals, 
				cols, vals, 2, 1) < 0) {
		ERR("Can't update record\n");
		return -1;
	}

	if (s.s) pkg_free(s.s);
	
	return 0;
}

int db_read_reg_avps_lt(db_con_t* con, struct ucontact *c) /* !!! FIXME: hacked version !!! */
{
	/* load on startup, ... */
	db_key_t keys[] = { regavp_uid_column, regavp_contact_column };
	db_op_t ops[] = { OP_EQ, OP_EQ };
	db_val_t k_vals[2];
	db_res_t *res = NULL;
	db_key_t result_cols[] = { serialized_reg_avp_column };
	char b[256];

	if (db_mode == NO_DB) {
		INFO("not reading attrs\n");
		return 0;
	}

	memcpy(b, c->domain->s, c->domain->len);
	b[c->domain->len] = '\0';
	
/*	INFO("reading attrs for uid: %.*s, contact: %.*s\n",
			c->uid->len, c->uid->s, 
			c->c.len, c->c.s);*/
	
	string_val(k_vals[0], c->uid);
	string_val(k_vals[1], &c->c);
	
	if (ul_dbf.use_table(con, b) < 0) {
		ERR("Error in use_table\n");
		return -1;
	}

	if (ul_dbf.query (con, keys, ops, k_vals,
			result_cols, 2, sizeof(result_cols) / sizeof(db_key_t), 
			0, &res) < 0) {
		ERR("Error while querying contact attrs\n");
		return -1;
	}
	
	if (!res) return 0; /* ? */
	
	if (res->n > 0) {
		str serialized_avps = STR_NULL;
		db_row_t *row = &res->rows[0];
		db_val_t *row_vals = ROW_VALUES(row);
		
		get_str_val(row_vals[0], serialized_avps);
		/* INFO("reading AVPs from db: %.*s\n", serialized_avps.len, serialized_avps.s); */
		c->avps = deserialize_avps(&serialized_avps);

	}
	ul_dbf.free_result(con, res);
	
	return 0;
}

/*** interface functions ***/

/* These functions call only implementation in xxx_lt or xxx_et functions.
 * 
 * _et means extra table - such fuction uses reg AVPs stored in extra table
 * contact_attrs.  
 *
 * _lt means location table - these functions use location table (more
 * precisely table from ucontact) for storing serialized AVPs into one column.
 */

static inline int serialize_reg_avps()
{
	if (serialized_reg_avp_column) 
		if (*serialized_reg_avp_column) return 1;

	return 0;
}

int db_save_reg_avps(struct ucontact* c)
{
	if (!use_reg_avps()) return 0;
	
	if (serialize_reg_avps())
		return db_save_reg_avps_lt(c);
	else 
		return db_save_reg_avps_et(c);
}

int db_delete_reg_avps(struct ucontact* c)
{
	if (!use_reg_avps()) return 0;
	
	if (serialize_reg_avps())
		return db_delete_reg_avps_lt(c);
	else 
		return db_delete_reg_avps_et(c);
}

int db_update_reg_avps(struct ucontact* c)
{
	if (!use_reg_avps()) return 0;

	if (serialize_reg_avps())
		return db_update_reg_avps_lt(c);
	else 
		return db_update_reg_avps_et(c);
}

int db_read_reg_avps(db_con_t* con, struct ucontact *c)
{
	if (!use_reg_avps()) return 0;

	if (serialize_reg_avps())
		return db_read_reg_avps_lt(con, c);
	else 
		return db_read_reg_avps_et(con, c);
}

