/*
 * Copyright (c) 2010 IPTEGO GmbH.
 * All rights reserved.
 *
 * This file is part of the BinRPC Library (libbinrpc).
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 
 *    1. Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 * 
 *    2. Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY IPTEGO GmbH ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN
 * NO EVENT SHALL IPTEGO GmbH OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 *
 * Author: Bogdan Pintea.
 */


#include <arpa/inet.h>
#include <string.h>
#include <assert.h>
#include <stdarg.h>

#include "mem.h"
#include "log.h"
#include "errnr.h"
#include "misc.h"
#include "config.h"
#include "value.h"

__LOCAL brpc_val_t *decode_by_type(uint8_t **, const uint8_t *, brpc_vtype_t);
__LOCAL brpc_val_t *decode_int(uint8_t **, const uint8_t *);
__LOCAL brpc_val_t *decode_str(uint8_t **, const uint8_t *);
__LOCAL brpc_val_t *decode_bin(uint8_t **, const uint8_t *);
__LOCAL brpc_val_t *decode_avp(uint8_t **, const uint8_t *);
__LOCAL brpc_val_t *decode_seq(uint8_t **, const uint8_t *, brpc_vtype_t);

__LOCAL brpc_val_t *_brpc_bin(uint8_t *val, size_t len);
__LOCAL brpc_val_t *_brpc_str(char *val, size_t len);
__LOCAL bool _brpc_map_add(brpc_val_t *map, brpc_val_t *avp);
__LOCAL bool _brpc_list_add(brpc_val_t *list, brpc_val_t *avp);

__LOCAL uint8_t *serialize_null(brpc_val_t *, uint8_t *, const uint8_t *);
__LOCAL uint8_t *serialize_int(brpc_val_t *, uint8_t *, const uint8_t *);
__LOCAL uint8_t *serialize_bin(brpc_val_t *, uint8_t *, const uint8_t *);
__LOCAL uint8_t *serialize_content(brpc_val_t *, uint8_t *, const uint8_t *);
__LOCAL uint8_t *serialize_complex(brpc_val_t *, uint8_t *, const uint8_t *);

#define IALLOC(_rec_, _type_) \
	do { \
		_rec_ = (brpc_val_t *)brpc_calloc(1, sizeof(brpc_val_t)); \
		if (! _rec_) { \
			WERRNO(ENOMEM); \
			return NULL; \
		} \
		(_rec_)->type = _type_; \
		INIT_LIST_HEAD(&(_rec_)->list); \
	} while (0)

#define brpc_seq_add(_head_, _new_) \
	do { \
		assert(	((_head_)->type == BRPC_VAL_AVP) || \
				((_head_)->type == BRPC_VAL_MAP) || \
				((_head_)->type == BRPC_VAL_LIST) ); \
		list_add_tail(&(_new_)->list, &(_head_)->val.seq.list); \
		(_head_)->val.seq.cnt ++; \
	} while (0)

void brpc_val_free(brpc_val_t *rec)
{
	struct brpc_list_head *k, *tmp;

	if (! rec) {
		WARN("trying to free NULL reference as record.\n");
		return;
	}
#if 0
	DBG("freeing BINRPC value of type %d.\n", rec->type);
#endif

	if (! rec->null)
		switch (rec->type) {
			case BRPC_VAL_INT: break;
			case BRPC_VAL_STR:
			case BRPC_VAL_BIN:
				if (! rec->locked)
					brpc_free(brpc_bin_val(rec).val);
				break;
			case BRPC_VAL_AVP:
			case BRPC_VAL_MAP:
			case BRPC_VAL_LIST:
				list_for_each_safe(k, tmp, &rec->val.seq.list) {
					list_del(k);
					brpc_val_free(_BRPC_VAL4LIST(k));
				}
				break;
			default:
				BUG("unknown record type 0x%x.\n", rec->type);
		}

	brpc_free(rec);
}

brpc_val_t *brpc_null(brpc_vtype_t type)
{
	brpc_val_t *rec;
	IALLOC(rec, type);
	if (rec)
		rec->null = true;
	return rec;
}

