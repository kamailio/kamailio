/*
 * $Id$
 *
 * CASSANDRA module interface
 *
 * Copyright (C) 2012 1&1 Internet AG
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
 * History:
 * --------
 * 2012-01  first version (Anca Vamanu)
 * 2012-09  Added support for CQL queries (Boudewyn Ligthart)
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>
#include <poll.h>
#include <iostream>
#include <boost/lexical_cast.hpp>
#include <protocol/TBinaryProtocol.h>
#include <transport/TSocket.h>
#include <transport/TTransportUtils.h>

extern "C" {
#include "../../timer.h"
#include "../../mem/mem.h"
#include "dbcassa_table.h"
}

#include "Cassandra.h"
#include "dbcassa_base.h"

namespace at  = apache::thrift;
namespace att = apache::thrift::transport;
namespace atp = apache::thrift::protocol;
namespace oac = org::apache::cassandra;

static const char cassa_key_delim  = ' ';
static const int  cassa_max_key_len= 512;

#define MAX_ROWS_NO        128     /* TODO: make this configurable or dynamic */
int row_slices[MAX_ROWS_NO][2];

/*
 * ----         Cassandra Connection Section               ----
 *  */

struct cassa_con {
	struct db_id* id;           /*!< Connection identifier       */
	unsigned int ref;           /*!< Reference count             */
	struct pool_con* next;      /*!< Next connection in the pool */

	str db_name;                /*!< Database name as str        */
	oac::CassandraClient* con;  /*!< Cassandra connection        */
};

#define CON_CASSA(db_con)    ((struct cassa_con*)db_con->tail)

/*!
 * \brief Open connection to Cassandra cluster
 * \param db_id
 */
oac::CassandraClient* dbcassa_open(struct db_id* id)
{
	try {
		boost::shared_ptr<att::TSocket> socket(new att::TSocket(id->host, id->port));
		boost::shared_ptr<att::TTransport> transport(new att::TFramedTransport (socket));
		boost::shared_ptr<atp::TProtocol> protocol(new atp::TBinaryProtocol(transport));

		socket->setConnTimeout(cassa_conn_timeout);
		socket->setSendTimeout(cassa_send_timeout);
		socket->setRecvTimeout(cassa_recv_timeout);

		std::auto_ptr<oac::CassandraClient> cassa_client(new oac::CassandraClient(protocol));

		transport->open();
		if (!transport->isOpen()) {
			LM_ERR("Failed to open transport to Cassandra\n");
			return 0;
		}

		/* database name ->  keyspace */

		cassa_client->set_keyspace(id->database);
		if(id->username && id->password) {
			oac::AuthenticationRequest au_req;
			std::map<std::string, std::string>  cred;
			cred.insert(std::pair<std::string, std::string>("username", id->username));
			cred.insert(std::pair<std::string, std::string>("password", id->password));
			au_req.credentials = cred;
			try {
				cassa_client->login(au_req);
			} catch (const oac::AuthenticationException& autx) {
				LM_ERR("Authentication failure: Credentials not valid, %s\n", autx.why.c_str());
			} catch (const oac::AuthorizationException & auzx) {
				LM_ERR("Authentication failure: Credentials not valid for the selected database, %s\n", auzx.why.c_str());
			}
		}

		LM_DBG("Opened connection to Cassandra cluster  %s:%d\n", id->host, id->port);
		return cassa_client.release();

	} catch (const oac::InvalidRequestException &irx) {
		LM_ERR("Database does not exist %s, %s\n", id->database, irx.why.c_str());
	} catch (const at::TException &tx) {
		LM_ERR("Failed to open connection to Cassandra cluster %s:%d, %s\n",
				id->database, id->port, tx.what());
	} catch (const std::exception &ex) {
		LM_ERR("Failed: %s\n", ex.what());
	} catch (...) {
		LM_ERR("Failed to open connection to Cassandra cluster\n");
	}

	return 0;
}

/*!
 * \brief Create new DB connection structure
 * \param db_id
 */
void* db_cassa_new_connection(struct db_id* id)
{
	struct cassa_con* ptr;

	if (!id) {
		LM_ERR("invalid db_id parameter value\n");
		return 0;
	}

	if (id->port) {
		LM_DBG("opening connection: cassa://xxxx:xxxx@%s:%d/%s\n", ZSW(id->host),
			id->port, ZSW(id->database));
	} else {
		LM_DBG("opening connection: cassa://xxxx:xxxx@%s/%s\n", ZSW(id->host),
			ZSW(id->database));
	}

	ptr = (struct cassa_con*)pkg_malloc(sizeof(struct cassa_con));
	if (!ptr) {
		LM_ERR("failed trying to allocated %lu bytes for connection structure."
				"\n", (unsigned long)sizeof(struct cassa_con));
		return 0;
	}
	LM_DBG("%p=pkg_malloc(%lu)\n", ptr, (unsigned long)sizeof(struct cassa_con));

	memset(ptr, 0, sizeof(struct cassa_con));

	ptr->db_name.s = id->database;
	ptr->db_name.len = strlen(id->database);
	ptr->id = id;
	ptr->ref = 1;

	ptr->con = dbcassa_open(id);
	if(!ptr->con) {
		LM_ERR("Failed to open connection to Cassandra cluster\n");
		pkg_free(ptr);
		return 0;
	}
	return ptr;
}


/*!
 * \brief Close Cassandra connection
 * \param CassandraConnection
 */
void dbcassa_close(oac::CassandraClient* con)
{
	if(! con) return;

	delete con;
}

/*!
 * \brief Close the connection and release memory
 * \param connection
 */
void db_cassa_free_connection(struct pool_con* con)
{
	struct cassa_con * _c;

	if (!con) return;

	_c = (struct cassa_con*) con;
	dbcassa_close(_c->con);
	pkg_free(_c);
}

/*!
 * \brief Reconnect to Cassandra cluster
 * \param connection
 */
void dbcassa_reconnect(struct cassa_con* con)
{
	dbcassa_close(con->con);
	con->con = dbcassa_open(con->id);
}


/*
 * ----              DB Operations Section                          ----
 * */

/*
 *	Util functions
 * */
