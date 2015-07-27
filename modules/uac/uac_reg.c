/**
 * Copyright (C) 2010 Daniel-Constantin Mierla (asipto.com)
 *
 * This file is part of kamailio, a free SIP server.
 *
 * Kamailio is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * Kamailio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <time.h>

#include "../../dprint.h"
#include "../../timer.h"

#include "../../mem/shm_mem.h"
#include "../../lib/srdb1/db.h"
#include "../../ut.h"
#include "../../trim.h"
#include "../../hashes.h"
#include "../../locking.h"
#include "../../parser/parse_uri.h"
#include "../../parser/parse_from.h"
#include "../../parser/parse_to.h"
#include "../../parser/contact/parse_contact.h"
#include "../../rpc.h"
#include "../../rpc_lookup.h"

#include "../../modules/tm/tm_load.h"

#include "auth.h"
#include "auth_hdr.h"
#include "uac_reg.h"

#define UAC_REG_DISABLED	(1<<0)
#define UAC_REG_ONGOING		(1<<1)
#define UAC_REG_ONLINE		(1<<2)
#define UAC_REG_AUTHSENT	(1<<3)

#define MAX_UACH_SIZE 2048
#define UAC_REG_GC_INTERVAL	150
#define UAC_REG_MAX_PASSWD_SIZE	63

typedef struct _reg_uac
{
	unsigned int h_uuid;
	unsigned int h_user;
	str   l_uuid;
	str   l_username;
	str   l_domain;
	str   r_username;
	str   r_domain;
	str   realm;
	str   auth_proxy;
	str   auth_username;
	str   auth_password;
	unsigned int flags;
	unsigned int expires;
	time_t timer_expires;
} reg_uac_t;

typedef struct _reg_item
{
	reg_uac_t *r;
	struct _reg_item *next;
} reg_item_t;


typedef struct _reg_entry
{
	unsigned int isize;
	unsigned int usize;
	reg_item_t *byuser;
	reg_item_t *byuuid;
	gen_lock_t lock;
} reg_entry_t;

typedef struct _reg_ht
{
	unsigned int htsize;
	time_t stime;
	reg_entry_t *entries;
} reg_ht_t;

static reg_ht_t *_reg_htable = NULL;
static reg_ht_t *_reg_htable_gc = NULL;
static gen_lock_t *_reg_htable_gc_lock = NULL;

int reg_use_domain = 0;
int reg_timer_interval = 90;
int reg_retry_interval = 0;
int reg_htable_size = 4;
int reg_fetch_rows = 1000;
str reg_contact_addr = STR_NULL;
str reg_db_url = STR_NULL;
str reg_db_table = str_init("uacreg");

str l_uuid_column = str_init("l_uuid");
str l_username_column = str_init("l_username");
str l_domain_column = str_init("l_domain");
str r_username_column = str_init("r_username");
str r_domain_column = str_init("r_domain");
str realm_column = str_init("realm");
str auth_username_column = str_init("auth_username");
str auth_password_column = str_init("auth_password");
str auth_proxy_column = str_init("auth_proxy");
str expires_column = str_init("expires");


#if 0
INSERT INTO version (table_name, table_version) values ('uacreg','1');
CREATE TABLE uacreg (
    id INT(10) UNSIGNED AUTO_INCREMENT PRIMARY KEY NOT NULL,
    l_uuid VARCHAR(64) NOT NULL,
    l_username VARCHAR(64) NOT NULL,
    l_domain VARCHAR(128) DEFAULT '' NOT NULL,
    r_username VARCHAR(64) NOT NULL,
    r_domain VARCHAR(128) NOT NULL,
    realm VARCHAR(64) NOT NULL,
    auth_username VARCHAR(64) NOT NULL,
    auth_password VARCHAR(64) NOT NULL,
    auth_proxy VARCHAR(128) DEFAULT '' NOT NULL,
    expires INT(10) UNSIGNED DEFAULT 0 NOT NULL,
    CONSTRAINT l_uuid_idx UNIQUE (l_uuid)
) ENGINE=MyISAM;
#endif


extern struct tm_binds uac_tmb;
extern pv_spec_t auth_username_spec;
extern pv_spec_t auth_realm_spec;
extern pv_spec_t auth_password_spec;

counter_handle_t regtotal;         /* Total number of registrations in memory */
counter_handle_t regactive;        /* Active registrations - 200 OK */
counter_handle_t regdisabled;      /* Disabled registrations */

/* Init counters */
static void uac_reg_counter_init()
{
	LM_DBG("*** Initializing UAC reg counters\n");
        counter_register(&regtotal, "uac", "regtotal", 0, 0, 0, "Total number of registration accounts in memory", 0);
        counter_register(&regactive, "uac", "regactive", 0, 0, 0, "Number of successfully registred accounts (200 OK)", 0);
        counter_register(&regdisabled, "uac", "regdisabled", 0, 0, 0, "Counter of failed registrations (not 200 OK)", 0);
}


/**
 * Init the in-memory registration database in hash table
 */
int uac_reg_init_ht(unsigned int sz)
{
	int i;

	_reg_htable_gc_lock = (gen_lock_t*)shm_malloc(sizeof(gen_lock_t));
	if(_reg_htable_gc_lock == NULL)
	{
		LM_ERR("no more shm for lock\n");
		return -1;
	}
	if(lock_init(_reg_htable_gc_lock)==0)
	{
		LM_ERR("cannot init global lock\n");
		shm_free((void*)_reg_htable_gc_lock);
		return -1;
	}
	_reg_htable_gc = (reg_ht_t*)shm_malloc(sizeof(reg_ht_t));
	if(_reg_htable_gc==NULL)
	{
		LM_ERR("no more shm\n");
		lock_destroy(_reg_htable_gc_lock);
		shm_free((void*)_reg_htable_gc_lock);
		return -1;
	}
	memset(_reg_htable_gc, 0, sizeof(reg_ht_t));
	_reg_htable_gc->htsize = sz;

	_reg_htable_gc->entries =
			(reg_entry_t*)shm_malloc(_reg_htable_gc->htsize*sizeof(reg_entry_t));
	if(_reg_htable_gc->entries==NULL)
	{
		LM_ERR("no more shm.\n");
		shm_free(_reg_htable_gc);
		lock_destroy(_reg_htable_gc_lock);
		shm_free((void*)_reg_htable_gc_lock);
		return -1;
	}
	memset(_reg_htable_gc->entries, 0, _reg_htable_gc->htsize*sizeof(reg_entry_t));


	_reg_htable = (reg_ht_t*)shm_malloc(sizeof(reg_ht_t));
	if(_reg_htable==NULL)
	{
		LM_ERR("no more shm\n");
		shm_free(_reg_htable_gc->entries);
		shm_free(_reg_htable_gc);
		lock_destroy(_reg_htable_gc_lock);
		shm_free((void*)_reg_htable_gc_lock);
		return -1;
	}
	memset(_reg_htable, 0, sizeof(reg_ht_t));
	_reg_htable->htsize = sz;

	_reg_htable->entries =
			(reg_entry_t*)shm_malloc(_reg_htable->htsize*sizeof(reg_entry_t));
	if(_reg_htable->entries==NULL)
	{
		LM_ERR("no more shm.\n");
		shm_free(_reg_htable_gc->entries);
		shm_free(_reg_htable_gc);
		shm_free(_reg_htable);
		lock_destroy(_reg_htable_gc_lock);
		shm_free((void*)_reg_htable_gc_lock);
		return -1;
	}
	memset(_reg_htable->entries, 0, _reg_htable->htsize*sizeof(reg_entry_t));
	for(i=0; i<_reg_htable->htsize; i++)
	{
		if(lock_init(&_reg_htable->entries[i].lock)==0)
		{
			LM_ERR("cannot initialize lock[%d] n", i);
			i--;
			while(i>=0)
			{
				lock_destroy(&_reg_htable->entries[i].lock);
				i--;
			}
			shm_free(_reg_htable->entries);
			shm_free(_reg_htable);
			shm_free(_reg_htable_gc->entries);
			shm_free(_reg_htable_gc);
			lock_destroy(_reg_htable_gc_lock);
			shm_free((void*)_reg_htable_gc_lock);
			return -1;
		}
	}

	/* Initialize uac reg counters */
	uac_reg_counter_init();

	return 0;
}