brpc_val_t *brpc_int(brpc_int_t val)
{
	brpc_val_t *rec;
	IALLOC(rec, BRPC_VAL_INT);
	brpc_int_val(rec) = val;
	return rec;
}

__LOCAL brpc_val_t *_brpc_bin(uint8_t *val, size_t len)
{
	brpc_val_t *rec;
#ifdef PARANOID
	if (INT_MAX < len) {
		ERR("string too large");
		WERRNO(EINVAL);
	}
#endif
	IALLOC(rec, BRPC_VAL_BIN);
	brpc_bin_val(rec).val = val;
	brpc_bin_val(rec).len = len;
	if (! len)
		rec->null = true;
	return rec;
}

__LOCAL brpc_val_t *_brpc_str(char *val, size_t len)
{
	brpc_val_t *rec;
#ifdef PARANOID
	if (INT_MAX < len) {
		ERR("string too large");
		WERRNO(EINVAL);
	}
#endif
	rec = _brpc_bin((uint8_t *)val, len); /* works safe due to the union. */
	if (rec)
		rec->type = BRPC_VAL_STR;
	return rec;
}

brpc_val_t *brpc_bin(uint8_t *val, size_t len)
{
	brpc_val_t *rec;
	uint8_t *dup;

	if (! len)
		return brpc_null(BRPC_VAL_BIN);

	dup = (uint8_t *)brpc_malloc(len * sizeof(uint8_t));
	if (! dup) {
		WERRNO(ENOMEM);
		return NULL;
	}
	memcpy(dup, val, len);

	rec = _brpc_bin(dup, len);
	if (! rec)
		brpc_free(dup);
	return rec;
}

brpc_val_t *brpc_str(char *val, size_t len)
{
	brpc_val_t *rec;
	char *dup;
	bool zterm; /* 0-term present ? */

	zterm = ((! len) || val[len - 1]) ? false : true;

	dup = (char *)brpc_malloc((len + (zterm ? 0 : 1)) * sizeof(char));
	if (! dup) {
		WERRNO(ENOMEM);
		return NULL;
	}
	memcpy(dup, val, len);
	if (! zterm)
		dup[len] = 0;
	
	rec = _brpc_str(dup, len + (zterm ? 0 : 1));
	if (! rec)
		brpc_free(dup);
	return rec;
}

brpc_val_t *brpc_avp(brpc_val_t *name, brpc_val_t *value)
{
	brpc_val_t *rec;

	if ((name->type != BRPC_VAL_STR)
#ifdef INT_AS_ID
			&& (name->type != BRPC_VAL_INT)
#endif
	) {
		WERRNO(EINVAL);
		ERR("invalid type (%d) as AVP identifier.\n", name->type);
		return NULL;
	}
	IALLOC(rec, BRPC_VAL_AVP);
	INIT_LIST_HEAD(&rec->val.seq.list);
	brpc_seq_add(rec, name);
	brpc_seq_add(rec, value);
	return rec;
}

brpc_val_t *brpc_seq(brpc_vtype_t type, ...)
{
	brpc_val_t *rec, *val;
	va_list ap;
	bool (*add)(brpc_val_t *, brpc_val_t *);
	
	switch (type) {
		case BRPC_VAL_MAP: add = _brpc_map_add; break;
		case BRPC_VAL_LIST: add = _brpc_list_add; break;
		case BRPC_VAL_AVP: add = NULL; break; /* force sigsegv */
		default:
			BUG("illegal or unsupported type as sequence (%d).\n", type);
			return NULL;
	}

	IALLOC(rec, type);
	INIT_LIST_HEAD(&rec->val.seq.list);

	va_start(ap, type);
	while ((val = va_arg(ap, brpc_val_t *)))
		if (! add(rec, val))
			goto error;
	va_end(ap);
	
	return rec;
error:
	INIT_LIST_HEAD(&rec->val.seq.list); /* avoid freeing the rcvd arguments */
	brpc_val_free(rec);
	return NULL;
}


