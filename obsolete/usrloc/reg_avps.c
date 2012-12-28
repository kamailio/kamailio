#include "reg_avps.h"
#include "dlist.h"
#include "ul_mod.h"
#include "../../ut.h"
#include "../../sr_module.h"

static avp_flags_t reg_avp_flag = 0;

/* ************************************************************** */
/* utility functions */

/*  FIXME - ugly */
extern avp_t *create_avp (avp_flags_t flags, avp_name_t name, avp_value_t val);


void trace_avp(const char *prolog, avp_t *avp)
{
	str *s;
			
	s = get_avp_name(avp);
	if (s) INFO("%s: \"%.*s\" (flags = %d)\n", prolog, s->len, s->s, avp->flags);
	else INFO("%s: unnamed AVP (flags = %d)\n", prolog, avp->flags);
}


int use_reg_avps()
{
	return (reg_avp_flag != 0);
}


/* ************************************************************** */
/* internal functions for storing/restoring AVPs into ucontact */

static inline avp_t *avp_dup(avp_t *avp)
{
	avp_value_t val;
	avp_name_t name;
	str *s;
	
	if (avp) {
		get_avp_val(avp, &val);
		if (avp->flags & AVP_NAME_STR) {
			s = get_avp_name(avp);
			if (s) name.s = *s;
			else {
				name.s.s = NULL;
				name.s.len = 0;
			}
		}
		else name.n = avp->id;
		return create_avp(avp->flags, name, val);
	}
	return NULL;
}

static void reg_destroy_avps(avp_t *avp)
{
	avp_t *n;

	while (avp) {
		n = avp->next;
		shm_free(avp); /* FIXME: really ?? */
		avp = n;
	}
}

static void remove_avps(avp_t *avp)
{
	struct search_state ss;
	avp_name_t name;
	avp_t *a;
	str *s;
	
	if (avp->flags & AVP_NAME_STR) {
		s = get_avp_name(avp);
		if (s) name.s = *s;
		else {
			name.s.s = NULL;
			name.s.len = 0;
		}
	}
	else name.n = avp->id;
	
	a = search_first_avp(avp->flags, name, 0, &ss);
	while(a) {
		destroy_avp(a);
		a = search_next_avp(&ss, 0);
	}
}

static int save_reg_avps_impl(struct ucontact *c)
{
	int i;
	struct usr_avp *avp, *dup;
	avp_t *first, *last;
	static unsigned short lists[] = {
		AVP_CLASS_USER | AVP_TRACK_FROM, 
		AVP_CLASS_USER | AVP_TRACK_TO, 
		AVP_CLASS_URI | AVP_TRACK_FROM, 
		AVP_CLASS_URI | AVP_TRACK_TO, 
		0
	};

	/* destroy old AVPs */
	/* if (c->avps) db_delete_reg_avps(c); */
	reg_destroy_avps(c->avps);

	last = NULL;
	first = NULL;
	
	for (i = 0; lists[i]; i++) {
		for (avp = get_avp_list(lists[i]); avp; avp = avp->next) {
			
			/* trace_avp("trying to save avp", avp); */
			
			if ((avp->flags & reg_avp_flag) == 0) continue;
			
			/* trace_avp("saving avp", avp); */
			
			dup = avp_dup(avp);
			if (dup) {
				/* add AVP into list */
				if (last) last->next = dup;
				else first = dup;
				last = dup;
			}
			
		}
	}

	c->avps = first;
/*	if (c->avps) db_save_reg_avps(c); */
	
	return 0;
}

static int restore_reg_avps(struct ucontact *info)
{
	avp_t *avp;
	avp_value_t val;
	avp_name_t name;
	str *s;	
	
	/* remove all these AVPs ? */
	avp = info->avps;
	while (avp) {
		remove_avps(avp);
		avp = avp->next;
	}

	/* add stored AVPs */
	avp = info->avps;
	while (avp) {
		get_avp_val(avp, &val);
		if (avp->flags & AVP_NAME_STR) {
			s = get_avp_name(avp);
			if (s) name.s = *s;
			else {
				name.s.s = NULL;
				name.s.len = 0;
			}
		}
		else name.n = avp->id;
		
		/* trace_avp("restoring avp", avp); */
		
		/* modify flags here? */
		add_avp(avp->flags, name, val);
		
		avp = avp->next;
	}
	
	return 0;
}

