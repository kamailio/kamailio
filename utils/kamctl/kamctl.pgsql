#
#
# control tool for maintaining Kamailio
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
	if [ -z "$DBCLI" ] ; then
		locate_tool psql
		if [ -z "$TOOLPATH" ] ; then
			echo "error: 'psql' tool not found: set PGSQL variable to correct tool path"
			exit
		fi
		PGSQL="$TOOLPATH"
	else
		PGSQL="$DBCLI"
	fi
fi


# input: sql query, optional pgsql command-line params
pgsql_query() {
	# if password not yet queried, query it now
	prompt_pw "PgSQL password for user '$DBRWUSER@$DBHOST'"
	mecho "pgsql_query: $PGSQL $2 -A -q -t -P fieldsep='	' -h $DBHOST -U $DBRWUSER $DBNAME -c '$1'"
	if [ -z "$DBPORT" ] ; then
		PGPASSWORD="$DBRWPW" $PGSQL $DBCLIPARAMS $2 \
			-A -q -t \
			-P fieldsep="	" \
			-h $DBHOST \
			-U $DBRWUSER \
			$DBNAME \
			-c "$1"
	else
		PGPASSWORD="$DBRWPW" $PGSQL $DBCLIPARAMS $2 \
			-A -q -t \
			-P fieldsep="	" \
			-h $DBHOST \
			-p $DBPORT \
			-U $DBRWUSER \
			$DBNAME \
			-c "$1"
	fi
}

# input: sql query, optional pgsql command-line params
pgsql_ro_query() {
	mdbg "pgsql_ro_query: $PGSQL $2 -A -q -t -h $DBHOST -U $DBROUSER $DBNAME -c '$1'"
	if [ -z "$DBPORT" ] ; then
		PGPASSWORD="$DBROPW" $PGSQL $DBCLIPARAMS $2 \
			-A -q -t \
			-h $DBHOST \
			-U $DBROUSER \
			$DBNAME \
			-c "$1"
	else
		PGPASSWORD="$DBROPW" $PGSQL $DBCLIPARAMS $2 \
			-A -q -t \
			-h $DBHOST \
			-p $DBPORT \
			-U $DBROUSER \
			$DBNAME \
			-c "$1"
	fi
}

DBCMD=pgsql_query
DBROCMD=pgsql_ro_query
DBRAWPARAMS="-A -q -t"