brpc_val_t *brpc_val_deser(uint8_t **_pos, const uint8_t *end)
{
	unsigned char hdr;
	uint8_t *rec_limit, *pos;
	int have_len_fld;
	size_t size, val_len;
	unsigned int type;
	brpc_val_t *new_rec;

	pos = *_pos;
	if (end <= pos)
		return NULL;

	hdr = *pos; pos ++;
	have_len_fld = hdr & REC_HAVE_SFLD;
	size = (hdr >> REC_SIZE_OFF) & REC_SSIZE_MASK;
	type = hdr & ((1 << REC_TYPE_BITS) - 1);
	DBG("decoding type %d.\n", type);

	if (! have_len_fld)
		val_len = size;
	else {
		if (end <= pos + size) {
			WERRNO(EBADMSG);
			ERR("record value size field out of bounds.\n");
			return NULL;
		}
		val_len = ntohz(pos, size); pos += size;
	}

	if (end < pos + val_len) {
		WERRNO(EBADMSG);
		ERR("record value field out of bounds.\n");
		return NULL;
	}

	if (val_len) {
		rec_limit = pos + val_len;
		new_rec = decode_by_type(&pos, rec_limit, type);
		if (new_rec && (pos != rec_limit)) {
			WERRNO(EBADMSG);
			ERR("unconsumed buffer segment [%zd] after decoding record.\n",
					rec_limit - pos);
			brpc_val_free(new_rec);
			return NULL;
		}
	} else {
		new_rec = brpc_null(type);
		new_rec->locked = true;
	}

	*_pos = pos;
	return new_rec;
}

__LOCAL brpc_val_t *decode_by_type(uint8_t **start, const uint8_t *end, 
		brpc_vtype_t type)
{

	switch (type) {
		case BRPC_VAL_INT: return decode_int(start, end);
		case BRPC_VAL_BIN: return decode_bin(start, end);
		case BRPC_VAL_STR: return decode_str(start, end);
		case BRPC_VAL_AVP: return decode_avp(start, end);
		case BRPC_VAL_LIST: return decode_seq(start, end, BRPC_VAL_LIST);
		case BRPC_VAL_MAP: return decode_seq(start, end, BRPC_VAL_MAP);
		default:
			WERRNO(EBADMSG);
			ERR("unknown record type 0x%x.\n", type);
	}
	return NULL;
}

__LOCAL brpc_val_t *decode_int(uint8_t **_start, const uint8_t *end)
{
	brpc_val_t *new_rec;
	size_t len;
	long integer;
	
	len = end - *_start;
	if (sizeof(brpc_int_t) < len) {
		WERRNO(EBADMSG);
		ERR("message's int representation (%zd) larger than local (%zd).\n",
				len, sizeof(brpc_int_t));
		return NULL;
	}
	if (! lntoh(*_start, len, &integer)) {
		WERRNO(EBADMSG);
		return NULL;
	}
	DBG("read [%zd]: %d.\n", len, integer);
	new_rec = brpc_int(integer);
	if (new_rec)
		new_rec->locked = true;
	*_start += len;
	return new_rec;
}

__LOCAL brpc_val_t *decode_bin(uint8_t **_start, const uint8_t *end)
{
	brpc_val_t *new_rec;
	new_rec = _brpc_bin(*_start, end - *_start);
	if (new_rec)
		new_rec->locked = true;
	*_start += new_rec->val.bin.len;
	DBG("read [%zd]: `%.*s.\n'", end - *_start, 
			new_rec->val.bin.len, new_rec->val.bin.val);
	return new_rec;
}

__LOCAL brpc_val_t *decode_str(uint8_t **_start, const uint8_t *end)
{
	brpc_val_t *new_rec;

	if (! (end - *_start)) {
		WERRNO(EBADMSG);
		ERR("string record can not be empty (need at least 0-term).\n");
		return NULL;
	}
	if (end[-1]) {
		WERRNO(EBADMSG);
		ERR("string record misses 0-terminator (got 0x%x).\n", end[-1]);
		return NULL;
	}

	new_rec = _brpc_str((char *)*_start, end - *_start);
	if (new_rec)
		new_rec->locked = true;
	*_start += new_rec->val.bin.len;
	DBG("read [%zd]: `%.*s.\n'", end - *_start, 
			new_rec->val.str.len, new_rec->val.str.val);
	return new_rec;
}

