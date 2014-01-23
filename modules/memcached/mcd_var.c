/*
 * Copyright (C) 2009, 2013 Henning Westerholt
 * Copyright (C) 2013 Charles Chance, sipcentric.com
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
#include "../../pvapi.h"
#include "../pv/pv_svar.h"
#include "../../md5utils.h"


/*!
 * \brief Checks for '=>' delimiter in key name string and if present, extracts expiry value.
 * \param data string to parse
 * \param key output string name
 * \param exp output int expiry (if present)
 * \return 0 on success, negative on failure
 */
static inline int pv_mcd_key_expiry_split_str(str *data, str *key, unsigned int *exp) {
	char *p;
	str str_exp;
	str_exp.s = NULL;
	str_exp.len = 0;

	if (data == NULL || data->s == NULL || data->len <= 0) {
		LM_ERR("invalid parameters\n");
		return -1;
	}

	p = data->s;
	key->s = p;
	key->len = 0;

	while(p < data->s + data->len) {
		if (*p == '=') {
			p++;
			if (*p == '>') {
				break;
			} else {
				key->len++;
			}
		} else {
	                key->len++;
			p++;
		}
	}

	if (key->len < data->len) {
		/* delimiter is present, try to extract expiry value */
		p++;
		if (p < data->s + data->len) {
			str_exp.s = p;
			str_exp.len = 0;
			while(p<data->s+data->len) {
				str_exp.len++;
				p++;
			}
		}
		if (str_exp.len > 0) {
			/* convert to int */
			*exp = atoi(str_exp.s);
		}
		LM_DBG("key is %.*s expiry is %d\n", key->len, key->s, *exp);
	}

	return 0;
}

/*!
 * \brief Checks if the key is avaiable and not too long, hashing it with MD5 if necessary.
 * \param msg SIP message
 * \param param pseudo-variable input parameter
 * \param key output string name
 * \param exp output int expiry (if present)
 * \return 0 on success, negative on failure
 */
static inline int pv_mcd_key_check(struct sip_msg *msg, pv_param_t *param, str * key, unsigned int * exp ) {

	str pvn;
	str tmp;

	static char hash[32];

	if (msg == NULL || param == NULL) {
		LM_ERR("bad parameters\n");
		return -1;
	}

	if (pv_printf_s(msg, param->pvn.u.dname, &pvn) != 0)
	{
		LM_ERR("cannot get pv name\n");
		return -1;
	}

	if (pv_mcd_key_expiry_split_str(&pvn, &tmp, exp) != 0) {
		return -1;
	}

	if (tmp.len < 250) {
		key->s = tmp.s;
		key->len = tmp.len;
	} else {
		LM_DBG("key too long (%d), hash it\n", tmp.len);
		MD5StringArray (hash, &tmp, 1);
		key->s = hash;
		key->len = 32;
	}
	return 0;
}

/*!
 * \brief Helper to get a cached value from memcached
 * \param msg SIP message
 * \param key value key
 * \param return_value returned value
 * \param flags returned flags
 * \return null on success, negative on failure
 */
static int pv_get_mcd_value_helper(struct sip_msg *msg, str *key,
		char **return_value, uint32_t *flags) {

	memcached_return rc;
	size_t return_value_length;

	*return_value = memcached_get(memcached_h, key->s, key->len, &return_value_length, flags, &rc);

	if (*return_value == NULL) {
		if (rc == MEMCACHED_NOTFOUND) {
			LM_DBG("key %.*s not found\n", key->len, key->s);
		} else {
			LM_ERR("could not get result for key %.*s - error was '%s'\n", key->len, key->s, memcached_strerror(memcached_h, rc));
		}
		return -1;
	}

	LM_DBG("result: %s for key %.*s with flag %d\n", *return_value, key->len, key->s, *flags);

	return 0;
}