/**
 *
 */
int uac_reg_free_ht(void)
{
	int i;
	reg_item_t *it = NULL;
	reg_item_t *it0 = NULL;

	if(_reg_htable_gc_lock != NULL)
	{
		lock_destroy(_reg_htable_gc_lock);
		shm_free((void*)_reg_htable_gc_lock);
		_reg_htable_gc_lock = NULL;
	}
	if(_reg_htable_gc!=NULL)
	{
		for(i=0; i<_reg_htable_gc->htsize; i++)
		{
			it = _reg_htable_gc->entries[i].byuuid;
			while(it)
			{
				it0 = it;
				it = it->next;
				shm_free(it0);
			}
			it = _reg_htable_gc->entries[i].byuser;
			while(it)
			{
				it0 = it;
				it = it->next;
				shm_free(it0->r);
				shm_free(it0);
			}
		}
		shm_free(_reg_htable_gc->entries);
		shm_free(_reg_htable_gc);
		_reg_htable_gc = NULL;
	}

	if(_reg_htable==NULL)
	{
		LM_DBG("no hash table\n");
		return -1;
	}
	for(i=0; i<_reg_htable->htsize; i++)
	{
		lock_get(&_reg_htable->entries[i].lock);
		/* free entries */
		it = _reg_htable->entries[i].byuuid;
		while(it)
		{
			it0 = it;
			it = it->next;
			shm_free(it0);
		}
		it = _reg_htable->entries[i].byuser;
		while(it)
		{
			it0 = it;
			it = it->next;
			shm_free(it0->r);
			shm_free(it0);
		}
		lock_destroy(&_reg_htable->entries[i].lock);
	}
	shm_free(_reg_htable->entries);
	shm_free(_reg_htable);
	_reg_htable = NULL;
	return 0;
}

/**
 *
 */
int uac_reg_reset_ht_gc(void)
{
	int i;
	reg_item_t *it = NULL;
	reg_item_t *it0 = NULL;

	if(_reg_htable_gc==NULL)
	{
		LM_DBG("no hash table\n");
		return -1;
	}
	for(i=0; i<_reg_htable_gc->htsize; i++)
	{
		/* free entries */
		it = _reg_htable_gc->entries[i].byuuid;
		while(it)
		{
			it0 = it;
			it = it->next;
			shm_free(it0);
		}
		_reg_htable_gc->entries[i].byuuid = NULL;
		_reg_htable_gc->entries[i].isize=0;
		it = _reg_htable_gc->entries[i].byuser;
		while(it)
		{
			it0 = it;
			it = it->next;
			shm_free(it0->r);
			shm_free(it0);
		}
		_reg_htable_gc->entries[i].byuser = NULL;
		_reg_htable_gc->entries[i].usize = 0;
	}
	/* Reset all counters */
	counter_reset(regtotal);
	counter_reset(regactive);
	counter_reset(regdisabled);
	return 0;
}

/**
 *
 */
int uac_reg_ht_shift(void)
{
	time_t tn;
	int i;

	if(_reg_htable==NULL || _reg_htable_gc==NULL)
	{
		LM_ERR("data struct invalid\n");
		return -1;
	}
	tn = time(NULL);

	lock_get(_reg_htable_gc_lock);
	if(_reg_htable_gc->stime > tn-UAC_REG_GC_INTERVAL) {
		lock_release(_reg_htable_gc_lock);
		LM_ERR("shifting the memory table is not possible in less than %d secs\n", UAC_REG_GC_INTERVAL);
		return -1;
	}
	uac_reg_reset_ht_gc();
	for(i=0; i<_reg_htable->htsize; i++)
	{
		/* shift entries */
		_reg_htable_gc->entries[i].byuuid = _reg_htable->entries[i].byuuid;
		_reg_htable_gc->entries[i].byuser = _reg_htable->entries[i].byuser;
		_reg_htable_gc->stime = time(NULL);

		/* reset active table entries */
		_reg_htable->entries[i].byuuid = NULL;
		_reg_htable->entries[i].isize=0;
		_reg_htable->entries[i].byuser = NULL;
		_reg_htable->entries[i].usize = 0;
	}
	lock_release(_reg_htable_gc_lock);
	return 0;
}

#define reg_compute_hash(_s)		get_hash1_raw((_s)->s,(_s)->len)
#define reg_get_entry(_h,_size)		((_h)&((_size)-1))

/**
 *
 */
int reg_ht_add_byuuid(reg_uac_t *reg)
{
	unsigned int slot;
	reg_item_t *ri = NULL;

	if(_reg_htable==NULL)
	{
		LM_ERR("reg hash table not initialized\n");
		return -1;
	}

	ri = (reg_item_t*)shm_malloc(sizeof(reg_item_t));
	if(ri==NULL)
	{
		LM_ERR("no more shm\n");
		return -1;
	}
	memset(ri, 0, sizeof(reg_item_t));
	slot = reg_get_entry(reg->h_uuid, _reg_htable->htsize);
	ri->r = reg;
	lock_get(&_reg_htable->entries[slot].lock);
	ri->next = _reg_htable->entries[slot].byuuid;
	_reg_htable->entries[slot].byuuid = ri;
	_reg_htable->entries[slot].isize++;
	lock_release(&_reg_htable->entries[slot].lock);
	return 0;
}

/**
 *
 */
int reg_ht_add_byuser(reg_uac_t *reg)
{
	unsigned int slot;
	reg_item_t *ri = NULL;

	if(_reg_htable==NULL)
	{
		LM_ERR("reg hash table not initialized\n");
		return -1;
	}

	ri = (reg_item_t*)shm_malloc(sizeof(reg_item_t));
	if(ri==NULL)
	{
		LM_ERR("no more shm\n");
		return -1;
	}
	memset(ri, 0, sizeof(reg_item_t));
	slot = reg_get_entry(reg->h_user, _reg_htable->htsize);
	ri->r = reg;
	lock_get(&_reg_htable->entries[slot].lock);
	ri->next = _reg_htable->entries[slot].byuser;
	_reg_htable->entries[slot].byuser = ri;
	_reg_htable->entries[slot].usize++;
	lock_release(&_reg_htable->entries[slot].lock);
	return 0;
}

