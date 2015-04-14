/* 
 * File:   parse_pani.h
 * Author: jaybeepee
 *
 * Created on 25 February 2015, 2:51 PM
 */

#ifndef PARSE_PANI_H
#define	PARSE_PANI_H

#include "../str.h"
#include "msg_parser.h"
#include "parse_to.h"

enum access_types { IEEE_80211a=0, IEEE_80211b, _3GPP_GERAN, _3GPP_UTRANFDD, _3GPP_UTRANTDD, _3GPP_EUTRANFDD, _3GPP_EUTRANTDD, _3GPP_CDMA_2000 };

typedef struct pani_body {
    enum access_types access_type;
    str access_info;
} pani_body_t;

int parse_pani_header(struct sip_msg* const msg);

///*! casting macro for accessing P-Asserted-Identity body */
//#define get_pani(p_msg)  ((p_id_body_t*)(p_msg)->pai->parsed)
//
///*! casting macro for accessing P-Preferred-Identity body */
//#define get_pani(p_msg)  ((p_id_body_t*)(p_msg)->ppi->parsed)

int free_pani_body(struct pani_body *pani_body);

#endif	/* PARSE_PANI_H */

