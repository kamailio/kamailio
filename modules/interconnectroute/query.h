/* 
 * File:   query.h
 * Author: jaybeepee
 *
 * Created on 14 October 2014, 3:50 PM
 */

#ifndef QUERY_H
#define	QUERY_H

#include "../../parser/msg_parser.h"

int ix_orig_trunk_query(struct sip_msg* msg);
int ix_term_trunk_query(struct sip_msg* msg, char* ext_trunk_id);
#endif	/* QUERY_H */