#define reg_copy_shm(dst, src) do { \
		if((src)->s!=NULL) { \
			(dst)->s = p; \
			strncpy((dst)->s, (src)->s, (src)->len); \
			(dst)->len = (src)->len; \
			(dst)->s[(dst)->len] = '\0'; \
			p = p + (dst)->len + 1; \
		} \
	} while(0);

/**
 *
 */
int reg_ht_add(reg_uac_t *reg)
{
	int len;
	reg_uac_t *nr = NULL;
	char *p;

	if(reg==NULL || _reg_htable==NULL)
	{
		LM_ERR("bad parameters: %p/%p\n", reg, _reg_htable);
		return -1;
	}
	if(reg->auth_password.len>UAC_REG_MAX_PASSWD_SIZE)
	{
		LM_ERR("bad parameters: %p/%p -- password too long %d\n",
				reg, _reg_htable, reg->auth_password.len);
		return -1;
	}

	len = reg->l_uuid.len + 1
			+ reg->l_username.len + 1
			+ reg->l_domain.len + 1
			+ reg->r_username.len + 1
			+ reg->r_domain.len + 1
			+ reg->realm.len + 1
			+ reg->auth_proxy.len + 1
			+ reg->auth_username.len + 1
			+ UAC_REG_MAX_PASSWD_SIZE /*reg->auth_password.len*/ + 1;
	nr = (reg_uac_t*)shm_malloc(sizeof(reg_uac_t) + len);
	if(nr==NULL)
	{
		LM_ERR("no more shm\n");
		return -1;
	}
	memset(nr, 0, sizeof(reg_uac_t) + len);
	nr->expires = reg->expires;
	nr->h_uuid = reg_compute_hash(&reg->l_uuid);
	nr->h_user = reg_compute_hash(&reg->l_username);
	
	p = (char*)nr + sizeof(reg_uac_t);

	reg_copy_shm(&nr->l_uuid, &reg->l_uuid);
	reg_copy_shm(&nr->l_username, &reg->l_username);
	reg_copy_shm(&nr->l_domain, &reg->l_domain);
	reg_copy_shm(&nr->r_username, &reg->r_username);
	reg_copy_shm(&nr->r_domain, &reg->r_domain);
	reg_copy_shm(&nr->realm, &reg->realm);
	reg_copy_shm(&nr->auth_proxy, &reg->auth_proxy);
	reg_copy_shm(&nr->auth_username, &reg->auth_username);
	/* password at the end, to be able to update it easily */
	reg_copy_shm(&nr->auth_password, &reg->auth_password);

	reg_ht_add_byuser(nr);
	reg_ht_add_byuuid(nr);
	counter_inc(regtotal);

	return 0;
}


/**
 *
 */
int reg_ht_update_password(reg_uac_t *reg)
{
	unsigned int slot;
	reg_item_t *ri = NULL;

	if(_reg_htable==NULL)
	{
		LM_ERR("reg hash table not initialized\n");
		return -1;
	}

	if(reg->auth_password.len>UAC_REG_MAX_PASSWD_SIZE)
	{
		LM_ERR("password is too big: %d\n", reg->auth_password.len);
		return -1;
	}

	slot = reg_get_entry(reg->h_user, _reg_htable->htsize);
	lock_get(&_reg_htable->entries[slot].lock);
	ri = _reg_htable->entries[slot].byuser;
	while(ri) {
		if(ri->r->l_uuid.len == reg->l_uuid.len
				&& strncmp(ri->r->l_uuid.s, reg->l_uuid.s, reg->l_uuid.len)==0)
		{
			/* record found */
			strncpy(ri->r->auth_password.s, reg->auth_password.s, reg->auth_password.len);
			ri->r->auth_password.len = reg->auth_password.len;
			ri->r->auth_password.s[reg->auth_password.len] = '\0';
			lock_release(&_reg_htable->entries[slot].lock);
			return 0;
		}
		ri = ri->next;
	}
	lock_release(&_reg_htable->entries[slot].lock);
	return -1;
}

/**
 *
 */
reg_uac_t *reg_ht_get_byuuid(str *uuid)
{
	unsigned int hash;
	unsigned int slot;
	reg_item_t *it = NULL;

	if(_reg_htable==NULL)
	{
		LM_ERR("reg hash table not initialized\n");
		return NULL;
	}

	hash = reg_compute_hash(uuid);
	slot = reg_get_entry(hash, _reg_htable->htsize);
	it = _reg_htable->entries[slot].byuuid;
	while(it)
	{
		if((it->r->h_uuid==hash) && (it->r->l_uuid.len==uuid->len)
				&& (strncmp(it->r->l_uuid.s, uuid->s, uuid->len)==0))
		{
			return it->r;
		}
		it = it->next;
	}
	return NULL;
}

/**
 *
 */
reg_uac_t *reg_ht_get_byuser(str *user, str *domain)
{
	unsigned int hash;
	unsigned int slot;
	reg_item_t *it = NULL;

	if(_reg_htable==NULL)
	{
		LM_ERR("reg hash table not initialized\n");
		return NULL;
	}

	hash = reg_compute_hash(user);
	slot = reg_get_entry(hash, _reg_htable->htsize);
	it = _reg_htable->entries[slot].byuser;
	while(it)
	{
		if((it->r->h_uuid==hash) && (it->r->l_username.len==user->len)
				&& (strncmp(it->r->l_username.s, user->s, user->len)==0))
		{
			if(domain!=NULL && domain->s!=NULL)
			{
				if((it->r->l_domain.len==domain->len)
				&& (strncmp(it->r->l_domain.s, domain->s, domain->len)==0))
				{
					return it->r;
				}
			} else {
				return it->r;
			}
		}
		it = it->next;
	}
	return NULL;
}

int uac_reg_tmdlg(dlg_t *tmdlg, sip_msg_t *rpl)
{
	if(tmdlg==NULL || rpl==NULL)
		return -1;

	if (parse_headers(rpl, HDR_EOH_F, 0) < 0) {
		LM_ERR("error while parsing all headers in the reply\n");
		return -1;
	}
	if(parse_to_header(rpl)<0 || parse_from_header(rpl)<0) {
		LM_ERR("error while parsing From/To headers in the reply\n");
		return -1;
	}
	memset(tmdlg, 0, sizeof(dlg_t));

	str2int(&(get_cseq(rpl)->number), &tmdlg->loc_seq.value);
	tmdlg->loc_seq.is_set = 1;

	tmdlg->id.call_id = rpl->callid->body;
	trim(&tmdlg->id.call_id);

	if (get_from(rpl)->tag_value.len) {
		tmdlg->id.loc_tag = get_from(rpl)->tag_value;
	}
#if 0
	if (get_to(rpl)->tag_value.len) {
		tmdlg->id.rem_tag = get_to(rpl)->tag_value;
	}
#endif
	tmdlg->loc_uri = get_from(rpl)->uri;
	tmdlg->rem_uri = get_to(rpl)->uri;
	tmdlg->state= DLG_CONFIRMED;
	return 0;
}

