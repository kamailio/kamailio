#!/bin/sh
#
# $Id$
#
# SER dbtext Database Administration Tool
#
# TODO: Check if user and group exist
#
# Copyright (C) 2006-2008 iptelorg GmbH
#

#################################################################
# configuration variables
#################################################################
DEFAULT_ROOTDIR="/var/local/lib/ser" # Default dbtext root directory
DEFAULT_DBNAME="ser"                 # Default database name
DEFAULT_OWNER="ser"                  # The owner of dbtext files
DEFAULT_GROUP="ser"                  # The group of dbtext files
DEFAULT_SRCDB_DIR=""                 # Default directory of the source data
DEFAULT_SRCDB="ser_db"      # Source data generated from XML description


usage() {
cat <<EOF
NAME
  $COMMAND - SER dbtext Database Administration Tool

SYNOPSIS
  $COMMAND [options] create
  $COMMAND [options] drop
  $COMMAND [options] backup [filename.tar] 
  $COMMAND [options] restore [filename.tar]

DESCRIPTION
  This tool is a simple shell wrapper that can be used to create, drop, or
  backup SER database stored in plain-text files on the filesystem (used by
  dbtext SER module). See section COMMANDS for brief overview of supported
  actions.

  The database template for SER dbtext database is stored in dbtext_template
  directory which can usualy be found in /var/lib/ser (depending on
  installation). You can use the template to create SER database manually if
  you cannot or do not want to use this shell wrapper.

COMMANDS
  create
    Create a new SER database from scratch. The database must not exist.  This
    command creates the database, the default name of the database is
    '${DEFAULT_DBNAME}' (the default name can be changed using a command line
    parameter, see below). The database will be created in the default dbtext
    database directory (${DEFAULT_ROOTDIR}) unless changed using -d command
    line option (see below). You can use command line options to change the
    default database name, owner username and group.

  drop
    This command can be used to delete SER database. WARNING: This command
    will delete all data in the database and this action cannot be undone
    afterwards. Make sure that you have backups if you want to keep the data
    from the database.

  backup <filename>
    Backup the contents of SER database. If you specify a filename then the
    contents of the database will be saved in that file, otherwise the tool
    will dumps the contents on the standard output.

  restore <filename>
    Load the contents of SER database from a file (if you specify one) or from
    the standard input.

OPTIONS
  -h, --help
      Display this help text.

  -n NAME, --name=NAME
      Database name of SER database.
      (Default value is '$DEFAULT_DBNAME')

  -d DIR, --dir=DIR
      Root directory of dbtext databases.
      (Default value is '$DEFAULT_ROOTDIR')

  -o USERNAME, --owner=USERNAME
      Owner of files in the database.
      (Default value is '$DEFAULT_OWNER')

  -g GROUP, --group=GROUP
      Group of files in the database.
      (Default value is '$DEFAULT_GROUP')

  -v, --verbose
      Enable verbose mode. This option can be given multiple times
      to produce more and more output.
        
AUTHOR
  Written by Jan Janak <jan@iptel.org>

COPYRIGHT
  Copyright (C) 2006-2008 iptelorg GmbH
  This is free software. You may redistribute copies of it under the termp of
  the GNU General Public License. There is NO WARRANTY, to the extent
  permitted by law.

FILES
  ${SRCDB_DIR}/${SRCDB}
    
REPORTING BUGS
  Report bugs to <ser-bugs@iptel.org>             
EOF
} #usage

dbg()
{
    if [ ! -z $VERBOSE ] ; then
	echo $@
    fi
}

err()
{
    echo "ERROR: $@"
}


# Dump the contents of the database to stdout
backup_db() 
{
    if [ ! -z $VERBOSE ] ; then
	(cd "${ROOTDIR}/$1"; tar cfv - *)
    else
	(cd "${ROOTDIR}/$1"; tar cf - * 2>/dev/null)
    fi
}


# Re-create the contents of database from a tar file
restore_db()
{
    if [ ! -d "${ROOTDIR}/$1" ] ; then
	err "Database $1 does not exist, create it first"
	exit 1
    fi
    if [ ! -z $VERBOSE ] ; then
	(cd $ROOTDIR/$1; rm -f *; tar xfv -)
    else
	(cd $ROOTDIR/$1; rm -f *; tar xf -)
    fi
}


