/*
 * $Id$
 */

#ifndef CHECKS_H
#define CHECKS_H

#include "../../msg_parser.h"
#include "../../str.h"


int check_to(struct sip_msg* _msg, char* _str1, char* _str2);
int check_from(struct sip_msg* _msg, char* _str1, char* _str2);

#endif
