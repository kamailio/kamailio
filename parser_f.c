/*
 * $Id$
 *
 * parser helper  functions
 *
 */

#include  "parser_f.h"

/* returns pointer to next line or after the end of buffer */
char* eat_line(char* buffer, unsigned int len)
{
	char* nl;
	char c;

	/* jku .. replace for search with a library function; not conformant
 		  as I do not care about CR
	*/
#ifdef NOCR
	nl=(char *)memchr( buffer, '\n', len );
	if ( nl ) { 
		c=* nl;
		if ( nl + 1 < buffer+len)  nl++;
		if (( nl+1<buffer+len) && * nl=='\r')  nl++;
	} else  nl=buffer+len;
#else
	for(nl=buffer;(nl<buffer+len)&& (*nl!='\r')&&(*nl!='\n') ;nl++);
	c=*nl;
	if (nl+1<buffer+len)  nl++;
	if ((nl+1<buffer+len) &&
			((c=='\r' && *nl=='\n')|| (c=='\n' && *nl=='\r'))) 
		nl++;
#endif
	
	/* end of jku */
	return nl;
}



/* returns pointer to first non  white char or after the end  of the buffer */

#ifndef MACROEATER

char* eat_space(char* buffer, unsigned int len)
{
	char* p;

	for(p=buffer;(p<buffer+len)&& (*p==' ' || *p=='\t') ;p++);
	return p;
}


/* returns pointer after the token (first whitespace char or CR/LF) */
char* eat_token(char* buffer, unsigned int len)
{
	char *p;

	for (p=buffer;(p<buffer+len)&&
			(*p!=' ')&&(*p!='\t')&&(*p!='\n')&&(*p!='\r');
		p++);
	return p;
}



/* returns pointer after the token (first delim char or CR/LF) */
char* eat_token2(char* buffer, unsigned int len, char delim)
{
	char *p;

	for (p=buffer;(p<buffer+len)&&
			(*p!=delim)&&(*p!='\n')&&(*p!='\r');
		p++);
	return p;
}

/* EoMACROEATER */
#endif



/* returns true if line started  at buffer contains only white space */
int is_empty(char* buffer, unsigned int len)
{
	char *p;
	
	p=eat_space(buffer, len);
	if ((p < buffer+len ) && (*p=='\r' || *p=='\n')) return 1;
	return 0;
}
