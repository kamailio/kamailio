#include "reg_avps.h"
#include "dlist.h"
#include "ul_mod.h"
#include "../../ut.h"
#include "../../sr_module.h"

#define TYPE_STRING 's'
#define TYPE_NUM    'n'

/* int use_serialized_reg_avps = 0; */

/*** table columns ***/

char *avp_column = NULL;

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

static char *get_nums(char *buf, int *_name_len, int *_value_len, int *_flags)
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

int serialize_avps(avp_t *first, str *dst) 
{
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


