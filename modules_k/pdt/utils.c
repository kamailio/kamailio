#include <stdio.h>
#include <string.h>

#include "utils.h"
#include "../../dprint.h"

int scmp(str* s1, str* s2)
{
	int r; 

	if(s1==NULL || s2==NULL || s1->s ==NULL || s2->s==NULL || s1->len<0 || s2->len<0)
	{
		LOG(L_ERR, "scmp: ERROR: bad parameters\n");
		return -2;
	}

	r = strncmp(s1->s, s2->s, s1->len<s2->len?s1->len:s2->len);
	if(r==0)
	{
		if(s1->len<s2->len)
			return 1;
		if(s1->len>s2->len)
			return -1;
	}
	return r;
}