__LOCAL brpc_val_t *decode_avp(uint8_t **_start, const uint8_t *end)
{
	brpc_val_t *name, *val, *new_rec;

	name = brpc_val_deser(_start, end);
	if (! name)
		return NULL;
	val = brpc_val_deser(_start, end);
	if (! val) {
		brpc_val_free(name);
		return NULL;
	}
	new_rec = brpc_avp(name, val);
	if (! new_rec) {
		brpc_val_free(name);
		brpc_val_free(val);
		return NULL;
	} else {
		new_rec->locked = true;
	}
	return new_rec;
}

__LOCAL brpc_val_t *decode_seq(uint8_t **_start, const uint8_t *end,
		brpc_vtype_t type)
{
	brpc_val_t *seq_val, *sub_val, *name;

	switch (type) {
		case BRPC_VAL_LIST: seq_val = brpc_list(NULL); break;
		case BRPC_VAL_MAP: seq_val = brpc_map(NULL); break;
		default:
			WERRNO(EINVAL);
			BUG("illegal or unsupported type (%u) as sequence.\n", type);
			return NULL;
	}
	if (! seq_val)
		return NULL;
	seq_val->locked = true;

	while (*_start < end) {
		sub_val = brpc_val_deser(_start, end);
		if (! sub_val)
			/* TODO: support for struct terminator (0x83) */
			goto error;

		switch (type) {
			case BRPC_VAL_MAP:
				if (sub_val->type != BRPC_VAL_AVP) {
					WERRNO(EPROTO);
					ERR("map type only accepts AVP as subvalue "
							"(received: 0x%x).\n", sub_val->type);
					goto err_subval;
				}
				name = brpc_avp_name(sub_val);
				if ((name->type != BRPC_VAL_STR)
#ifdef INT_AS_ID
						&& (name->type != BRPC_VAL_INT)
#endif
						) {
					WERRNO(EPROTO);
					ERR("unsupported record type (0x%x) as AVP identifier.\n",
							name->type);
					goto err_subval;
				}
				break;
			default: break;
		}

		brpc_seq_add(seq_val, sub_val);
	}

	return seq_val;

err_subval:
	brpc_val_free(sub_val);
error:
	brpc_val_free(seq_val);
	return NULL;
}

bool brpc_list_add(brpc_val_t *list, brpc_val_t *val)
{
	return _brpc_list_add(list, val);
}

__LOCAL bool _brpc_list_add(brpc_val_t *list, brpc_val_t *_val)
{
	brpc_val_t *val;

	if (list->type != BRPC_VAL_LIST) {
		WERRNO(EINVAL);
		ERR("type (%d) does support enlisting values.\n", list->type);
		return false;
	}
	if (list->locked) {
		WERRNO(EINVAL);
		ERR("can not enlist into locked list.\n");
		return false;
	}

	if (! list_empty(&_val->list)) {
		WERRNO(EINVAL);
		ERR("can not enlist already enlisted value (type: %d).\n",
				_val->type);
		return false;
	}

	if (_val->locked) {
		val = brpc_val_clone(_val);
		if (! val)
			return false;
	} else {
		val = _val;
	}

	brpc_seq_add(list, val);
	return true;
}

bool brpc_map_add(brpc_val_t *map, brpc_val_t *avp)
{
	return _brpc_map_add(map, avp);
}

__LOCAL bool _brpc_map_add(brpc_val_t *map, brpc_val_t *_avp)
{
	brpc_val_t *name, *avp;

	if (map->type != BRPC_VAL_MAP) {
		WERRNO(EINVAL);
		ERR("type (%d) does not mapping values.\n", map->type);
		return false;
	}

	if (map->locked) {
		WERRNO(EINVAL);
		ERR("can not do mapping into locked map.\n");
		return false;
	}

	if (_avp->type != BRPC_VAL_AVP) {
		WERRNO(EINVAL);
		ERR("map type only accepts AVPs as subvals (tried: %d).\n", 
				_avp->type);
		return false;
	}

	if (! list_empty(&_avp->list)) {
		WERRNO(EINVAL);
		ERR("can not do mapping with already mapped AVP.\n");
		return false;
	}

	if (_avp->locked) {
		avp = brpc_val_clone(_avp);
		if (! avp)
			return false;
	} else {
		avp = _avp;
	}
	
	name = brpc_avp_name(avp);
	if ((name->type != BRPC_VAL_STR)
#ifdef INT_AS_ID
			&& (name->type != BRPC_VAL_INT)
#endif /* INT_AS_ID */
			) {
		WERRNO(EINVAL);
		ERR("unsupported record type (%d) as AVP identifier.\n", name->type);
		return false;
	}

	brpc_seq_add(map, avp);
	return true;
}

