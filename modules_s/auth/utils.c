/* 
 * $Id$ 
 */

#include <ctype.h>
#include "utils.h"
#include <string.h>
#include "../../dprint.h"


/*
 * Miscelaneous utilities
 */


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
	
	if (!_b) return NULL;

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
 * Remove any leading spaces and tabs
 */
inline char* trim_leading(char* _s)
{
#ifdef PARANOID
	if (!_s) return NULL;
#endif
	     /* Remove spaces and tabs from the begining of string */
	while ((*_s == ' ') || (*_s == '\t')) _s++;
	return _s;
}


/*
 * Remove any trailing spaces and tabs
 */
inline char* trim_trailing(char* _s)
{
	int len;
	char* end;

#ifdef PARANOID
	if (!_s) return NULL;
#endif
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
 * Remove any tabs and spaces from the begining and the end of
 * a string
 */
char* trim(char* _s)
{
	_s = trim_leading(_s);
	return trim_trailing(_s);
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

	for(i = 0; i < _len; i++) {
		_s[i] = tolower(_s[i]);
	}
	return _s;
}


char* strupper(char* _s, int _len)
{
	int i;

	for(i = 0; i < _len; i++) {
		_s[i] = toupper(_s[i]);
	}

	return _s;
}




/*
 * This function returns pointer to the beginning of URI
 *
 * PARAMS : char* _b : input buffer
 * RETURNS: char*    : points immediately after name part
 *
 */
char* eat_name(char* _b)
{
	int quoted = 0;
	char* b = _b;
	char* last_ws = _b;

	if (!_b) return NULL;

	     /* < means start of URI, : is URI scheme
	      * separator, these two characters cannot
	      * occur in non-quoted name string
	      */
	while(*b) {
		if (!quoted) {
			if ((*b == ' ') || (*b == '\t')) {
				last_ws = b;
			} else {
				if (*b == '<') return b;  /* We will end here if there is a name */
				if (*b == ':') return last_ws; /* There is no name in this case */
				if (*b == '\"') quoted = 1;
			}
		} else {
			if ((*b == '\"') && (*(b-1) != '\\')) quoted = 0;
		}
		b++;
	}

	return _b;  /* Some error */
}
		
