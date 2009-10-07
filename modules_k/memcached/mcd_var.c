/**
 * $Id$
 *
 * Copyright (C) 2009 Henning Westerholt
 *
 * This file is part of Kamailio, a free SIP server.
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
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, see <http://www.gnu.org/licenses/>.
 */

/*!
 * \file
 * \brief memcached module
 */

#include "mcd_var.h"

#include "memcached.h"
#include "../../ut.h"
#include "../../mem/mem.h"
#include "../pv/pv_svar.h"
#include "../../md5utils.h"


/*!
 * \brief Checks if the key is avaiable and not too long, hash it with MD5 if necessary
 * \param msg SIP message
 * \param param pseudo-variable input parameter
 * \param out output string
 * \return 0 on success, negative on failure
 */
static inline int pv_mcd_key_check(struct sip_msg *msg, pv_param_t *param, str * out) {

	str tmp;
	static char hash[32];

	if (msg == NULL || param == NULL) {
		LM_ERR("bad parameters\n");
		return -1;
	}

	if (pv_printf_s(msg, param->pvn.u.dname, &tmp) != 0)
	{
		LM_ERR("cannot get key name\n");
		return -1;
	}

	if (tmp.len < 250) {
		out->s = tmp.s;
		out->len = tmp.len;
	} else {
		LM_DBG("key too long (%d), hash it\n", tmp.len);
		MD5StringArray (hash, &tmp, 1);
		out->s = hash;
		out->len = 32;
	}
	return 0;
}

/*!
 * \brief Helper to get a cached value from memcached
 * \param msg SIP message
 * \param key value key
 * \param mcd_req request
 * \param mcd_res result
 * \return null on success, negative on failure
 */
static int pv_get_mcd_value_helper(struct sip_msg *msg, str *key,
		struct memcache_req **mcd_req, struct memcache_res **mcd_res) {

	/* we don't use mc_aget here, because we're multi-process */
	if ( (*mcd_req = mc_req_new()) == NULL) {
		PKG_MEM_ERROR;
		return -1;
	}
	LM_DBG("allocate new memcache request at %p\n", *mcd_req);

	if ( (*mcd_res = mc_req_add(*mcd_req, key->s, key->len)) == NULL) {
		PKG_MEM_ERROR;
		return -1;
	}
	LM_DBG("allocate new memcache result at %p\n", *mcd_res);

	mc_get(memcached_h, *mcd_req);
	if (! ( (*mcd_res)->_flags & MCM_RES_FOUND)) {
		LM_ERR("could not get result for key %.*s\n", key->len, key->s);
		LM_DBG("free memcache request and result at %p\n", mcd_req);
		mc_req_free(*mcd_req);
		return -1;
	}
	LM_DBG("result: %.*s for key %.*s with flag %d\n", (*mcd_res)->bytes, (char*)(*mcd_res)->val,
		key->len, key->s, (*mcd_res)->flags);

	return 0;
}

/*!
 * \brief Get a cached value from memcached
 * \param msg SIP message
 * \param param parameter
 * \param res result
 * \return null on success, negative on failure
 */
int pv_get_mcd_value(struct sip_msg *msg, pv_param_t *param, pv_value_t *res) {

	unsigned int res_int = 0;
	str key, res_str;
	struct memcache_req *mcd_req = NULL;
	struct memcache_res *mcd_res = NULL;

	if (pv_mcd_key_check(msg, param, &key) < 0) {
		return pv_get_null(msg, param, res);
	}

	if (res==NULL)
		return pv_get_null(msg, param, res);

	if (pv_get_mcd_value_helper(msg, &key, &mcd_req, &mcd_res) < 0) {
		return pv_get_null(msg, param, res);
	}

	res_str.len = mcd_res->bytes;
	res_str.s = mcd_res->val;
	/* apparently memcached adds whitespaces to the beginning of the value after atomic operations */
	trim_len(res_str.len, res_str.s, res_str);

	if(mcd_res->flags&VAR_VAL_STR) {
		 if (pkg_str_dup(&(res->rs), &res_str) < 0) {
			LM_ERR("could not copy string\n");
			goto errout;
		}
		res->flags = PV_VAL_STR;
	} else {
		if (str2int(&res_str, &res_int) < 0) {
			LM_ERR("could not convert string %.*s to integer value\n", res_str.len, res_str.s);
			goto errout;
		}
		res->rs = res_str;
		res->ri = res_int;
		res->flags = PV_VAL_STR|PV_VAL_INT|PV_TYPE_INT;
	}
	LM_DBG("free memcache request and result at %p\n", mcd_req);
	mc_req_free(mcd_req);

	return 0;

errout:
	LM_DBG("free memcache request and result at %p\n", mcd_req);
	mc_req_free(mcd_req);
	return pv_get_null(msg, param, res);
}


