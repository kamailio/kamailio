/* 
 * $Id$
 */

#ifndef parser_f_h
#define parser_f_h

char* eat_line(char* buffer, unsigned int len);

/* macro now
int is_empty(char* buffer, unsigned int len);
*/

/* MACROEATER no more optional */
/* #ifdef MACROEATER */

/* turn the most frequently called functions into macros */


#define eat_space_end(buffer,pend)                                       \
  ( {   char *p;                                                 	\
        for(p=(buffer);(p<pend)&& (*p==' ' || *p=='\t') ;p++);		\
        p;                                                              \
  } )

#define eat_token_end(buffer,pend)					\
  ( { char *p       ;							\
      for (p=(buffer);(p<pend)&&					\
                        (*p!=' ')&&(*p!='\t')&&(*p!='\n')&&(*p!='\r');	\
                p++);							\
      p;								\
  } )

#define eat_token2_end(buffer,pend,delim)					\
  ( { char *p       ;							\
      for (p=(buffer);(p<pend)&&					\
                        (*p!=(delim))&&(*p!='\n')&&(*p!='\r');		\
                p++);							\
      p;								\
  } )

#define is_empty_end(buffer, pend )					\
  ( { char *p;								\
      p=eat_space_end( buffer, pend );					\
      ((p<pend ) && (*p=='\r' || *p=='\n')) ? 1 : 0;			\
  } )


/*
#else


char* eat_space(char* buffer, unsigned int len);
char* eat_token(char* buffer, unsigned int len);
char* eat_token2(char* buffer, unsigned int len, char delim);

#endif
*/
/* EoMACROEATER */

#endif
