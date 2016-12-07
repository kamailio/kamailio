/*
* Kamailio LDAP Module
*
* Copyright (C) 2007 University of North Carolina
*
* Original author: Christian Schlatter, cs@unc.edu
*
*
* This file is part of Kamailio, a free SIP server.
*
* Kamailio is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version
*
* Kamailio is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*
*/



/*
Based upon libiniparser, by Nicolas Devillard
Hacked into 1 file (m-iniparser) by Freek/2005
Original terms following:

-- -

Copyright (c) 2000 by Nicolas Devillard (ndevilla AT free DOT fr).

Written by Nicolas Devillard. Not derived from licensed software.

Permission is granted to anyone to use this software for any
purpose on any computer system, and to redistribute it freely,
subject to the following restrictions:

1. The author is not responsible for the consequences of use of
this software, no matter how awful, even if they arise
from defects in it.

2. The origin of this software must not be misrepresented, either
by explicit claim or by omission.

3. Altered versions must be plainly marked as such, and must not
be misrepresented as being the original software.

4. This notice may not be removed or altered.

*/


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "iniparser.h"

#ifdef __cplusplus
extern "C" {
#endif

	/* strlib.c following */

#define ASCIILINESZ 1024
	/*-------------------------------------------------------------------------*/
	/**
@brief    Convert a string to lowercase.
@param    s   String to convert.
@return   ptr to statically allocated string.

This function returns a pointer to a statically allocated string
containing a lowercased version of the input string. Do not free
or modify the returned string! Since the returned string is statically
allocated, it will be modified at each function call (not re-entrant).
*/
	/*--------------------------------------------------------------------------*/

	static char * strlwc(char * s)
	{
		static char l[ASCIILINESZ+1];
		int i ;

		if (s==NULL) return NULL ;
		memset(l, 0, ASCIILINESZ+1);
		i=0 ;
		while (s[i] && i<ASCIILINESZ) {
			l[i] = (char)tolower((int)s[i]);
			i++ ;
		}
		l[ASCIILINESZ]=(char)0;
		return l ;
	}



	/*-------------------------------------------------------------------------*/
	/**
@brief    Convert a string to uppercase.
@param    s   String to convert.
@return   ptr to statically allocated string.

This function returns a pointer to a statically allocated string
containing an uppercased version of the input string. Do not free
or modify the returned string! Since the returned string is statically
allocated, it will be modified at each function call (not re-entrant).
*/
	/*--------------------------------------------------------------------------*/

	/*
static char * strupc(char * s)
{
	static char l[ASCIILINESZ+1];
	int i ;

	if (s==NULL) return NULL ;
	memset(l, 0, ASCIILINESZ+1);
	i=0 ;
	while (s[i] && i<ASCIILINESZ) {
		l[i] = (char)toupper((int)s[i]);
		i++ ;
	}
	l[ASCIILINESZ]=(char)0;
	return l ;
}
*/



	/*-------------------------------------------------------------------------*/
	/**
@brief    Skip blanks until the first non-blank character.
@param    s   String to parse.
@return   Pointer to char inside given string.

This function returns a pointer to the first non-blank character in the
given string.
*/
	/*--------------------------------------------------------------------------*/

	static char * strskp(char * s)
	{
		char * skip = s;
		if (s==NULL) return NULL ;
		while (isspace((int)*skip) && *skip) skip++;
		return skip ;
	}



	/*-------------------------------------------------------------------------*/
	/**
@brief    Remove blanks at the end of a string.
@param    s   String to parse.
@return   ptr to statically allocated string.

This function returns a pointer to a statically allocated string,
which is identical to the input string, except that all blank
characters at the end of the string have been removed.
Do not free or modify the returned string! Since the returned string
is statically allocated, it will be modified at each function call
(not re-entrant).
*/
	/*--------------------------------------------------------------------------*/

	static char * strcrop(char * s)
	{
		static char l[ASCIILINESZ+1];
		char * last ;

		if (s==NULL) return NULL ;
		memset(l, 0, ASCIILINESZ+1);
		strcpy(l, s);
		last = l + strlen(l);
		while (last > l) {
			if (!isspace((int)*(last-1)))
			break ;
			last -- ;
		}
		*last = (char)0;
		return l ;
	}



	/*-------------------------------------------------------------------------*/
	/**
@brief    Remove blanks at the beginning and the end of a string.
@param    s   String to parse.
@return   ptr to statically allocated string.

This function returns a pointer to a statically allocated string,
which is identical to the input string, except that all blank
characters at the end and the beg. of the string have been removed.
Do not free or modify the returned string! Since the returned string
is statically allocated, it will be modified at each function call
(not re-entrant).
*/
	/*--------------------------------------------------------------------------*/
	/*
static char * strstrip(char * s)
{
	static char l[ASCIILINESZ+1];
	char * last ;

	if (s==NULL) return NULL ;

	while (isspace((int)*s) && *s) s++;

	memset(l, 0, ASCIILINESZ+1);
	strcpy(l, s);
	last = l + strlen(l);
	while (last > l) {
		if (!isspace((int)*(last-1)))
			break ;
		last -- ;
	}
	*last = (char)0;

	return (char*)l ;
}
*/

	/* dictionary.c.c following */
	/** Maximum value size for integers and doubles. */
#define MAXVALSZ    1024

	/** Minimal allocated number of entries in a dictionary */
#define DICTMINSZ   128

	/** Invalid key token */
#define DICT_INVALID_KEY    ((char*)-1)

	/*
Doubles the allocated size associated to a pointer
'size' is the current allocated size.
*/
	static void * mem_double(void * ptr, int size)
	{
		void *newptr;

		newptr = calloc(2*size, 1);
		memcpy(newptr, ptr, size);
		free(ptr);
		return newptr ;
	}


	/*---------------------------------------------------------------------------
							Function codes
---------------------------------------------------------------------------*/

	/*-------------------------------------------------------------------------*/
	/**
@brief    Compute the hash key for a string.
@param    key     Character string to use for key.
@return   1 unsigned int on at least 32 bits.

This hash function has been taken from an Article in Dr Dobbs Journal.
This is normally a collision-free function, distributing keys evenly.
The key is stored anyway in the struct so that collision can be avoided
by comparing the key itself in last resort.
*/
	/*--------------------------------------------------------------------------*/

	static unsigned dictionary_hash(char * key)
	{
		int         len ;
		unsigned    hash ;
		int         i ;

		len = strlen(key);
		for (hash=0, i=0 ; i<len ; i++) {
			hash += (unsigned)key[i] ;
			hash += (hash<<10);
			hash ^= (hash>>6) ;
		}
		hash += (hash <<3);
		hash ^= (hash >>11);
		hash += (hash <<15);
		return hash ;
	}


	/*-------------------------------------------------------------------------*/
	/**
@brief    Create a new dictionary object.
@param    size    Optional initial size of the dictionary.
@return   1 newly allocated dictionary objet.

This function allocates a new dictionary object of given size and returns
it. If you do not know in advance (roughly) the number of entries in the
dictionary, give size=0.
*/
	/*--------------------------------------------------------------------------*/

	static dictionary * dictionary_new(int size)
	{
		dictionary *d ;

		/* If no size was specified, allocate space for DICTMINSZ */
		if (size<DICTMINSZ) size=DICTMINSZ ;

		d = (dictionary *)calloc(1, sizeof(dictionary));
		d->size = size ;
		d->val  = (char **)calloc(size, sizeof(char*));
		d->key  = (char **)calloc(size, sizeof(char*));
		d->hash = (unsigned int *)calloc(size, sizeof(unsigned));

		return d;
	}


	/*-------------------------------------------------------------------------*/
	/**
@brief    Delete a dictionary object
@param    d   dictionary object to deallocate.
@return   void

Deallocate a dictionary object and all memory associated to it.
*/
	/*--------------------------------------------------------------------------*/

	static void dictionary_del(dictionary * d)
	{
		int     i ;

		if (d==NULL) return ;
		for (i=0 ; i<d->size ; i++) {
			if (d->key[i]!=NULL)
			free(d->key[i]);
			if (d->val[i]!=NULL)
			free(d->val[i]);
		}
		free(d->val);
		free(d->key);
		free(d->hash);
		free(d);

		return;
	}



	/*-------------------------------------------------------------------------*/
	/**
@brief    Get a value from a dictionary.
@param    d       dictionary object to search.
@param    key     Key to look for in the dictionary.
@param    def     Default value to return if key not found.
@return   1 pointer to internally allocated character string.

This function locates a key in a dictionary and returns a pointer to its
value, or the passed 'def' pointer if no such key can be found in
dictionary. The returned character pointer points to data internal to the
dictionary object, you should not try to free it or modify it.
*/
	/*--------------------------------------------------------------------------*/
	static char * dictionary_get(dictionary * d, char * key, char * def)
	{
		unsigned    hash ;
		int         i ;

		hash = dictionary_hash(key);
		for (i=0 ; i<d->size ; i++) {
			if (d->key==NULL)
			continue ;
			/* Compare hash */
			if (hash==d->hash[i]) {
				/* Compare string, to avoid hash collisions */
				if (!strcmp(key, d->key[i])) {
					return d->val[i] ;
				}
			}
		}
		return def ;
	}


	/*-------------------------------------------------------------------------*/
	/**
@brief    Set a value in a dictionary.
@param    d       dictionary object to modify.
@param    key     Key to modify or add.
@param    val     Value to add.
@return   void

If the given key is found in the dictionary, the associated value is
replaced by the provided one. If the key cannot be found in the
dictionary, it is added to it.

It is Ok to provide a NULL value for val, but NULL values for the dictionary
or the key are considered as errors: the function will return immediately
in such a case.

Notice that if you dictionary_set a variable to NULL, a call to
dictionary_get will return a NULL value: the variable will be found, and
its value (NULL) is returned. In other words, setting the variable
content to NULL is equivalent to deleting the variable from the
dictionary. It is not possible (in this implementation) to have a key in
the dictionary without value.
*/
	/*--------------------------------------------------------------------------*/

	static void dictionary_set(dictionary * d, char * key, char * val)
	{
		int         i ;
		unsigned    hash ;

		if (d==NULL || key==NULL) return ;

		/* Compute hash for this key */
		hash = dictionary_hash(key) ;
		/* Find if value is already in blackboard */
		if (d->n>0) {
			for (i=0 ; i<d->size ; i++) {
				if (d->key[i]==NULL)
				continue ;
				if (hash==d->hash[i]) { /* Same hash value */
					if (!strcmp(key, d->key[i])) {   /* Same key */
						/* Found a value: modify and return */
						if (d->val[i]!=NULL)
						free(d->val[i]);
						d->val[i] = val ? strdup(val) : NULL ;
						/* Value has been modified: return */
						return ;
					}
				}
			}
		}
		/* Add a new value */
		/* See if dictionary needs to grow */
		if (d->n==d->size) {

			/* Reached maximum size: reallocate blackboard */
			d->val  = (char **)mem_double(d->val,  d->size * sizeof(char*)) ;
			d->key  = (char **)mem_double(d->key,  d->size * sizeof(char*)) ;
			d->hash = (unsigned int *)mem_double(d->hash, d->size * sizeof(unsigned)) ;

			/* Double size */
			d->size *= 2 ;
		}

		/* Insert key in the first empty slot */
		for (i=0 ; i<d->size ; i++) {
			if (d->key[i]==NULL) {
				/* Add key here */
				break ;
			}
		}
		/* Copy key */
		d->key[i]  = strdup(key);
		d->val[i]  = val ? strdup(val) : NULL ;
		d->hash[i] = hash;
		d->n ++ ;
		return ;
	}

	/*-------------------------------------------------------------------------*/
	/**
@brief    Delete a key in a dictionary
@param    d       dictionary object to modify.
@param    key     Key to remove.
@return   void

This function deletes a key in a dictionary. Nothing is done if the
key cannot be found.
*/
	/*--------------------------------------------------------------------------*/
	static void dictionary_unset(dictionary * d, char * key)
	{
		unsigned    hash ;
		int         i ;

		hash = dictionary_hash(key);
		for (i=0 ; i<d->size ; i++) {
			if (d->key[i]==NULL)
			continue ;
			/* Compare hash */
			if (hash==d->hash[i]) {
				/* Compare string, to avoid hash collisions */
				if (!strcmp(key, d->key[i])) {
					/* Found key */
					break ;
				}
			}
		}
		if (i>=d->size)
		/* Key not found */
		return ;

		free(d->key[i]);
		d->key[i] = NULL ;
		if (d->val[i]!=NULL) {
			free(d->val[i]);
			d->val[i] = NULL ;
		}
		d->hash[i] = 0 ;
		d->n -- ;
		return ;
	}


	/*-------------------------------------------------------------------------*/
	/**
@brief    Dump a dictionary to an opened file pointer.
@param    d   Dictionary to dump
@param    f   Opened file pointer.
@return   void

Dumps a dictionary onto an opened file pointer. Key pairs are printed out
as @c [Key]=[Value], one per line. It is Ok to provide stdout or stderr as
output file pointers.
*/
	/*--------------------------------------------------------------------------*/

	static void dictionary_dump(dictionary *d, FILE *f)
	{
		int i;

		if (d==NULL || f==NULL) return;

		for (i=0; i<d->size; i++) {
			if (d->key[i] == NULL)
			continue ;
			if (d->val[i] != NULL) {
				fprintf(f, "[%s]=[%s]\n", d->key[i], d->val[i]);
			} else {
				fprintf(f, "[%s]=UNDEF\n", d->key[i]);
			}
		}

		return;
	}


	/* iniparser.c.c following */
#define ASCIILINESZ         1024
#define INI_INVALID_KEY     ((char*)-1)

	/* Private: add an entry to the dictionary */
	static void iniparser_add_entry(
	dictionary * d,
	char * sec,
	char * key,
	char * val)
	{
		char longkey[2*ASCIILINESZ+1];

		/* Make a key as section:keyword */
		if (key!=NULL) {
			sprintf(longkey, "%s:%s", sec, key);
		} else {
			strcpy(longkey, sec);
		}

		/* Add (key,val) to dictionary */
		dictionary_set(d, longkey, val);
		return ;
	}


	/*-------------------------------------------------------------------------*/
	/**
@brief    Get number of sections in a dictionary
@param    d   Dictionary to examine
@return   int Number of sections found in dictionary

This function returns the number of sections found in a dictionary.
The test to recognize sections is done on the string stored in the
dictionary: a section name is given as "section" whereas a key is
stored as "section:key", thus the test looks for entries that do not
contain a colon.

This clearly fails in the case a section name contains a colon, but
this should simply be avoided.

This function returns -1 in case of error.
*/
	/*--------------------------------------------------------------------------*/

	int iniparser_getnsec(dictionary * d)
	{
		int i ;
		int nsec ;

		if (d==NULL) return -1 ;
		nsec=0 ;
		for (i=0 ; i<d->size ; i++) {
			if (d->key[i]==NULL)
			continue ;
			if (strchr(d->key[i], ':')==NULL) {
				nsec ++ ;
			}
		}
		return nsec ;
	}


	/*-------------------------------------------------------------------------*/
	/**
@brief    Get name for section n in a dictionary.
@param    d   Dictionary to examine
@param    n   Section number (from 0 to nsec-1).
@return   Pointer to char string

This function locates the n-th section in a dictionary and returns
its name as a pointer to a string statically allocated inside the
dictionary. Do not free or modify the returned string!

This function returns NULL in case of error.
*/
	/*--------------------------------------------------------------------------*/

	char * iniparser_getsecname(dictionary * d, int n)
	{
		int i ;
		int foundsec ;

		if (d==NULL || n<0) return NULL ;
		foundsec=0 ;
		for (i=0 ; i<d->size ; i++) {
			if (d->key[i]==NULL)
			continue ;
			if (strchr(d->key[i], ':')==NULL) {
				foundsec++ ;
				if (foundsec>n)
				break ;
			}
		}
		if (foundsec<=n) {
			return NULL ;
		}
		return d->key[i] ;
	}


	/*-------------------------------------------------------------------------*/
	/**
@brief    Dump a dictionary to an opened file pointer.
@param    d   Dictionary to dump.
@param    f   Opened file pointer to dump to.
@return   void

This function prints out the contents of a dictionary, one element by
line, onto the provided file pointer. It is OK to specify @c stderr
or @c stdout as output files. This function is meant for debugging
purposes mostly.
*/
	/*--------------------------------------------------------------------------*/
	void iniparser_dump(dictionary * d, FILE * f)
	{
		dictionary_dump(d,f);
	}


	/*-------------------------------------------------------------------------*/
	/**
@brief    Save a dictionary to a loadable ini file
@param    d   Dictionary to dump
@param    f   Opened file pointer to dump to
@return   void

This function dumps a given dictionary into a loadable ini file.
It is Ok to specify @c stderr or @c stdout as output files.
*/
	/*--------------------------------------------------------------------------*/

	void iniparser_dump_ini(dictionary * d, FILE * f)
	{
		int     i, j ;
		char    keym[ASCIILINESZ+1];
		int     nsec ;
		char *  secname ;
		int     seclen ;

		if (d==NULL || f==NULL) return ;

		nsec = iniparser_getnsec(d);
		if (nsec<1) {
			/* No section in file: dump all keys as they are */
			for (i=0 ; i<d->size ; i++) {
				if (d->key[i]==NULL)
				continue ;
				fprintf(f, "%s = %s\n", d->key[i], d->val[i]);
			}
			return ;
		}
		for (i=0 ; i<nsec ; i++) {
			secname = iniparser_getsecname(d, i) ;
			seclen  = (int)strlen(secname);
			fprintf(f, "\n[%s]\n", secname);
			sprintf(keym, "%s:", secname);
			for (j=0 ; j<d->size ; j++) {
				if (d->key[j]==NULL)
				continue ;
				if (!strncmp(d->key[j], keym, seclen+1)) {
					fprintf(f,
					"%-30s = %s\n",
					d->key[j]+seclen+1,
					d->val[j] ? d->val[j] : "");
				}
			}
		}
		fprintf(f, "\n");
		return ;
	}

	/*-------------------------------------------------------------------------*/
	/**
@brief    Get the string associated to a key, return NULL if not found
@param    d   Dictionary to search
@param    key Key string to look for
@return   pointer to statically allocated character string, or NULL.

This function queries a dictionary for a key. A key as read from an
ini file is given as "section:key". If the key cannot be found,
NULL is returned.
The returned char pointer is pointing to a string allocated in
the dictionary, do not free or modify it.

This function is only provided for backwards compatibility with
previous versions of iniparser. It is recommended to use
iniparser_getstring() instead.
*/
	/*--------------------------------------------------------------------------*/
	char * iniparser_getstr(dictionary * d, char * key)
	{
		return iniparser_getstring(d, key, NULL);
	}


	/*-------------------------------------------------------------------------*/
	/**
@brief    Get the string associated to a key
@param    d       Dictionary to search
@param    key     Key string to look for
@param    def     Default value to return if key not found.
@return   pointer to statically allocated character string

This function queries a dictionary for a key. A key as read from an
ini file is given as "section:key". If the key cannot be found,
the pointer passed as 'def' is returned.
The returned char pointer is pointing to a string allocated in
the dictionary, do not free or modify it.
*/
	/*--------------------------------------------------------------------------*/
	char * iniparser_getstring(dictionary * d, char * key, char * def)
	{
		char * lc_key ;
		char * sval ;

		if (d==NULL || key==NULL)
		return def ;

		lc_key = strdup(strlwc(key));
		sval = dictionary_get(d, lc_key, def);
		free(lc_key);
		return sval ;
	}



	/*-------------------------------------------------------------------------*/
	/**
@brief    Get the string associated to a key, convert to an int
@param    d Dictionary to search
@param    key Key string to look for
@param    notfound Value to return in case of error
@return   integer

This function queries a dictionary for a key. A key as read from an
ini file is given as "section:key". If the key cannot be found,
the notfound value is returned.
*/
	/*--------------------------------------------------------------------------*/
	int iniparser_getint(dictionary * d, char * key, int notfound)
	{
		char    *   str ;

		str = iniparser_getstring(d, key, INI_INVALID_KEY);
		if (str==INI_INVALID_KEY) return notfound ;
		return atoi(str);
	}


	/*-------------------------------------------------------------------------*/
	/**
@brief    Get the string associated to a key, convert to a double
@param    d Dictionary to search
@param    key Key string to look for
@param    notfound Value to return in case of error
@return   double

This function queries a dictionary for a key. A key as read from an
ini file is given as "section:key". If the key cannot be found,
the notfound value is returned.
*/
	/*--------------------------------------------------------------------------*/
	double iniparser_getdouble(dictionary * d, char * key, double notfound)
	{
		char    *   str ;

		str = iniparser_getstring(d, key, INI_INVALID_KEY);
		if (str==INI_INVALID_KEY) return notfound ;
		return atof(str);
	}



	/*-------------------------------------------------------------------------*/
	/**
@brief    Get the string associated to a key, convert to a boolean
@param    d Dictionary to search
@param    key Key string to look for
@param    notfound Value to return in case of error
@return   integer

This function queries a dictionary for a key. A key as read from an
ini file is given as "section:key". If the key cannot be found,
the notfound value is returned.

A true boolean is found if one of the following is matched:

- A string starting with 'y'
- A string starting with 'Y'
- A string starting with 't'
- A string starting with 'T'
- A string starting with '1'

A false boolean is found if one of the following is matched:

- A string starting with 'n'
- A string starting with 'N'
- A string starting with 'f'
- A string starting with 'F'
- A string starting with '0'

The notfound value returned if no boolean is identified, does not
necessarily have to be 0 or 1.
*/
	/*--------------------------------------------------------------------------*/
	int iniparser_getboolean(dictionary * d, char * key, int notfound)
	{
		char    *   c ;
		int         ret ;

		c = iniparser_getstring(d, key, INI_INVALID_KEY);
		if (c==INI_INVALID_KEY) return notfound ;
		if (c[0]=='y' || c[0]=='Y' || c[0]=='1' || c[0]=='t' || c[0]=='T') {
			ret = 1 ;
		} else if (c[0]=='n' || c[0]=='N' || c[0]=='0' || c[0]=='f' || c[0]=='F') {
			ret = 0 ;
		} else {
			ret = notfound ;
		}
		return ret;
	}


	/*-------------------------------------------------------------------------*/
	/**
@brief    Finds out if a given entry exists in a dictionary
@param    ini     Dictionary to search
@param    entry   Name of the entry to look for
@return   integer 1 if entry exists, 0 otherwise

Finds out if a given entry exists in the dictionary. Since sections
are stored as keys with NULL associated values, this is the only way
of querying for the presence of sections in a dictionary.
*/
	/*--------------------------------------------------------------------------*/

	int iniparser_find_entry(
	dictionary  *   ini,
	char        *   entry
	)
	{
		int found=0 ;
		if (iniparser_getstring(ini, entry, INI_INVALID_KEY)!=INI_INVALID_KEY) {
			found = 1 ;
		}
		return found ;
	}



	/*-------------------------------------------------------------------------*/
	/**
@brief    Set an entry in a dictionary.
@param    ini     Dictionary to modify.
@param    entry   Entry to modify (entry name)
@param    val     New value to associate to the entry.
@return   int 0 if Ok, -1 otherwise.

If the given entry can be found in the dictionary, it is modified to
contain the provided value. If it cannot be found, -1 is returned.
It is Ok to set val to NULL.
*/
	/*--------------------------------------------------------------------------*/

	int iniparser_setstr(dictionary * ini, char * entry, char * val)
	{
		dictionary_set(ini, strlwc(entry), val);
		return 0 ;
	}

	/*-------------------------------------------------------------------------*/
	/**
@brief    Delete an entry in a dictionary
@param    ini     Dictionary to modify
@param    entry   Entry to delete (entry name)
@return   void

If the given entry can be found, it is deleted from the dictionary.
*/
	/*--------------------------------------------------------------------------*/
	void iniparser_unset(dictionary * ini, char * entry)
	{
		dictionary_unset(ini, strlwc(entry));
	}


	/*-------------------------------------------------------------------------*/
	/**
@brief    Parse an ini file and return an allocated dictionary object
@param    ininame Name of the ini file to read.
@return   Pointer to newly allocated dictionary

This is the parser for ini files. This function is called, providing
the name of the file to be read. It returns a dictionary object that
should not be accessed directly, but through accessor functions
instead.

The returned dictionary must be freed using iniparser_free().
*/
	/*--------------------------------------------------------------------------*/

	dictionary * iniparser_new(char *ininame)
	{
		dictionary  *   d ;
		char        lin[ASCIILINESZ+1];
		char        sec[ASCIILINESZ+1];
		char        key[ASCIILINESZ+1];
		char        val[ASCIILINESZ+1];
		char    *   where ;
		FILE    *   ini ;
		int         lineno ;

		if ((ini=fopen(ininame, "r"))==NULL) {
			return NULL ;
		}

		sec[0]=0;

		/*
	* Initialize a new dictionary entry
	*/
		d = dictionary_new(0);
		lineno = 0 ;
		while (fgets(lin, ASCIILINESZ, ini)!=NULL) {
			lineno++ ;
			where = strskp(lin); /* Skip leading spaces */
			if (*where==';' || *where=='#' || *where==0)
			continue ; /* Comment lines */
			else {
				if (sscanf(where, "[%[^]]", sec)==1) {
					/* Valid section name */
					strcpy(sec, strlwc(sec));
					iniparser_add_entry(d, sec, NULL, NULL);
				} else if (sscanf (where, "%[^=] = \"%[^\"]\"", key, val) == 2
						||  sscanf (where, "%[^=] = '%[^\']'",   key, val) == 2
						||  sscanf (where, "%[^=] = %[^;#]",     key, val) == 2) {
					strcpy(key, strlwc(strcrop(key)));
					/*
				* sscanf cannot handle "" or '' as empty value,
				* this is done here
				*/
					if (!strcmp(val, "\"\"") || !strcmp(val, "''")) {
						val[0] = (char)0;
					} else {
						strcpy(val, strcrop(val));
					}
					iniparser_add_entry(d, sec, key, val);
				}
			}
		}
		fclose(ini);
		return d ;
	}



	/*-------------------------------------------------------------------------*/
	/**
@brief    Free all memory associated to an ini dictionary
@param    d Dictionary to free
@return   void

Free all memory associated to an ini dictionary.
It is mandatory to call this function before the dictionary object
gets out of the current context.
*/
	/*--------------------------------------------------------------------------*/

	void iniparser_free(dictionary * d)
	{
		dictionary_del(d);
	}

#ifdef __cplusplus
}
#endif
