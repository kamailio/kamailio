#include <errno.h>

#include "../../parser/parse_from.h"
#include "../../parser/parse_cseq.h"
#include "../../parser/parse_content.h"
#include "../../parser/parse_uri.h"
#include "../../parser/contact/parse_contact.h"

#include "../../data_lump.h"
#include "../../msg_translator.h"
#include "auth_identity.h"

/*
 * Dynamic string functions
 */

int initdynstr(dynstr *sout, int isize)
{
	memset(sout,0,sizeof(*sout));
	getstr_dynstr(sout).s=pkg_malloc(isize);
	if (!getstr_dynstr(sout).s) {
		LOG(L_WARN,
			"AUTH_IDENTITY:initdynstr: Not enough memory error\n");
		return -1;
	}
	sout->size=isize;

	return 0;
}

int cpy2dynstr(dynstr *sout, str *s2app)
{
	char *stmp;
	int isize = s2app->len;

	if (isize > sout->size) {
		stmp=pkg_realloc(sout->sd.s, isize);
		if (!stmp) {
			LOG(L_ERR, "AUTH_INDENTITY:cpy2dynstr: Not enough memory error\n");
			return -1;
		}
		sout->sd.s=stmp;
		sout->size=isize;
	}

	memcpy(sout->sd.s,s2app->s,s2app->len);
	sout->sd.len = isize;

	return 0;
}

int app2dynchr(dynstr *sout, char capp)
{
	char *stmp;
	int isize = sout->sd.len + 1;

	if (isize > sout->size) {
		stmp=pkg_realloc(sout->sd.s, isize);
		if (!stmp) {
			LOG(L_ERR, "AUTH_INDENTITY:app2dynchr: Not enough memory error\n");
			return -1;
		}
		sout->sd.s=stmp;
		sout->size++;
	}

	sout->sd.s[sout->sd.len]=capp;
	sout->sd.len++;

	return 0;
}

int app2dynstr(dynstr *sout, str *s2app)
{
	char *stmp;
	int isize = sout->sd.len + s2app->len;

	if (isize > sout->size) {
		stmp=pkg_realloc(sout->sd.s, isize);
		if (!stmp) {
			LOG(L_ERR, "AUTH_INDENTITY:app2dynstr: Not enough memory error\n");
			return -1;
		}
		sout->sd.s=stmp;
		sout->size=isize;
	}

	memcpy(&sout->sd.s[sout->sd.len],s2app->s,s2app->len);
	sout->sd.len = isize;

	return 0;
}
