/*
 * $Id$
 *
 * 32-bit Digest parameter name parser
 *
 * Copyright (C) 2001-2003 Fhg Fokus
 *
 * This file is part of ser, a free SIP server.
 *
 * ser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * For a license to use the ser software under conditions
 * other than those described here, or to purchase support for this
 * software, please contact iptel.org by e-mail at the following addresses:
 *    info@iptel.org
 *
 * ser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */



#include "param_parser.h"
#include "digest_keys.h"
#include "../../trim.h"
#include "../../ut.h"

/*
 * Precalculated hash table size
 * WARNING: The size MUST be recalculated
 *          again if new parameter names
 *          should be parsed
 */
#define HASH_TABLE_SIZE 859


/*
 * Hash function
 */
#define HASH_FUNC(val) ((val) % HASH_TABLE_SIZE)


/*
 * This constant marks an empty hash table element
 */
#define HASH_EMPTY 0x2d2d2d2d


/*
 * Hash table entry
 */
struct ht_entry {
	unsigned int key;
	unsigned int value;
};


static struct ht_entry hash_table[HASH_TABLE_SIZE];


/*
 * Pointer to the hash table
 */
/*
static struct ht_entry *hash_table;
*/


/*
 * Declarations
 */
static void set_entry (unsigned int key, unsigned int val);
static inline int unify (int key);



/*
 * Used to initialize hash table
 */
static void set_entry(unsigned int key, unsigned int val)
{
	hash_table[HASH_FUNC(key)].key = key;
	hash_table[HASH_FUNC(key)].value = val;
}


static inline int unify(int key)
{
	register struct ht_entry* en;

	en = &hash_table[HASH_FUNC(key)];
	if (en->key == key) {
		return en->value;
	} else {
		return key;
	}
}


/*
 * Parse short (less than 4 bytes) parameter names
 */
#define PARSE_SHORT                                                   \
	switch(*p) {                                                  \
	case 'u':                                                     \
	case 'U':                                                     \
		if ((*(p + 1) == 'r') || (*(p + 1) == 'R')) {         \
			if ((*(p + 2) == 'i') || (*(p + 2) == 'I')) { \
				*_type = PAR_URI;                     \
                                p += 3;                               \
				goto end;                             \
			}                                             \
		}                                                     \
		break;                                                \
                                                                      \
	case 'q':                                                     \
	case 'Q':                                                     \
		if ((*(p + 1) == 'o') || (*(p + 1) == 'O')) {         \
			if ((*(p + 2) == 'p') || (*(p + 2) == 'P')) { \
				*_type = PAR_QOP;                     \
                                p += 3;                               \
				goto end;                             \
			}                                             \
		}                                                     \
		break;                                                \
                                                                      \
	case 'n':                                                     \
	case 'N':                                                     \
		if ((*(p + 1) == 'c') || (*(p + 1) == 'C')) {         \
			*_type = PAR_NC;                              \
                        p += 2;                                       \
			goto end;                                     \
		}                                                     \
		break;                                                \
	}


/*
 * Read 4-bytes from memory and store them in an integer variable
 * Reading byte by byte ensures, that the code works also on HW which
 * does not allow reading 4-bytes at once from unaligned memory position
 * (Sparc for example)
 */
#define READ(val) \
(*(val + 0) + (*(val + 1) << 8) + (*(val + 2) << 16) + (*(val + 3) << 24))


#define name_CASE                      \
        switch(val) {                  \
        case _name_:                   \
		*_type = PAR_USERNAME; \
                p += 4;                \
		goto end;              \
        }


#define user_CASE         \
        p += 4;           \
        val = READ(p);    \
        name_CASE;        \
                          \
        val = unify(val); \
        name_CASE;        \
        goto other;


#define real_CASE                         \
        p += 4;                           \
        if ((*p == 'm') || (*p == 'M')) { \
		*_type = PAR_REALM;       \
                p++;                      \
		goto end;                 \
	}


#define nonc_CASE                         \
        p += 4;                           \
        if ((*p == 'e') || (*p == 'E')) { \
	        *_type = PAR_NONCE;       \
                p++;                      \
		goto end;                 \
	}


#define onse_CASE                      \
        switch(val) {                  \
        case _onse_:                   \
		*_type = PAR_RESPONSE; \
                p += 4;                \
		goto end;              \
        }


#define resp_CASE         \
        p += 4;           \
        val = READ(p);    \
        onse_CASE;        \
                          \
        val = unify(val); \
        onse_CASE;        \
        goto other;


#define cnon_CASE                                 \
        p += 4;                                   \
        if ((*p == 'c') || (*p == 'C')) {         \
		p++;                              \
		if ((*p == 'e') || (*p == 'E')) { \
			*_type = PAR_CNONCE;      \
                        p++;                      \
			goto end;                 \
		}                                 \
	}                                         \
        goto other;


#define opaq_CASE                                 \
        p += 4;                                   \
        if ((*p == 'u') || (*p == 'U')) {         \
		p++;                              \
		if ((*p == 'e') || (*p == 'E')) { \
			*_type = PAR_OPAQUE;      \
                        p++;                      \
			goto end;                 \
		}                                 \
	}                                         \
        goto other;


