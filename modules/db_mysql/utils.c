#include "utils.h"
#include <string.h>
#include "defs.h"

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
