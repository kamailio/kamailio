#!/bin/sh
#
# $Id$
#
# Script for adding and dropping OpenSER Postgres tables
#
# TO-DO: update_structures command for migriting to new
#        table definitons
# USAGE: call the command without any parameters for info
#
# 2003-01-21 changed SILO table definition, by dcm
#
# History:
# 2003-03-12 added replication mark and state columns to location (nils)
# 2003-03-05: Changed user to username, user is reserved word (janakj)
# 2003-01-26 statistics table introduced (jiri)
# 2003-01-25: Optimized keys of some core tables (janakj)
# 2003-01-25: USER_ID changed to user everywhere (janakj)
# 2003-01-24: Changed realm column of subscriber and pending
#             tables to domain (janakj)
# 2003-04-14  reinstall introduced (jiri)
# 2004-07-05  new definition of table silo (dcm)
# 2005-07-26  modify mysqldb.sh for postgres (darilion), known issues:
#  -  int unsigned replaced by bigint
#  -  postgresql creates some implicit indexes, thus some of the
#     indexes are doubled
#  -  msilo: blob replaced by text, is this fine?
#  -  datetime types not sure
# 2006-04-07  removed gen_ha1 dependency - use md5sum;
#             separated the serweb from openser tables (bogdan)
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
#
# 2007-05-21  Move SQL database definitions out of this script (henning)



PATH=$PATH:/usr/local/sbin

# include resource files, if any
if [ -f /etc/openser/.openserpostgresqlrc ]; then
	. /etc/openser/.openserpostgresqlrc
fi
if [ -f /usr/local/etc/openser/.openserpostgresqlrc ]; then
	. /usr/local/etc/openser/.openserpostgresqlrc
fi
if [ -f ~/.openserpostgresqlrc ]; then
	. ~/.openserpostgresqlrc
fi

# path to the database schemas
DATA_DIR="/usr/local/share/openser"
if [ -d "$DATA_DIR/postgres" ] ; then
	DB_SCHEMA="$DATA_DIR/postgres"
else
	DB_SCHEMA="./postgres"
fi

#################################################################
# config vars
#################################################################
# name of the database to be used by SER
if [ -z "$DBNAME" ]; then
	DBNAME="openser"
fi
# address of Postgres server
if [ -z "$DBHOST" ]; then
	DBHOST="localhost"
fi
# user with full privileges over DBNAME database
if [ -z "$DBRWUSER" ]; then
	DBRWUSER="openser"
fi
# password user with full privileges over DBNAME database
if [ -z "$DBRWPW" ]; then
	DBRWPW="openserrw"
fi
# read-only user
if [ -z "$DBROUSER" ]; then
	DBROUSER="openserro"
fi
# password for read-only user
if [ -z "$DBROPW" ]; then
	DBROPW="openserro"
fi
# full privileges Postgres user
if [ -z "$DBROOTUSER" ]; then
	DBROOTUSER="postgres"
	if [ ! -r ~/.pgpass ]; then
		echo "~./pgpass does not exist, please create this file and support proper credentials for user postgres."
		echo "Note: you need at least postgresql>= 7.3"
		exit 1
	fi
fi

CMD="psql -h $DBHOST -U $DBROOTUSER "
# the following commands are untested:
#   DUMP_CMD="pg_dump -h $DBHOST -u$DBROOTUSER -c -t "
#   BACKUP_CMD="mysqldump -h $DBHOST -u$DBROOTUSER -c "

# type of sql tables
if [ -z "$TABLE_TYPE" ]; then
	TABLE_TYPE=""
fi
# user name column
if [ -z "$USERCOL" ]; then
	USERCOL="username"
fi

# Program to calculate a message-digest fingerprint 
if [ -z "$MD5" ]; then
	MD5="md5sum"
fi
if [ -z "$AWK" ]; then
	AWK="awk"
fi

# if you change this definitions here, then you must change them 
# in db/schema/entities.xml too.
# FIXME
DUMMY_DATE="1900-01-01 00:00:01"
FOREVER="2020-05-28 21:32:15"

DEFAULT_ALIASES_EXPIRES=$FOREVER
DEFAULT_Q="1.0"
DEFAULT_CALLID="Default-Call-ID"
DEFAULT_CSEQ="42"
DEFAULT_LOCATION_EXPIRES=$FOREVER

#################################################################