#define rith_CASE                                 \
        switch(val) {                             \
	case _rith_:                              \
		p += 4;                           \
		if ((*p == 'm') || (*p == 'M')) { \
			*_type = PAR_ALGORITHM;   \
                        p++;                      \
			goto end;                 \
		}                                 \
		goto other;                       \
	}


#define algo_CASE         \
        p += 4;           \
        val = READ(p);    \
        rith_CASE;        \
                          \
        val = unify(val); \
        rith_CASE;        \
        goto other;


#define FIRST_QUATERNIONS       \
        case _user_: user_CASE; \
        case _real_: real_CASE; \
        case _nonc_: nonc_CASE; \
        case _resp_: resp_CASE; \
        case _cnon_: cnon_CASE; \
        case _opaq_: opaq_CASE; \
        case _algo_: algo_CASE;




int parse_param_name(str* _s, dig_par_t* _type)
{
	register char* p;
	register int val;
	char* end;

	end = _s->s + _s->len;

	p = _s->s;
	val = READ(p);

	if (_s->len < 4) {
		goto other;
	}

	switch(val) {
	FIRST_QUATERNIONS;
	default:
		PARSE_SHORT;

		val = unify(val);
		switch(val) {
		FIRST_QUATERNIONS;
		default: goto other;
		}
        }

 end:
	_s->len -= p - _s->s;
	_s->s = p;

	trim_leading(_s);
	if (_s->s[0] == '=') {
		return 0;
	}
	
 other:
	p = q_memchr(p, '=', end - p);
	if (!p) {
		return -1; /* Parse error */
	} else {
		*_type = PAR_OTHER;
		_s->len -= p - _s->s;
		_s->s = p;
		return 0;
	}
}


/* Number of distinct keys */
#define NUM_KEYS  160

/* Number of distinct values */
#define NUM_VALS 10


/*
 * Create synonym-less (precalculated) hash table
 */
void init_digest_htable(void)
{
	int i, j, k;

	unsigned int init_val[NUM_VALS] = {
		_user_, _name_, _real_, _nonc_,
		_resp_, _onse_, _cnon_, _opaq_,
		_algo_, _rith_
	};

	unsigned int key_nums[NUM_VALS] = {
		16, 16, 16, 16, 16,
		16, 16, 16, 16, 16
	};
	
	unsigned int init_key[NUM_KEYS] = {
		_user_, _useR_, _usEr_, _usER_, _uSer_, _uSeR_, _uSEr_, _uSER_, 
		_User_, _UseR_, _UsEr_, _UsER_, _USer_, _USeR_, _USEr_, _USER_, 
		_name_, _namE_, _naMe_, _naME_, _nAme_, _nAmE_, _nAMe_, _nAME_, 
		_Name_, _NamE_, _NaMe_, _NaME_, _NAme_, _NAmE_, _NAMe_, _NAME_, 
		_real_, _reaL_, _reAl_, _reAL_, _rEal_, _rEaL_, _rEAl_, _rEAL_, 
		_Real_, _ReaL_, _ReAl_, _ReAL_, _REal_, _REaL_, _REAl_, _REAL_, 
		_nonc_, _nonC_, _noNc_, _noNC_, _nOnc_, _nOnC_, _nONc_, _nONC_, 
		_Nonc_, _NonC_, _NoNc_, _NoNC_, _NOnc_, _NOnC_, _NONc_, _NONC_, 
		_resp_, _resP_, _reSp_, _reSP_, _rEsp_, _rEsP_, _rESp_, _rESP_, 
		_Resp_, _ResP_, _ReSp_, _ReSP_, _REsp_, _REsP_, _RESp_, _RESP_, 
		_onse_, _onsE_, _onSe_, _onSE_, _oNse_, _oNsE_, _oNSe_, _oNSE_, 
		_Onse_, _OnsE_, _OnSe_, _OnSE_, _ONse_, _ONsE_, _ONSe_, _ONSE_, 
		_cnon_, _cnoN_, _cnOn_, _cnON_, _cNon_, _cNoN_, _cNOn_, _cNON_, 
		_Cnon_, _CnoN_, _CnOn_, _CnON_, _CNon_, _CNoN_, _CNOn_, _CNON_, 
		_opaq_, _opaQ_, _opAq_, _opAQ_, _oPaq_, _oPaQ_, _oPAq_, _oPAQ_, 
		_Opaq_, _OpaQ_, _OpAq_, _OpAQ_, _OPaq_, _OPaQ_, _OPAq_, _OPAQ_, 
		_algo_, _algO_, _alGo_, _alGO_, _aLgo_, _aLgO_, _aLGo_, _aLGO_, 
		_Algo_, _AlgO_, _AlGo_, _AlGO_, _ALgo_, _ALgO_, _ALGo_, _ALGO_, 
		_rith_, _ritH_, _riTh_, _riTH_, _rIth_, _rItH_, _rITh_, _rITH_, 
		_Rith_, _RitH_, _RiTh_, _RiTH_, _RIth_, _RItH_, _RITh_, _RITH_
	};


	     /* Mark all elements as empty */
	for(i = 0; i < HASH_TABLE_SIZE; i++) {
		set_entry(HASH_EMPTY, HASH_EMPTY);
	}

	k = 0;

	     /* Initialize hash table content */
	for(i = 0; i < NUM_VALS; i++) {
		for(j = 0; j < key_nums[i]; j++) {
			set_entry(init_key[k++], init_val[i]);
		}
	}
}
