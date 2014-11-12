/*
 * Copyright (C) 2014 Federico Cabiddu, federico.cabiddu@gmail.com
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
 *
 */

/*!
 * \file
 * \brief Functions and definitions related to per user transaction indexing and searching
 * \ingroup tsilo
 * Module: \ref tsilo
 */

#ifndef _TS_HASH_H_
#define _TS_HASH_H_

#include "../../locking.h"
#include "../../lib/kmi/mi.h"
#include "../../modules/tm/tm_load.h"

#define MAX_TS_LOCKS  2048
#define MIN_TS_LOCKS  2

typedef struct ts_transaction
{
	unsigned int		tindex;		/*!< transaction index */
	unsigned int		tlabel;		/*!< transaction label */

	struct ts_urecord	*urecord;	/*!< > urecord entry the transaction belongs to */

	struct ts_transaction	*next;		/*!< next entry in the list */
	struct ts_transaction	*prev;		/*!< previous entry in the list */
} ts_transaction_t;

/*! entries in the transaction list */
typedef struct ts_urecord
{
	str		     ruri;		/*!< request uri of the transaction */
	unsigned int	     rurihash;		/*!< hash request uri of the transaction */

	struct ts_entry     *entry;		/*!< Collision slot in the hash table */
	ts_transaction_t    *transactions;	/*!< One or more transactions */

	struct ts_urecord   *next;		/*!< next entry in the list */
	struct ts_urecord   *prev;		/*!< previous entry in the list */
} ts_urecord_t;


/*! entries in the main transaction table */
typedef struct ts_entry
{
	int n;                  	    /*!< Number of elements in the collision slot */
	struct ts_urecord    *first;	/*!< urecord list */
	struct ts_urecord    *last;	    /*!< optimisation, end of the urecord list */
	unsigned int       next_id;	    /*!< next id */
	unsigned int       lock_idx;	/*!< lock index */
} ts_entry_t;


/*! main transaction table */
typedef struct ts_table
{
	unsigned int       size;	    /*!< size of the tsilo table */
	struct ts_entry    *entries;	/*!< urecord hash table */
	unsigned int       locks_no;	/*!< number of locks */
	gen_lock_set_t     *locks;	    /*!< lock table */
} ts_table_t;

/*! global transactions table */
extern ts_table_t *t_table;

/*!
 * \brief Set a transaction lock
 * \param _table transaction table
 * \param _entry locked entry
 */
#define ts_lock(_table, _entry) \
		lock_set_get( (_table)->locks, (_entry)->lock_idx);

/*!
 * \brief Release a transaction lock
 * \param _table transaction table
 * \param _entry locked entry
 */
#define ts_unlock(_table, _entry) \
		lock_set_release( (_table)->locks, (_entry)->lock_idx);

void lock_entry(ts_entry_t *entry);
void unlock_entry(ts_entry_t *entry);

void lock_entry_by_ruri(str* ruri);
void unlock_entry_by_ruri(str* ruri);

/*!
 * \brief Initialize the per user transactions table
 * \param size size of the table
 * \return 0 on success, -1 on failure
 */
int init_ts_table(unsigned int size);

/*!
 * \brief Destroy the per user transaction table
 */
void destroy_ts_table(void);

/*
 * Obtain a urecord pointer if the urecord exists in the table
 */
int get_ts_urecord(str* ruri, struct ts_urecord** _r);

/*!
 * \brief Create and initialize new record structure
 * \param ruri request uri
 * \param _r pointer to the new record
 * \return 0 on success, negative on failure
 */
int new_ts_urecord(str* ruri, ts_urecord_t** _r);

/*!
 * \brief Insert a new record into transactions table
 * \param ruri request uri
 * \return 0 on success, -1 on failure
 */
int insert_ts_urecord(str* ruri, ts_urecord_t** _r);

/*!
 * \brief remove a urecord from table and free the memory
 * \param urecord t
 * \return 0 on success, -1 on failure
 */
void remove_ts_urecord(ts_urecord_t* _r);

/*!
 * \brief Insert a new transaction structure into urecord
 * \param _r urecord
 * \param tindex transaction index in tm table
 * \param tlabel transaction label in tm table
 * \return 0 on success, -1 otherwise
 */
int insert_ts_transaction(struct cell* t, sip_msg_t* msg, struct ts_urecord* _r);

/*!
 * \brief Create a new transaction structure
 * \param tindex transaction index in tm table
 * \param tlabel transaction label in tm table
 * \return created transaction structure on success, NULL otherwise
 */
ts_transaction_t* new_ts_transaction(int tindex, int tlabel);

/*!
 * \brief Clone a transaction structure
 * \param tma transaction to be cloned
 * \return cloned transaction structure on success, NULL otherwise
 */
ts_transaction_t* clone_ts_transaction(ts_transaction_t* ts);

/*!
 * \brief remove a transaction from the urecord transactions list
 * \param tma unlinked transaction
 */
void remove_ts_transaction(ts_transaction_t* ts_t);

void free_ts_transaction(void *ts_t);
#endif