void uac_reg_tm_callback( struct cell *t, int type, struct tmcb_params *ps)
{
	char *uuid;
	str suuid;
	reg_uac_t *ri = NULL;
	contact_t* c;
	int expires;
	struct sip_uri puri;
	struct hdr_field *hdr;
	HASHHEX response;
	str *new_auth_hdr = NULL;
	static struct authenticate_body auth;
	struct uac_credential cred;
	char  b_ruri[MAX_URI_SIZE];
	str   s_ruri;
#ifdef UAC_OLD_AUTH
	char  b_turi[MAX_URI_SIZE];
	str   s_turi;
#endif
	char  b_hdrs[MAX_UACH_SIZE];
	str   s_hdrs;
	uac_req_t uac_r;
	str method = {"REGISTER", 8};
	int ret;
	dlg_t tmdlg;

	if(ps->param==NULL || *ps->param==0)
	{
		LM_DBG("uuid not received\n");
		return;
	}
	uuid = *((char**)ps->param);
	LM_DBG("completed with status %d [uuid: %s]\n",
		ps->code, uuid);
	suuid.s = uuid;
	suuid.len = strlen(suuid.s);
	ri = reg_ht_get_byuuid(&suuid);

	if(ri==NULL)
	{
		LM_DBG("no user with uuid %s\n", uuid);
		goto done;
	}

	if(ps->code == 200)
	{
		if (parse_headers(ps->rpl, HDR_EOH_F, 0) == -1)
		{
			LM_ERR("failed to parse headers\n");
			goto error;
		}
		if (ps->rpl->contact==NULL)
		{
			LM_ERR("no Contact found\n");
			goto error;
		}
		if (parse_contact(ps->rpl->contact) < 0)
		{
			LM_ERR("failed to parse Contact HF\n");
			goto error;
		}
		if (((contact_body_t*)ps->rpl->contact->parsed)->star)
		{
			LM_DBG("* Contact found\n");
			goto done;
		}

		if (contact_iterator(&c, ps->rpl, 0) < 0)
			goto done;
		while(c)
		{
			if(parse_uri(c->uri.s, c->uri.len, &puri)!=0)
			{
				LM_ERR("failed to parse c-uri\n");
				goto error;
			}
			if(suuid.len==puri.user.len
					&& (strncmp(puri.user.s, suuid.s, suuid.len)==0))
			{
				/* calculate expires */
				expires=0;
				if(c->expires==NULL || c->expires->body.len<=0)
				{
					if(ps->rpl->expires!=NULL && ps->rpl->expires->body.len>0)
						expires = atoi(ps->rpl->expires->body.s);
				} else {
					str2int(&c->expires->body, (unsigned int*)(&expires));
				}
				ri->timer_expires = ri->timer_expires + expires;
				ri->flags |= UAC_REG_ONLINE;
				goto done;
			}
			if (contact_iterator(&c, ps->rpl, c) < 0)
			{
				LM_DBG("local contact not found\n");
				goto done;
			}
		}
	}

	if(ps->code == 401 || ps->code == 407)
	{
		if(ri->flags & UAC_REG_AUTHSENT)
		{
			LM_ERR("authentication failed for <%.*s>\n",
					ri->l_uuid.len, ri->l_uuid.s);
			goto error;
		}
		hdr = get_autenticate_hdr(ps->rpl, ps->code);
		if (hdr==0)
		{
			LM_ERR("failed to extract authenticate hdr\n");
			goto error;
		}

		LM_DBG("auth header body [%.*s]\n",
			hdr->body.len, hdr->body.s);

		if (parse_authenticate_body(&hdr->body, &auth)<0)
		{
			LM_ERR("failed to parse auth hdr body\n");
			goto error;
		}
		if (ri->realm.len>0) {
			/* only check if realms match if it is non-empty */
			if(auth.realm.len!=ri->realm.len
					|| strncmp(auth.realm.s, ri->realm.s, ri->realm.len)!=0)
			{
				LM_ERR("realms do not match. requested realm: [%.*s]\n",
					auth.realm.len, auth.realm.s);
				goto error;
			}
		}
		cred.realm = auth.realm;
		cred.user = ri->auth_username; 
		cred.passwd = ri->auth_password;
 		cred.next = NULL;

		snprintf(b_ruri, MAX_URI_SIZE, "sip:%.*s",
				ri->r_domain.len, ri->r_domain.s);
		s_ruri.s = b_ruri; s_ruri.len = strlen(s_ruri.s);

		do_uac_auth(&method, &s_ruri, &cred, &auth, response);
		new_auth_hdr=build_authorization_hdr(ps->code, &s_ruri, &cred,
						&auth, response);
		if (new_auth_hdr==0)
		{
			LM_ERR("failed to build authorization hdr\n");
			goto error;
		}
		
#ifdef UAC_OLD_AUTH
		snprintf(b_turi, MAX_URI_SIZE, "sip:%.*s@%.*s",
				ri->r_username.len, ri->r_username.s,
				ri->r_domain.len, ri->r_domain.s);
		s_turi.s = b_turi; s_turi.len = strlen(s_turi.s);
#endif
		snprintf(b_hdrs, MAX_UACH_SIZE,
				"Contact: <sip:%.*s@%.*s>\r\n"
				"Expires: %d\r\n"
				"%.*s",
				ri->l_uuid.len, ri->l_uuid.s,
				reg_contact_addr.len, reg_contact_addr.s,
				ri->expires,
				new_auth_hdr->len, new_auth_hdr->s);
		s_hdrs.s = b_hdrs; s_hdrs.len = strlen(s_hdrs.s);
		pkg_free(new_auth_hdr->s);

		memset(&uac_r, 0, sizeof(uac_r));
		if(uac_reg_tmdlg(&tmdlg, ps->rpl)<0)
		{
			LM_ERR("failed to build tm dialog\n");
			goto error;
		}
		tmdlg.rem_target = s_ruri;
		if(ri->auth_proxy.len)
			tmdlg.dst_uri = ri->auth_proxy;
		uac_r.method = &method;
		uac_r.headers = &s_hdrs;
		uac_r.dialog = &tmdlg;
		uac_r.cb_flags = TMCB_LOCAL_COMPLETED;
		/* Callback function */
		uac_r.cb  = uac_reg_tm_callback;
		/* Callback parameter */
		uac_r.cbp = (void*)uuid;
#ifdef UAC_OLD_AUTH
		ret = uac_tmb.t_request(&uac_r,  /* UAC Req */
				&s_ruri, /* Request-URI */
				&s_turi, /* To */
				&s_turi, /* From */
				(ri->auth_proxy.len)?&ri->auth_proxy:NULL /* outbound uri */
			);
#endif
		ret = uac_tmb.t_request_within(&uac_r);

		if(ret<0) {
			LM_ERR("failed to send request with authentication for [%.*s]",
			       ri->l_uuid.len, ri->l_uuid.s);
			goto error;
		}

		ri->flags |= UAC_REG_AUTHSENT;
		return;
	} else
	{
		LM_ERR("got sip response %d while registering [%.*s]\n",
		       ps->code, ri->l_uuid.len, ri->l_uuid.s);
		goto error;
	}

error:
	if(reg_retry_interval) {
		ri->timer_expires = time(NULL) + reg_retry_interval;
	} else {
		ri->flags |= UAC_REG_DISABLED;
		counter_inc(regdisabled);
	}
done:
	if(ri)
		ri->flags &= ~(UAC_REG_ONGOING|UAC_REG_AUTHSENT);
	shm_free(uuid);
	counter_inc(regactive);
}

