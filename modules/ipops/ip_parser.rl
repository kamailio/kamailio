#include "ip_parser.h"


/** Ragel machine **/
%%{
  machine ip_parser;

  action is_ipv4 {
    ip_type = ip_type_ipv4;
  }

  action is_ipv6 {
    ip_type = ip_type_ipv6;
  }

  action is_ipv6_reference {
    ip_type = ip_type_ipv6_reference;
  }

  DIGIT                = "0".."9";
  HEXDIG               = DIGIT | "A"i | "B"i | "C"i | "D"i | "E"i | "F"i;
  dec_octet            = DIGIT | ( 0x31..0x39 DIGIT ) | ( "1" DIGIT{2} ) |
                         ( "2" 0x30..0x34 DIGIT ) | ( "25" 0x30..0x35 );
  IPv4address          = dec_octet "." dec_octet "." dec_octet "." dec_octet;
  h16                  = HEXDIG{1,4};
  ls32                 = ( h16 ":" h16 ) | IPv4address;
  IPv6address          = ( ( h16 ":" ){6} ls32 ) |
                         ( "::" ( h16 ":" ){5} ls32 ) |
                         ( h16? "::" ( h16 ":" ){4} ls32 ) |
                         ( ( ( h16 ":" )? h16 )? "::" ( h16 ":" ){3} ls32 ) |
                         ( ( ( h16 ":" ){,2} h16 )? "::" ( h16 ":" ){2} ls32 ) |
                         ( ( ( h16 ":" ){,3} h16 )? "::" h16 ":" ls32 ) |
                         ( ( ( h16 ":" ){,4} h16 )? "::" ls32 ) |
                         ( ( ( h16 ":" ){,5} h16 )? "::" h16 ) |
                         ( ( ( h16 ":" ){,6} h16 )? "::" );
  IPv6reference        = "[" IPv6address "]";

  main                := IPv4address @is_ipv4 |
                         IPv6address @is_ipv6 |
                         IPv6reference @is_ipv6_reference;
}%%

/** Data **/
%% write data;


/** exec **/
enum enum_ip_type ip_parser_execute(const char *str, size_t len)
{
  int cs = 0;
  const char *p, *pe;
  enum enum_ip_type ip_type = ip_type_error;

  p = str;
  pe = str+len;

  %% write init;
  %% write exec;

  if(len != p-str)
    return ip_type_error;
  else
    return ip_type;
}