bool brpc_avp_add(brpc_val_t *avp, brpc_val_t *_member)
{
	brpc_val_t *member;

	if (avp->type != BRPC_VAL_AVP) {
		WERRNO(EINVAL);
		ERR("type (%d) is not of type attribute.\n", avp->type);
		return false;
	}

	if (avp->locked) {
		WERRNO(EINVAL);
		ERR("attribute locked: can not add value");
		return false;
	}

	if (! list_empty(&_member->list)) {
		WERRNO(EINVAL);
		ERR("can not attribute already used value.\n");
		return false;
	}

	if (list_empty(&avp->val.seq.list)) {
		if ((_member->type != BRPC_VAL_STR)
#ifdef INT_AS_ID
				&& (_member->type != BRPC_VAL_INT)
#endif
		) {
			WERRNO(EINVAL);
			ERR("type (%d) can not be used as attribute identifier.\n", 
					_member->type);
			return false;
		}
	} else if (&avp->val.seq.list != avp->val.seq.list.next->next) {
		WERRNO(EEXIST);
		ERR("attribute already has a value.\n");
		return false;
	}

	if (_member->locked) {
		member = brpc_val_clone(_member);
		if (! member)
			return false;
	} else {
		member = _member;
	}

	brpc_seq_add(avp, member);
	return true;
}

uint8_t* brpc_val_ser(brpc_val_t *val, uint8_t *start, 
		const uint8_t *end)
{
	DBG("serializing BINRPC value of type %d.\n", val->type);

	if (val->null)
		return serialize_null(val, start, end);

	switch (val->type) {
		case BRPC_VAL_INT: return serialize_int(val, start, end);
		case BRPC_VAL_STR:
		case BRPC_VAL_BIN: return serialize_bin(val, start, end);

		case BRPC_VAL_AVP: 
		case BRPC_VAL_MAP:
		case BRPC_VAL_LIST: return serialize_complex(val, start, end);
		default:
			WERRNO(EINVAL);
			BUG("unknow val type %d to serialize.\n", val->type);
			return NULL;
	}
}

__LOCAL uint8_t *serialize_null(brpc_val_t *val, uint8_t *pos, 
		const uint8_t *end)
{
	if ((end - pos) < REC_HDR_SIZE) {
		WERRNO(ENOBUFS);
		return NULL;
	}
	*pos = val->type;
	return pos + REC_HDR_SIZE;
}

__LOCAL uint8_t *serialize_int(brpc_val_t *val, uint8_t *pos, 
		const uint8_t *end)
{
	size_t ilen;
	bool needs; /* does the len need a field of its own? */

	ilen = sizeofl(val->val.int32);
	ilen = clss(ilen);

	needs = (MAX_REC_SSIZE < ilen) ? true : false;
	if (end - pos < REC_HDR_SIZE + ilen + (needs ? 1 : 0)) {
		WERRNO(ENOBUFS);
		return NULL;
	}

	*pos = ilen << REC_TYPE_BITS;
	*pos |= BRPC_VAL_INT;
	pos ++;

	if (needs) {
		assert(ilen < 0xFF);
		pos += lhton(pos, ilen);
	}

	pos += lhton(pos, val->val.int32);
	
	DBG("written (%zd:%zd): %d.\n", REC_HDR_SIZE + (needs?1:0), ilen, 
			val->val.int32);
	return pos;
}