static int cassa_get_res_col(std::vector<oac::ColumnOrSuperColumn> result, int r_si, int r_fi, int prefix_len, db_key_t qcol)
{
	str res_col_name;

	for (int i = r_si; i< r_fi; i++) {
		res_col_name.s = (char*)result[i].column.name.c_str()+prefix_len;
		res_col_name.len = (int)result[i].column.name.size() - prefix_len;

		if(res_col_name.len == qcol->len &&
				strncmp(res_col_name.s, qcol->s, qcol->len )==0)
			return i;
	}
	return -1;
}

static int cassa_convert_result(db_key_t qcol, std::vector<oac::ColumnOrSuperColumn> result,
		int r_si, int r_fi, int prefix_len, db_val_t* sr_cell)
{
	str col_val;
	int idx_rescol;
	oac::Column res_col;

	idx_rescol = cassa_get_res_col(result, r_si, r_fi, prefix_len, qcol);
	if(idx_rescol< 0) {
		LM_DBG("Column not found in result %.*s\n", qcol->len, qcol->s);
		sr_cell->nul  = 1;
		return 0;
	}
	res_col = result[idx_rescol].column;

	col_val.s = (char*)res_col.value.c_str();

	if(!col_val.s) {
		LM_DBG("Column not found in result %.*s- NULL\n", qcol->len, qcol->s);
		sr_cell->nul  = 1;
		return 0;
	}
	col_val.len = strlen(col_val.s);

	sr_cell->nul  = 0;
	sr_cell->free  = 0;

	switch (sr_cell->type) {
		case DB1_INT:
			if(str2int(&col_val, (unsigned int*)&sr_cell->val.int_val) < 0) {
				LM_ERR("Wrong value [%s] - len=%d, expected integer\n", col_val.s, col_val.len);
				return -1;
			}
			break;
		case DB1_BIGINT:
			if(sscanf(col_val.s, "%lld", &sr_cell->val.ll_val) < 0) {
				LM_ERR("Wrong value [%s], expected integer\n", col_val.s);
				return -1;
			}
			break;
		case DB1_DOUBLE:
			if(sscanf(col_val.s, "%lf", &sr_cell->val.double_val) < 0) {
				LM_ERR("Wrong value [%s], expected integer\n", col_val.s);
				return -1;
			}
			break;
		case DB1_STR:
			pkg_str_dup(&sr_cell->val.str_val, &col_val);
			sr_cell->free  = 1;
			break;
		case DB1_STRING:
			col_val.len++;
			pkg_str_dup(&sr_cell->val.str_val, &col_val);
			sr_cell->val.str_val.len--;
			sr_cell->val.str_val.s[col_val.len-1]='\0';
			sr_cell->free  = 1;
			break;
		case DB1_BLOB:
			pkg_str_dup(&sr_cell->val.blob_val, &col_val);
			sr_cell->free  = 1;
			break;
		case DB1_BITMAP:
			if(str2int(&col_val, &sr_cell->val.bitmap_val) < 0) {
				LM_ERR("Wrong value [%s], expected integer\n", col_val.s);
				return -1;
			}
			break;
		case DB1_DATETIME:
			if(sscanf(col_val.s, "%ld", (long int*)&sr_cell->val.time_val) < 0) {
				LM_ERR("Wrong value [%s], expected integer\n", col_val.s);
				return -1;
			}
			break;
	}
	return 0;
}

static char* dbval_to_string(db_val_t dbval, char* pk)
{
	switch(dbval.type) {
		case DB1_STRING: strcpy(pk, dbval.val.string_val);
						   pk+= strlen(dbval.val.string_val);
						   break;
		case DB1_STR:    memcpy(pk, dbval.val.str_val.s, dbval.val.str_val.len);
						   pk+= dbval.val.str_val.len;
						   break;
		case DB1_INT:    pk+= sprintf(pk, "%d", dbval.val.int_val);
						   break;
		case DB1_BIGINT: pk+= sprintf(pk, "%lld", dbval.val.ll_val);
						   break;
		case DB1_DOUBLE: pk+= sprintf(pk, "%lf", dbval.val.double_val);
						   break;
		case DB1_BLOB:   pk+= sprintf(pk, "%.*s", dbval.val.blob_val.len, dbval.val.blob_val.s);
						   break;
		case DB1_BITMAP: pk+= sprintf(pk, "%u", dbval.val.bitmap_val);
						   break;
		case DB1_DATETIME:pk+= sprintf(pk, "%ld", (long int)dbval.val.time_val);
						  break;
	}
	return pk;
}


int cassa_constr_key( const db_key_t* _k, const db_val_t* _v,
		int _n, int key_len, dbcassa_column_p* key_array, int *no_kc, char* key)
{
	int i, j;
	char* pk = key;

	if(!key_array)
		return 0;

	for(j = 0; j< _n; j++) {
		LM_DBG("query col = %.*s\n",  _k[j]->len,  _k[j]->s);
	}

	for(i = 0; i< key_len; i++) {
		/* look in the received columns to search the key column */
		for(j = 0; j< _n; j++) {
			if(_k[j]->len == key_array[i]->name.len &&
					!strncmp(_k[j]->s, key_array[i]->name.s, _k[j]->len))
				break;
		}
		if(j == _n) {
			LM_DBG("The key column with name [%.*s] not found in values\n", key_array[i]->name.len, key_array[i]->name.s);
			break;
		}
		pk= dbval_to_string(_v[j], pk);
		*(pk++) = cassa_key_delim;
	}
	if(pk > key)
		*(--pk) = '\0';
	else
		*key = '\0';

	if(no_kc)
		*no_kc = i;

	LM_DBG("key = %s\n", key);

	return pk - key;
}


