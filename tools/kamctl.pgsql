#
# $Id$
#
# control tool for maintaining Kamailio
#
#===================================================================

##### ----------------------------------------------- #####
### PGSQL specific variables and functions
#

##### ----------------------------------------------- #####
### load SQL base
#
if [ -f "$MYLIBDIR/kamctl.sqlbase" ]; then
	. "$MYLIBDIR/kamctl.sqlbase"
else
	echo "Cannot load SQL core functions '$MYLIBDIR/kamctl.sqlbase' - exiting ..."
	exit -1
fi

##### ----------------------------------------------- #####
### binaries
if [ -z "$PGSQL" ] ; then
	locate_tool psql
	if [ -z "$TOOLPATH" ] ; then
		echo "error: 'psql' tool not found: set PGSQL variable to correct tool path"
		exit
	fi
	PGSQL="$TOOLPATH"
fi


# input: sql query, optional pgsql command-line params
pgsql_query() {
	# if password not yet queried, query it now
	prompt_pw "PgSQL password for user '$DBRWUSER@$DBHOST'"
	mecho "pgsql_query: $PGSQL $2 -A -q -t -P fieldsep='	' -h $DBHOST -U $DBRWUSER $DBNAME -c '$1'"
	PGPASSWORD="$DBRWPW" $PGSQL $2 \
		-A -q -t \
		-P fieldsep="	" \
		-h $DBHOST \
		-U $DBRWUSER \
		$DBNAME \
		-c "$1"
}

# input: sql query, optional pgsql command-line params
pgsql_ro_query() {
	mdbg "pgsql_ro_query: $PGSQL $2 -h $DBHOST -U $DBROUSER $DBNAME -c '$1'"
	PGPASSWORD="$DBROPW" $PGSQL $2 \
		-h $DBHOST \
		-U $DBROUSER \
		$DBNAME \
		-c "$1"
}

DBCMD=pgsql_query
DBROCMD=pgsql_ro_query
DBRAWPARAMS="-A -q -t"

