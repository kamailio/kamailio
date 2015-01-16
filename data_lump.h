/*
 * adding/removing headers or any other data chunk from a message
 *
 * Copyright (C) 2001-2003 FhG Fokus
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * Kamailio is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * Kamailio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

/*!
 * \file
 * \brief Kamailio core :: Data_lumps
 * \author jiri, andrei, janakj
 * adding/removing headers or any other data chunk from a message
 * \ingroup core
 * Module: \ref core
 */


#ifndef data_lump_h
#define data_lump_h

#include "lump_struct.h"
#include "parser/msg_parser.h"
#include "parser/hf.h"


/* adds a header right after an anchor point if exists */
struct lump* add_new_lump(struct lump** list, char* new_hdr,
							 int len, enum _hdr_types_t type);

/*! \brief adds a header to the end */
struct lump* append_new_lump(struct lump** list, char* new_hdr,
							 int len, enum _hdr_types_t type);
/*! \brief inserts a header to the beginning */
struct lump* insert_new_lump(struct lump** list, char* new_hdr,
							  int len, enum _hdr_types_t type);
struct lump* insert_new_lump_after(struct lump* after,
									char* new_hdr, int len, enum _hdr_types_t type);
struct lump* insert_new_lump_before(struct lump* before, char* new_hdr,
									int len,enum _hdr_types_t type);
/*! \brief substitutions (replace with ip address, port etc) */
struct lump* insert_subst_lump_after(struct lump* after,  enum lump_subst subst,
									enum _hdr_types_t type);
struct lump* insert_subst_lump_before(struct lump* before,enum lump_subst subst,
									enum _hdr_types_t type);

/*! \brief conditional lumps */
struct lump* insert_cond_lump_after(struct lump* after, enum lump_conditions c,
									enum _hdr_types_t type);
struct lump* insert_cond_lump_before(struct lump* after, enum lump_conditions c,
									enum _hdr_types_t type);

/* set an anchor if there is no existing one at the given offset,
 * otherwise return the existing anchor */
struct lump* anchor_lump2(struct sip_msg* msg, int offset, int len, enum _hdr_types_t type,
								int *is_ref);
/*! \brief removes an already existing header */
struct lump* del_lump(struct sip_msg* msg, int offset, int len, enum _hdr_types_t type);
/*! \brief set an anchor */
struct lump* anchor_lump(struct sip_msg* msg, int offset, int len, enum _hdr_types_t type);



/*! \brief duplicates a lump list shallowly in pkg-mem */
struct lump* dup_lump_list( struct lump *l );
/*! \brief frees a shallowly duplicated lump list */
void free_duped_lump_list(struct lump* l);


/*! \brief remove all non-SHMEM lumps from the list */
void del_nonshm_lump( struct lump** lump_list );

/*! \brief remove the lump from the internal lists */
int remove_lump(sip_msg_t *msg, struct lump *l);
#endif
