/*
 * $Id$
 */

#include "utils.h"
#include "../../dprint.h"
#include <string.h>


/*
 * To field parser for usrloc module
 */

/*
 * Returns username of a To field, NULL is
 * returned in case of error
 */
char* get_to_username(char* _to, int _len)
{
	char* ptr, *at, *dcolon;
#ifdef PARANOID
	if (!_to) {
		LOG(L_ERR, "get_to_username(): Invalid _to parameter value\n");
		return FALSE;
	}
#endif
	dcolon = find_not_quoted(_to, ':');

	if (!dcolon) return NULL;
	at = strchr(_to, '@');
	if (!at) return NULL;
	else *at = '\0';

	if (dcolon < at) return dcolon + 1;
	else return NULL;
}