static int delete_reg_avps_impl(struct ucontact *info)
{
/*	db_delete_reg_avps(info); */
	if (info->avps) reg_destroy_avps(info->avps);
	info->avps = NULL;
	return 0;
}

/* ************************************************************** */

int set_reg_avpflag_name(char *name)
{
	reg_avp_flag = 0;
	
	if (name) {
		if (!(*name)) return 0; /* -> don't use reg AVPs when zero length */
		
		reg_avp_flag = register_avpflag(name);
		if (!reg_avp_flag) {
			ERR("can't register AVP flag %s\n", name);
			return -1;
		}
	} /* else not use reg AVPs */
	return 0;
}


/*
 * Take AVPS from the current lists and store them in the contact
 * structure as registration AVPs. Existing registration AVPs will
 * be destroyed.
 */
int save_reg_avps(struct ucontact *contact)
{
	/* no locking here! */

	if (!use_reg_avps()) return 0;

	/* INFO("saving registration AVP flags\n"); */
	return save_reg_avps_impl(contact);
}


/*
 * Delete registration AVPs from the contact
 */
int delete_reg_avps(struct ucontact* c)
{
	/* no locking here! */

	if (!use_reg_avps()) return 0;

	/*INFO("removing registration AVP flags\n");*/
	return delete_reg_avps_impl(c);
}


/*
 * Take registration AVPs from the contact and copy
 * them to the current AVP lists
 */
int load_reg_avps(struct ucontact *contact)
{
	/* lock udomain here! */
	
	if (!use_reg_avps()) return 0;
	
	/* INFO("loading registration AVP flags\n"); */
	return restore_reg_avps(contact);
}


int read_reg_avps_fixup(void** param, int param_no)
{
	udomain_t* d;

	switch (param_no) {
		case 1:
			if (register_udomain((char*)*param, &d) < 0) {
				ERR("Error while registering domain\n");
				return -1;
			}
			*param = (void*)d;
			break;
		case 2:
			return fixup_var_str_2(param, param_no);
	}
	return 0;
}


int read_reg_avps(struct sip_msg *m, char* _domain, char* fp)
{
	urecord_t* r = NULL;
	struct ucontact *contact = NULL;
	udomain_t *d;
	str uid;
	
	if (!use_reg_avps()) return 1;

	d = (udomain_t*)_domain;
	if (get_str_fparam(&uid, m, (fparam_t*)fp) < 0) {
		ERR("invalid parameter\n");
		return -1;
	}
	
/*	INFO("reading avps for uid=%.*s\n", uid.len, ZSW(uid.s)); */

	lock_udomain(d);
	
	if (get_urecord(d, &uid, &r) != 0) {
		unlock_udomain(d);
		WARN("urecord not found\n");
		return -1;
	}

	if (get_ucontact(r, &m->new_uri, &contact) != 0) {
		unlock_udomain(d);
		WARN("ucontact not found\n");
		return -1;
	}

	load_reg_avps(contact);
	
	unlock_udomain(d);
	
	return 1;
}


int dup_reg_avps(struct ucontact *dst, struct ucontact *src)
{
	struct usr_avp *avp, *dup;
	avp_t *first, *last;
	
	/* no locking here! TODO: do it in package memory !!! */

	if (!use_reg_avps()) return 0; /* don't use reg avps */
	
	/* destroy old AVPs */
	/* if (dst->avps) db_delete_reg_avps(dst); */
	reg_destroy_avps(dst->avps);

	last = NULL;
	first = NULL;
	
	avp = src->avps;
	while (avp) {
		dup = avp_dup(avp);
		if (dup) {
			/* add AVP into list */
			if (last) last->next = dup;
			else first = dup;
			last = dup;
		}
		avp = avp->next;
	}

	dst->avps = first;
/*	if (dst->avps) db_save_reg_avps(dst); */
	
	return 0;
}