int cassa_result_separate_rows(std::vector<oac::ColumnOrSuperColumn> result) {
	int rows_no =0, i = 0;
	int res_size = result.size();

	while(i< res_size) {
		size_t found;
		std::string curr_seckey;

		found = result[i].column.name.find(cassa_key_delim);
		if(found< 0) {
			LM_ERR("Wrong formated column name - secondary key part not found [%s]\n",
					result[i].column.name.c_str());
			return -1;
		}
		curr_seckey = result[i].column.name.substr(0, found);

		while(++i < res_size) {
			if(result[i].column.name.compare(0, found, curr_seckey)) {
				LM_DBG("Encountered a new secondary key %s - %s\n", result[i].column.name.c_str(), curr_seckey.c_str());
				break;
			}
		}
		/* the current row stretches until index 'i' and the corresponding key prefix has length 'found' */
		row_slices[rows_no][0] = i;
		row_slices[rows_no][1] = found +1;
		rows_no++;
	}

	/* debug messages */
	for(int i = 0; i< rows_no; i++) {
		LM_DBG("Row %d until index %d with prefix len %d\n", i, row_slices[i][0], row_slices[i][1]);
	}

	return rows_no;
}

dbcassa_column_p cassa_search_col(dbcassa_table_p tbc, db_key_t col_name)
{
	dbcassa_column_p colp;

	colp = tbc->cols;
	while(colp) {
		if(colp->name.len == col_name->len && !strncmp(colp->name.s, col_name->s, col_name->len))
			return colp;
		colp = colp->next;
	}
	return 0;
}

typedef std::vector<oac::ColumnOrSuperColumn>  ColumnVec;
typedef std::auto_ptr<ColumnVec>  ColumnVecPtr;

ColumnVecPtr cassa_translate_query(const db1_con_t* _h, const db_key_t* _k,
		const db_val_t* _v, const db_key_t* _c, int _n, int _nc, int* ret_rows_no)
{
	char row_key[cassa_max_key_len];
	char sec_key[cassa_max_key_len];
	int key_len=0, seckey_len = 0;
	int no_kc, no_sec_kc;
	dbcassa_table_p tbc;
	char pk[256];

	/** Lock table schema and construct primary and secondary key **/
	if(_k) {
		tbc = dbcassa_db_get_table(&CON_CASSA(_h)->db_name, CON_TABLE(_h));
		if(!tbc) {
			LM_ERR("table %.*s does not exist!\n", CON_TABLE(_h)->len, CON_TABLE(_h)->s);
			return ColumnVecPtr(NULL);
		}
		cassa_constr_key(_k, _v, _n, tbc->key_len, tbc->key, &no_kc, row_key);

		if(no_kc != tbc->key_len) {/* was not able to construct the whole key */
			LM_ERR("Query not supported - key not provided\n");
			dbcassa_lock_release(tbc);
			return ColumnVecPtr(NULL);
		}
		key_len = tbc->key_len;

		cassa_constr_key(_k, _v, _n, tbc->seckey_len, tbc->sec_key, &no_sec_kc, sec_key);
		seckey_len = tbc->seckey_len;

		dbcassa_lock_release(tbc);
	}

	try {
		oac::SlicePredicate sp;
		if(seckey_len) { // seckey defined for this table
			if(no_sec_kc == seckey_len) { // was able to build the complete secondary key
				if(_c) { /* if queried for specific columns */
					/* query for the specific columns */
					for(int i=0; i< _nc; i++) {
						std::string col_name = sec_key;
						col_name.push_back(cassa_key_delim);
						col_name.append(_c[i]->s);
						sp.column_names.push_back(col_name);
						LM_DBG("Query col: %s\n", col_name.c_str());
					}
					sp.__isset.column_names = true; // set
				} else { /* query for columns starting with this secondary key */
					oac::SliceRange sr;
					sr.start = sec_key;
					sr.start.push_back(cassa_key_delim);
					sr.finish = sec_key;
					sr.finish.push_back(cassa_key_delim +1);
					sp.slice_range = sr;
					sp.__isset.slice_range = true; // set
				}
			} else {  /* query all columns */
				oac::SliceRange sr;
				sr.start = "";
				sr.finish = "";
				sp.slice_range = sr;
				sp.__isset.slice_range = true; // set
			}
		} else { /* the table doesn't have any secondary key defined */
			if(_c) {
				for(int i=0; i< _nc; i++) {
					/*sp.column_names.push_back(_c[i]->s);*/
					if(_c[i]->len>255) {
						LM_ERR("column key is too long [%.*s]\n", _c[i]->len, _c[i]->s);
						return ColumnVecPtr(NULL);
					}
					memcpy(pk, _c[i]->s, _c[i]->len);
					pk[_c[i]->len] = '\0';
					sp.column_names.push_back(pk);
					LM_DBG("Query col: %s\n", _c[i]->s);
				}
				LM_DBG("get %d columns\n", _nc);
				sp.__isset.column_names = true; // set
			} else {
				/* return all columns */
				oac::SliceRange sr;
				sr.start = "";
				sr.finish = "";
				sp.slice_range = sr;
				sp.__isset.slice_range = true; // set
				LM_DBG("get all columns\n");
			}
		}

		unsigned int retr = 0;
		oac::ColumnParent cparent;
		cparent.column_family = _h->table->s;
		ColumnVecPtr cassa_result(new std::vector<oac::ColumnOrSuperColumn>);
		do {
			if(CON_CASSA(_h)->con) {
				try {

					if(_k) {
						CON_CASSA(_h)->con->get_slice(*cassa_result, row_key, cparent, sp, oac::ConsistencyLevel::ONE);
						*ret_rows_no = 1;
					} else {
						oac::KeyRange keyRange;
						keyRange.start_key = "";
						keyRange.start_key = "";
						std::vector<oac::KeySlice> key_slice_vect;
						keyRange.__isset.start_key = 1;
						keyRange.__isset.end_key = 1;
						ColumnVec::iterator it = cassa_result->begin();

						/* get in a loop 100 records at a time */
						int rows_no =0;
						while(1) {
							CON_CASSA(_h)->con->get_range_slices(key_slice_vect, cparent, sp, keyRange, oac::ConsistencyLevel::ONE);
							/* construct cassa_result */
							LM_DBG("Retuned %d key slices\n", (int)key_slice_vect.size());
							for(unsigned int i = 0; i< key_slice_vect.size(); i++) {
								if(key_slice_vect[i].columns.size()==0) {
									continue;
								}
								cassa_result->insert(it, key_slice_vect[i].columns.begin(), key_slice_vect[i].columns.end());
								it = cassa_result->begin();
								row_slices[rows_no][0] = cassa_result->size();
								row_slices[rows_no][1] = 0;
								rows_no++;
							}
							if(key_slice_vect.size() < (unsigned int)keyRange.count)
								break;
						}

						*ret_rows_no = rows_no;
					}

					return cassa_result;
				} catch (const att::TTransportException &tx) {
					LM_ERR("Failed to query: %s\n", tx.what());
				}
			}
			dbcassa_reconnect(CON_CASSA(_h));
		} while(cassa_auto_reconnect && retr++ < cassa_retries);
		LM_ERR("Failed to connect, retries exceeded.\n");
	} catch (const oac::InvalidRequestException ir) {
		LM_ERR("Failed Invalid query request: %s\n", ir.why.c_str());
	} catch (const at::TException &tx) {
		LM_ERR("Failed generic Thrift error: %s\n", tx.what());
	} catch (const std::exception &ex) {
		LM_ERR("Failed std error: %s\n", ex.what());
	} catch (...) {
		LM_ERR("Failed generic error\n");
	}

	LM_DBG("Query with get slice no_kc=%d tbc->key_len=%d  _n=%d\n", no_kc, key_len,_n);
	return ColumnVecPtr(NULL);
}


