/* 
 * $Id$
 */

#ifndef parser_f_h
#define parser_f_h

char* eat_line(char* buffer, unsigned int len);
int is_empty(char* buffer, unsigned int len);

#ifdef MACROEATER

/* turn the most frequently called functions into macros */


#define eat_space(buffer,len)                                          \
  ( {   char *p;                                                     	\
        for(p=(buffer);(p<(buffer)+(len))&& (*p==' ' || *p=='\t') ;p++);\
        p;                                                              \
  } )

#define eat_token(buffer,len)						\
  ( { char *p;								\
      for (p=(buffer);(p<(buffer)+(len))&&				\
                        (*p!=' ')&&(*p!='\t')&&(*p!='\n')&&(*p!='\r');	\
                p++);							\
      p;								\
  } )

#define eat_token2(buffer,len,delim)					\
  ( { char *p;								\
      for (p=(buffer);(p<(buffer)+(len))&&				\
                        (*p!=(delim))&&(*p!='\n')&&(*p!='\r');		\
                p++);							\
      p;								\
  } )


#else


char* eat_space(char* buffer, unsigned int len);
char* eat_token(char* buffer, unsigned int len);
char* eat_token2(char* buffer, unsigned int len, char delim);

/* EoMACROEATER */
#endif

#endif
