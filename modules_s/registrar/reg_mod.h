/* 
 * $Id$ 
 *
 * registrar module interface
 */

#ifndef REG_MOD_H
#define REG_MOD_H

#include "../../parser/msg_parser.h"

extern int default_expires;
extern int default_q;
extern int append_branches;

extern float def_q;

extern int (*sl_reply)(struct sip_msg* _m, char* _s1, char* _s2);

#endif /* REG_MOD_H */