/** 
 *  This function check the CQLresult of the CQL query and   
 *  adds the columns to the returning result structure. 
 *
 * \param _cql_res  handle for the CQLResult
 * \param _r result set for storage
 * \return zero on success, negative value on failure
 */
int cql_get_columns(oac::CqlResult& _cql_res, db1_res_t* _r, dbcassa_table_p tbc)
{
	std::vector<oac::CqlRow>  res_cql_rows = _cql_res.rows;
	int rows_no = res_cql_rows.size();
	int cols_no = 0;

	LM_DBG("cqlrow Vector size =%d\n", rows_no);
	
	if (rows_no > 0) {
		cols_no = res_cql_rows[0].columns.size();
		LM_DBG("There are %d columns available, this should be the case for all %d rows (consider cql).\n", cols_no, rows_no);
	} else {
		LM_DBG("Got 0 rows. There is no result from the query.\n");
		return 0;
	}

	RES_COL_N(_r) = cols_no;
	if (!RES_COL_N(_r)) {
		LM_ERR("no columns returned from the query\n");
		return -2;
	} else {
		LM_DBG("%d columns returned from the query\n", RES_COL_N(_r));
	}

	if (db_allocate_columns(_r, RES_COL_N(_r)) != 0) {
		LM_ERR("Could not allocate columns\n");
		return -3;
	}

	/* For fields we will use the columns inside the first columns */

	for(int col = 0; col < RES_COL_N(_r); col++) {
		RES_NAMES(_r)[col] = (str*)pkg_malloc(sizeof(str));
		if (! RES_NAMES(_r)[col]) {
			LM_ERR("no private memory left\n");
			RES_COL_N(_r) = col;
			db_free_columns(_r);
			return -4;
		}
		LM_DBG("Allocated %lu bytes for RES_NAMES[%d] at %p\n",
			(unsigned long)sizeof(str), col, RES_NAMES(_r)[col]);

		/* The pointer that is here returned is part of the result structure. */
		RES_NAMES(_r)[col]->s = (char*) res_cql_rows[0].columns[col].name.c_str();
		RES_NAMES(_r)[col]->len = strlen(RES_NAMES(_r)[col]->s);

		/* search the column in table schema to get the type */
		dbcassa_column_p colp = cassa_search_col(tbc, (db_key_t) RES_NAMES(_r)[col]);
		if(!colp) {
			LM_ERR("No column with name [%.*s] found\n", RES_NAMES(_r)[col]->len, RES_NAMES(_r)[col]->s);
			RES_COL_N(_r) = col;
			db_free_columns(_r);
			return -4;
		}

		RES_TYPES(_r)[col] = colp->type;

		LM_DBG("Column with name [%.*s] found: %d\n", RES_NAMES(_r)[col]->len, RES_NAMES(_r)[col]->s, colp->type);
		LM_DBG("RES_NAMES(%p)[%d]=[%.*s]\n", RES_NAMES(_r)[col], col,
			RES_NAMES(_r)[col]->len, RES_NAMES(_r)[col]->s);
	}
	return 0;
}

static int cassa_convert_result_raw(db_val_t* sr_cell, str *col_val) {

	if(!col_val->s) {
		LM_DBG("Column not found in result - NULL\n");
		sr_cell->nul  = 1;
		return 0;
	}
	col_val->len = strlen(col_val->s);

	sr_cell->nul  = 0;
	sr_cell->free  = 0;

	switch (sr_cell->type) {
		case DB1_INT:
			if(str2int(col_val, (unsigned int*)&sr_cell->val.int_val) < 0) {
				LM_ERR("Wrong value [%s] - len=%d, expected integer\n", col_val->s, col_val->len);
				return -1;
			}
			break;
		case DB1_BIGINT:
			if(sscanf(col_val->s, "%lld", &sr_cell->val.ll_val) < 0) {
				LM_ERR("Wrong value [%s], expected integer\n", col_val->s);
				return -1;
			}
			break;
		case DB1_DOUBLE:
			if(sscanf(col_val->s, "%lf", &sr_cell->val.double_val) < 0) {
				LM_ERR("Wrong value [%s], expected integer\n", col_val->s);
				return -1;
			}
			break;
		case DB1_STR:
			pkg_str_dup(&sr_cell->val.str_val, col_val);
			sr_cell->free  = 1;
			break;
		case DB1_STRING:
			col_val->len++;
			pkg_str_dup(&sr_cell->val.str_val, col_val);
			sr_cell->val.str_val.len--;
			sr_cell->val.str_val.s[col_val->len-1]='\0';
			sr_cell->free  = 1;
			break;
		case DB1_BLOB:
			pkg_str_dup(&sr_cell->val.blob_val, col_val);
			sr_cell->free  = 1;
			break;
		case DB1_BITMAP:
			if(str2int(col_val, &sr_cell->val.bitmap_val) < 0) {
				LM_ERR("Wrong value [%s], expected integer\n", col_val->s);
				return -1;
			}
			break;
		case DB1_DATETIME:
			if(sscanf(col_val->s, "%ld", (long int*)&sr_cell->val.time_val) < 0) {
				LM_ERR("Wrong value [%s], expected integer\n", col_val->s);
				return -1;
			}
			break;
	}
	return 0;
}


