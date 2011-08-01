
#line 1 "rfc1918_parser.rl"
#include "rfc1918_parser.h"


/** Ragel machine **/

#line 21 "rfc1918_parser.rl"



/** Data **/

#line 15 "rfc1918_parser.c"
static const int rfc1918_parser_start = 1;
static const int rfc1918_parser_first_final = 28;
static const int rfc1918_parser_error = 0;

static const int rfc1918_parser_en_main = 1;


#line 26 "rfc1918_parser.rl"


/** exec **/
unsigned int rfc1918_parser_execute(const char *str, size_t len)
{
  int cs = 0;
  const char *p, *pe;
  unsigned int is_ip_rfc1918 = 0;

  p = str;
  pe = str+len;

  
#line 37 "rfc1918_parser.c"
	{
	cs = rfc1918_parser_start;
	}

#line 39 "rfc1918_parser.rl"
  
#line 44 "rfc1918_parser.c"
	{
	if ( p == pe )
		goto _test_eof;
	switch ( cs )
	{
case 1:
	if ( (*p) == 49 )
		goto st2;
	goto st0;
st0:
cs = 0;
	goto _out;
st2:
	if ( ++p == pe )
		goto _test_eof2;
case 2:
	switch( (*p) ) {
		case 48: goto st3;
		case 55: goto st17;
		case 57: goto st23;
	}
	goto st0;
st3:
	if ( ++p == pe )
		goto _test_eof3;
case 3:
	if ( (*p) == 46 )
		goto st4;
	goto st0;
st4:
	if ( ++p == pe )
		goto _test_eof4;
case 4:
	switch( (*p) ) {
		case 48: goto st5;
		case 49: goto st13;
		case 50: goto st15;
	}
	if ( 51 <= (*p) && (*p) <= 57 )
		goto st14;
	goto st0;
st5:
	if ( ++p == pe )
		goto _test_eof5;
case 5:
	if ( (*p) == 46 )
		goto st6;
	goto st0;
st6:
	if ( ++p == pe )
		goto _test_eof6;
case 6:
	switch( (*p) ) {
		case 48: goto st7;
		case 49: goto st9;
		case 50: goto st11;
	}
	if ( 51 <= (*p) && (*p) <= 57 )
		goto st10;
	goto st0;
st7:
	if ( ++p == pe )
		goto _test_eof7;
case 7:
	if ( (*p) == 46 )
		goto st8;
	goto st0;
st8:
	if ( ++p == pe )
		goto _test_eof8;
case 8:
	switch( (*p) ) {
		case 48: goto tr16;
		case 49: goto tr17;
		case 50: goto tr18;
	}
	if ( 51 <= (*p) && (*p) <= 57 )
		goto tr19;
	goto st0;
tr16:
#line 8 "rfc1918_parser.rl"
	{
    is_ip_rfc1918 = 1;
  }
	goto st28;
st28:
	if ( ++p == pe )
		goto _test_eof28;
case 28:
#line 134 "rfc1918_parser.c"
	goto st0;
tr17:
#line 8 "rfc1918_parser.rl"
	{
    is_ip_rfc1918 = 1;
  }
	goto st29;
st29:
	if ( ++p == pe )
		goto _test_eof29;
case 29:
#line 146 "rfc1918_parser.c"
	if ( 48 <= (*p) && (*p) <= 57 )
		goto tr19;
	goto st0;
tr19:
#line 8 "rfc1918_parser.rl"
	{
    is_ip_rfc1918 = 1;
  }
	goto st30;
st30:
	if ( ++p == pe )
		goto _test_eof30;
case 30:
#line 160 "rfc1918_parser.c"
	if ( 48 <= (*p) && (*p) <= 57 )
		goto tr16;
	goto st0;
tr18:
#line 8 "rfc1918_parser.rl"
	{
    is_ip_rfc1918 = 1;
  }
	goto st31;
st31:
	if ( ++p == pe )
		goto _test_eof31;
case 31:
#line 174 "rfc1918_parser.c"
	if ( (*p) == 53 )
		goto tr31;
	if ( (*p) > 52 ) {
		if ( 54 <= (*p) && (*p) <= 57 )
			goto tr16;
	} else if ( (*p) >= 48 )
		goto tr19;
	goto st0;
tr31:
#line 8 "rfc1918_parser.rl"
	{
    is_ip_rfc1918 = 1;
  }
	goto st32;
st32:
	if ( ++p == pe )
		goto _test_eof32;
case 32:
#line 193 "rfc1918_parser.c"
	if ( 48 <= (*p) && (*p) <= 53 )
		goto tr16;
	goto st0;
st9:
	if ( ++p == pe )
		goto _test_eof9;
case 9:
	if ( (*p) == 46 )
		goto st8;
	if ( 48 <= (*p) && (*p) <= 57 )
		goto st10;
	goto st0;
st10:
	if ( ++p == pe )
		goto _test_eof10;
case 10:
	if ( (*p) == 46 )
		goto st8;
	if ( 48 <= (*p) && (*p) <= 57 )
		goto st7;
	goto st0;
st11:
	if ( ++p == pe )
		goto _test_eof11;
case 11:
	switch( (*p) ) {
		case 46: goto st8;
		case 53: goto st12;
	}
	if ( (*p) > 52 ) {
		if ( 54 <= (*p) && (*p) <= 57 )
			goto st7;
	} else if ( (*p) >= 48 )
		goto st10;
	goto st0;
st12:
	if ( ++p == pe )
		goto _test_eof12;
case 12:
	if ( (*p) == 46 )
		goto st8;
	if ( 48 <= (*p) && (*p) <= 53 )
		goto st7;
	goto st0;
st13:
	if ( ++p == pe )
		goto _test_eof13;
case 13:
	if ( (*p) == 46 )
		goto st6;
	if ( 48 <= (*p) && (*p) <= 57 )
		goto st14;
	goto st0;
st14:
	if ( ++p == pe )
		goto _test_eof14;
case 14:
	if ( (*p) == 46 )
		goto st6;
	if ( 48 <= (*p) && (*p) <= 57 )
		goto st5;
	goto st0;
st15:
	if ( ++p == pe )
		goto _test_eof15;
case 15:
	switch( (*p) ) {
		case 46: goto st6;
		case 53: goto st16;
	}
	if ( (*p) > 52 ) {
		if ( 54 <= (*p) && (*p) <= 57 )
			goto st5;
	} else if ( (*p) >= 48 )
		goto st14;
	goto st0;
st16:
	if ( ++p == pe )
		goto _test_eof16;
case 16:
	if ( (*p) == 46 )
		goto st6;
	if ( 48 <= (*p) && (*p) <= 53 )
		goto st5;
	goto st0;
st17:
	if ( ++p == pe )
		goto _test_eof17;
case 17:
	if ( (*p) == 50 )
		goto st18;
	goto st0;
st18:
	if ( ++p == pe )
		goto _test_eof18;
case 18:
	if ( (*p) == 46 )
		goto st19;
	goto st0;
st19:
	if ( ++p == pe )
		goto _test_eof19;
case 19:
	switch( (*p) ) {
		case 49: goto st20;
		case 50: goto st21;
		case 51: goto st22;
	}
	goto st0;
st20:
	if ( ++p == pe )
		goto _test_eof20;
case 20:
	if ( 54 <= (*p) && (*p) <= 57 )
		goto st5;
	goto st0;
st21:
	if ( ++p == pe )
		goto _test_eof21;
case 21:
	if ( 48 <= (*p) && (*p) <= 57 )
		goto st5;
	goto st0;
st22:
	if ( ++p == pe )
		goto _test_eof22;
case 22:
	if ( 48 <= (*p) && (*p) <= 49 )
		goto st5;
	goto st0;
st23:
	if ( ++p == pe )
		goto _test_eof23;
case 23:
	if ( (*p) == 50 )
		goto st24;
	goto st0;
st24:
	if ( ++p == pe )
		goto _test_eof24;
case 24:
	if ( (*p) == 46 )
		goto st25;
	goto st0;
st25:
	if ( ++p == pe )
		goto _test_eof25;
case 25:
	if ( (*p) == 49 )
		goto st26;
	goto st0;
st26:
	if ( ++p == pe )
		goto _test_eof26;
case 26:
	if ( (*p) == 54 )
		goto st27;
	goto st0;
st27:
	if ( ++p == pe )
		goto _test_eof27;
case 27:
	if ( (*p) == 56 )
		goto st5;
	goto st0;
	}
	_test_eof2: cs = 2; goto _test_eof; 
	_test_eof3: cs = 3; goto _test_eof; 
	_test_eof4: cs = 4; goto _test_eof; 
	_test_eof5: cs = 5; goto _test_eof; 
	_test_eof6: cs = 6; goto _test_eof; 
	_test_eof7: cs = 7; goto _test_eof; 
	_test_eof8: cs = 8; goto _test_eof; 
	_test_eof28: cs = 28; goto _test_eof; 
	_test_eof29: cs = 29; goto _test_eof; 
	_test_eof30: cs = 30; goto _test_eof; 
	_test_eof31: cs = 31; goto _test_eof; 
	_test_eof32: cs = 32; goto _test_eof; 
	_test_eof9: cs = 9; goto _test_eof; 
	_test_eof10: cs = 10; goto _test_eof; 
	_test_eof11: cs = 11; goto _test_eof; 
	_test_eof12: cs = 12; goto _test_eof; 
	_test_eof13: cs = 13; goto _test_eof; 
	_test_eof14: cs = 14; goto _test_eof; 
	_test_eof15: cs = 15; goto _test_eof; 
	_test_eof16: cs = 16; goto _test_eof; 
	_test_eof17: cs = 17; goto _test_eof; 
	_test_eof18: cs = 18; goto _test_eof; 
	_test_eof19: cs = 19; goto _test_eof; 
	_test_eof20: cs = 20; goto _test_eof; 
	_test_eof21: cs = 21; goto _test_eof; 
	_test_eof22: cs = 22; goto _test_eof; 
	_test_eof23: cs = 23; goto _test_eof; 
	_test_eof24: cs = 24; goto _test_eof; 
	_test_eof25: cs = 25; goto _test_eof; 
	_test_eof26: cs = 26; goto _test_eof; 
	_test_eof27: cs = 27; goto _test_eof; 

	_test_eof: {}
	_out: {}
	}

#line 40 "rfc1918_parser.rl"

  return is_ip_rfc1918;
}

