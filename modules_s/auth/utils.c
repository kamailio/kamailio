/* 
 * $Id$ 
 */

#include "utils.h"
#include <string.h>
#include <ctype.h>


/*
 * Finds specified character, that is not quoted 
 * If there is no such character, returns NULL
 *
 * PARAMS : char* _b : input buffer
 *        : char _c  : character to find
 * RETURNS: char*    : points to character found, NULL if not found
 */
char* find_not_quoted(char* _b, char _c)
{
	int quoted = 0;

	while (*_b) {
		if (!quoted) {
			if (*_b == '\"') quoted = 1;
			else if (*_b == _c) return _b;
		} else {
			if ((*_b == '\"') && (*(_b-1) != '\\')) quoted = 0;
		}
		_b++;
	}
	return NULL;
}


/*
 * Remove any tabs and spaces from the begining and the end of
 * a string
 */
char* trim(char* _s)
{
	int len;
	char* end;

	     /* Null pointer, there is nothing to do */
	if (!_s) return _s;

	     /* Remove spaces and tabs from the begining of string */
	while ((*_s == ' ') || (*_s == '\t')) _s++;

	len = strlen(_s);

        end = _s + len - 1;

	     /* Remove trailing spaces and tabs */
	while ((*end == ' ') || (*end == '\t')) end--;
	if (end != (_s + len - 1)) {
		*(end+1) = '\0';
	}

	return _s;
}


/* 
 * Eat linear white space 
 *
 * PARAMS : char* _b : input buffer
 * RETURNS: char*    : points after skipped WS
 */
char* eat_lws(char* _b)
{
	while ((*_b == ' ') || (*_b == '\t')) _b++;
	while ((*_b == '\r') || (*_b == '\n')) _b++;
	while ((*_b == ' ') || (*_b == '\t')) _b++;
	return _b;
}


/* Substitute CR LF characters in field body with spaces */
struct hdr_field* remove_crlf(struct hdr_field* _hf)
{
	char* tmp;
	for (tmp = _hf->body.s; (*tmp); tmp++)
		if ((*tmp == '\r') || (*tmp == '\n'))
			*tmp = ' ';
	return _hf;
}



char* strlower(char* _s, int _len)
{
	int i;
#ifdef PARANOID
	if (!_s) return NULL;
#endif

	for(i = 0; i < _len; i++) {
		_s[i] = tolower(_s[i]);
	}
	return _s;
}


struct hdr_field* duplicate_hf(const struct hdr_field* _hf)
{
	struct hf* res;
#ifdef PARANOID
	if (!_hf) return NULL;
#endif
	res = pkg_malloc(sizeof(struct hf));
	if (!res) {
		return NULL;
	}


	res->s = pkg_malloc(_hf->len + 1);
	if (!res->s) {
		pkg_free(res);
		return NULL;
	}

	memcpy(res->s, _hf->s, _hf->len + 1);
	res->len = _hf->len;
	
	return res;
}



void free_hf(struct hf* _hf)
{
#ifdef PARANOID
	if (!_hf) return;
#endif
	pkg_free(_hf->s);
	pkg_free(_hf);
}

/*
 * Find the firs occurence of a space, tab, CR or LF
 */
char* find_lws(char* _s)
{
#ifdef PARANOID
	if (!_s) return NULL;
#endif
	while ((*_s != ' ') &&  (*_s != '\t') && 
	       (*_s != '\r') && (*_s != 'n') && (*_s)) _s++;
	return _s;
}


/*
 * Find the first occurence of not quoted LWS
 */
char* find_not_quoted_lws(char* _s)
{
	int quoted = 0;
#ifdef PARANOID
	if (!_s) return NULL;
#endif
	while (((*_s != ' ') &&  (*_s != '\t') && (*_s != '\r') && (*_s != 'n')) || (level)) {
		if (!quoted) {
			if (*_s == '\"') {
				quoted = 1;
			} 
		} else {
			if ((*_s == '\"') && (*(_s-1) != '\\')) quoted = 0;
		}
		_s++;
		if (*_s == '\0') break;
	}
	return _s;
}