/**
 *  This function convert the rows returned in CQL query 
 *  and adds the values to the returning result structure.
 *
 * Handle CQLresult
 * \param _cql_res  handle for the CQLResult
 * \param _r result set for storage
 * \return zero on success, negative value on failure
 */

int cql_convert_row(oac::CqlResult& _cql_res, db1_res_t* _r)
{
	std::vector<oac::CqlRow>  res_cql_rows = _cql_res.rows;
        int rows_no = res_cql_rows.size();
        int cols_no = res_cql_rows[0].columns.size();
        str col_val;
        RES_ROW_N(_r) = rows_no;

        if (db_allocate_rows(_r) < 0) {
                LM_ERR("Could not allocate rows.\n");
                return -1;
        }

        for(int ri=0; ri < rows_no; ri++) {
                if (db_allocate_row(_r, &(RES_ROWS(_r)[ri])) != 0) {
                        LM_ERR("Could not allocate row.\n");
                        return -2;
                }

		// complete the row with the columns 
		for(int col = 0; col< cols_no; col++) {
			col_val.s = (char*)res_cql_rows[ri].columns[col].value.c_str();
                        col_val.len = strlen(col_val.s);

			RES_ROWS(_r)[ri].values[col].type = RES_TYPES(_r)[col];	
			cassa_convert_result_raw(&RES_ROWS(_r)[ri].values[col], &col_val);

			LM_DBG("Field index %d. %s = %s.\n", col,
				res_cql_rows[ri].columns[col].name.c_str(),
				res_cql_rows[ri].columns[col].value.c_str());
		}
	}
	return 0;
} 

/*
 *	The functions for the DB Operations: query, delete, update.
 * */

/*
 * Extracts table name from DML query being used
 *
 * */
static int get_table_from_query(const str *cql, str *table) {

	char *ptr = cql->s,
		*begin = NULL;

        if (cql->s[0] == 's' || cql->s[0] == 'S') {
                ptr = strcasestr(cql->s, "from");
                ptr += sizeof(char) * 4;
        }
        else if (cql->s[0] == 'u' || cql->s[0] == 'U') {
                ptr = cql->s + sizeof("update") - 1;
        }
        else if (cql->s[0] == 'd' || cql->s[0] == 'D') {
                ptr = strcasestr(cql->s, "from");
                ptr += sizeof(char) * 4;
        }
        else if (cql->s[0] == 'i' || cql->s[0] == 'I') {
                ptr = strcasestr(cql->s, "into");
                ptr += sizeof(char) * 4;
        }
	else 
		goto error;

        while (*ptr == ' ' && (ptr - cql->s) <= cql->len) {
                ptr++;
        }

        begin = ptr;
        ptr   = strchr(begin, ' ');

        if (ptr == NULL)
		ptr = cql->s + cql->len;
	
	if (ptr - begin <= 0)
		goto error;
	
	table->s = begin;
	table->len = ptr - begin;

	return 0;

error:
	LM_ERR("Unable to determine operation in cql [%*s]\n", cql->len, cql->s);
	return -1;
}

/**
 * Execute a raw SQL query.
 * \param _h handle for the database
 * \param _s raw query string
 * \param _r result set for storage
 * \return zero on success, negative value on failure
 */
int db_cassa_raw_query(const db1_con_t* _h, const str* _s, db1_res_t** _r)
{
	db1_res_t* db_res = 0;
	str table_name;
	dbcassa_table_p tbc;
	std::vector<oac::CqlRow>  res_cql_rows;

	if (!_h || !_r) {
		LM_ERR("Invalid parameter value\n");
		return -1;
	}
	
	if (get_table_from_query(_s, &table_name) < 0) { 
		LM_ERR("Error parsing table name in CQL string");
		return -1;
	}

	LM_DBG("query table=%.*s\n", table_name.len, table_name.s);
	LM_DBG("CQL=%s\n", _s->s);

	tbc = dbcassa_db_get_table(&CON_CASSA(_h)->db_name, &table_name);
        if(!tbc) {
                LM_ERR("table %.*s does not exist!\n", table_name.len, table_name.s);
                return -1;
        }

	std::string cql_query(_s->s);

	oac::CqlResult cassa_cql_res;

	try {
		CON_CASSA(_h)->con->execute_cql_query(cassa_cql_res, cql_query , oac::Compression::NONE);
	} catch (const oac::InvalidRequestException &irx) {
		LM_ERR("Invalid Request caused error details: %s.\n", irx.why.c_str());
	} catch (const at::TException &tx) {
		LM_ERR("T Exception %s\n", tx.what());
	} catch (const std::exception &ex) {
		LM_ERR("Failed: %s\n", ex.what());
	} catch (...) {
		LM_ERR("Failed to open connection to Cassandra cluster\n");
	}

	if (!cassa_cql_res.__isset.rows) {
		LM_ERR("The resultype rows was not set, no point trying to parse result.\n");
		goto error;
	}

	res_cql_rows = cassa_cql_res.rows;

	/* TODO Handle the other types */
	switch(cassa_cql_res.type) {
		case 1:  LM_DBG("Result set is an ROW Type.\n");
			break;
		case 2: LM_DBG("Result set is an VOID Type.\n");
			break;
		case 3: LM_DBG("Result set is an INT Type.\n");
			break;
	}

	db_res = db_new_result();
	if (!db_res) {
		LM_ERR("no memory left\n");
		goto error;
	}

	if(res_cql_rows.size() == 0) {
		LM_DBG("The query returned no result\n");
		RES_ROW_N(db_res) = 0;
		RES_COL_N(db_res)= 0;
		*_r = db_res;
		goto done;
	}

	if (cql_get_columns(cassa_cql_res, db_res, tbc) < 0) {
		LM_ERR("Error getting column names.");
		goto error;
	}

	if (cql_convert_row(cassa_cql_res, db_res) < 0) {
		LM_ERR("Error converting rows");
		goto error;
	}

	*_r = db_res;
done:
	dbcassa_lock_release(tbc);

	LM_DBG("Exited with success\n");
	return 0;

error:
	if(db_res)
		db_free_result(db_res);
	
	dbcassa_lock_release(tbc);
	return -1;
}