static void pv_free_mcd_value(char** buf) {
	if (*buf!=NULL) {
		if (mcd_memory) {
			pkg_free(*buf);
		} else {
			free(*buf);
		}
	}
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
	unsigned int expiry = mcd_expire;

  	char *return_value;
	uint32_t return_flags;

	if (pv_mcd_key_check(msg, param, &key, &expiry) < 0) {
		return pv_get_null(msg, param, res);
	}

	if (res==NULL)
		return pv_get_null(msg, param, res);

	if (pv_get_mcd_value_helper(msg, &key, &return_value, &return_flags) < 0) {
		goto errout;
	}


	res_str.len = strlen(return_value);
	res_str.s = return_value;


	/* apparently memcached adds whitespaces to the beginning of the value after atomic operations */

	trim_len(res_str.len, res_str.s, res_str);

	if(return_flags&VAR_VAL_STR || mcd_stringify) {
		res->rs.s = pv_get_buffer();
		res->rs.len = pv_get_buffer_size();
		if(res_str.len>=res->rs.len) {
			LM_ERR("value is too big (%d) - increase pv buffer size\n", res_str.len);
			goto errout;
		}
		memcpy(res->rs.s, res_str.s, res_str.len);
		res->rs.len = res_str.len;
		res->rs.s[res->rs.len] = '\0';
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

	pv_free_mcd_value(&return_value);
	return 0;

errout:
	pv_free_mcd_value(&return_value);
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
	unsigned int expiry = mcd_expire;

	if (pv_mcd_key_check(msg, param, &key, &expiry) < 0)
		return -1;

	if (val == NULL) {
		if (memcached_delete(memcached_h, key.s, key.len, 0) != MEMCACHED_SUCCESS) {
			LM_ERR("could not delete key %.*s\n", param->pvn.u.isname.name.s.len,
				param->pvn.u.isname.name.s.s);
			return -1;
		}
		LM_WARN("delete key %.*s\n", key.len, key.s);
		return 0;
	}

	if (val->flags&PV_VAL_INT) {
		val_str.s = int2str(val->ri, &val_str.len);
	} else {
		val_str = val->rs;
		val_flag = VAR_VAL_STR;
	}

	if (mcd_mode == 0) {
		if (memcached_set(memcached_h, key.s, key.len, val_str.s, val_str.len, expiry, val_flag) != MEMCACHED_SUCCESS) {
			LM_ERR("could not set value for key %.*s\n", key.len, key.s);
			return -1;
		}
	} else {
		if (memcached_add(memcached_h, key.s, key.len, val_str.s, val_str.len, expiry, val_flag) != MEMCACHED_SUCCESS) {
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
 * \param atomic_ops function pointer to the atomic operation from the memcached library
 * \return 0 on success, -1 on failure
 */
static int pv_mcd_atomic_helper(struct sip_msg* msg, pv_param_t *param, int op, pv_value_t *val,
		memcached_return (* atomic_ops) (memcached_st *mc, const char *key, size_t key_length, uint32_t offset, uint64_t *value)) {

	uint64_t value = 0;
	str key;
	unsigned int expiry = mcd_expire;
	char *return_value;
	uint32_t return_flags;
	memcached_return rc;

	if (!(val->flags&PV_VAL_INT)) {
		LM_ERR("invalid value %.*s for atomic operation, strings not allowed\n",
			val->rs.len, val->rs.s);
		return -1;
	}

	if (pv_mcd_key_check(msg, param, &key, &expiry) < 0)
		return -1;

	if (pv_get_mcd_value_helper(msg, &key, &return_value, &return_flags) < 0) {
		pv_free_mcd_value(&return_value);
		return -1;
	}

	pv_free_mcd_value(&return_value);

	if(return_flags&VAR_VAL_STR) {
		LM_ERR("could not do atomic operations on string for key %.*s\n", key.len, key.s);
		return -1;
	}

	if ((rc = atomic_ops(memcached_h, key.s, key.len, val->ri, &value)) != MEMCACHED_SUCCESS) {
		LM_ERR("error performing atomic operation on key %.*s - %s\n", key.len, key.s, memcached_strerror(memcached_h, rc));
		return -1;
	}

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
	return pv_mcd_atomic_helper(msg, param, op, val, memcached_increment);
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
	return pv_mcd_atomic_helper(msg, param, op, val, memcached_decrement);
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
	unsigned int expiry = mcd_expire;
	char *return_value;
	uint32_t return_flags;
	memcached_return rc;

	if (!(val->flags&PV_VAL_INT)) {
		LM_ERR("invalid value %.*s for expire time, strings not allowed\n",
			val->rs.len, val->rs.s);
		return -1;
	}

	if (pv_mcd_key_check(msg, param, &key, &expiry) < 0)
		return -1;

	if (pv_get_mcd_value_helper(msg, &key, &return_value, &return_flags) < 0) {
		goto errout;
	}

	LM_DBG("set expire time %d for key %.*s with flag %d\n", val->ri, key.len, key.s, return_flags);

	if ((rc= memcached_set(memcached_h, key.s, key.len, return_value, strlen(return_value), val->ri, return_flags)) != MEMCACHED_SUCCESS) {
		LM_ERR("could not set expire time %d for key %.*s - error was %s\n", val->ri, key.len, key.s, memcached_strerror(memcached_h, rc));
		goto errout;
	}

	pv_free_mcd_value(&return_value);
	return 0;

errout:
	pv_free_mcd_value(&return_value);
	return -1;
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
