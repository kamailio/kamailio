/*
 * Warning: This file is auto generated from a ragel syntax (ip_parser.rl),
 * do not change it!
 */   

#ifndef ip_parser_h
#define ip_parser_h


#include <sys/types.h>


enum enum_ip_type {
  ip_type_ipv4 = 1,
  ip_type_ipv6,
  ip_type_ipv6_reference,
  ip_type_error
};


enum enum_ip_type ip_parser_execute(const char *str, size_t len);


#endif