/*
 * Query table for specified rows
 * _h: structure representing database connection
 * _k: key names
 * _op: operators
 * _v: values of the keys that must match
 * _c: column names to return
 * _n: number of key=values pairs to compare
 * _nc: number of columns to return
 * _o: order by the specified column
 */
int db_cassa_query(const db1_con_t* _h, const db_key_t* _k, const db_op_t* _op,
		const db_val_t* _v, const db_key_t* _c, int _n, int _nc,
		const db_key_t _o, db1_res_t** _r)
{
	db1_res_t* db_res = 0;
	int rows_no;
	ColumnVecPtr cassa_result;
	dbcassa_table_p tbc;
	int seckey_len;

	if (!_h || !CON_TABLE(_h) || !_r) {
		LM_ERR("invalid parameter value\n");
		return -1;
	}
	LM_DBG("query table=%s\n", _h->table->s);

	/** Construct and send the query to Cassandra Cluster **/

	cassa_result = cassa_translate_query(_h, _k, _v, _c, _n, _nc, &rows_no);

	if(cassa_result.get() == NULL) {
		LM_ERR("Failed to query Cassandra cluster\n");
		return -1;
	}

	/* compare the number of queried cols with the key cols*/
//	if(no_kc + no_sec_kc < _n) { /* TODO */
		/* filter manually for the rest of the values */
//	}

	db_res = db_new_result();
	if (!db_res) {
		LM_ERR("no memory left\n");
		goto error;
	}
	RES_COL_N(db_res)= _nc;
	if(!db_allocate_columns(db_res, _nc) < 0) {
		LM_ERR("no more memory\n");
		goto error;
	}

	tbc = dbcassa_db_get_table(&CON_CASSA(_h)->db_name, CON_TABLE(_h));
	if(!tbc) {
		LM_ERR("table %.*s does not exist!\n", CON_TABLE(_h)->len, CON_TABLE(_h)->s);
		return -1;
	}

	/** Convert the result from Cassandra **/
	/* fill in the columns name and type */
	for(int col = 0; col < _nc; col++) {
		RES_NAMES(db_res)[col] = (str*)pkg_malloc(sizeof(str));
		if (! RES_NAMES(db_res)[col]) {
			LM_ERR("no private memory left\n");
			dbcassa_lock_release(tbc);
			RES_COL_N(db_res) = col;
			db_free_columns(db_res);
			goto error;
		}

		*RES_NAMES(db_res)[col]   = *_c[col];

		/* search the column in table schema to get the type */
		dbcassa_column_p colp = cassa_search_col(tbc, _c[col]);
		if(!colp) {
			LM_ERR("No column with name [%.*s] found\n", _c[col]->len, _c[col]->s);
			dbcassa_lock_release(tbc);
			RES_COL_N(db_res) = col;
			db_free_columns(db_res);
			goto error;
		}
		RES_TYPES(db_res)[col] = colp->type;

		LM_DBG("RES_NAMES(%p)[%d]=[%.*s]\n", RES_NAMES(db_res)[col], col,
				RES_NAMES(db_res)[col]->len, RES_NAMES(db_res)[col]->s);
	}
	/* TODO  if all columns asked - take from table schema */
	seckey_len = tbc->seckey_len;
	dbcassa_lock_release(tbc);

	if(!cassa_result->size()) {
		LM_DBG("The query returned no result\n");
		RES_ROW_N(db_res) = 0;
		goto done;
	}

	/* Initialize the row_slices vector for the case with one column and no secondary key */
	if(rows_no == 1) {
		row_slices[0][0]= cassa_result->size();
		row_slices[0][1]= 0;

		if(seckey_len) { /* if the table has a secondary key defined */
			/* pass through the result once to see how many rows there are */
			rows_no = cassa_result_separate_rows(*cassa_result);
			if(rows_no < 0) {
				LM_ERR("Wrong formated column names\n");
				goto error;
			}
		}
	}

	RES_ROW_N(db_res) = rows_no;

	if (db_allocate_rows(db_res) < 0) {
		LM_ERR("could not allocate rows");
		goto error;
	}

	for(int ri=0; ri < rows_no; ri++) {
		if (db_allocate_row(db_res, &(RES_ROWS(db_res)[ri])) != 0) {
			LM_ERR("could not allocate row");
			goto error;
		}

		/* complete the row with the columns */
		for(int col = 0; col< _nc; col++) {
			RES_ROWS(db_res)[ri].values[col].type = RES_TYPES(db_res)[col];
			cassa_convert_result(_c[col], *cassa_result, (ri>0?row_slices[ri-1][0]:0),  row_slices[ri][0],
					row_slices[ri][1], &RES_ROWS(db_res)[ri].values[col]);
		}
	}

done:
	*_r = db_res;
	LM_DBG("Exited with success\n");
	return 0;

error:
	if(db_res)
		db_free_result(db_res);
	return -1;
}

/*
 * Insert or update the table for specified row key
 * _h: structure representing database connection
 * _k: key names
 * _op: operators
 * _v: values of the keys that must match
 * _uk: column names to update
 * _uv: values for the columns to update
 * _n: number of key=values pairs to compare
 * _un: number of columns to update
 */
