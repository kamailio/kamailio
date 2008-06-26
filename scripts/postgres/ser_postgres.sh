#!/bin/sh
#
# $Id$
#
# Script for adding and dropping SER Postgres tables
#

#################################################################
# config vars
#################################################################
DEFAULT_DBNAME="ser"
DEFAULT_SQLUSER="postgres"

DEFAULT_SCRIPT_DIR=""

DEFAULT_PSQL="/usr/bin/psql"
DEFAULT_PG_DUMP="/usr/bin/pg_dump"

DEFAULT_CREATE_SCRIPT="pg_create.sql"
DEFAULT_DATA_SCRIPT="pg_data.sql"
DEFAULT_DROP_SCRIPT="pg_drop.sql"

#DBHOST="localhost"

usage() {
cat <<EOF
Usage: $COMMAND create  [database]
       $COMMAND drop    [database]
       $COMMAND backup  [database] <file> 
       $COMMAND restore [database] <file>

  Command 'create' creates database named '${DBNAME}' containing tables needed
  for SER and SERWeb. In addition to that two users are created, one with
  read/write permissions and one with read-only permissions.

  Commmand 'drop' deletes database named '${DBNAME}' and associated users.

  Command 'backup' Dumps the contents of the database in <file>. If no
  database name is provided on the command line then the default '${DBNAME}'
  database will be used.

  Command 'restore' will load the datata previously saved with 'backup'
  command in the database. If no database name is provided on the command
  line then '${DBNAME}' database will be loaded.
    Note: Make sure that you have no conflicting data in the database before
          you execute 'restore' command.

  Environment variables:
    DBHOST    Hostname of the Postgres server (${DBHOST})
    DBNAME    Default name of SER database (${DBNAME})
    SQLUSER   Database username with administrator privileges (${SQLUSER})
              (Make sure that the specified user has sufficient permissions
               to create databases, tables, and users)
    PSQL      Full path to mysql command (${PSQL})
           
Report bugs to <ser-bugs@iptel.org>
EOF
} #usage


# Dump the contents of the database to stdout
db_save() 
{
    if [ $# -ne 2 ] ; then
	echo "ERROR: Bug in $COMMAND"
	exit 1
    fi
    $DUMP_CMD $1 > $2
}


# Load the contents of the database from a file
db_load() #pars: <database name> <filename>
{
    if [ $# -ne 2 ] ; then
	echo "ERROR: Bug in $COMMAND"
	exit 1
    fi
    echo "CREATE DATABASE $1" | $CMD "template1"
    $CMD $1 < $2
}


# Drop SER database
db_drop()
{
    # Drop dabase
    # Revoke user permissions

    echo "Dropping database $1"
    $CMD "template1" < ${SCRIPT_DIR}/${DROP_SCRIPT}
    echo "DROP DATABASE $1" | $CMD "template1" 
}


# Create SER database
db_create ()
{
    echo "Creating database $1"
    echo "CREATE DATABASE $1" | $CMD "template1"
    $CMD $1 < ${SCRIPT_DIR}/${CREATE_SCRIPT}
    $CMD $1 < ${SCRIPT_DIR}/${DATA_SCRIPT}
}


# Convert relative path to the script directory to absolute if necessary by
# extracting the directory of this script and prefixing the relative path with
# it.
abs_script_dir()
{
  	my_dir=`dirname $0`;
  	if [ "${SCRIPT_DIR:0:1}" != "/" ] ; then
  	    SCRIPT_DIR="${my_dir}/${SCRIPT_DIR}"
  	fi
}


# Main program

COMMAND=`basename $0`

if [ ! -z "$DBHOST" ]; then
    DBHOST="-h ${DBHOST}"
fi

if [ -z "$DBNAME" ]; then
    DBNAME=$DEFAULT_DBNAME;
fi

if [ -z "$SQLUSER" ]; then
    SQLUSER=$DEFAULT_SQLUSER;
fi

if [ -z "$PSQL" ]; then
    PSQL=$DEFAULT_PSQL;
fi

if [ -z "$PG_DUMP" ]; then
    PG_DUMP=$DEFAULT_PG_DUMP;
fi  

if [ -z "$CREATE_SCRIPT" ]; then
    CREATE_SCRIPT=$DEFAULT_CREATE_SCRIPT;
fi

if [ -z "$DATA_SCRIPT" ]; then
    DATA_SCRIPT=$DEFAULT_DATA_SCRIPT;
fi

if [ -z "$DROP_SCRIPT" ]; then
    DROP_SCRIPT=$DEFAULT_DROP_SCRIPT;
fi

if [ -z "$SCRIPT_DIR" ]; then
	SCRIPT_DIR=$DEFAULT_SCRIPT_DIR;
fi

if [ $# -eq 0 ]; then
    usage
    exit 1
fi

if [ ! -x $PSQL ]; then
    echo "ERROR: Could not execute Postgres tool $PSQL, please set PSQL variable"
    echo "       Run ($COMMAND without parameters for more information)"
    exit 1
fi

CMD="$PSQL ${DBHOST} -U $SQLUSER"
DUMP_CMD="$PG_DUMP ${DBHOST} -U $SQLUSER"

abs_script_dir

case $1 in
    create) # Create SER database and users
	shift
	if [ $# -eq 1 ]; then
	    db_create $1
        elif [ $# -eq 0 ]; then
	    db_create ${DBNAME}
        else
	    usage
	    exit 1
	fi
	exit $?
	;;
    
    drop) # Drop SER database and users
	shift
	if [ $# -eq 1 ]; then
	    db_drop $1
        elif [ $# -eq 0 ]; then
	    db_drop ${DBNAME}
        else
	    usage
	    exit 1
	fi
	exit $?
	;;

    backup) # backup SER database
	shift
	if [ $# -eq 1 ]; then
	    db_save ${DBNAME} $1
	elif [ $# -eq 2 ]; then
	    db_save $1 $2
	else
	    usage
	    exit 1
	fi
	exit $?
	;;

    restore) # restore SER database
	shift
	if [ $# -eq 1 ]; then
	    db_load ${DBNAME} $1
	elif [ $# -eq 2 ]; then
	    db_load $1 $2
	else
	    usage
	    exit 1
	fi
	exit $?
	;;

    *)
	usage
	exit 1;
	;;
esac
