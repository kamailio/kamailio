/* 
 * $Id$ 
 */

#include "db_val.h"
#include "defs.h"
#include <stdlib.h>
#include <stdio.h>
#include "db_utils.h"
#include "../../dprint.h"


static int str2int   (const char* _s, int* _v);
static int str2double(const char* _s, double* _v);
static int str2time  (const char* _s, time_t* _v);
static int int2str   (int _v, char* _s, int* _l);
static int double2str(double _v, char* _s, int* _l);
static int time2str  (time_t _v, char* _s, int* _l);


/*
 * Does not copy strings
 */
int str2val(db_type_t _t, db_val_t* _v, const char* _s)
{
#ifdef PARANOID
	if (!_v) {
		log(L_ERR, "str2val(): Invalid parameter value\n");
		return FALSE;
	}
#endif

	if (!_s) {
		VAL_TYPE(_v) = _t;
		VAL_NULL(_v) = 1;
		return TRUE;
	}

	switch(_t) {
	case DB_INT:
		if (str2int(_s, &VAL_INT(_v)) == FALSE) {
			log(L_ERR, "str2val(): Error while converting integer value from string\n");
			return FALSE;
		} else {
			VAL_TYPE(_v) = DB_INT;
			return TRUE;
		}
		break;
	
	case DB_DOUBLE:
		if (str2double(_s, &VAL_DOUBLE(_v)) == FALSE) {
			log(L_ERR, "str2val(): Error while converting double value from string\n");
			return FALSE;
		} else {
			VAL_TYPE(_v) = DB_DOUBLE;
			return TRUE;
		}
		break;

	case DB_STRING:
		VAL_STRING(_v) = _s;
		VAL_TYPE(_v) = DB_STRING;
		return TRUE;

	case DB_DATETIME:
		if (str2time(_s, &VAL_TIME(_v)) == FALSE) {
			log(L_ERR, "str2val(): Error while converting datetime value from string\n");
			return FALSE;
		} else {
			VAL_TYPE(_v) = DB_DATETIME;
			return TRUE;
		}
		break;
	}
	return FALSE;
}


int val2str(db_val_t* _v, char* _s, int* _len)
{
	int l;
#ifdef PARANOID
	if ((!_v) || (!_s) || (!_len) || (!*_len)) {
		log(L_ERR, "val2str(): Invalid parameter value\n");
		return FALSE;
	}
#endif
	if (VAL_NULL(_v)) {
		*_len = snprintf(_s, *_len, "NULL");
		return TRUE;
	}
	
	switch(VAL_TYPE(_v)) {
	case DB_INT:
		if (int2str(VAL_INT(_v), _s, _len) == FALSE) {
			log(L_ERR, "val2str(): Error while converting string to int\n");
			return FALSE;
		} else {
			return TRUE;
		}
		break;

	case DB_DOUBLE:
		if (double2str(VAL_DOUBLE(_v), _s, _len) == FALSE) {
			log(L_ERR, "val2str(): Error while converting string to double\n");
			return FALSE;
		} else {
			return TRUE;
		}
		break;

	case DB_STRING:
		l = strlen(VAL_STRING(_v));
		if (*_len < (l + 2)) {
			log(L_ERR, "val2str(): Destination buffer too short\n");
			return FALSE;
		} else {
			*_s++ = '\'';
			memcpy(_s, VAL_STRING(_v), l);
			*(_s + l) = '\'';
			*_len = l + 2;
			return TRUE;
		}
		break;

	case DB_DATETIME:
		if (time2str(VAL_TIME(_v), _s, _len) == FALSE) {
			log(L_ERR, "val2str(): Error while converting string to time_t\n");
			return FALSE;
		} else {
			return TRUE;
		}
		break;

	default:
		printf("val2str(): Unknow data type\n");
		return FALSE;
	}
	return FALSE;
}


static int str2int(const char* _s, int* _v)
{
#ifdef PARANOID
	if ((!_s) || (!_v)) {
		log(L_ERR, "str2int(): Invalid parameter value\n");
		return FALSE;
	}
#endif
	*_v = atoi(_s);
	return TRUE;
}


static int str2double(const char* _s, double* _v)
{
#ifdef PARANOID
	if ((!_s) || (!_v)) {
		log(L_ERR, "str2double(): Invalid parameter value\n");
		return FALSE;
	}
#endif
	*_v = atof(_s);
	return TRUE;
}


static int str2time(const char* _s, time_t* _v)
{
#ifdef PARANOID
	if ((!_s) || (!_v)) {
		log(L_ERR, "str2time(): Invalid parameter value\n");
		return FALSE;
	}
#endif
	*_v = mysql2time(_s);
	return TRUE;
}


static int int2str(int _v, char* _s, int* _l)
{
#ifdef PARANOID
	if ((!_s) || (!_l) || (!*_l)) {
		log(L_ERR, "int2str(): Invalid parameter value\n");
		return FALSE;
	}
#endif
	*_l = snprintf(_s, *_l, "%-d", _v);
	return TRUE;
}


static int double2str(double _v, char* _s, int* _l)
{
#ifdef PARANOID
	if ((!_s) || (!_l) || (!*_l)) {
		log(L_ERR, "double2str(): Invalid parameter value\n");
		return FALSE;
	}
#endif
	*_l = snprintf(_s, *_l, "%-10.2f", _v);
	return TRUE;
}


static int time2str(time_t _v, char* _s, int* _l)
{
	int l;
#ifdef PARANOID
	if ((!_s) || (!_l) || (*_l < 2))  {
		log(L_ERR, "Invalid parameter value\n");
		return FALSE;
	}
#endif
	*_s++ = '\'';
	l = time2mysql(_v, _s, *_l - 1);
	*(_s + l) = '\'';
	*_l = l + 2;
	return TRUE;
}
