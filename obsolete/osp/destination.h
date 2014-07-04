/*
 * ser osp module. 
 *
 * This module enables ser to communicate with an Open Settlement 
 * Protocol (OSP) server.  The Open Settlement Protocol is an ETSI 
 * defined standard for Inter-Domain VoIP pricing, authorization
 * and usage exchange.  The technical specifications for OSP 
 * (ETSI TS 101 321 V4.1.1) are available at www.etsi.org.
 *
 * Uli Abend was the original contributor to this module.
 * 
 * Copyright (C) 2001-2005 Fhg Fokus
 *
 * This file is part of ser, a free SIP server.
 *
 * ser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * For a license to use the ser software under conditions
 * other than those described here, or to purchase support for this
 * software, please contact iptel.org by e-mail at the following addresses:
 *    info@iptel.org
 *
 * ser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef _OSP_MOD_DESTINATION_H_
#define _OSP_MOD_DESTINATION_H_

#include <time.h>
#include "osp_mod.h"

typedef struct _osp_dest {
    char validafter[OSP_STRBUF_SIZE];
    char validuntil[OSP_STRBUF_SIZE];
    char callid[OSP_STRBUF_SIZE];
    char called[OSP_STRBUF_SIZE];
    char calling[OSP_STRBUF_SIZE];
    char source[OSP_STRBUF_SIZE];
    char srcdev[OSP_STRBUF_SIZE];
    char host[OSP_STRBUF_SIZE];
    char destdev[OSP_STRBUF_SIZE];
    char networkid[OSP_STRBUF_SIZE];
    unsigned char token[OSP_TOKENBUF_SIZE];
    unsigned int callidsize;
    unsigned int tokensize;
    unsigned int timelimit;
    int lastcode;
    time_t authtime;
    time_t time100;
    time_t time180;
    time_t time200;
    int type;
    unsigned long long transid;
    int supported;
    int used;
    int reported;
    unsigned int destinationCount;
    char origcalled[OSP_STRBUF_SIZE];
} osp_dest;

osp_dest* ospInitDestination(osp_dest* dest);
int ospSaveOrigDestination(osp_dest* dest);
int ospSaveTermDestination(osp_dest* dest);
int ospCheckOrigDestination(void);
osp_dest* ospGetNextOrigDestination(void);
osp_dest* ospGetLastOrigDestination(void);
osp_dest* ospGetTermDestination(void);
void ospRecordEvent(int clientcode, int servercode);
void ospDumpDestination(osp_dest* dest);
void ospDumpAllDestination(void);
void ospConvertAddress(char* src, char* dst, int buffersize);

#endif /* _OSP_MOD_DESTINATION_H_ */