# Drop SER database
drop_db()
{
    dbg "Removing dbtext database directory ${ROOTDIR}/${DBNAME}"
    rm -rf "${ROOTDIR}/${DBNAME}"
} # drop_db


# Create SER database
create_db ()
{
    dbg "Creating database directory ${ROOTDIR}/${DBNAME}"
    mkdir -p "${ROOTDIR}/${DBNAME}"
    if [ ! -d ] ; then
	err "Could not create directory ${ROOTDIR}/${DBNAME}"
	exit 1
    fi
    
    dbg "Copying template files from ${SRCDB_DIR}/${SRCDB} to ${ROOTDIR}/${DBNAME}"
    cp -a ${SRCDB_DIR}/${SRCDB}/* "${ROOTDIR}/${DBNAME}"

    dbg "Setting owner and group of new files to ${OWNER}:${GROUP}"
    chown -R "${OWNER}:${GROUP}" "${ROOTDIR}/${DBNAME}"
    chmod -R 0660 ${ROOTDIR}/${DBNAME}/*
} # create_db


# Convert relative path to the script directory to absolute if necessary by
# extracting the directory of this script and prefixing the relative path with
# it.
abs_srcdb_dir()
{
  	my_dir=`dirname $0`;
  	if [ "${SRCDB_DIR:0:1}" != "/" ] ; then
  	    SCRIPT_DIR="${my_dir}/${SRCDB_DIR}"
  	fi
}


# Main program
COMMAND=`basename $0`

if [ -z "$DBNAME" ] ; then DBNAME="$DEFAULT_DBNAME"; fi;
if [ -z "$OWNER" ]  ; then OWNER="$DEFAULT_OWNER"; fi;
if [ -z "$GROUP" ]  ; then GROUP="$DEFAULT_GROUP"; fi;
if [ -z "$SRCDB_DIR" ] ; then SRCDB_DIR="$DEFAULT_SRCDB_DIR"; fi;
if [ -z "$SRCDB" ]  ; then SRCDB="$DEFAULT_SRCDB"; fi

abs_srcdb_dir

TEMP=`getopt -o hn:d:o:g:v --long help,name:,dir:,owner:,group:,verbose -n $COMMAND -- "$@"`
if [ $? != 0 ] ; then exit 1; fi
eval set -- "$TEMP"

while true ; do
    case "$1" in
	-h|--help)    usage; exit 0 ;;
	-n|--name)    DBNAME=$2; shift 2 ;;
        -d|--dir)     ROOTDIR=$2; shift 2 ;;
	-o|--owner)   OWNER=$2;  shift 2 ;;
	-g|--group)   GROUP=$2;  shift 2 ;;
        -v|--verbose) export OPTS="${OPTS} -v "; VERBOSE=1; shift ;;
	--)           shift; break ;;
	*)            echo "Internal error"; exit 1 ;;
    esac
done

if [ $# -eq 0 ]; then
    usage
    exit 1
fi

TEMP=`id $OWNER > /dev/null 2>&1`
if [ $? != 0 ] ; then
    err "User '$OWNER' does not exist"
    exit 1
fi

TEMP=`id -g $GROUP > /dev/null 2>&1`
if [ $? != 0 ] ; then
    err "Group '$GROUP' does not exist"
    exit 1
fi

case $1 in
    create) # Create SER database
	create_db
	exit $?
	;;
    
    drop) # Drop SER database
	drop_db
	exit $?
	;;

    backup) # backup SER database
	shift
	if [ $# -eq 1 ]; then
	    dbg "Backing up ${ROOTDIR}/$DBNAME"
	    backup_db $DBNAME > $1
	elif [ $# -eq 0 ]; then
	    dbg "Backing up ${ROOTDIR}/$DBNAME"
	    backup_db $DBNAME
	else
	    usage
	    exit 1
	fi
	exit $?
	;;

    restore) # restore SER database
	shift
	if [ $# -eq 1 ]; then
	    dbg "Restoring ${ROOTDIR}/$DBNAME"
	    cat $1 | restore_db $DBNAME
	elif [ $# -eq 0 ]; then
	    dbg "Restoring ${ROOTDIR}/$DBNAME"
	    restore_db $DBNAME
	else
	    usage
	    exit 1
	fi
	exit $?
	;;
        
    *)
	echo "Unknown command '$1'"
	usage
	exit 1;
	;;
esac