int db_cassa_modify(const db1_con_t* _h, const db_key_t* _k, const db_val_t* _v,
		const db_key_t* _uk, const db_val_t* _uv, int _n, int _un)
{
	dbcassa_table_p tbc;
	char row_key[cassa_max_key_len];
	char sec_key[cassa_max_key_len];
	int64_t ts = 0;
	str ts_col_name={0, 0};
	int seckey_len;
	unsigned int curr_time = time(NULL);

	if (!_h || !CON_TABLE(_h) || !_k || !_v) {
		LM_ERR("invalid parameter value\n");
		return -1;
	}

	LM_DBG("modify table=%s\n", _h->table->s);

	/** Lock table schema and construct primary and secondary key **/
	tbc = dbcassa_db_get_table(&CON_CASSA(_h)->db_name, CON_TABLE(_h));
	if(!tbc) {
		LM_ERR("table %.*s does not exist!\n", CON_TABLE(_h)->len, CON_TABLE(_h)->s);
		return -1;
	}
	if(tbc->ts_col)
		pkg_str_dup(&ts_col_name, (const str*)&tbc->ts_col->name);

	cassa_constr_key(_k, _v, _n, tbc->key_len, tbc->key, 0, row_key);
	cassa_constr_key(_k, _v, _n, tbc->seckey_len, tbc->sec_key, 0, sec_key);
	seckey_len = tbc->seckey_len;

	dbcassa_lock_release(tbc);

	/** Construct and send the query to Cassandra Cluster **/
	try {
		/* Set the columns */
		std::vector<oac::Mutation> mutations;
		for(int i=0; i< _un; i++) {
			if(_uv[i].nul)
				continue;

			std::stringstream out;
			std::string value;
			int cont = 0;

			switch(_uv[i].type) {
				case DB1_INT:	out << _uv[i].val.int_val;
								value = out.str();
								break;
				case DB1_BIGINT:out << _uv[i].val.ll_val;
								value = out.str();
								break;
				case DB1_DOUBLE:out << _uv[i].val.double_val;
								value = out.str();
								break;
				case DB1_BITMAP:out << _uv[i].val.bitmap_val;
								value = out.str();
								break;
				case DB1_STRING:value = _uv[i].val.string_val;
								break;
				case DB1_STR:	if(!_uv[i].val.str_val.s) {
									cont = 1;
									break;
								}
								value = std::string(_uv[i].val.str_val.s, _uv[i].val.str_val.len);
								break;
				case DB1_BLOB:	value = std::string(_uv[i].val.blob_val.s, _uv[i].val.blob_val.len);
								break;
				case DB1_DATETIME:	unsigned int exp_time = (unsigned int)_uv[i].val.time_val;
									out << exp_time;
									value = out.str();
									if(ts_col_name.s && ts_col_name.len==_uk[i]->len &&
											strncmp(ts_col_name.s, _uk[i]->s, ts_col_name.len)==0) {
										ts = exp_time;
										LM_DBG("Found timestamp col [%.*s]\n", ts_col_name.len, ts_col_name.s);
									}
									break;
			}
			if (cont)
				continue;

			LM_DBG("ADDED column [%.*s] type [%d], value [%s]\n", _uk[i]->len, _uk[i]->s,
				_uv[i].type, value.c_str());

			oac::Mutation mut;
			oac::ColumnOrSuperColumn col;
			if(seckey_len) {
				col.column.name = sec_key;
				col.column.name.push_back(cassa_key_delim);
				col.column.name.append(_uk[i]->s);
			}
			else
				col.column.name = _uk[i]->s;
			col.column.value = value;
			col.column.__isset.value = true;
			col.__isset.column = true;
			col.column.timestamp = curr_time;
			col.column.__isset.timestamp = true;
			mut.column_or_supercolumn = col;
			mut.__isset.column_or_supercolumn = true;
			mutations.push_back(mut);
		}
		if(ts_col_name.s)
			pkg_free(ts_col_name.s);
		ts_col_name.s = 0;

		if(ts) {
			int32_t ttl = ts - curr_time;
			LM_DBG("Set expires to %d seconds\n", ttl);
			for(size_t mi=0; mi< mutations.size(); mi++) {
				mutations[mi].column_or_supercolumn.column.ttl = ttl;
				mutations[mi].column_or_supercolumn.column.__isset.ttl = true;
			}
		}

		LM_DBG("Perform the mutation, add [%d] columns\n", (int)mutations.size());

		std::map<std::string, std::vector<oac::Mutation> > innerMap;
		innerMap.insert(std::pair<std::string, std::vector<oac::Mutation> > (_h->table->s, mutations));
		std::map <std::string, std::map<std::string, std::vector<oac::Mutation> > > CFMap;
		CFMap.insert(std::pair<std::string, std::map<std::string, std::vector<oac::Mutation> > >(row_key, innerMap));
		unsigned int retr = 0;

		do {
			if(CON_CASSA(_h)->con) {
				try{
					CON_CASSA(_h)->con->batch_mutate(CFMap, oac::ConsistencyLevel::ONE);
					return 0;
				}  catch (const att::TTransportException &tx) {
					LM_ERR("Failed to query: %s\n", tx.what());
				}
			}
			dbcassa_reconnect(CON_CASSA(_h));
		} while (cassa_auto_reconnect && retr++ < cassa_retries);
		LM_ERR("Failed to connect, retries exceeded.\n");
	} catch (const oac::InvalidRequestException ir) {
		LM_ERR("Failed Invalid query request: %s\n", ir.why.c_str());
	} catch (const at::TException &tx) {
		LM_ERR("Failed generic Thrift error: %s\n", tx.what());
	} catch (const std::exception &ex) {
		LM_ERR("Failed std error: %s\n", ex.what());
	} catch (...) {
		LM_ERR("Failed generic error\n");
	}

	LM_ERR("Insert/Update query failed\n");
	return -1;
}

int db_cassa_replace(const db1_con_t* _h, const db_key_t* _k, const db_val_t* _v,
		int _n, const int _un, const int _m)
{
	LM_DBG("db_cassa_replace:\n");
	return db_cassa_modify(_h, _k, _v, _k, _v, _n, _n);
}

int db_cassa_insert(const db1_con_t* _h, const db_key_t* _k, const db_val_t* _v,
		int _n)
{
	LM_DBG("db_cassa_insert:\n");
	return db_cassa_modify(_h, _k, _v, _k, _v, _n, _n);
}


int db_cassa_update(const db1_con_t* _h, const db_key_t* _k, const db_op_t* _o,
		const db_val_t* _v, const db_key_t* _uk, const db_val_t* _uv,
		int _n, int _un)
{
	LM_DBG("db_cassa_update:\n");
	return db_cassa_modify(_h, _k, _v, _uk, _uv, _n, _un);
}


