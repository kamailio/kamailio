/*
 *$Id$
 *
 */


#ifndef _modfix_h
#define _modfix_h

#include "mem/mem.h"
#include "str.h"
#include "error.h"


/*  
 * Convert char* parameter to str* parameter   
 */
static int str_fixup(void** param, int param_no)
{
	str* s;
	
	if (param_no == 1 || param_no == 2 ) {
		s = (str*)pkg_malloc(sizeof(str));
		if (!s) {
			LOG(L_ERR, "str_fixup(): No memory left\n");
			return E_UNSPEC;
		}
		
		s->s = (char*)*param;
		s->len = strlen(s->s);
		*param = (void*)s;
	}
	
	return 0;
}

#endif
