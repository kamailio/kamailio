# $Id$
#
# Script for adding and dropping Kamailio Postgres tables
#
# History:
# 2006-05-16  added ability to specify MD5 from a configuration file
#             FreeBSD does not have the md5sum function (norm)
# 2006-07-14  Corrected syntax from MySQL to Postgres (norm)
#             moved INDEX creation out of CREATE table statement into 
#                  CREATE INDEX (usr_preferences, trusted)
#             auto_increment isn't valid in Postgres, replaced with 
#                  local AUTO_INCREMENT
#             datetime isn't valid in Postgres, replaced with local DATETIME 
#             split GRANTs for SERWeb tables so that it is only executed 
#                  if SERWeb tables are created
#             added GRANTs for re_grp table
#             added CREATE pdt table (from PDT module)
#             corrected comments to indicate Postgres as opposed to MySQL
#             made last_modified/created stamps consistent to now() using 
#                  local TIMESTAMP
# 2006-10-19  Added address table (bogdan)
# 2006-10-27  subscriber table cleanup; some columns are created only if
#             serweb is installed (bogdan)
# 2007-01-26  added seperate installation routine for presence related tables
#             and fix permissions for the SERIAL sequences.
# 2007-05-21  Move SQL database definitions out of this script (henning)
# 2007-05-31  Move common definitions to kamdbctl.base file (henningw)
#
# 2007-06-11  Use a common control tool for database tasks, like the kamctl

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
		merr "~./pgpass does not exist, please create this file and support proper credentials for user postgres."
		merr "Note: you need at least postgresql>= 7.3"
		exit 1
	fi
fi

CMD="psql -q -h $DBHOST -U $DBROOTUSER "
DUMP_CMD="pg_dump -h $DBHOST -U $DBROOTUSER -c"
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

minfo "Core Kamailio tables succesfully created."

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

minfo "Presence tables succesfully created."
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
	if [ $TABLE != "route_tree" ] ; then
		sql_query "$1" "GRANT ALL PRIVILEGES ON TABLE "$TABLE"_id_seq TO $DBRWUSER;"
	    sql_query "$1" "GRANT SELECT ON TABLE "$TABLE"_id_seq TO $DBROUSER;"
	fi
	if [ $? -ne 0 ] ; then
		merr "Grant privileges to extra tables failed!"
		exit 1
	fi
done

minfo "Extra tables succesfully created."
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
	minfo "UID tables succesfully created."
}  # end uid_create
