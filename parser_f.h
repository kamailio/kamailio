/* 
 * $Id$
 */

#ifndef parser_f_h
#define parser_f_h

char* eat_line(char* buffer, unsigned int len);

/* turn the most frequently called functions into macros */


#define eat_space_end(buffer,pend)                                       \
  ( {   char *_p;                                                 	\
	char *_pe=(pend);						\
        for(_p=(buffer);(_p<_pe)&& (*_p==' ' || *_p=='\t') ;_p++);		\
        _p;                                                              \
  } )

#define eat_token_end(buffer,pend)					\
  ( { char *_p       ;							\
      char *_pe=(pend);						\
      for (_p=(buffer);(_p<_pe)&&					\
                        (*_p!=' ')&&(*_p!='\t')&&(*_p!='\n')&&(*_p!='\r');	\
                _p++);							\
      _p;								\
  } )

#define eat_token2_end(buffer,pend,delim)					\
  ( { char *_p       ;							\
      char *_pe=(pend);						\
      for (_p=(buffer);(_p<_pe)&&					\
                        (*_p!=(delim))&&(*_p!='\n')&&(*_p!='\r');		\
                _p++);							\
      _p;								\
  } )

#define is_empty_end(buffer, pend )					\
  ( { char *_p;								\
      char *_pe=(pend);						\
      _p=eat_space_end( buffer, _pe );					\
      ((_p<(pend )) && (*_p=='\r' || *_p=='\n')) ? 1 : 0;			\
  } )


#endif
