#
# Script for adding and dropping Kamailio Postgres tables
#
# This file is part of Kamailio, a free SIP server.
#
# SPDX-License-Identifier: GPL-2.0-or-later
#
# Kamailio is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version
#
# Kamailio is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
#

# path to the database schemas
DATA_DIR="/usr/local/share/kamailio"
if [ -d "$DATA_DIR/postgres" ] ; then
	DB_SCHEMA="$DATA_DIR/postgres"
else
	DB_SCHEMA="./postgres"
fi

#################################################################
# config vars
#################################################################

# full privileges Postgres user
if [ -z "$DBROOTUSER" ]; then
	DBROOTUSER="postgres"
	if [ ! -r ~/.pgpass ]; then
		merr "~/.pgpass does not exist"
		merr "create this file and add proper credentials for user postgres"
		merr "Note: you need at least postgresql>= 7.3"
		merr "Hint: .pgpass hostname must match DBHOST"
		exit 1
	fi
fi
if [ -z "$DBCLI" ] ; then
	DBCLI="psql"
fi

if [ -z "$DBROOTPORT" ] ; then
	CMD="$DBCLI $DBCLIPARAMS -q -h $DBROOTHOST -U $DBROOTUSER "
	DUMP_CMD="pg_dump -h $DBROOTHOST -U $DBROOTUSER -c"
else
	CMD="$DBCLI $DBCLIPARAMS -q -h $DBROOTHOST -p $DBROOTPORT -U $DBROOTUSER "
	DUMP_CMD="pg_dump -h $DBROOTHOST -p $DBROOTPORT -U $DBROOTUSER -c"
fi

#################################################################


