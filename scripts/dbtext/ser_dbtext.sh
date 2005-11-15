#!/bin/sh
#
# $Id$
#
# Script for dbtext database maintenance
#
# TODO: Check if user and group exist
#

#################################################################
# config vars
#################################################################
DEFAULT_ROOTDIR="/var/local/lib/ser"
DEFAULT_DBNAME="ser"
DEFAULT_OWNER="ser"
DEFAULT_GROUP="ser"

DEFAULT_SRCDB="ser_db"

usage() {
cat <<EOF
Usage: $COMMAND create 
       $COMMAND drop   
       $COMMAND backup [database] <file.tar> 
       $COMMAND restore [database] <file.tar>
       $COMMAND copy <source_db> <dest_db>
   
  Command 'create' creates database named '${DBNAME}' containing tables 
  needed for SER and SERWeb. 

  Commmand 'drop' deletes database named '${DBNAME}'.

  Command 'backup' Dumps the contents of the database in <file>. If no
  database name is provided on the command line then the default '${DBNAME}'
  database will be used.

  Command 'restore' will load the datata previously saved with 'backup'
  command in the database. If no database name is provided on the command
  line then '${DBNAME}' database will be loaded.
    WARNING: Any existing data in the database being restored will be
             overwritten. Make sure that SER is not running when performing
	     restore command, otherwise it could corrupt the database !

  Command 'copy' will copy the contents of <source_db> to database <dest_db>.
  The destination database must not exist -- it will be created.

  Environment variables:
    ROOTDIR   The root directory of all dbtext databases (${ROOTDIR})
    DBNAME    Default name of SER database (${DBNAME})
    OWNER     The username of owner of all database files. This is usualy
              the username SER runs under.
    GROUP     THe group of all database files. This should be same as
              the group of SER processes.
           
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
    (cd $ROOTDIR/$1; tar cf - *) > $2
}


# Load the contents of the database from a file
db_load() #pars: <database name> <filename>
{
    if [ $# -ne 2 ] ; then
	echo "ERROR: Bug in $COMMAND"
	exit 1
    fi
    rm -rf $ROOTDIR/$1
    mkdir -p $ROOTDIR/$1
    chown $USER:$GROUP $ROOTDIR/$1
    tar xf $2 -C $ROOTDIR/$1 
}


# copy a database to database_bak
db_copy() # par: <source_database> <destination_database>
{
	if [ $# -ne 2 ] ; then
		echo  "ERROR: Bug in $COMMAND"
		exit 1
	fi
	cp -a $ROOTDIR/$1 $ROOTDIR/$2
}


# Drop SER database
ser_drop()
{
    # Drop dabase
    # Revoke user permissions

    echo "Dropping SER database"
    rm -rf $ROOTDIR/$DBNAME
} #ser_drop


# Create SER database
ser_create ()
{
    echo "Creating SER database"
    mkdir -p $ROOTDIR/$DBNAME
    cp -a $SRCDB/* $ROOTDIR/$DBNAME
    chown -R $OWNER:$GROUP $ROOTDIR/$DBNAME
    chmod -R 0660 $ROOTDIR/$DBNAME
} # ser_create



# Main program

COMMAND=`basename $0`

if [ -z "$ROOTDIR" ]; then
    ROOTDIR=$DEFAULT_ROOTDIR;
fi

if [ -z "$DBNAME" ]; then
    DBNAME=$DEFAULT_DBNAME;
fi

if [ -z "$OWNER" ]; then
    OWNER=$DEFAULT_OWNER;
fi

if [ -z "$GROUP" ]; then
    GROUP=$DEFAULT_GROUP;
fi

if [ -z "$SRCDB" ]; then
    SRCDB=$DEFAULT_SRCDB;
fi

if [ $# -eq 0 ]; then
    usage
    exit 1
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