/*!
 * \brief Set a value in the cache of memcached
 * \todo Replacement of already existing values is not done atomically at the moment.
 * Here the provided replace function should be used.
 * \param msg SIP message
 * \param param parameter
 * \param op not used
 * \param val value
 * \return 0 on success, -1 on failure
 */
 int pv_set_mcd_value(struct sip_msg* msg, pv_param_t *param, int op, pv_value_t *val) {

	unsigned int val_flag = 0;
	str val_str, key;

	if (pv_mcd_key_check(msg, param, &key) < 0)
		return -1;

	if (val == NULL) {
		if (mc_delete(memcached_h, key.s, key.len, 0) != 0) {
			LM_ERR("could not delete key %.*s\n", param->pvn.u.isname.name.s.len,
				param->pvn.u.isname.name.s.s);
		}
		LM_DBG("delete key %.*s\n", key.len, key.s);
		return 0;
	}

	if (val->flags&PV_VAL_INT) {
		val_str.s = int2str(val->ri, &val_str.len);
	} else {
		val_str = val->rs;
		val_flag = VAR_VAL_STR;
	}

	if (memcached_mode == 0) {
		if (mc_set(memcached_h, key.s, key.len, val_str.s, val_str.len, memcached_expire, val_flag) != 0) {
			LM_ERR("could not set value for key %.*s\n", key.len, key.s);
			return -1;
		}
	} else {
		if (mc_add(memcached_h, key.s, key.len, val_str.s, val_str.len, memcached_expire, val_flag) != 0) {
			LM_ERR("could not add value for key %.*s\n", key.len, key.s);
			return -1;
		}
	}
	LM_DBG("set value %.*s for key %.*s with flag %d\n", val_str.len, val_str.s, key.len, key.s, val_flag);

	return 0;
}


/*!
 * \brief Helper function for the memcached atomic operations
 * \note The checks on value existence and type are not done atomically, so there is a small
 * chance that the later atomic operation fails. This is hard to detect because this function
 * don't return a proper result code. Checking for the incremented value is also not possible,
 * because in the mean time the value could be incremented from some other client.
 * \param msg SIP message
 * \param param parameter
 * \param op not used
 * \param val value
 * \param atomic_ops function pointer to the atomic operation from the memcache library
 * \return 0 on success, -1 on failure
 */
static int pv_mcd_atomic_helper(struct sip_msg* msg, pv_param_t *param, int op, pv_value_t *val,
		unsigned int (* atomic_ops) (struct memcache *mc, char *key, const size_t key_len,
		const unsigned int val)) {

	unsigned int value = 0;
	str key;
	struct memcache_req *mcd_req = NULL;
	struct memcache_res *mcd_res = NULL;
	
	if (! val->flags&PV_VAL_INT) {
		LM_ERR("invalid value %.*s for atomic operation, strings not allowed\n",
			val->rs.len, val->rs.s);
		return -1;
	}

	if (pv_mcd_key_check(msg, param, &key) < 0)
		return -1;

	if (pv_get_mcd_value_helper(msg, &key, &mcd_req, &mcd_res) < 0) {
		return -1;
	}

	if(mcd_res->flags&VAR_VAL_STR) {
		LM_ERR("could not do atomic operations on string for key %.*s\n", key.len, key.s);
		LM_DBG("free memcache request and result at %p\n", mcd_req);
		mc_req_free(mcd_req);
		return -1;
	}

	LM_DBG("atomic operation on result %.*s for %d with flag %d\n", mcd_res->bytes, (char*)mcd_res->val, val->ri, mcd_res->flags);
	LM_DBG("free memcache request and result at %p\n", mcd_req);
	mc_req_free(mcd_req);

	value = atomic_ops(memcached_h, key.s, key.len, val->ri);
	LM_DBG("value from atomic operation %d\n", value);

	return 0;
}