__LOCAL uint8_t *serialize_bin(brpc_val_t *val, uint8_t *pos,
		const uint8_t *end)
{
	size_t lensize;

	if (val->val.bin.len <= MAX_REC_SSIZE) {
		/* do I have enough room? */
		if ((end-pos) < REC_HDR_SIZE + val->val.bin.len) {
			WERRNO(ENOBUFS);
			return NULL;
		}
		*pos = val->val.bin.len << REC_TYPE_BITS;
		*pos |= val->type;
		pos ++;
	
		DBG("written bin/str (%zd:%zd): `%.*s'.\n", REC_HDR_SIZE, 
				val->val.bin.len, val->val.bin.len, val->val.bin.val);
	} else {
		/* need to use 'opt value len' header */
		lensize = sizeofz(val->val.bin.len);
		assert(lensize < MAX_REC_SSIZE); /* not a string with len >= 1<<32 */

		/* do I have enough room? */
		if ((end - pos) < REC_HDR_SIZE + lensize + val->val.bin.len) {
			WERRNO(ENOBUFS);
			return NULL;
		}
		
		/* looks good: do copy */
		*pos = lensize << REC_TYPE_BITS;
		*pos |= REC_HAVE_SFLD; /* record uses optional lenth */
		*pos |= val->type;
		pos ++;

		htonz(pos, val->val.bin.len);
		pos += lensize;
	
		DBG("written bin/str (%zd:%zd): `%.*s'.\n", REC_HDR_SIZE + 
			sizeofz(val->val.bin.len), val->val.bin.len,
			val->val.bin.len, val->val.bin.val);
	}
	
	memcpy(pos, val->val.bin.val, val->val.bin.len);
	pos += val->val.bin.len;

	return pos;
}

__LOCAL uint8_t *serialize_complex(brpc_val_t *val, uint8_t *pos,
		const uint8_t *end)
{
	uint8_t *start;
	size_t clen, avail, lensize;

	start = pos;
	if (! (pos = serialize_content(val, pos, end)))
		return NULL;

	clen = pos - start;
	avail = end - pos;
	
	if (clen <= (1 << REC_SIZE_BITS) - 1) {
		/* enough buffer room? */
		if (avail < REC_HDR_SIZE) {
			WERRNO(ENOBUFS);
			return NULL;
		}
	
		/* move all buffer up REC_HDR_SIZE bytes */
		memmove(start + REC_HDR_SIZE, start, clen);
		*start = clen << REC_TYPE_BITS;
		*start |= val->type;

		pos += REC_HDR_SIZE;
	} else {
		/* need to use 'opt value len' header */
		lensize = sizeofz(clen);
		if (avail < REC_HDR_SIZE + lensize) {
			WERRNO(ENOBUFS);
			return NULL;
		}

		memmove(start + REC_HDR_SIZE + lensize, start, clen);
		*start = lensize << REC_TYPE_BITS;
		*start |= REC_HAVE_SFLD;
		*start |= val->type;
		start ++;
		htonz(start, clen);

		pos += REC_HDR_SIZE + lensize;
	}

	return pos;
}

__LOCAL uint8_t *serialize_content(brpc_val_t *val, uint8_t *pos,
		const uint8_t *end)
{
	struct brpc_list_head *k;

	switch (val->type) {
		case BRPC_VAL_AVP:
		case BRPC_VAL_MAP:
		case BRPC_VAL_LIST:
			list_for_each(k, &val->val.seq.list) {
				pos = brpc_val_ser(_BRPC_VAL4LIST(k), pos, end);
				if (! pos)
					return NULL;
			}
			break;

		default:
			WERRNO(EINVAL);
			BUG("value type 0x%x not a complex type.\n", val->type);
			return NULL;
	}

	return pos;
}

