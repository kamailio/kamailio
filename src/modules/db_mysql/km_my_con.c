/*
 * Copyright (C) 2001-2004 iptel.org
 * Copyright (C) 2008 1&1 Internet AG
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

/*! \file
 *  \brief DB_MYSQL :: Connections
 *  \ingroup db_mysql
 *  Module: \ref db_mysql
 */


#include "km_my_con.h"
#include "km_db_mysql.h"
#include <mysql.h>

/* MariaDB exports MYSQL_VERSION_ID as well, but changed numbering scheme */
#if MYSQL_VERSION_ID > 80000 && !defined MARIADB_BASE_VERSION
#include <stdbool.h>
#endif

#include "../../core/mem/mem.h"
#include "../../core/dprint.h"
#include "../../core/ut.h"
#include "db_mysql.h"

extern int db_mysql_opt_ssl_mode;

/*! \brief
 * Create a new connection structure,
 * open the MySQL connection and set reference count to 1
 */
struct my_con *db_mysql_new_connection(const struct db_id *id)
{
	struct my_con *ptr;
	char *host, *grp, *egrp;
	unsigned int connection_flag = 0;

#if MYSQL_VERSION_ID > 50012
#if MYSQL_VERSION_ID > 80000 && !defined MARIADB_BASE_VERSION
	bool rec;
#else
	my_bool rec;
#endif
#endif

	if(!id) {
		LM_ERR("invalid parameter value\n");
		return 0;
	}

	ptr = (struct my_con *)pkg_malloc(sizeof(struct my_con));
	if(!ptr) {
		PKG_MEM_ERROR;
		return 0;
	}

	egrp = 0;
	memset(ptr, 0, sizeof(struct my_con));
	ptr->ref = 1;

	ptr->con = (MYSQL *)pkg_malloc(sizeof(MYSQL));
	if(!ptr->con) {
		PKG_MEM_ERROR;
		goto err;
	}

	mysql_init(ptr->con);

	if(id->host[0] == '[' && (egrp = strchr(id->host, ']')) != NULL) {
		grp = id->host + 1;
		*egrp = '\0';
		host = egrp;
		if(host != id->host + strlen(id->host) - 1) {
			host += 1; // host found after closing bracket
		} else {
			// let mysql read host info from my.cnf
			// (defaults to "localhost")
			host = NULL;
		}
		// read [client] and [<grp>] sections in the order
		// given in my.cnf
		mysql_options(ptr->con, MYSQL_READ_DEFAULT_GROUP, (const void *)grp);
	} else {
		host = id->host;
	}

	if(id->port) {
		LM_DBG("opening connection: mysql://xxxx:xxxx@%s:%d/%s\n", ZSW(host),
				id->port, ZSW(id->database));
	} else {
		LM_DBG("opening connection: mysql://xxxx:xxxx@%s/%s\n", ZSW(host),
				ZSW(id->database));
	}

	// set connect, read and write timeout, the value counts three times
	mysql_options(ptr->con, MYSQL_OPT_CONNECT_TIMEOUT,
			(const void *)&db_mysql_timeout_interval);
	mysql_options(ptr->con, MYSQL_OPT_READ_TIMEOUT,
			(const void *)&db_mysql_timeout_interval);
	mysql_options(ptr->con, MYSQL_OPT_WRITE_TIMEOUT,
			(const void *)&db_mysql_timeout_interval);
#if MYSQL_VERSION_ID > 50710 && !defined(MARIADB_BASE_VERSION)
	if(db_mysql_opt_ssl_mode != 0) {
		unsigned int optuint = 0;
		if(db_mysql_opt_ssl_mode == 1) {
			if(db_mysql_opt_ssl_mode != SSL_MODE_DISABLED) {
				LM_WARN("ssl mode disabled is not 1 (value %u) - enforcing\n",
						SSL_MODE_DISABLED);
			}
			optuint = SSL_MODE_DISABLED;
		} else {
			optuint = (unsigned int)db_mysql_opt_ssl_mode;
		}
		mysql_options(ptr->con, MYSQL_OPT_SSL_MODE, (const void *)&optuint);
	}
#else
	if(db_mysql_opt_ssl_mode != 0) {
		LM_WARN("ssl mode not supported by mysql version (value %u) - "
				"ignoring\n",
				(unsigned int)db_mysql_opt_ssl_mode);
	}
#endif

#if MYSQL_VERSION_ID > 50012
	/* set reconnect flag if enabled */
	if(db_mysql_auto_reconnect) {
		rec = 1;
		mysql_options(ptr->con, MYSQL_OPT_RECONNECT, (const void *)&rec);
	}
#else
	if(db_mysql_auto_reconnect)
		ptr->con->reconnect = 1;
	else
		ptr->con->reconnect = 0;
#endif

	if(db_mysql_update_affected_found) {
		connection_flag |= CLIENT_FOUND_ROWS;
	}

#if(MYSQL_VERSION_ID >= 40100)
	if(!mysql_real_connect(ptr->con, host, id->username, id->password,
			   id->database, id->port, 0,
			   connection_flag | CLIENT_MULTI_STATEMENTS)) {
#else
	if(!mysql_real_connect(ptr->con, host, id->username, id->password,
			   id->database, id->port, 0, connection_flag)) {
#endif
		LM_ERR("driver error: %s\n", mysql_error(ptr->con));
		/* increase error counter */
		counter_inc(mysql_cnts_h.driver_err);
		mysql_close(ptr->con);
		goto err;
	}

	LM_DBG("connection type is %s\n", mysql_get_host_info(ptr->con));
	LM_DBG("protocol version is %d\n", mysql_get_proto_info(ptr->con));
	LM_DBG("server version is %s\n", mysql_get_server_info(ptr->con));

	ptr->timestamp = time(0);
	ptr->id = (struct db_id *)id;
	if(egrp)
		*egrp = ']';
	return ptr;

err:
	if(ptr && ptr->con)
		pkg_free(ptr->con);
	if(ptr)
		pkg_free(ptr);
	if(egrp)
		*egrp = ']';
	return 0;
}


/*! \brief
 * Close the connection and release memory
 */
void db_mysql_free_connection(struct pool_con *con)
{
	struct my_con *_c;

	if(!con)
		return;

	_c = (struct my_con *)con;

	if(_c->id)
		free_db_id(_c->id);
	if(_c->con) {
		mysql_close(_c->con);
		pkg_free(_c->con);
	}
	pkg_free(_c);
}
