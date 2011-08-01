#include "rfc1918_parser.h"


/** Ragel machine **/
%%{
  machine rfc1918_parser;

  action is_rfc1918 {
    is_ip_rfc1918 = 1;
  }

  DIGIT                = "0".."9";
  dec_octet            = DIGIT | ( 0x31..0x39 DIGIT ) | ( "1" DIGIT{2} ) |
                         ( "2" 0x30..0x34 DIGIT ) | ( "25" 0x30..0x35 );

  RFC1918_address      = ( "10." dec_octet "." dec_octet "." dec_octet ) |
                         ( "172." ( ( "1" "6".."9" ) | ( "2" DIGIT ) | ( "3" "0".."1" ) ) "." dec_octet "." dec_octet ) |
                         ( "192.168." dec_octet "." dec_octet );

   main                := RFC1918_address @is_rfc1918;
}%%


/** Data **/
%% write data;


/** exec **/
unsigned int rfc1918_parser_execute(const char *str, size_t len)
{
  int cs = 0;
  const char *p, *pe;
  unsigned int is_ip_rfc1918 = 0;

  p = str;
  pe = str+len;

  %% write init;
  %% write exec;

  return is_ip_rfc1918;
}

