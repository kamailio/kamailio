/*
 * $Id$
 *
 */


#include "str.h"
#include "dprint.h"



struct host_alias{
	str alias;
	struct host_alias* next;
};


extern struct host_alias* aliases;



/* returns 1 if  name is in the alias list*/
static inline int grep_aliases(char* name, int len)
{
	struct  host_alias* a;
	
	for(a=aliases;a;a=a->next)
		if ((a->alias.len==len) && (strncasecmp(a->alias.s, name, len)==0))
			return 1;
	return 0;
}



/* adds an alias to the list (only if it isn't already there)
 * returns 1 if a new alias was added, 0 if the alias was already on the list
 * and  -1 on error */
static inline int add_alias(char* name, int len)
{
	struct host_alias* a;
	
	if (grep_aliases(name,len)) return 0;
	a=0;
	a=(struct host_alias*)malloc(sizeof(struct host_alias));
	if(a==0) goto error;
	a->alias.s=(char*)malloc(len+1);
	if (a->alias.s==0) goto error;
	a->alias.len=len;
	memcpy(a->alias.s, name, len);
	a->alias.s[len]=0; /* null terminate for easier printing*/
	a->next=aliases;
	aliases=a;
	return 1;
error:
	LOG(L_ERR, "ERROR: add_alias: memory allocation error\n");
	if (a) free(a);
	return -1;
}



