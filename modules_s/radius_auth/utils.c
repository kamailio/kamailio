/* $Id: utils.c
 *
 * Set of utils to extract the user-name from the FROM field
 * borrowed from the auth module.
 * @author Stelios Sidiroglou-Douskos <ssi@fokus.gmd.de>
 * $Id$
 */
#include "utils.h"
#include "../../ut.h"

/*
 * This method simply cleans off the trailing character of the string body.
 * params: str body
 * returns: the new char* or NULL on failure
 */
char * cleanbody(str body) 
{	
	char* tmp;
	/*
	 * This works because when the structure is created it is memset to 0
	 */
	if (body.s == NULL)
		return NULL;
		
	tmp = &body.s[0];
	tmp[body.len] = '\0';

	return tmp;
}