int db_cassa_free_result(db1_con_t* _h, db1_res_t* _r)
{
	return db_free_result(_r);
}

/*
 * Delete after primary or primary and secondary key
 * _h: structure representing database connection
 * _k: key names
 * _op: operators
 * _v: values of the keys that must match
 * _n: number of key=values pairs to compare
 */
int db_cassa_delete(const db1_con_t* _h, const db_key_t* _k, const db_op_t* _o,
		const db_val_t* _v, int _n)
{
	oac::CassandraClient* cassa_client = CON_CASSA(_h)->con;
	char row_key[cassa_max_key_len];
	char sec_key[cassa_max_key_len];
	dbcassa_table_p tbc;
	int no_kc, no_sec_kc;
	unsigned int retr = 0;
	int seckey_len;
	oac::Mutation m;

	if (!_h || !CON_TABLE(_h) || !_k || !_v) {
		LM_ERR("invalid parameter value\n");
		return -1;
	}

	LM_DBG("query table=%s\n", _h->table->s);

	/* get the table schema and construct primary and secondary key */
	tbc = dbcassa_db_get_table(&CON_CASSA(_h)->db_name, CON_TABLE(_h));
	if(!tbc)
	{
		LM_ERR("table %.*s does not exist!\n", CON_TABLE(_h)->len, CON_TABLE(_h)->s);
		return -1;
	}

	cassa_constr_key(_k, _v, _n, tbc->key_len, tbc->key, &no_kc,   row_key);
	cassa_constr_key(_k, _v, _n, tbc->seckey_len, tbc->sec_key, &no_sec_kc, sec_key);
	seckey_len = tbc->seckey_len;

	if (_n != no_kc && no_sec_kc == seckey_len) {
		/* if the conditions are also for secondary key */
		LM_DBG("Delete after primary and secondary key %s %s\n", row_key, sec_key);
		dbcassa_column_p colp = tbc->cols;
		try {
			while(colp) {
				std::string col_name = sec_key;
				col_name.push_back(cassa_key_delim);
				col_name.append(colp->name.s);
				m.deletion.predicate.column_names.push_back(col_name);
				colp = colp->next;
			}
		} catch (...) {
			LM_ERR("Failed to construct the list of column names\n");
			dbcassa_lock_release(tbc);
			return -1;
		}
	}

	dbcassa_lock_release(tbc);

	for(int i=0; i < _n; i++)
		LM_DBG("delete query col = %.*s\n", _k[i]->len, _k[i]->s);

	if(no_kc == 0 ) {
		LM_DBG("Delete operation not supported\n");
		return -1;
	}

	try {
		if (_n == no_kc) {
			LM_DBG("Delete after row key %s\n", row_key);
			oac::ColumnPath cp;
			cp.column_family = _h->table->s;
			do {
				if(CON_CASSA(_h)->con) {
					try {
						cassa_client->remove(row_key, cp, (int64_t)time(0), oac::ConsistencyLevel::ONE);
						return 0;
					} catch  (const att::TTransportException &tx) {
							LM_ERR("Failed to query: %s\n", tx.what());
					}
				}
				dbcassa_reconnect(CON_CASSA(_h));
			} while(cassa_auto_reconnect && retr++ < cassa_retries);
			LM_ERR("Failed to connect, retries exceeded.\n");
		} else {

			if(!seckey_len) {
				LM_ERR("Delete operation not supported\n");
				return -1;
			}

//			oac::Mutation m;
			m.deletion.timestamp = (int64_t)time(0);
			m.deletion.__isset.timestamp = true;
			m.__isset.deletion = true;

#if 0
			/* push all columns for the corresponding secondary key */
			tbc = dbcassa_db_get_table(&CON_CASSA(_h)->db_name, CON_TABLE(_h));
			if(!tbc)
			{
				LM_ERR("table %.*s does not exist!\n", CON_TABLE(_h)->len, CON_TABLE(_h)->s);
				return -1;
			}
			dbcassa_column_p colp = tbc->cols;
			try {
				while(colp) {
					std::string col_name = sec_key;
					col_name.push_back(cassa_key_delim);
					col_name.append(colp->name.s);
					m.deletion.predicate.column_names.push_back(col_name);
					colp = colp->next;
				}
			} catch (...) {
				LM_ERR("Failed to construct the list of column names\n");
				dbcassa_lock_release(tbc);
				return -1;
			}
			dbcassa_lock_release(tbc);
#endif
			m.deletion.__isset.predicate = true;
			m.deletion.predicate.__isset.column_names = true; // set

			std::vector<oac::Mutation> mutations;
			mutations.push_back(m);

			/* innerMap - column_family + mutations vector */
			std::map<std::string, std::vector<oac::Mutation> > innerMap;
			innerMap.insert(std::pair<std::string, std::vector<oac::Mutation> > (_h->table->s, mutations));
			std::map <std::string, std::map<std::string, std::vector<oac::Mutation> > > CFMap;
			CFMap.insert(std::pair<std::string, std::map<std::string, std::vector<oac::Mutation> > >(row_key, innerMap));

			do {
				if(CON_CASSA(_h)->con) {
					try {
						cassa_client->batch_mutate(CFMap, oac::ConsistencyLevel::ONE);
						return 0;
					} catch  (const att::TTransportException &tx) {
							LM_ERR("Failed to query: %s\n", tx.what());
					}
				}
				dbcassa_reconnect(CON_CASSA(_h));
			} while(cassa_auto_reconnect && retr++ < cassa_retries);
		}
		LM_ERR("Failed to connect, retries exceeded.\n");
	} catch (const oac::InvalidRequestException ir) {
		LM_ERR("Invalid query: %s\n", ir.why.c_str());
	} catch (const at::TException &tx) {
		LM_ERR("Failed TException: %s\n", tx.what());
	} catch (std::exception &e) {
		LM_ERR("Failed: %s\n", e.what());
	} catch (...) {
		LM_ERR("Failed generic error\n");
	}

	return -1;
}

