#!/bin/sh
#
# $Id$
#
# Script for adding and dropping ser MySQL tables
#

#################################################################
# config vars
#################################################################
DEFAULT_DBHOST="localhost"
DEFAULT_DBNAME="ser"
DEFAULT_SQLUSER="root"

DEFAULT_MYSQL="/usr/bin/mysql"
DEFAULT_MYSQLDUMP="/usr/bin/mysqldump"

DEFAULT_CREATE_SCRIPT="my_create.sql"
DEFAULT_DROP_SCRIPT="my_drop.sql"

CMD="$MYSQL -f -h$DBHOST -u$SQLUSER"

usage() {
cat <<EOF
Usage: $COMMAND create 
       $COMMAND drop   
       $COMMAND backup [database] <file> 
       $COMMAND restore [database] <file>
       $COMMAND copy <source_db> <dest_db>
   
  Command 'create' creates database named '${DBNAME}' containing tables 
  needed for SER and SERWeb. In addition to that two users are created, 
  one with read/write permissions and one with read-only permissions.

  Commmand 'drop' deletes database named '${DBNAME}' and associated users.

  Command 'backup' Dumps the contents of the database in <file>. If no
  database name is provided on the command line then the default '${DBNAME}'
  database will be used.

  Command 'restore' will load the datata previously saved with 'backup'
  command in the database. If no database name is provided on the command
  line then '${DBNAME}' database will be loaded.
    Note: Make sure that you have no conflicting data in the database before
          you execute 'restore' command.

  Command 'copy' will copy the contents of <source_db> to database <dest_db>.
  The destination database must not exist -- it will be created.
    Note: The default users (ser, serro) will not have sufficient permissions
          to access the new database.

  Environment variables:
    DBHOST    Hostname of the MySQL server (${DBHOST})
    DBNAME    Default name of SER database (${DBNAME})
    SQLUSER   Database username with administrator privileges (${SQLUSER})
              (Make sure that the specified user has sufficient permissions
               to create databases, tables, and users)
    MYSQL     Full path to mysql command (${MYSQL})
    MYSQLDUMP Full path to mysqldump command (${MYSQLDUMP})
           
Report bugs to <ser-bugs@iptel.org>
EOF
} #usage


# read password
prompt_pw()
{
	savetty=`stty -g`
	printf "Enter password for MySQL user ${SQLUSER} (hit enter for no password): "
	stty -echo
	read PW
	stty $savetty
	echo
}

# execute sql command
sql_query()
{
    $CMD $PW "$@"
}


# Dump the contents of the database to stdout
db_save() 
{
    if [ $# -ne 2 ] ; then
	echo "ERROR: Bug in $COMMAND"
	exit 1
    fi
    $DUMP_CMD -t $PW $1 > $2
}


# Load the contents of the database from a file
db_load() #pars: <database name> <filename>
{
    if [ $# -ne 2 ] ; then
	echo "ERROR: Bug in $COMMAND"
	exit 1
    fi
    sql_query $1 < $2
}


# copy a database to database_bak
db_copy() # par: <source_database> <destination_database>
{
	if [ $# -ne 2 ] ; then
		echo  "ERROR: Bug in $COMMAND"
		exit 1
	fi

	BU=/tmp/mysql_bup.$$
	$DUMP_CMD $PW $1 > $BU
	if [ "$?" -ne 0 ] ; then
		echo "ERROR: Failed to copy the source database"
		exit 1
	fi
	sql_query <<EOF
	CREATE DATABASE $2;
EOF

	db_load $2 $BU
	if [ "$?" -ne 0 ]; then
		echo "ERROR: Failed to create the destination database (database exists or insuffucient permissions)"
		rm -f $BU
		exit 1
	fi
}


# Drop SER database
ser_drop()
{
    # Drop dabase
    # Revoke user permissions

    echo "Dropping SER database"
    sql_query < $DROP_SCRIPT
} #ser_drop


# Create SER database
ser_create ()
{
    echo "Creating SER database"
    sql_query < $CREATE_SCRIPT
} # ser_create



# Main program

COMMAND=`basename $0`

if [ -z "$DBHOST" ]; then
    DBHOST=$DEFAULT_DBHOST;
fi

if [ -z "$DBNAME" ]; then
    DBNAME=$DEFAULT_DBNAME;
fi

if [ -z "$SQLUSER" ]; then
    SQLUSER=$DEFAULT_SQLUSER;
fi

if [ -z "$MYSQL" ]; then
    MYSQL=$DEFAULT_MYSQL;
fi

if [ -z "$MYSQLDUMP" ]; then
    MYSQLDUMP=$DEFAULT_MYSQLDUMP;
fi

if [ -z "$CREATE_SCRIPT" ]; then
    CREATE_SCRIPT=$DEFAULT_CREATE_SCRIPT;
fi

if [ -z "$DROP_SCRIPT" ]; then
    DROP_SCRIPT=$DEFAULT_DROP_SCRIPT;
fi

if [ $# -eq 0 ]; then
    usage
    exit 1
fi

if [ ! -x $MYSQL ]; then
    echo "ERROR: Could not execute MySQL tool $MYSQL, please set MYSQL variable"
    echo "       Run ($COMMAND without parameters for more information)"
    exit 1
fi

if [ ! -x $MYSQLDUMP ]; then
    echo "ERROR: Could not execute MySQL tool $MYSQLDUMP, please set MYSQLDUMP variable"
    echo "       (Run $COMMAND without parameters for more information"
    exit 1
fi

CMD="$MYSQL -h$DBHOST -u$SQLUSER"
DUMP_CMD="${MYSQLDUMP} -h$DBHOST -u$SQLUSER -c -a -e --add-locks --all"

export PW
prompt_pw

if [ -z "$PW" ]; then
    unset PW
else
    PW="-p$PW"
fi

case $1 in
    create) # Create SER database and users
	ser_create
	exit $?
		;;
    
    drop) # Drop SER database and users
	ser_drop
	exit $?
	;;

    backup) # backup SER database
	shift
	if [ $# -eq 1 ]; then
	    db_save $DBNAME $1
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
	    db_load $DBNAME $1
	elif [ $# -eq 2 ]; then
	    db_load $1 $2
	else
	    usage
	    exit 1
	fi
	exit $?
	;;
        
    copy)
	shift
	if [ $# -ne 2 ]; then
	    usage
	    exit 1
	fi
	db_copy $1 $2
	exit $?
	;;
    
    *)
	usage
	exit 1;
	;;
esac