brpc_val_t *brpc_val_clone(const brpc_val_t *orig)
{
	brpc_val_t *clo, *dup;
	bool (*add)(brpc_val_t *, brpc_val_t *);
	struct brpc_list_head *pos;

	if (orig->null)
		return brpc_null(orig->type);

	switch (orig->type) {
		case BRPC_VAL_INT:
			return brpc_int(orig->val.int32);
		case BRPC_VAL_BIN:
			return brpc_bin(orig->val.bin.val, orig->val.bin.len);
		case BRPC_VAL_STR:
			return brpc_str(orig->val.str.val, orig->val.str.len);

		do {
		case BRPC_VAL_AVP:
			dup = brpc_empty_avp();
			add = brpc_avp_add;
			break;
		case BRPC_VAL_MAP:
			dup = brpc_map(NULL);
			add = brpc_map_add;
			break;
		case BRPC_VAL_LIST: 
			dup = brpc_list(NULL);
			add = brpc_list_add;
		} while (0);
			if (! dup)
				return NULL;
			list_for_each(pos, &orig->val.seq.list) {
				clo = brpc_val_clone(_BRPC_VAL4LIST(pos));
				if (! add(dup, clo))
					brpc_val_free(dup);
					return NULL;
			}
			return dup;

		default:
			BUG("illegal val type (%d).\n", orig->type);
			return NULL;
	}
}

__LOCAL bool write_repr(brpc_str_t *repr, ssize_t *pos, char val)
{
	char *ptr;
	size_t nsz;

	if (repr->len < *pos + /*0-term*/1) {
		switch (repr->len) {
			case 0: nsz = BINRPC_MAX_REPR_LEN; break;
			default: nsz = 2 * (repr->len - /*0-term*/1);
		}
		nsz ++; /*0-term*/
		ptr = brpc_realloc(repr->val, nsz * sizeof(char));
		if (! ptr) {
			WERRNO(ENOMEM);
			return false;
		}
		repr->len = nsz;
		repr->val = ptr;
	}
	repr->val[(*pos)++] = val;
	return true;
}

__LOCAL bool repr_val(brpc_val_t *rec, brpc_str_t *repr, ssize_t *pos)
{
	char val, clo;

	switch (rec->type) {
		do {
		case BRPC_VAL_INT: val = 'i'; break;
		case BRPC_VAL_STR: val = 's'; break;
		case BRPC_VAL_BIN: val = 'b'; break;
		} while (0);
			return write_repr(repr, pos, val);

		do {
		case BRPC_VAL_AVP: val = '<'; clo = '>'; break;
		case BRPC_VAL_MAP: val = '{'; clo = '}'; break;
		case BRPC_VAL_LIST: val = '['; clo = ']'; break;
		} while (0);
			if (! write_repr(repr, pos, val))
				return false;
			if (! brpc_vals_repr(&rec->val.seq.list, repr, pos))
				return false;
			return write_repr(repr, pos, clo);

		default:
			BUG("illegal value type (%d); unknown reprriptor.\n", rec->type);
	}
	return false;
}

bool brpc_vals_repr(struct brpc_list_head *head, brpc_str_t *repr, ssize_t *pos)
{
	struct brpc_list_head *k;
	brpc_val_t *val;
	list_for_each(k, head) {
		val = _BRPC_VAL4LIST(k);
		if (! repr_val(val, repr, pos))
			return false;
	}
	return true;
}

ssize_t brpc_val_repr(brpc_val_t *val, char *into, size_t *len)
{
	brpc_str_t repr = {0, 0};
	ssize_t cp, zoff, pos = 0;

	if ((! into) || (! len) || (! *len)) {
		WERRNO(EINVAL);
		goto err;
	}

	if (repr_val(val, &repr, &pos)) {
		if (pos + 1 < *len) {
			cp = pos;
			zoff = pos;
			*len = pos + 1;
		} else {
			cp = *len - 2;
			zoff = *len -1;
		}
		memcpy(into, repr.val, cp);
		into[zoff] = 0;
		brpc_free(repr.val);
		return pos;
	}
err:
	return -1;
}

brpc_val_t *brpc_val_fetch_val(brpc_val_t *seq, size_t index)
{
	struct brpc_list_head *k;
	size_t idx = 0;
	if (index < 0) {
		WERRNO(EINVAL);
		return NULL;
	}
	list_for_each(k, &brpc_seq_cells(seq)) {
		if (idx == index)
			return _BRPC_VAL4LIST(k);
		idx ++;
	}
	return NULL;
}