/*!
 * \brief Increment a key atomically in the cache
 * \param msg SIP message
 * \param param parameter
 * \param op not used
 * \param val value
 * \return 0 on success, -1 on failure
 */
int inline pv_inc_mcd_value(struct sip_msg* msg, pv_param_t *param, int op, pv_value_t *val) {
	return pv_mcd_atomic_helper(msg, param, op, val, mc_incr);
}


/*!
 * \brief Decrement a key atomically in the cache
 * \param msg SIP message
 * \param param parameter
 * \param op not used
 * \param val value
 * \return 0 on success, -1 on failure
 */
int inline pv_dec_mcd_value(struct sip_msg* msg, pv_param_t *param, int op, pv_value_t *val) {
	return pv_mcd_atomic_helper(msg, param, op, val, mc_decr);
}


/*!
 * \brief Set the expire value in the cache of memcached
 * \note The memcache library don't provide functions to change the expiration
 * time for a certain key after creation, so we need to do a get and set here.
 * \param msg SIP message
 * \param param parameter
 * \param op not used
 * \param val value
 * \return 0 on success, -1 on failure
 */
int pv_set_mcd_expire(struct sip_msg* msg, pv_param_t *param, int op, pv_value_t *val)
{
	str key;
	struct memcache_req *mcd_req = NULL;
	struct memcache_res *mcd_res = NULL;

	if (! val->flags&PV_VAL_INT) {
		LM_ERR("invalid value %.*s for expire time, strings not allowed\n",
			val->rs.len, val->rs.s);
		return -1;
	}

	if (pv_mcd_key_check(msg, param, &key) < 0)
		return -1;

	if (pv_get_mcd_value_helper(msg, &key, &mcd_req, &mcd_res) < 0) {
		return -1;
	}

	LM_DBG("set expire time %d on result %.*s for %d with flag %d\n", val->ri, mcd_res->bytes, (char*)mcd_res->val, val->ri, mcd_res->flags);

	if (mc_set(memcached_h, key.s, key.len, mcd_res->val, mcd_res->bytes, val->ri, mcd_res->flags) != 0) {
		LM_ERR("could not set expire time %d for key %.*s\n", val->ri, key.len, key.s);
		LM_DBG("free memcache request and result at %p\n", mcd_req);
		mc_req_free(mcd_req);
		return -1;
	}
	LM_DBG("free memcache request and result at %p\n", mcd_req);
	mc_req_free(mcd_req);

	return 0;
}


/*!
 * \brief Parse the pseudo-variable specification parameter
 * \param sp pseudo-variable specification
 * \param in parameter string
 * \return 0 on success, -1 on failure
 */
int pv_parse_mcd_name(pv_spec_p sp, str *in) {

	pv_elem_t * tmp = NULL;

	if(sp==NULL || in==NULL || in->len<=0)
		return -1;


	tmp = pkg_malloc(sizeof(pv_elem_t));
	if (tmp == NULL) {
		PKG_MEM_ERROR;
		return -1;
	}
	memset(tmp, 0, sizeof(pv_elem_t));

	if(pv_parse_format(in, &tmp) || tmp==NULL) {
		LM_ERR("wrong format [%.*s]\n", in->len, in->s);
		return -1;
	}

	sp->pvp.pvn.u.dname = tmp;
	sp->pvp.pvn.type = PV_NAME_PVAR;

	return 0;
}