int uac_reg_update(reg_uac_t *reg, time_t tn)
{
	char *uuid;
	uac_req_t uac_r;
	str method = {"REGISTER", 8};
	int ret;
	char  b_ruri[MAX_URI_SIZE];
	str   s_ruri;
	char  b_turi[MAX_URI_SIZE];
	str   s_turi;
	char  b_hdrs[MAX_UACH_SIZE];
	str   s_hdrs;

	if(uac_tmb.t_request==NULL)
		return -1;
	if(reg->expires==0)
		return 1;
	if(reg->flags&UAC_REG_ONGOING) {
		if (reg->timer_expires > tn - reg_retry_interval)
			return 2;
		LM_DBG("record marked as ongoing registration (%d) - resetting\n",
				(int)reg->flags);
		reg->flags &= ~(UAC_REG_ONLINE|UAC_REG_AUTHSENT);
	}
	if(reg->flags&UAC_REG_DISABLED)
		return 4;
	if(reg->timer_expires > tn + reg_timer_interval + 3)
		return 3;
	uuid = (char*)shm_malloc(reg->l_uuid.len+1);
	if(uuid==NULL)
	{
		LM_ERR("no more shm\n");
		return -1;
	}
	reg->timer_expires = tn;
	reg->flags |= UAC_REG_ONGOING;
	reg->flags &= ~UAC_REG_ONLINE;
	counter_add(regactive, -1);		/* Take it out of the active pool while re-registering */
	memcpy(uuid, reg->l_uuid.s, reg->l_uuid.len);
	uuid[reg->l_uuid.len] = '\0';

	snprintf(b_ruri, MAX_URI_SIZE, "sip:%.*s",
			reg->r_domain.len, reg->r_domain.s);
	s_ruri.s = b_ruri; s_ruri.len = strlen(s_ruri.s);

	snprintf(b_turi, MAX_URI_SIZE, "sip:%.*s@%.*s",
			reg->r_username.len, reg->r_username.s,
			reg->r_domain.len, reg->r_domain.s);
	s_turi.s = b_turi; s_turi.len = strlen(s_turi.s);

	snprintf(b_hdrs, MAX_UACH_SIZE,
			"Contact: <sip:%.*s@%.*s>\r\n"
			"Expires: %d\r\n",
			reg->l_uuid.len, reg->l_uuid.s,
			reg_contact_addr.len, reg_contact_addr.s,
			reg->expires);
	s_hdrs.s = b_hdrs; s_hdrs.len = strlen(s_hdrs.s);

	memset(&uac_r, '\0', sizeof(uac_r));
	uac_r.method = &method;
	uac_r.headers = &s_hdrs;
	uac_r.cb_flags = TMCB_LOCAL_COMPLETED;
	/* Callback function */
	uac_r.cb  = uac_reg_tm_callback;
	/* Callback parameter */
	uac_r.cbp = (void*)uuid;
	ret = uac_tmb.t_request(&uac_r,  /* UAC Req */
			&s_ruri, /* Request-URI */
			&s_turi, /* To */
			&s_turi, /* From */
			(reg->auth_proxy.len)?&reg->auth_proxy:NULL /* outbound uri */
		);

	if(ret<0)
	{
		LM_ERR("failed to send request for [%.*s]", reg->l_uuid.len, reg->l_uuid.s);
		shm_free(uuid);
		if (reg_retry_interval)
			reg->timer_expires = (tn ? tn : time(NULL)) + reg_retry_interval;
		else {
			reg->flags |= UAC_REG_DISABLED;
			counter_inc(regdisabled);
		}
		reg->flags &= ~UAC_REG_ONGOING;
		return -1;
	}
	return 0;
}

/**
 *
 */
void uac_reg_timer(unsigned int ticks)
{
	int i;
	reg_item_t *it = NULL;
	time_t tn;

	if(_reg_htable==NULL)
		return;

	tn = time(NULL);
	for(i=0; i<_reg_htable->htsize; i++)
	{
		/* walk through entries */
		it = _reg_htable->entries[i].byuuid;
		while(it)
		{
			uac_reg_update(it->r, tn);
			it = it->next;
		}
	}

	if(_reg_htable_gc!=NULL)
	{
		lock_get(_reg_htable_gc_lock);
		if(_reg_htable_gc->stime!=0
				&& _reg_htable_gc->stime < tn - UAC_REG_GC_INTERVAL)
			uac_reg_reset_ht_gc();
		lock_release(_reg_htable_gc_lock);
	}
}

#define reg_db_set_attr(attr, pos) do { \
		if(!VAL_NULL(&RES_ROWS(db_res)[i].values[pos])) { \
			reg.attr.s = \
				(char*)(RES_ROWS(db_res)[i].values[pos].val.string_val); \
			reg.attr.len = strlen(reg.attr.s); \
			if(reg.attr.len == 0) { \
				LM_ERR("empty value not allowed for column[%d]='%.*s' - ignoring record\n", \
						pos, db_cols[pos]->len, db_cols[pos]->s); \
				continue; \
			} \
		} \
	} while(0);

/**
 *
 */
