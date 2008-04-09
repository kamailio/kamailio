#include "orasel.h"
#include <math.h>
#include <assert.h>

/*
 * Uncomment next string if you will sell 'NULL' on unitialized NON text field
 */
//#define NULL_ID "NULL"

static char st_buf[65536];

enum type_t {
  DB_STR = 0,
  DB_DATETIME,
  /* end of left alignment */
  DB_INT,
  DB_BITMAP,
  DB_DOUBLE   /* MUST belast */
};

//---------------------------------------------------------
struct dmap {
    OCIDefine** defh;
    union {
	dvoid* v;
	double* f;
	int* i;
	char* c;
	OCIDate* o;
    }* pv;
    dvoid** pval;
    ub2* ilen;
    sb2* ind;
    ub2* len;
};
typedef struct dmap dmap_t;

//-----------------------------------------------------------------------------
static void dmap_init(dmap_t* _d, unsigned n)
{
	size_t sz = sizeof(*_d->defh) + sizeof(*_d->pv) + sizeof(*_d->pval) +
		    sizeof(*_d->ilen) + sizeof(*_d->ind) + sizeof(*_d->len);
	unsigned char *p = safe_malloc(sz * n);

	_d->defh = (void*)p;
	p += n*sizeof(*_d->defh);
	_d->pv = (void*)p;
	p += n*sizeof(*_d->pv);
	_d->pval = (void*)p;
	p += n*sizeof(*_d->pval);
	_d->ilen = (void*)p;
	p += n*sizeof(*_d->ilen);
	_d->ind = (void*)p;
	p += n*sizeof(*_d->ind);
	_d->len = (void*)p;
//	p += n*sizeof(*_d->len);
}

//-----------------------------------------------------------------------------
/*
 * Get and convert columns from a result. Define handlers and buffers
 */
static void get_columns(const con_t* con, res_t* _r, dmap_t* _d)
{
	OCIParam *param;
	size_t tsz;
	ub4 i, n;
	sword status;

	status = OCIAttrGet(con->stmthp, OCI_HTYPE_STMT, &n, NULL,
			OCI_ATTR_PARAM_COUNT, con->errhp);

	if (status != OCI_SUCCESS) oraxit(status, con);

	if (!n) donegood("Empty table");

	dmap_init(_d, n);

	_r->names = (Str**)safe_malloc(n * sizeof(Str*));
	_r->types = (unsigned char*)safe_malloc(n * sizeof(unsigned char));
	_r->col_n = n;

	tsz = 0;
	memset(_d->defh, 0, sizeof(_d->defh[0]) * n);
	for (i = 0; i < n; i++) {
		ub4 len;
		ub2 dtype;
		unsigned char ctype = DB_DOUBLE;

		status = OCIParamGet(con->stmthp, OCI_HTYPE_STMT, con->errhp,
				(dvoid**)(dvoid*)&param, i+1);
		if (status != OCI_SUCCESS) goto ora_err;

		{
			text *name;
			status = OCIAttrGet(param, OCI_DTYPE_PARAM, 
					(dvoid**)(dvoid*)&name, &len,
					OCI_ATTR_NAME, con->errhp);
			if (status != OCI_SUCCESS) goto ora_err;
			_r->names[i] = str_alloc((char*)name, len);
		}

		status = OCIAttrGet(param, OCI_DTYPE_PARAM,
				(dvoid**)(dvoid*)&dtype, NULL,
				OCI_ATTR_DATA_TYPE, con->errhp);
		if (status != OCI_SUCCESS) goto ora_err;

		switch (dtype) {
		case SQLT_UIN:
set_bitmap:
			ctype = DB_BITMAP;
			len = sizeof(unsigned);
			break;

		case SQLT_INT:
set_int:
			ctype = DB_INT;
			len = sizeof(int);
			break;

		case SQLT_VNU:
		case SQLT_NUM:
			len = 0;  /* PRECISION is ub1 (byte) */
			status = OCIAttrGet(param, OCI_DTYPE_PARAM,
					(dvoid**)(dvoid*)&len, NULL,
					OCI_ATTR_PRECISION, con->errhp);
			if (status != OCI_SUCCESS) goto ora_err;
			if (len <= 11) {
				sb1 sc;
				status = OCIAttrGet(param, OCI_DTYPE_PARAM,
						(dvoid**)(dvoid*)&sc, NULL,
						OCI_ATTR_SCALE, con->errhp);
				if (status != OCI_SUCCESS) goto ora_err;
				if (!sc) {
					dtype = SQLT_INT;
					if (len != 11) goto set_int;
					dtype = SQLT_UIN;
					goto set_bitmap;
				}
				if(sc < 0) sc = 0;
				ctype += sc;
			}
		case SQLT_FLT:
		case SQLT_BFLOAT:
		case SQLT_BDOUBLE:
		case SQLT_IBFLOAT:
		case SQLT_IBDOUBLE:
		case SQLT_PDN:
			len = sizeof(double);
			dtype = SQLT_FLT;
			break;

		case SQLT_DATE:
		case SQLT_DAT:
		case SQLT_ODT:
		case SQLT_TIMESTAMP:
		case SQLT_TIMESTAMP_TZ:
		case SQLT_TIMESTAMP_LTZ:
			ctype = DB_DATETIME;
			len = sizeof(OCIDate);
			dtype = SQLT_ODT;
			break;

		case SQLT_CLOB:
		case SQLT_BLOB:
		case SQLT_CHR:
		case SQLT_STR:
		case SQLT_VST:
		case SQLT_VCS:
		case SQLT_AFC:
		case SQLT_AVC:
			ctype = DB_STR;
			dtype = SQLT_CHR;
			len = 0;  /* DATA_SIZE is ub2 (word) */
			status = OCIAttrGet(param, OCI_DTYPE_PARAM,
					(dvoid**)(dvoid*)&len, NULL,
					OCI_ATTR_DATA_SIZE, con->errhp);
			if (status != OCI_SUCCESS) goto ora_err;
			++len;
			break;

		default:
			errxit("unsupported datatype");
		}
		_r->types[i] = ctype;
		_d->ilen[i] = (ub2)len;
		_d->pv[i].v = st_buf + tsz;
		tsz += len;
		status = OCIDefineByPos(con->stmthp, &_d->defh[i], con->errhp,
				i+1, _d->pv[i].v, len, dtype, &_d->ind[i],
				&_d->len[i], NULL, OCI_DEFAULT);
		if (status != OCI_SUCCESS) goto ora_err;
	}

	if (tsz > sizeof(st_buf)) errxit("too large row");
	return;

ora_err:
	oraxit(status, con);
}

