#include "db_val.h"
#include "defs.h"
#include <stdlib.h>
#include <stdio.h>
#include "../../dprint.h"


static int str2int   (const char* _s, int* _v);
static int str2float (const char* _s, float* _v);
static int str2time  (const char* _s, time_t* _v);
static int int2str   (int _v, char* _s, int* _l);
static int float2str (float _v, char* _s, int* _l);
static int time2str  (time_t _v, char* _s, int* _l);


/*
 * Does not copy strings
 */
int str2val(db_type_t _t, db_val_t* _v, const char* _s)
{
#ifdef PARANOID
	if (!_v) return FALSE;
	if (!_s) return FALSE;
#endif
	switch(_t) {
	case DB_INT:
		if (str2int(_s, &(_v->val.int_val)) == FALSE) {
			log(L_ERR, "str2val(): Error while converting integer value from string\n");
			return FALSE;
		} else {
			_v->type = DB_INT;
			return TRUE;
		}
		break;
	
	case DB_FLOAT:
		if (str2float(_s, &(_v->val.float_val)) == FALSE) {
			log(L_ERR, "str2val(): Error while converting float value from string\n");
			return FALSE;
		} else {
			_v->type = DB_FLOAT;
			return TRUE;
		}
		break;

	case DB_STRING:
		_v->val.string_val = _s;
		_v->type = DB_STRING;
		return TRUE;

	case DB_DATETIME:
		if (str2time(_s, &(_v->val.time_val)) == FALSE) {
			log(L_ERR, "str2val(): Error while converting datetime value from string\n");
			return FALSE;
		} else {
			_v->type = DB_DATETIME;
			return TRUE;
		}
		break;
	}
}


int val2str(db_val_t* _v, char* _s, int* _len)
{
	int l;
#ifdef PARANOID
	if (!_v) return FALSE;
	if (!_s) return FALSE;
	if (!_len) return FALSE;
	if (!*_len) return FALSE;
#endif
	switch(_v->type) {
	case DB_INT:
		if (int2str(_v->val.int_val, _s, _len) == FALSE) {
			log(L_ERR, "val2str(): Error while converting string to int\n");
			return FALSE;
		} else {
			return TRUE;
		}
		break;

	case DB_FLOAT:
		if (float2str(_v->val.float_val, _s, _len) == FALSE) {
			log(L_ERR, "val2str(): Error while converting string to float\n");
			return FALSE;
		} else {
			return TRUE;
		}
		break;

	case DB_STRING:
		l = strlen(_v->val.string_val);
		if (*_len < (l + 2)) {
			log(L_ERR, "val2str(): Destination buffer too short\n");
			return FALSE;
		} else {
			*_s++ = '\'';
			memcpy(_s, _v->val.string_val, l);
			*(_s + l) = '\'';
			*_len = l + 2;
			return TRUE;
		}
		break;

	case DB_DATETIME:
		if (time2str(_v->val.time_val, _s, _len)) {
			log(L_ERR, "val2str(): Error while converting string to time_t\n");
			return FALSE;
		} else {
			return TRUE;
		}
		break;
	}
}


static int str2int(const char* _s, int* _v)
{
#ifdef PARANOID
	if (!_s) return FALSE;
	if (!_v) return FALSE;
#endif
	*_v = atoi(_s);
	return TRUE;
}


static int str2float(const char* _s, float* _v)
{
#ifdef PARANOID
	if (!_s) return FALSE;
	if (!_v) return FALSE;
#endif
	*_v = atof(_s);
	return TRUE;
}


static int str2time(const char* _s, time_t* _v)
{
#ifdef PARANOID
	if (!_s) return FALSE;
	if (!_v) return FALSE;
#endif
	*_v = mysql2time(_s);
	return TRUE;
}


static int int2str(int _v, char* _s, int* _l)
{
#ifdef PARANOID
	if ((!_s) || (!_l) || (!*_l)) return FALSE;
#endif
	*_l = snprintf(_s, *_l, "%d", _v);
	return TRUE;
}


static int float2str(float _v, char* _s, int* _l)
{
#ifdef PARANOID
	if ((!_s) || (!_l) || (!*_l)) return FALSE;
#endif
	*_l = snprintf(_s, *_l, "%10.2f", _v);
	return TRUE;
}


static int time2str(time_t _v, char* _s, int* _l)
{
#ifdef PARANOID
	if ((!_s) || (!_l) || (!*_l))  return FALSE;
#endif
	*_l = time2mysql(_v, _s, *_l);
	return TRUE;
}