int uac_reg_load_db(void)
{
	db1_con_t *reg_db_con = NULL;
	db_func_t reg_dbf;
	reg_uac_t reg;
	db_key_t db_cols[10] = {
		&l_uuid_column,
		&l_username_column,
		&l_domain_column,
		&r_username_column,
		&r_domain_column,
		&realm_column,
		&auth_username_column,
		&auth_password_column,
		&auth_proxy_column,
		&expires_column
	};
	db1_res_t* db_res = NULL;
	int i, ret;

	/* binding to db module */
	if(reg_db_url.s==NULL)
	{
		LM_ERR("no db url\n");
		return -1;
	}

	if(db_bind_mod(&reg_db_url, &reg_dbf))
	{
		LM_ERR("database module not found\n");
		return -1;
	}

	if (!DB_CAPABILITY(reg_dbf, DB_CAP_ALL))
	{
		LM_ERR("database module does not "
		    "implement all functions needed by the module\n");
		return -1;
	}

	/* open a connection with the database */
	reg_db_con = reg_dbf.init(&reg_db_url);
	if(reg_db_con==NULL)
	{
		LM_ERR("failed to connect to the database\n");
		return -1;
	}
	if (reg_dbf.use_table(reg_db_con, &reg_db_table) < 0)
	{
		LM_ERR("failed to use_table\n");
		return -1;
	}

	if (DB_CAPABILITY(reg_dbf, DB_CAP_FETCH)) {
		if(reg_dbf.query(reg_db_con, 0, 0, 0, db_cols, 0, 10, 0, 0) < 0)
		{
			LM_ERR("Error while querying db\n");
			return -1;
		}
		if(reg_dbf.fetch_result(reg_db_con, &db_res, reg_fetch_rows)<0)
		{
			LM_ERR("Error while fetching result\n");
			if (db_res)
				reg_dbf.free_result(reg_db_con, db_res);
			goto error;
		} else {
			if(RES_ROW_N(db_res)==0)
			{
				goto done;
			}
		}
	} else {
		if((ret=reg_dbf.query(reg_db_con, NULL, NULL, NULL, db_cols,
				0, 10, 0, &db_res))!=0
			|| RES_ROW_N(db_res)<=0 )
		{
			reg_dbf.free_result(reg_db_con, db_res);
			if( ret==0)
			{
				return 0;
			} else {
				goto error;
			}
		}
	}

	do {
		for(i=0; i<RES_ROW_N(db_res); i++)
		{
			memset(&reg, 0, sizeof(reg_uac_t));;
			/* check for NULL values ?!?! */
			reg_db_set_attr(l_uuid, 0);
			reg_db_set_attr(l_username, 1);
			reg_db_set_attr(l_domain, 2);
			reg_db_set_attr(r_username, 3);
			reg_db_set_attr(r_domain, 4);
			/* realm may be empty */
			if(!VAL_NULL(&RES_ROWS(db_res)[i].values[5])) {
				reg.realm.s = (char*)(RES_ROWS(db_res)[i].values[5].val.string_val);
				reg.realm.len = strlen(reg.realm.s);
			}
			reg_db_set_attr(auth_username, 6);
			reg_db_set_attr(auth_password, 7);
			reg_db_set_attr(auth_proxy, 8);
			reg.expires
				= (unsigned int)RES_ROWS(db_res)[i].values[9].val.int_val;
			
			if(reg_ht_add(&reg)<0)
			{
				LM_ERR("Error adding reg to htable\n");
				goto error;
			}
	 	}
		if (DB_CAPABILITY(reg_dbf, DB_CAP_FETCH)) {
			if(reg_dbf.fetch_result(reg_db_con, &db_res, reg_fetch_rows)<0) {
				LM_ERR("Error while fetching!\n");
				if (db_res)
					reg_dbf.free_result(reg_db_con, db_res);
				goto error;
			}
		} else {
			break;
		}
	}  while(RES_ROW_N(db_res)>0);
	reg_dbf.free_result(reg_db_con, db_res);
	reg_dbf.close(reg_db_con);

done:
	return 0;

error:
	if (reg_db_con) {
		reg_dbf.free_result(reg_db_con, db_res);
		reg_dbf.close(reg_db_con);
	}
	return -1;
}

/**
 *
 */
int uac_reg_db_refresh(str *pl_uuid)
{
	db1_con_t *reg_db_con = NULL;
	db_func_t reg_dbf;
	reg_uac_t reg;
	db_key_t db_cols[10] = {
		&l_uuid_column,
		&l_username_column,
		&l_domain_column,
		&r_username_column,
		&r_domain_column,
		&realm_column,
		&auth_username_column,
		&auth_password_column,
		&auth_proxy_column,
		&expires_column
	};
	db_key_t db_keys[1] = {&l_uuid_column};
	db_val_t db_vals[1];

	db1_res_t* db_res = NULL;
	int i, ret;

	/* binding to db module */
	if(reg_db_url.s==NULL)
	{
		LM_ERR("no db url\n");
		return -1;
	}

	if(db_bind_mod(&reg_db_url, &reg_dbf))
	{
		LM_ERR("database module not found\n");
		return -1;
	}

	if (!DB_CAPABILITY(reg_dbf, DB_CAP_ALL))
	{
		LM_ERR("database module does not "
		    "implement all functions needed by the module\n");
		return -1;
	}

	/* open a connection with the database */
	reg_db_con = reg_dbf.init(&reg_db_url);
	if(reg_db_con==NULL)
	{
		LM_ERR("failed to connect to the database\n");
		return -1;
	}
	if (reg_dbf.use_table(reg_db_con, &reg_db_table) < 0)
	{
		LM_ERR("failed to use_table\n");
		return -1;
	}

	db_vals[0].type = DB1_STR;
	db_vals[0].nul = 0;
	db_vals[0].val.str_val.s = pl_uuid->s;
	db_vals[0].val.str_val.len = pl_uuid->len;

	if((ret=reg_dbf.query(reg_db_con, db_keys, NULL, db_vals, db_cols,
			1 /*nr keys*/, 10 /*nr cols*/, 0, &db_res))!=0
		|| RES_ROW_N(db_res)<=0 )
	{
		reg_dbf.free_result(reg_db_con, db_res);
		if( ret==0)
		{
			return 0;
		} else {
			goto error;
		}
	}

	memset(&reg, 0, sizeof(reg_uac_t));;
	i = 0;
	/* check for NULL values ?!?! */
	reg_db_set_attr(l_uuid, 0);
	reg_db_set_attr(l_username, 1);
	reg_db_set_attr(l_domain, 2);
	reg_db_set_attr(r_username, 3);
	reg_db_set_attr(r_domain, 4);
	/* realm may be empty */
	if(!VAL_NULL(&RES_ROWS(db_res)[i].values[5])) {
		reg.realm.s = (char*)(RES_ROWS(db_res)[i].values[5].val.string_val);
		reg.realm.len = strlen(reg.realm.s);
	}
	reg_db_set_attr(auth_username, 6);
	reg_db_set_attr(auth_password, 7);
	reg_db_set_attr(auth_proxy, 8);
	reg.expires = (unsigned int)RES_ROWS(db_res)[i].values[9].val.int_val;
	reg.h_uuid = reg_compute_hash(&reg.l_uuid);
	reg.h_user = reg_compute_hash(&reg.l_username);

	lock_get(_reg_htable_gc_lock);
	if(reg_ht_get_byuuid(pl_uuid)!=NULL)
	{
		if(reg_ht_update_password(&reg)<0)
		{
			lock_release(_reg_htable_gc_lock);
			LM_ERR("Error updating reg to htable\n");
			goto error;
		}
	} else {
		if(reg_ht_add(&reg)<0)
		{
			lock_release(_reg_htable_gc_lock);
			LM_ERR("Error adding reg to htable\n");
			goto error;
		}
	}
	lock_release(_reg_htable_gc_lock);

	reg_dbf.free_result(reg_db_con, db_res);
	reg_dbf.close(reg_db_con);

	return 0;

error:
	if (reg_db_con) {
		reg_dbf.free_result(reg_db_con, db_res);
		reg_dbf.close(reg_db_con);
	}
	return -1;
}

/**
 *
 */
