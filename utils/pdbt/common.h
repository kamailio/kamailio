/*
 * Copyright (C) 2009 1&1 Internet AG
 *
 * This file is part of sip-router, a free SIP server.
 *
 * sip-router is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * sip-router is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef _COMMON_H_
#define _COMMON_H_




#include <stdint.h> 




/*
 0 no carrier id defined.
 1..999 are regular carrier ids.
 1000 is used as fake carrier id when merging carriers we are not interested in.
 -1000..-1 used in dtm to indicate a carrier id and that no more nodes will follow (leaf node compression).
 -1001 used in dtm to mark a pointer to a child node as NULL.
*/
#define MIN_PDB_CARRIERID 1
#define MAX_PDB_CARRIERID 999
#define OTHER_CARRIERID 1000
#define MAX_CARRIERID 1000
#define NULL_CARRIERID -1001
#define PAYLOADSIZE 256


#define IS_VALID_PDB_CARRIERID(id) ((id>=MIN_PDB_CARRIERID) && (id<=MAX_PDB_CARRIERID))
#define IS_VALID_CARRIERID(id) ((id>=MIN_PDB_CARRIERID) && (id<=MAX_CARRIERID))

#define PDB_VERSION     1



typedef int16_t carrier_t;

enum __attribute__((packed)) pdb_versions {
    PDB_VERSION_1 = 1,
    PDB_VERSION_MAX
};

enum __attribute__((packed)) pdb_types {
    PDB_TYPE_REQUEST_ID = 0,    /* request pdb type */
    PDB_TYPE_REPLY_ID,          /* reply pdb type */
    PDB_TYPE_MAX
};

enum __attribute__((packed)) pdb_codes {
    PDB_CODE_DEFAULT = 0,   /* for request */
    PDB_CODE_OK,            /* for response - OK */
    PDB_CODE_NOT_NUMBER,    /* for response - letters found in the number */
    PDB_CODE_NOT_FOUND,     /* for response - no pdb_id found for the number */
    PDB_CODE_MAX
};

struct __attribute__((packed)) pdb_hdr {
    uint8_t version;
    uint8_t type;
    uint8_t code;
    uint8_t length;
    uint16_t id;
};

struct __attribute__((packed)) pdb_bdy {
    char payload[PAYLOADSIZE];
};

struct __attribute__((packed)) pdb_msg {
    struct pdb_hdr hdr;
    struct pdb_bdy bdy;
};



void pdb_msg_dbg (struct pdb_msg msg);
int pdb_msg_format_send(struct pdb_msg *msg,
                            uint8_t version, uint8_t type,
                            uint8_t code, uint16_t id,
                            char *payload, uint16_t payload_len);

#endif