usage() {
COMMAND=`basename $0`
cat <<EOF
usage: $COMMAND create
       $COMMAND drop   (!!entirely deletes tables)
       $COMMAND reinit (!!entirely deletes and than re-creates tables
       $COMMAND presence (adds the presence related tables)
       $COMMAND extra (adds the extra tables - imc,cpl,siptrace,domainpolicy)
       $COMMAND serweb (adds the SERWEB specific tables)

 NOTE: the following commands are not tested with postgresql,
 thus they are disabled.
       $COMMAND backup (dumps current database to stdout)
       $COMMAND restore <file> (restores tables from a file)
       $COMMAND copy <new_db> (creates a new db from an existing one)
       $COMMAND reinstall (updates to a new OpenSER database)

       if you want to manipulate database as other postgresql user than
       "postgres", want to change database name from default value "$DBNAME",
       or want to use other values for users and password, edit the
       "config vars" section of the command $COMMAND

EOF
} #usage


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


# dump all rows
openser_dump()  # pars: <database name>
{
	if [ $# -ne 1 ] ; then
		echo "openser_dump function takes one param"
		exit 1
	fi
	$DUMP_CMD "-p$PW" $1
}


# copy a database to database_bak
openser_backup() # par: <database name>
{
	if [ $# -ne 1 ] ; then
		echo  "openser_backup function takes one param"
		exit 1
	fi
	BU=/tmp/mysql_bup.$$
	$BACKUP_CMD "-p$PW" $1 > $BU
	if [ "$?" -ne 0 ] ; then
		echo "ser backup dump failed"
		exit 1
	fi
	sql_query <<EOF
	create database $1_bak;
EOF

	openser_restore $1_bak $BU
	if [ "$?" -ne 0 ]; then
		echo "ser backup/restore failed"
		rm $BU
		exit 1
	fi
}

openser_restore() #pars: <database name> <filename>
{
if [ $# -ne 2 ] ; then
	echo "openser_restore function takes two params"
	exit 1
fi
sql_query $1 < $2
}


openser_drop()  # pars: <database name>
{
	if [ $# -ne 1 ] ; then
		echo "openser_drop function takes two params"
		exit 1
	fi

	# postgresql users are not dropped automatically
	DROP_USER="DROP USER \"$DBRWUSER\";
	           DROP USER \"$DBROUSER\";"

	sql_query << EOF
	drop database $1;
	$DROP_USER
EOF
} #openser_drop

# read realm
prompt_realm()
{
	printf "Domain (realm) for the default user 'admin': "
	read SIP_DOMAIN
	echo
}

# calculate credentials for admin
credentials()
{
	HA1=`echo -n "admin:$SIP_DOMAIN:$DBRWPW" | $MD5 | $AWK '{ print $1 }'`
	if [ $? -ne 0 ] ; then
		echo "HA1 calculation failed"
		exit 1
	fi
	HA1B=`echo -n "admin@$SIP_DOMAIN:$SIP_DOMAIN:$DBRWPW" | $MD5 | $AWK '{ print $1 }'`
	if [ $? -ne 0 ] ; then
		echo "HA1B calculation failed"
		exit 1
	fi

	#PHPLIB_ID of users should be difficulty to guess for security reasons
	NOW=`date`;
	PHPLIB_ID=`echo -n "$RANDOM:$NOW:$SIP_DOMAIN" | $MD5 | $AWK '{ print $1 }'`
	if [ $? -ne 0 ] ; then
		echo "PHPLIB_ID calculation failed"
		exit 1
	fi
}

openser_create () # pars: <database name>
{
if [ $# -ne 1 ] ; then
	echo "openser_create function takes one param"
	exit 1
fi

# define what modules should be installed
# openser standard modules
STANDARD_MODULES="standard acc lcr domain group permissions
                  registrar usrloc msilo alias_db uri_db
                  speeddial avpops auth_db pdt"

# openser extra modules
EXTRA_MODULES="imc cpl siptrace domainpolicy"

echo "creating database $1 ..."

sql_query "template1" "create database $1;"
if [ $? -ne 0 ] ; then
	echo "Creating database failed!"
	exit 1
fi

sql_query "$1" "CREATE FUNCTION "concat" (text,text) RETURNS text AS 'SELECT \$1 || \$2;' LANGUAGE 'sql';
	        CREATE FUNCTION "rand" () RETURNS double precision AS 'SELECT random();' LANGUAGE 'sql';"
# emulate mysql proprietary functions used by the lcr module in postgresql

if [ $? -ne 0 ] ; then
	echo "Creating mysql emulation functions failed!"
	exit 1
fi

for TABLE in $STANDARD_MODULES; do
    echo "Creating core table: $TABLE"
    sql_query "$1" < $DB_SCHEMA/$TABLE-create.sql
    if [ $? -ne 0 ] ; then
	echo "Creating core tables failed!"
	exit 1
    fi
done

sql_query "$1" "CREATE USER $DBRWUSER WITH PASSWORD '$DBRWPW';
		CREATE USER $DBROUSER WITH PASSWORD '$DBROPW';
		GRANT ALL PRIVILEGES ON TABLE version, acc, acc_id_seq, address, address_id_seq, 
		aliases, aliases_id_seq, dbaliases, dbaliases_id_seq, domain, domain_id_seq, 
		grp, grp_id_seq, gw, gw_id_seq, gw_grp, gw_grp_grp_id_seq, lcr, lcr_id_seq, 
		location, location_id_seq, missed_calls, missed_calls_id_seq, pdt, pdt_id_seq, 
		re_grp, re_grp_id_seq, silo, silo_id_seq, speed_dial, speed_dial_id_seq, 
		subscriber, subscriber_id_seq, trusted, trusted_id_seq, uri, uri_id_seq, 
		usr_preferences, usr_preferences_id_seq TO $DBRWUSER;
		GRANT SELECT ON TABLE version, acc, address, aliases, dbaliases, domain, grp, 
		gw, gw_grp, lcr, location, missed_calls, pdt, re_grp, silo, speed_dial, 
		subscriber, trusted, uri, usr_preferences TO $DBROUSER;"

if [ $? -ne 0 ] ; then
	echo "Grant privileges to database failed!"
	exit 1
fi

echo "Core OpenSER tables succesfully created."

echo -n "Install presence related tables ?(y/n):"
read INPUT
if [ "$INPUT" = "y" ] || [ "$INPUT" = "Y" ]
then
	presence_create $1
fi

echo -n "Install extra tables - imc,cpl,siptrace,domainpolicy ?(y/n):"
read INPUT
if [ "$INPUT" = "y" ] || [ "$INPUT" = "Y" ]
then
	extra_create $1
fi

echo -n "Install SERWEB related tables ?(y/n):"
read INPUT
if [ "$INPUT" = "y" ] || [ "$INPUT" = "Y" ]
then
	serweb_create $1
fi
} # openser_create


presence_create () # pars: <database name>
{
if [ $# -ne 1 ] ; then
	echo "presence_create function takes one param"
	exit 1
fi

echo "creating presence tables into $1 ..."

sql_query "$1" < $DB_SCHEMA/presence-create.sql

if [ $? -ne 0 ] ; then
	echo "Failed to create presence tables!"
	exit 1
fi

sql_query "$1" "GRANT ALL PRIVILEGES ON TABLE 	active_watchers, active_watchers_id_seq,
		presentity, presentity_id_seq, watchers, watchers_id_seq, xcap_xml,
		xcap_xml_id_seq, pua, pua_id_seq TO $DBRWUSER;
		GRANT SELECT ON TABLE active_watchers, presentity, watchers, xcap_xml,
		pua TO $DBROUSER;"

if [ $? -ne 0 ] ; then
	echo "Grant privileges to presences tables failed!"
	exit 1
fi

echo "Presence tables succesfully created."
}  # end presence_create


extra_create () # pars: <database name>
{
if [ $# -ne 1 ] ; then
	echo "extra_create function takes one param"
	exit 1
fi

for TABLE in $EXTRA_MODULES; do
    echo "Creating extra table: $TABLE"
    sql_query "$1" < $DB_SCHEMA/$TABLE-create.sql
    if [ $? -ne 0 ] ; then
	echo "Creating extra tables failed!"
	exit 1
    fi
done

sql_query "$1" "GRANT ALL PRIVILEGES ON TABLE cpl, cpl_id_seq, imc_members,
		imc_members_id_seq, imc_rooms, imc_rooms_id_seq, sip_trace, 
		sip_trace_id_seq, domainpolicy, domainpolicy_id_seq
		TO $DBRWUSER;
		GRANT SELECT ON TABLE cpl, imc_members, imc_rooms, sip_trace,
		domainpolicy TO $DBROUSER;"

if [ $? -ne 0 ] ; then
	echo "Grant privileges to extra tables failed!"
	exit 1
fi


echo "creating extra tables into $1 ..."


if [ $? -ne 0 ] ; then
	echo "Failed to create extra tables!"
	exit 1
fi

echo "Extra tables succesfully created."
}  # end extra_create


serweb_create () # pars: <database name>
{
if [ $# -ne 1 ] ; then
	echo "serweb_create function takes one param"
	exit 1
fi

echo "creating serweb tables into $1 ..."

# Extend table 'subscriber' with serweb specific columns
# It would be easier to drop the table and create a new one,
# but in case someone want to add serweb and has already
# a populated subscriber table, we show here how this
# can be done without deleting the existing data.
# Tested with postgres 7.4.7

sql_query "$1" "ALTER TABLE subscriber ADD COLUMN phplib_id varchar(32);
		ALTER TABLE subscriber ADD COLUMN phone varchar(15);
		ALTER TABLE subscriber ADD COLUMN datetime_modified timestamp;
		ALTER TABLE subscriber ADD COLUMN confirmation varchar(64);
		ALTER TABLE subscriber ADD COLUMN flag char(1);
		ALTER TABLE subscriber ADD COLUMN sendnotification varchar(50);
		ALTER TABLE subscriber ADD COLUMN greeting varchar(50);
		ALTER TABLE subscriber ADD COLUMN allow_find char(1);
		ALTER TABLE subscriber ADD CONSTRAINT phplib_id_key unique (phplib_id);

		ALTER TABLE subscriber ALTER phplib_id SET NOT NULL;
		ALTER TABLE subscriber ALTER phplib_id SET DEFAULT '';
		ALTER TABLE subscriber ALTER phone SET NOT NULL;
		ALTER TABLE subscriber ALTER phone SET DEFAULT '';
		ALTER TABLE subscriber ALTER datetime_modified SET NOT NULL;
		ALTER TABLE subscriber ALTER datetime_modified SET DEFAULT NOW();
		ALTER TABLE subscriber ALTER confirmation SET NOT NULL;
		ALTER TABLE subscriber ALTER confirmation SET DEFAULT '';
		ALTER TABLE subscriber ALTER flag SET NOT NULL;
		ALTER TABLE subscriber ALTER flag SET DEFAULT 'o';
		ALTER TABLE subscriber ALTER sendnotification SET NOT NULL;
		ALTER TABLE subscriber ALTER sendnotification SET DEFAULT '';
		ALTER TABLE subscriber ALTER greeting SET NOT NULL;
		ALTER TABLE subscriber ALTER greeting SET DEFAULT '';
		ALTER TABLE subscriber ALTER allow_find SET NOT NULL;
		ALTER TABLE subscriber ALTER allow_find SET DEFAULT '0';

		UPDATE subscriber SET phplib_id = DEFAULT, phone = DEFAULT, datetime_modified = DEFAULT,
		confirmation = DEFAULT, flag = DEFAULT, sendnotification = DEFAULT, 
		greeting = DEFAULT, allow_find = DEFAULT;"

if [ $? -ne 0 ] ; then
	echo "Failed to alter subscriber table for serweb!"
	exit 1
fi

sql_query "$1" < $DB_SCHEMA/serweb-create.sql

if [ $? -ne 0 ] ; then
	echo "Failed to create presence tables!"
	exit 1
fi

sql_query "$1" "GRANT ALL PRIVILEGES ON TABLE phonebook, phonebook_id_seq, pending,
		pending_id_seq, active_sessions, server_monitoring, server_monitoring_agg,
		usr_preferences_types, admin_privileges to $DBRWUSER; 
		GRANT SELECT ON TABLE phonebook, pending, active_sessions, server_monitoring,
		server_monitoring_agg, usr_preferences_types, admin_privileges to $DBROUSER;" 

if [ $? -ne 0 ] ; then
	echo "Grant privileges to serweb tables failed!"
	exit 1
fi

if [ -z "$NO_USER_INIT" ] ; then
	if [ -z "$SIP_DOMAIN" ] ; then
		prompt_realm
	fi
	credentials
	sql_query "$1" "INSERT INTO subscriber ($USERCOL, password, first_name, last_name,
			phone, email_address, datetime_created, datetime_modified, confirmation,
			flag, sendnotification, greeting, ha1, domain, ha1b, phplib_id )
			VALUES ( 'admin', '$DBRWPW', 'Initial', 'Admin', '123', 'root@localhost', 
			'2002-09-04 19:37:45', '$DUMMY_DATE', '57DaSIPuCm52UNe54LF545750cfdL48OMZfroM53',
			'o', '', '', '$HA1', '$SIP_DOMAIN', '$HA1B', '$PHPLIB_ID' );
			INSERT INTO admin_privileges ($USERCOL, domain, priv_name, priv_value)
			VALUES ('admin', '$SIP_DOMAIN', 'is_admin', '1');
			INSERT INTO admin_privileges ($USERCOL, domain, priv_name, priv_value)
			VALUES ('admin', '$SIP_DOMAIN', 'change_privileges', '1');"

	if [ $? -ne 0 ] ; then
		echo "Failed to create serweb credentials tables!"
		exit 1
	fi
fi

# emulate mysql proprietary functions used by the serweb in postgresql
sql_query "$1" "CREATE FUNCTION "truncate" (numeric,int) RETURNS numeric AS 'SELECT trunc(\$1,\$2);' LANGUAGE 'sql';
		create function unix_timestamp(timestamp) returns integer as 'select date_part(''epoch'', \$1)::int4 
		as result' language 'sql';"

if [ $? -ne 0 ] ; then
	echo "Failed to create mysql emulation functions for serweb!"
	exit 1
fi

echo "SERWEB tables succesfully created."
echo ""
echo "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!"
echo "!                                                 !"
echo "!                  WARNING                        !"
echo "!                                                 !"
echo "! There was a default admin user created:         !"
echo "!    username: admin@$SIP_DOMAIN "
echo "!    password: $DBRWPW  "
echo "!                                                 !"
echo "! Please change this password or remove this user !"
echo "! from the subscriber and admin_privileges table. !"
echo "!                                                 !"
echo "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!"

}  # end serweb_create



#export PW
#if [ "$#" -ne 0 ]; then
#  prompt_pw
#fi

case $1 in
#	reinstall)
#
#		#1 create a backup database (named *_bak)
#		echo "creating backup database"
#		openser_backup $DBNAME
#		if [ "$?" -ne 0 ] ; then
#			echo "reinstall: openser_backup failed"
#			exit 1
#		fi
#		#2 dump original database and change names in it
#		echo "dumping table content ($DBNAME)"
#		tmp_file=/tmp/openser_mysql.$$
#		openser_dump $DBNAME  > $tmp_file
#		if [ "$?" -ne 0 ] ; then
#			echo "reinstall: dumping original db failed"
#			exit 1
#		fi
#		sed "s/[uU][sS][eE][rR]_[iI][dD]/user/g" $tmp_file |
#			sed "s/[uU][sS][eE][rR]\($\|[^a-zA-Z]\)/$USERCOL\1/g" |
#			sed "s/[rR][eE][aA][lL][mM]/domain/g"> ${tmp_file}.2
#		#3 drop original database
#		echo "dropping table ($DBNAME)"
#		openser_drop $DBNAME
#		if [ "$?" -ne 0 ] ; then
#			echo "reinstall: dropping table failed"
#			rm $tmp_file*
#			exit 1
#		fi
#		#4 change names in table definition and restore
#		echo "creating new structures"
#		NO_USER_INIT="yes"
#		openser_create $DBNAME
#		if [ "$?" -ne 0 ] ; then
#			echo "reinstall: creating new table failed"
#			rm $tmp_file*
#			exit 1
#		fi
#		#5 restoring table content
#		echo "restoring table content"
#		openser_restore $DBNAME ${tmp_file}.2
#		if [ "$?" -ne 0 ] ; then
#			echo "reinstall: restoring table failed"
#			rm $tmp_file*
#			exit 1
#		fi
#
##		rm $tmp_file*
#		exit 0
#		;;
#	copy)
#		# copy database to some other name
#		shift
#		if [ $# -ne 1 ]; then
#			usage
#			exit 1
#		fi
#		tmp_file=/tmp/openser_mysql.$$
#		openser_dump $DBNAME  > $tmp_file
#		ret=$?
#		if [ "$ret" -ne 0 ]; then
#			rm $tmp_file
#			exit $ret
#		fi
#		NO_USER_INIT="yes"
#		openser_create $1
#		ret=$?
#		if [ "$ret" -ne 0 ]; then
#			rm $tmp_file
#			exit $ret
#		fi
#		openser_restore $1 $tmp_file
#		ret=$?
#		rm $tmp_file
#		exit $ret
#		;;
#	backup)
#		# backup current database
#		openser_dump $DBNAME
#		exit $?
#		;;
#	restore)
#		# restore database from a backup
#		shift
#		if [ $# -ne 1 ]; then
#			usage
#			exit 1
#		fi
#		openser_restore $DBNAME $1
#		exit $?
#		;;
	create)
		# create new database structures
		shift
		if [ $# -eq 1 ] ; then
			DBNAME="$1"
		fi
		openser_create $DBNAME
		exit $?
		;;
	serweb)
		serweb_create $DBNAME
		exit $?
		;;
	presence)
		presence_create $DBNAME
		exit $?
		;;
	extra)
		extra_create $DBNAME
		exit $?
		;;
	drop)
		# delete openser database
		openser_drop $DBNAME
		exit $?
		;;
	reinit)
		# delete database and create a new one
		openser_drop $DBNAME
		ret=$?
		if [ "$ret" -ne 0 ]; then
			exit $ret
		fi
		openser_create $DBNAME
		exit $?
		;;
	*)
		usage
		exit 1;
		;;
esac

