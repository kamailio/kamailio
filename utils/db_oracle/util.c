#include "orasel.h"

//-----------------------------------------------------------------------------
void __attribute__((noreturn)) donegood(const char *msg)
{
	OCITerminate(OCI_DEFAULT);
	if (msg && !outmode.emp)
		printf("%s\n", msg);
	exit(0);
}

//-----------------------------------------------------------------------------
void __attribute__((noreturn)) errxit(const char *msg)
{
	OCITerminate(OCI_DEFAULT);
	fprintf(stderr, "ERROR: %s\n", msg);
	exit(1);
}

//-----------------------------------------------------------------------------
void __attribute__((noreturn)) oraxit(sword status, const con_t* con)
{
	const char *p = NULL;
	char buf[512];
	sword ecd;

	switch (status) {
	case OCI_SUCCESS_WITH_INFO:
	case OCI_ERROR:
		ecd = 0;
		if(OCIErrorGet(con->errhp, 1, NULL, &ecd, (OraText*)buf,
			sizeof(buf), OCI_HTYPE_ERROR) != OCI_SUCCESS)
		{
			snprintf(buf, sizeof(buf), "unknown ORAERR %u", ecd);
		}
		break;

	default:
		snprintf(buf, sizeof(buf), "unknown status %u", status);
		break;

	case OCI_SUCCESS:
		p = "success";
		break;

	case OCI_NEED_DATA:
		p = "need data";
		break;

	case OCI_NO_DATA:
		p = "no data";
		break;

	case OCI_INVALID_HANDLE:
		p = "invalid handle";
		break;

	case OCI_STILL_EXECUTING: /* ORA-3123 */
		p = "executing";
		break;

	case OCI_CONTINUE:
		p = "continue";
		break;
	}
	if (p) {
		snprintf(buf, sizeof(buf), "logic error (%s)", p);
	}
	errxit(buf);
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
static void __attribute__((noreturn)) nomem(void)
{
	errxit("no enough memory");
}

//-----------------------------------------------------------------------------
void* safe_malloc(size_t sz)
{
	void *p = malloc(sz);
	if (!p) nomem();
	return p;
}

//-----------------------------------------------------------------------------
Str* str_alloc(const char *s, size_t len)
{
	Str* ps = (Str*)safe_malloc(sizeof(Str) + len + 1);
	ps->len = len;
	memcpy(ps->s, s, len);
	ps->s[len] = '\0';
	return ps;
}

//-----------------------------------------------------------------------------