# execute sql command with optional db name
sql_query()
{
	if [ $# -gt 1 ] ; then
		if [ -n "$1" ]; then
			DB="$1"
		else
			DB=""
		fi
		shift
		$CMD -d $DB -c "$@"
	else
		$CMD "$@"
	fi
}


kamailio_drop()  # pars: <database name>
{
if [ $# -ne 1 ] ; then
	merr "kamailio_drop function takes two params"
	exit 1
fi

sql_query "template1" "drop database \"$1\";"
if [ $? -ne 0 ] ; then
	merr "Dropping database $1 failed!"
	exit 1
fi

# postgresql users are not dropped automatically
sql_query "template1" "drop user \"$DBRWUSER\"; drop user \"$DBROUSER\";"

if [ $? -ne 0 ] ; then
	mwarn "Could not drop $DBRWUSER or $DBROUSER users, try to continue.."
else
	minfo "Database user deleted"
fi

minfo "Database $1 dropped"
} #kamailio_drop


kamailio_create () # pars: <database name>
{
if [ $# -ne 1 ] ; then
	merr "kamailio_create function takes one param"
	exit 1
fi

minfo "creating database $1 ..."

sql_query "template1" "create database \"$1\";"
if [ $? -ne 0 ] ; then
	merr "Creating database failed!"
	exit 1
fi

sql_query "$1" "CREATE FUNCTION "concat" (text,text) RETURNS text AS 'SELECT \$1 || \$2;' LANGUAGE 'sql';
	        CREATE FUNCTION "rand" () RETURNS double precision AS 'SELECT random();' LANGUAGE 'sql';"
# emulate mysql proprietary functions used by the lcr module in postgresql

if [ $? -ne 0 ] ; then
	merr "Creating mysql emulation functions failed!"
	exit 1
fi

for TABLE in $STANDARD_MODULES; do
    mdbg "Creating core table: $TABLE"
    sql_query "$1" < $DB_SCHEMA/$TABLE-create.sql
    if [ $? -ne 0 ] ; then
	merr "Creating core tables failed!"
	exit 1
    fi
done

sql_query "$1" "CREATE USER $DBRWUSER WITH PASSWORD '$DBRWPW';
		CREATE USER $DBROUSER WITH PASSWORD '$DBROPW';"
if [ $? -ne 0 ] ; then
	mwarn "Create user in database failed, perhaps they allready exist? Try to continue.."
fi

for TABLE in $STANDARD_TABLES; do
	sql_query "$1" "GRANT ALL PRIVILEGES ON TABLE $TABLE TO $DBRWUSER;"
	sql_query "$1" "GRANT SELECT ON TABLE $TABLE TO $DBROUSER;"
	if [ $TABLE != "version" ] ; then
		sql_query "$1" "GRANT ALL PRIVILEGES ON TABLE "$TABLE"_id_seq TO $DBRWUSER;"
    	sql_query "$1" "GRANT SELECT ON TABLE "$TABLE"_id_seq TO $DBROUSER;"
	fi

	if [ $? -ne 0 ] ; then
		merr "Grant privileges to standard tables failed!"
		exit 1
	fi
done

if [ -e $DB_SCHEMA/extensions-create.sql ]
then
	minfo "Creating custom extensions tables"
	sql_query $1 < $DB_SCHEMA/extensions-create.sql
	if [ $? -ne 0 ] ; then
	merr "Creating custom extensions tables failed!"
	exit 1
	fi
fi

minfo "Core Kamailio tables successfully created."

get_answer $INSTALL_PRESENCE_TABLES "Install presence related tables? (y/n): "
if [ "$ANSWER" = "y" ]; then
	presence_create $1
fi

get_answer $INSTALL_EXTRA_TABLES "Install tables for $EXTRA_MODULES? (y/n): "
if [ "$ANSWER" = "y" ]; then
	extra_create $1
fi
} # kamailio_create


presence_create () # pars: <database name>
{
if [ $# -ne 1 ] ; then
	merr "presence_create function takes one param"
	exit 1
fi

minfo "creating presence tables into $1 ..."

sql_query "$1" < $DB_SCHEMA/presence-create.sql

if [ $? -ne 0 ] ; then
	merr "Failed to create presence tables!"
	exit 1
fi

sql_query "$1" < $DB_SCHEMA/rls-create.sql

if [ $? -ne 0 ] ; then
	merr "Failed to create rls-presence tables!"
	exit 1
fi

for TABLE in $PRESENCE_TABLES; do
	sql_query "$1" "GRANT ALL PRIVILEGES ON TABLE $TABLE TO $DBRWUSER;"
	sql_query "$1" "GRANT SELECT ON TABLE $TABLE TO $DBROUSER;"
	sql_query "$1" "GRANT ALL PRIVILEGES ON TABLE "$TABLE"_id_seq TO $DBRWUSER;"
    sql_query "$1" "GRANT SELECT ON TABLE "$TABLE"_id_seq TO $DBROUSER;"
	if [ $? -ne 0 ] ; then
		merr "Grant privileges to presence tables failed!"
		exit 1
	fi
done

minfo "Presence tables successfully created."
}  # end presence_create


extra_create () # pars: <database name>
{
if [ $# -ne 1 ] ; then
	merr "extra_create function takes one param"
	exit 1
fi

minfo "creating extra tables into $1 ..."

for TABLE in $EXTRA_MODULES; do
    mdbg "Creating extra table: $TABLE"
    sql_query "$1" < $DB_SCHEMA/$TABLE-create.sql
    if [ $? -ne 0 ] ; then
	merr "Creating extra tables failed!"
	exit 1
    fi
done

for TABLE in $EXTRA_TABLES; do
	sql_query "$1" "GRANT ALL PRIVILEGES ON TABLE $TABLE TO $DBRWUSER;"
	sql_query "$1" "GRANT SELECT ON TABLE $TABLE TO $DBROUSER;"
	if [ $TABLE != "route_tree" ] && [ $TABLE != "dr_gateways" ] && [ $TABLE != "dr_rules" ] ; then
		sql_query "$1" "GRANT ALL PRIVILEGES ON TABLE "$TABLE"_id_seq TO $DBRWUSER;"
		sql_query "$1" "GRANT SELECT ON TABLE "$TABLE"_id_seq TO $DBROUSER;"
	fi
	if [ $? -ne 0 ] ; then
		merr "Grant privileges to extra tables failed!"
		exit 1
	fi
done

minfo "Extra tables successfully created."
}  # end extra_create


dbuid_create () # pars: <database name>
{
	if [ $# -ne 1 ] ; then
		merr "dbuid_create function takes one param"
		exit 1
	fi

	minfo "creating uid tables into $1 ..."

	for TABLE in $DBUID_MODULES; do
		mdbg "Creating uid table: $TABLE"
		sql_query $1 < $DB_SCHEMA/$TABLE-create.sql
		if [ $? -ne 0 ] ; then
			merr "Creating uid tables failed at $TABLE!"
			exit 1
			fi
		done
	minfo "UID tables successfully created."
}  # end uid_create