int  uac_reg_lookup(struct sip_msg *msg, str *src, pv_spec_t *dst, int mode)
{
	char  b_ruri[MAX_URI_SIZE];
	str   s_ruri;
	struct sip_uri puri;
	reg_uac_t *reg = NULL;
	pv_value_t val;

	if(!pv_is_w(dst))
	{
		LM_ERR("dst is not w/\n");
		return -1;
	}
	if(mode==0)
	{
		reg = reg_ht_get_byuuid(src);
		if(reg==NULL)
		{
			LM_DBG("no uuid: %.*s\n", src->len, src->s);
			return -1;
		}
		snprintf(b_ruri, MAX_URI_SIZE, "sip:%.*s@%.*s",
			reg->l_username.len, reg->l_username.s,
			reg->l_domain.len, reg->l_domain.s);
		s_ruri.s = b_ruri; s_ruri.len = strlen(s_ruri.s);
	} else {
		if(parse_uri(src->s, src->len, &puri)!=0)
		{
			LM_ERR("failed to parse uri\n");
			return -2;
		}
		reg = reg_ht_get_byuser(&puri.user, (reg_use_domain)?&puri.host:NULL);
		if(reg==NULL)
		{
			LM_DBG("no user: %.*s\n", src->len, src->s);
			return -1;
		}
		snprintf(b_ruri, MAX_URI_SIZE, "%.*s",
			reg->l_uuid.len, reg->l_uuid.s);
		s_ruri.s = b_ruri; s_ruri.len = strlen(s_ruri.s);
	}
	memset(&val, 0, sizeof(pv_value_t));
	val.flags |= PV_VAL_STR;
	val.rs = s_ruri;
	if(pv_set_spec_value(msg, dst, 0, &val)!=0)
		return -1;

	return 1;
}

/**
 *
 */
int uac_reg_request_to(struct sip_msg *msg, str *src, unsigned int mode)
{
	char ruri[MAX_URI_SIZE];
	struct sip_uri puri;
	reg_uac_t *reg = NULL;
	pv_value_t val;
	struct action act;
	struct run_act_ctx ra_ctx;

	switch(mode)
	{
		case 0:
			reg = reg_ht_get_byuuid(src);
			break;
		case 1:
			if(reg_use_domain)
			{
				if (parse_uri(src->s, src->len, &puri)!=0)
				{
					LM_ERR("failed to parse uri\n");
					return -2;
				}
				reg = reg_ht_get_byuser(&puri.user, &puri.host);
			} else {
				reg = reg_ht_get_byuser(src, NULL);
			}
			break;
		default:
			LM_ERR("unknown mode: %d\n", mode);
			return -1;
	}

	if(reg==NULL)
	{
		LM_DBG("no user: %.*s\n", src->len, src->s);
		return -1;
	}

	// Set uri ($ru)
	snprintf(ruri, MAX_URI_SIZE, "sip:%.*s@%.*s",
		reg->r_username.len, reg->r_username.s,
		reg->r_domain.len, reg->r_domain.s);
	memset(&act, 0, sizeof(act));
	act.type = SET_URI_T;
	act.val[0].type = STRING_ST;
	act.val[0].u.string = ruri;
	init_run_actions_ctx(&ra_ctx);
	if (do_action(&ra_ctx, &act, msg) < 0) {
		LM_ERR("error while setting request uri\n");
		return -1;
	}

	// Set auth_proxy ($du)
	if (set_dst_uri(msg, &reg->auth_proxy) < 0) {
		LM_ERR("error while setting outbound proxy\n");
		return -1;
	}

	memset(&val, 0, sizeof(pv_value_t));
	val.flags |= PV_VAL_STR;

	// Set auth_realm
	val.rs = reg->realm;
	if(pv_set_spec_value(msg, &auth_realm_spec, 0, &val)!=0) {
		LM_ERR("error while setting auth_realm\n");
		return -1;
	}

	// Set auth_username
	val.rs = reg->auth_username;
	if(pv_set_spec_value(msg, &auth_username_spec, 0, &val)!=0) {
		LM_ERR("error while setting auth_username\n");
		return -1;
	}

	// Set auth_password
	val.rs = reg->auth_password;
	if(pv_set_spec_value(msg, &auth_password_spec, 0, &val)!=0) {
		LM_ERR("error while setting auth_password\n");
		return -1;
	}

	return 1;
}

static const char* rpc_uac_reg_dump_doc[2] = {
	"Dump the contents of user registrations table.",
	0
};

static void rpc_uac_reg_dump(rpc_t* rpc, void* ctx)
{
	int i;
	reg_item_t *reg = NULL;
	void* th;
	str none = {"none", 4};
	time_t tn;

	if(_reg_htable==NULL)
	{
		rpc->fault(ctx, 500, "Not enabled");
		return;
	}

	tn = time(NULL);

	for(i=0; i<_reg_htable->htsize; i++)
	{
		lock_get(&_reg_htable->entries[i].lock);
		/* walk through entries */
		reg = _reg_htable->entries[i].byuuid;
		while(reg)
		{
			/* add entry node */
			if (rpc->add(ctx, "{", &th) < 0)
			{
				rpc->fault(ctx, 500, "Internal error creating rpc");
				return;
			}
			if(rpc->struct_add(th, "SSSSSSSSSdddd",
					"l_uuid",        &reg->r->l_uuid,
					"l_username",    &reg->r->l_username,
					"l_domain",      &reg->r->l_domain,
					"r_username",    &reg->r->r_username,
					"r_domain",      &reg->r->r_domain,
					"realm",         &reg->r->realm,
					"auth_username", &reg->r->auth_username,
					"auth_password", &reg->r->auth_password,
					"auth_proxy",    (reg->r->auth_proxy.len)?
										&reg->r->auth_proxy:&none,
					"expires",       (int)reg->r->expires,
					"flags",         (int)reg->r->flags,
					"diff_expires",  (int)(reg->r->timer_expires - tn),
					"timer_expires", (int)reg->r->timer_expires
				)<0)
			{
				lock_release(&_reg_htable->entries[i].lock);
				rpc->fault(ctx, 500, "Internal error adding item");
				return;
			}
			reg = reg->next;
		}
		lock_release(&_reg_htable->entries[i].lock);
	}
}

static const char* rpc_uac_reg_info_doc[2] = {
	"Return the details of registration for a particular record.",
	0
};

