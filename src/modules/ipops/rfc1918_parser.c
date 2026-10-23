
#line 1 "rfc1918_parser.rl"
#include "rfc1918_parser.h"


/** Ragel machine **/

#line 21 "rfc1918_parser.rl"


/** Data **/

#line 10 "rfc1918_parser.c"
static const char _rfc1918_parser_actions[] = {0, 1, 0};

static const char _rfc1918_parser_key_offsets[] = {0, 0, 1, 4, 5, 10, 11, 16,
		17, 22, 25, 28, 34, 37, 40, 43, 49, 52, 53, 54, 57, 59, 61, 63, 64, 65,
		66, 67, 68, 68, 70, 72, 77};

static const char _rfc1918_parser_trans_keys[] = {49, 48, 55, 57, 46, 48, 49,
		50, 51, 57, 46, 48, 49, 50, 51, 57, 46, 48, 49, 50, 51, 57, 46, 48, 57,
		46, 48, 57, 46, 53, 48, 52, 54, 57, 46, 48, 53, 46, 48, 57, 46, 48, 57,
		46, 53, 48, 52, 54, 57, 46, 48, 53, 50, 46, 49, 50, 51, 54, 57, 48, 57,
		48, 49, 50, 46, 49, 54, 56, 48, 57, 48, 57, 53, 48, 52, 54, 57, 48, 53,
		0};

static const char _rfc1918_parser_single_lengths[] = {0, 1, 3, 1, 3, 1, 3, 1, 3,
		1, 1, 2, 1, 1, 1, 2, 1, 1, 1, 3, 0, 0, 0, 1, 1, 1, 1, 1, 0, 0, 0, 1, 0};

static const char _rfc1918_parser_range_lengths[] = {0, 0, 0, 0, 1, 0, 1, 0, 1,
		1, 1, 2, 1, 1, 1, 2, 1, 0, 0, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 1, 1, 2, 1};

static const char _rfc1918_parser_index_offsets[] = {0, 0, 2, 6, 8, 13, 15, 20,
		22, 27, 30, 33, 38, 41, 44, 47, 52, 55, 57, 59, 63, 65, 67, 69, 71, 73,
		75, 77, 79, 80, 82, 84, 88};

static const char _rfc1918_parser_indicies[] = {0, 1, 2, 3, 4, 1, 5, 1, 6, 7, 8,
		9, 1, 10, 1, 11, 12, 13, 14, 1, 15, 1, 16, 17, 18, 19, 1, 15, 14, 1, 15,
		11, 1, 15, 20, 14, 11, 1, 15, 11, 1, 10, 9, 1, 10, 6, 1, 10, 21, 9, 6,
		1, 10, 6, 1, 22, 1, 23, 1, 24, 25, 26, 1, 6, 1, 6, 1, 6, 1, 27, 1, 28,
		1, 29, 1, 30, 1, 6, 1, 1, 19, 1, 16, 1, 31, 19, 16, 1, 16, 1, 0};

static const char _rfc1918_parser_trans_targs[] = {2, 0, 3, 17, 23, 4, 5, 13,
		15, 14, 6, 7, 9, 11, 10, 8, 28, 29, 31, 30, 12, 16, 18, 19, 20, 21, 22,
		24, 25, 26, 27, 32};

static const char _rfc1918_parser_trans_actions[] = {0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1};

static const int rfc1918_parser_start = 1;
static const int rfc1918_parser_first_final = 28;
static const int rfc1918_parser_error = 0;

static const int rfc1918_parser_en_main = 1;


#line 26 "rfc1918_parser.rl"


/** exec **/
unsigned int rfc1918_parser_execute(const char *str, size_t len)
{
	int cs = 0;
	const char *p, *pe;
	unsigned int is_ip_rfc1918 = 0;

	p = str;
	pe = str + len;


#line 106 "rfc1918_parser.c"
	{
		cs = rfc1918_parser_start;
	}

#line 39 "rfc1918_parser.rl"

#line 109 "rfc1918_parser.c"
	{
		int _klen;
		unsigned int _trans;
		const char *_acts;
		unsigned int _nacts;
		const char *_keys;

		if(p == pe)
			goto _test_eof;
		if(cs == 0)
			goto _out;
	_resume:
		_keys = _rfc1918_parser_trans_keys + _rfc1918_parser_key_offsets[cs];
		_trans = _rfc1918_parser_index_offsets[cs];

		_klen = _rfc1918_parser_single_lengths[cs];
		if(_klen > 0) {
			const char *_lower = _keys;
			const char *_mid;
			const char *_upper = _keys + _klen - 1;
			while(1) {
				if(_upper < _lower)
					break;

				_mid = _lower + ((_upper - _lower) >> 1);
				if((*p) < *_mid)
					_upper = _mid - 1;
				else if((*p) > *_mid)
					_lower = _mid + 1;
				else {
					_trans += (unsigned int)(_mid - _keys);
					goto _match;
				}
			}
			_keys += _klen;
			_trans += _klen;
		}

		_klen = _rfc1918_parser_range_lengths[cs];
		if(_klen > 0) {
			const char *_lower = _keys;
			const char *_mid;
			const char *_upper = _keys + (_klen << 1) - 2;
			while(1) {
				if(_upper < _lower)
					break;

				_mid = _lower + (((_upper - _lower) >> 1) & ~1);
				if((*p) < _mid[0])
					_upper = _mid - 2;
				else if((*p) > _mid[1])
					_lower = _mid + 2;
				else {
					_trans += (unsigned int)((_mid - _keys) >> 1);
					goto _match;
				}
			}
			_trans += _klen;
		}

	_match:
		_trans = _rfc1918_parser_indicies[_trans];
		cs = _rfc1918_parser_trans_targs[_trans];

		if(_rfc1918_parser_trans_actions[_trans] == 0)
			goto _again;

		_acts = _rfc1918_parser_actions + _rfc1918_parser_trans_actions[_trans];
		_nacts = (unsigned int)*_acts++;
		while(_nacts-- > 0) {
			switch(*_acts++) {
				case 0:
#line 8 "rfc1918_parser.rl"
				{
					is_ip_rfc1918 = 1;
				} break;
#line 187 "rfc1918_parser.c"
			}
		}

	_again:
		if(cs == 0)
			goto _out;
		if(++p != pe)
			goto _resume;
	_test_eof : {
	}
	_out : {
	}
	}

#line 40 "rfc1918_parser.rl"


	if(is_ip_rfc1918 && p == pe)
		return 1;

	return 0;
}