//-----------------------------------------------------------------------------
/*
 * Convert data fron db format to internal format
 */
static void convert_row(const res_t* _res, Str*** _r, const dmap_t* _d)
{
	unsigned i, n = _res->col_n;
	Str** v;

	*_r = v = (Str**)safe_malloc(n * sizeof(Str**));

	for (i = 0; i < n; i++, v++) {
		char buf[64];
		unsigned char t = _res->types[i];

		if (_d->ind[i] == -1) {
			static const struct {
			    unsigned len;
			    char     s[1];
			}_empty = { 0, "" };
#ifdef NULL_ID
			static const struct {
			    unsigned len;
			    char     s[sizeof(NULL_ID)];
			}_null = { sizeof(NULL_ID)-1, NULL_ID };

			*v = (Str*)&_null;
			if (t != DB_STR) continue;
#endif			
			*v = (Str*)&_empty;
			continue;
		}

//		if (_d->ind[i]) errxit("truncated value in DB");

		switch (t) {
		case DB_STR:
			*v = str_alloc(_d->pv[i].c, _d->len[i]);
			break;

		case DB_INT:
			*v = str_alloc(buf, snprintf(buf, sizeof(buf), "%i",
					*_d->pv[i].i));
			break;

		case DB_BITMAP:
			*v = str_alloc(buf, snprintf(buf, sizeof(buf), "0x%X",
					*_d->pv[i].i));
			break;

		case DB_DATETIME:
			{
				struct tm tm;
				memset(&tm, 0, sizeof(tm));
				OCIDateGetTime(_d->pv[i].o, &tm.tm_hour,
						&tm.tm_min, &tm.tm_sec);
				OCIDateGetDate(_d->pv[i].o, &tm.tm_year,
						&tm.tm_mon, &tm.tm_mday);
				if (tm.tm_mon)
					--tm.tm_mon;
				if (tm.tm_year >= 1900)
					tm.tm_year -= 1900;
				*v = str_alloc(buf, strftime(buf, sizeof(buf),
						"%d-%b-%Y %T", &tm));
			}
			break;

		case DB_DOUBLE:
			*v = str_alloc(buf, snprintf(buf, sizeof(buf), "%g",
					*_d->pv[i].f));
			break;

		default:
			{
				double x = fabs(*_d->pv[i].f);
				const char *fmt = "%.*f";
				if (x && (x >= 1.0e6 || x < 1.0e-5))
					fmt = "%.*e";
				*v = str_alloc(buf, snprintf(buf, sizeof(buf),
						fmt, (t - DB_DOUBLE), *_d->pv[i].f));
			}
			break;
		}
	}
}

//-----------------------------------------------------------------------------
/*
 * Get rows and convert it from oracle to db API representation
 */
static void get_rows(const con_t* con, res_t* _r, dmap_t* _d)
{
	ub4 rcnt;
	sword status;
	unsigned n = _r->col_n;

	memcpy(_d->len, _d->ilen, sizeof(_d->len[0]) * n);

	status = OCIStmtFetch2(con->stmthp, con->errhp, 1, OCI_FETCH_LAST, 0,
				OCI_DEFAULT);
	if (status != OCI_SUCCESS) {
		if (status == OCI_NO_DATA) donegood("Empty set");
		goto ora_err;
	}

	status = OCIAttrGet(con->stmthp, OCI_HTYPE_STMT, &rcnt, NULL,
			OCI_ATTR_CURRENT_POSITION, con->errhp);
	if (status != OCI_SUCCESS) goto ora_err;
	if (!rcnt) errxit("lastpos==0");

	_r->row_n = rcnt;
	_r->rows = (Str***)safe_malloc(rcnt * sizeof(Str**));
	while ( 1 ) {
		convert_row(_r, &_r->rows[--rcnt], _d);
		if (!rcnt) return;

		memcpy(_d->len, _d->ilen, sizeof(_d->len[0]) * n);
		status = OCIStmtFetch2(con->stmthp, con->errhp, 1,
					OCI_FETCH_PRIOR, 0, OCI_DEFAULT);
		if (status != OCI_SUCCESS) break;
	}
ora_err:
	oraxit(status, con);
}

//-----------------------------------------------------------------------------
/*
 * Read database answer and fill the structure
 */
void get_res(const con_t* con, res_t* _r)
{
	dmap_t dmap;
	unsigned n;
	unsigned char *pt;

	get_columns(con, _r, &dmap);
	get_rows(con, _r, &dmap);
	n = _r->col_n;
	pt = _r->types;
	do {
		--n;
		assert(DB_STR == 0 && DB_DATETIME == 1);
		pt[n] = (pt[n] <= DB_DATETIME);
	}while(n);
}

//-----------------------------------------------------------------------------