static void rpc_uac_reg_info(rpc_t* rpc, void* ctx)
{
	int i;
	reg_item_t *reg = NULL;
	void* th;
	str none = {"none", 4};
	time_t tn;
	str attr = {0};
	str val = {0};
	str *rval;

	if(_reg_htable==NULL)
	{
		rpc->fault(ctx, 500, "Not enabled");
		return;
	}

	if(rpc->scan(ctx, "S.S", &attr, &val)<2)
	{
		rpc->fault(ctx, 500, "Invalid Parameters");
		return;
	}
	if(attr.len<=0 || attr.s==NULL || val.len<=0 || val.s==NULL)
	{
		LM_ERR("bad parameter values\n");
		rpc->fault(ctx, 500, "Invalid Parameter Values");
		return;
	}

	tn = time(NULL);

	for(i=0; i<_reg_htable->htsize; i++)
	{
		/* walk through entries */
		lock_get(&_reg_htable->entries[i].lock);
		reg = _reg_htable->entries[i].byuuid;
		while(reg)
		{
			if(attr.len==10 && strncmp(attr.s, "l_username", 10)==0) {
				rval = &reg->r->l_username;
			} else if(attr.len==10 && strncmp(attr.s, "r_username", 10)==0) {
				rval = &reg->r->r_username;
			} else if(attr.len==6 && strncmp(attr.s, "l_uuid", 6)==0) {
				rval = &reg->r->l_uuid;
			} else if(attr.len==13 && strncmp(attr.s, "auth_username", 13)==0) {
				rval = &reg->r->auth_username;
			} else {
				lock_release(&_reg_htable->entries[i].lock);
				LM_ERR("usupoorted filter attribute %.*s\n", attr.len, attr.s);
				rpc->fault(ctx, 500, "Unsupported Filter Attribtue");
				return;
			}

			if(rval->len==val.len && strncmp(val.s, rval->s, val.len)==0) {
				/* add entry node */
				if (rpc->add(ctx, "{", &th) < 0)
				{
					rpc->fault(ctx, 500, "Internal error creating rpc");
					return;
				}
				if(rpc->struct_add(th, "SSSSSSSSSdddd",
						"l_uuid",        &reg->r->l_uuid,
						"l_username",    &reg->r->l_username,
						"l_domain",      &reg->r->l_domain,
						"r_username",    &reg->r->r_username,
						"r_domain",      &reg->r->r_domain,
						"realm",         &reg->r->realm,
						"auth_username", &reg->r->auth_username,
						"auth_password", &reg->r->auth_password,
						"auth_proxy",    (reg->r->auth_proxy.len)?
											&reg->r->auth_proxy:&none,
						"expires",       (int)reg->r->expires,
						"flags",         (int)reg->r->flags,
						"diff_expires",  (int)(reg->r->timer_expires - tn),
						"timer_expires", (int)reg->r->timer_expires
					)<0)
				{
					lock_release(&_reg_htable->entries[i].lock);
					rpc->fault(ctx, 500, "Internal error adding item");
					return;
				}
				lock_release(&_reg_htable->entries[i].lock);
				return;
			}
			reg = reg->next;
		}
		lock_release(&_reg_htable->entries[i].lock);
	}
}


static void rpc_uac_reg_update_flag(rpc_t* rpc, void* ctx, int mode, int fval)
{
	int i;
	reg_item_t *reg = NULL;
	str attr = {0};
	str val = {0};
	str *rval;

	if(_reg_htable==NULL)
	{
		rpc->fault(ctx, 500, "Not enabled");
		return;
	}

	if(rpc->scan(ctx, "S.S", &attr, &val)<2)
	{
		rpc->fault(ctx, 500, "Invalid Parameters");
		return;
	}
	if(attr.len<=0 || attr.s==NULL || val.len<=0 || val.s==NULL)
	{
		LM_ERR("bad parameter values\n");
		rpc->fault(ctx, 500, "Invalid Parameter Values");
		return;
	}

	for(i=0; i<_reg_htable->htsize; i++)
	{
		lock_get(&_reg_htable->entries[i].lock);
		/* walk through entries */
		reg = _reg_htable->entries[i].byuuid;
		while(reg)
		{
			if(attr.len==10 && strncmp(attr.s, "l_username", 10)==0) {
				rval = &reg->r->l_username;
			} else if(attr.len==10 && strncmp(attr.s, "r_username", 10)==0) {
				rval = &reg->r->r_username;
			} else if(attr.len==6 && strncmp(attr.s, "l_uuid", 6)==0) {
				rval = &reg->r->l_uuid;
			} else if(attr.len==13 && strncmp(attr.s, "auth_username", 13)==0) {
				rval = &reg->r->auth_username;
			} else {
				lock_release(&_reg_htable->entries[i].lock);
				LM_ERR("usupoorted filter attribute %.*s\n", attr.len, attr.s);
				rpc->fault(ctx, 500, "Unsupported Filter Attribtue");
				return;
			}

			if(rval->len==val.len && strncmp(val.s, rval->s, val.len)==0) {
				/* found */
				if(mode==1) {
					reg->r->flags |= fval;
				} else {
					reg->r->flags &= ~fval;
				}
				reg->r->timer_expires = time(NULL) + 1;
				lock_release(&_reg_htable->entries[i].lock);
				return;
			}
			reg = reg->next;
		}
		lock_release(&_reg_htable->entries[i].lock);
	}
}

static const char* rpc_uac_reg_enable_doc[2] = {
	"Enable registration for a particular record.",
	0
};

static void rpc_uac_reg_enable(rpc_t* rpc, void* ctx)
{
	rpc_uac_reg_update_flag(rpc, ctx, 0, UAC_REG_DISABLED);
	counter_add(regdisabled, -1);
}

static const char* rpc_uac_reg_disable_doc[2] = {
	"Disable registration for a particular record.",
	0
};

static void rpc_uac_reg_disable(rpc_t* rpc, void* ctx)
{
	rpc_uac_reg_update_flag(rpc, ctx, 1, UAC_REG_DISABLED);
	counter_inc(regdisabled);
}

static const char* rpc_uac_reg_reload_doc[2] = {
	"Reload records from database.",
	0
};

static void rpc_uac_reg_reload(rpc_t* rpc, void* ctx)
{
	int ret;
	if(uac_reg_ht_shift()<0) {
		rpc->fault(ctx, 500, "Failed to shift records - check log messages");
		return;
	}
	lock_get(_reg_htable_gc_lock);
	ret =  uac_reg_load_db();
	lock_release(_reg_htable_gc_lock);
	if(ret<0) {
		rpc->fault(ctx, 500, "Failed to reload records - check log messages");
		return;
	}
}

static const char* rpc_uac_reg_refresh_doc[2] = {
	"Refresh a record from database.",
	0
};

static void rpc_uac_reg_refresh(rpc_t* rpc, void* ctx)
{
	int ret;
	str l_uuid;

	if(rpc->scan(ctx, "S", &l_uuid)<1)
	{
		rpc->fault(ctx, 500, "Invalid Parameters");
		return;
	}

	ret =  uac_reg_db_refresh(&l_uuid);
	if(ret<0) {
		rpc->fault(ctx, 500, "Failed to refresh record - check log messages");
		return;
	}
}

rpc_export_t uac_reg_rpc[] = {
	{"uac.reg_dump", rpc_uac_reg_dump, rpc_uac_reg_dump_doc, 0},
	{"uac.reg_info", rpc_uac_reg_info, rpc_uac_reg_info_doc, 0},
	{"uac.reg_enable",  rpc_uac_reg_enable,  rpc_uac_reg_enable_doc,  0},
	{"uac.reg_disable", rpc_uac_reg_disable, rpc_uac_reg_disable_doc, 0},
	{"uac.reg_reload",  rpc_uac_reg_reload,  rpc_uac_reg_reload_doc,  0},
	{"uac.reg_refresh", rpc_uac_reg_refresh, rpc_uac_reg_refresh_doc, 0},
	{0, 0, 0, 0}
};

int uac_reg_init_rpc(void)
{
	if (rpc_register_array(uac_reg_rpc)!=0)
	{
		LM_ERR("failed to register RPC commands\n");
		return -1;
	}
	return 0;
}
