/* 
 * $Id$
 */

#ifndef parser_f_h
#define parser_f_h

char* eat_line(char* buffer, unsigned int len);

/* turn the most frequently called functions into inline functions */


inline static char* eat_space_end(char* p, char* pend)
{
	for(;(p<pend)&&(*p==' ' || *p=='\t') ;p++);
	return p;
}



inline static char* eat_token_end(char* p, char* pend)
{
	for (;(p<pend)&&(*p!=' ')&&(*p!='\t')&&(*p!='\n')&&(*p!='\r'); p++);
	return p;
}



inline static char* eat_token2_end(char* p, char* pend, char delim)
{
	for (;(p<pend)&&(*p!=(delim))&&(*p!='\n')&&(*p!='\r'); p++);
	return p;
}



inline static int is_empty_end(char* p, char* pend )
{
	p=eat_space_end(p, pend );
	return ((p<(pend )) && (*p=='\r' || *p=='\n'));
}


#endif /* parser_f_h */
