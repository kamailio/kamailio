#!/bin/sh
#
# $Id$
#
# Script for adding and dropping OpenSER MySQL tables
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
# 2006-04-07  removed gen_ha1 dependency - use md5sum;
#             separated the serweb from openser tables;
#             fixed the reinstall functionality (bogdan)
# 2006-05-16  added ability to specify MD5 from a configuration file
#             FreeBSD does not have the md5sum function (norm)
# 2006-09-02  Added address table (juhe)
# 2006-10-27  subscriber table cleanup; some columns are created only if
#             serweb is installed (bogdan)
# 2007-02-28  DB migration added (bogdan)
#
# 2007-05-21  Move SQL database definitions out of this script (henning)

PATH=$PATH:/usr/local/sbin

# include resource files, if any
if [ -f /etc/openser/.opensermysqlrc ]; then
	. /etc/openser/.opensermysqlrc
fi
if [ -f /usr/local/etc/openser/.opensermysqlrc ]; then
	. /usr/local/etc/openser/.opensermysqlrc
fi
if [ -f ~/.opensermysqlrc ]; then
	. ~/.opensermysqlrc
fi

# PATH to the database schemas
DATA_DIR="/usr/local/share/openser"
if [ -d "$DATA_DIR/mysql" ] ; then
	DB_SCHEMA="$DATA_DIR/mysql"
else
	DB_SCHEMA="./mysql"
fi

#################################################################
# config vars
#################################################################
# name of the database to be used by SER
if [ -z "$DBNAME" ]; then
	DBNAME="openser"
fi
# address of MySQL server
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
# full privileges MySQL user
if [ -z "$DBROOTUSER" ]; then
	DBROOTUSER="root"
fi

CMD="mysql -h $DBHOST -u$DBROOTUSER "
DUMP_CMD="mysqldump -h $DBHOST -u$DBROOTUSER -c -t "
BACKUP_CMD="mysqldump -h $DBHOST -u$DBROOTUSER -c "


# if you change this definitions here, then you must change them 
# in db/schema/entities.xml too, 
# FIXME

# type of mysql tables
if [ -z "$TABLE_TYPE" ]; then
	TABLE_TYPE="TYPE=MyISAM"
fi
# user name column
if [ -z "$USERCOL" ]; then
	USERCOL="username"
fi

FOREVER="2020-05-28 21:32:15"

DEFAULT_ALIASES_EXPIRES=$FOREVER
DEFAULT_Q="1.0"
DEFAULT_CALLID="Default-Call-ID"
DEFAULT_CSEQ="13"
DEFAULT_LOCATION_EXPIRES=$FOREVER

# Program to calculate a message-digest fingerprint 
if [ -z "$MD5" ]; then
	MD5="md5sum"
fi
if [ -z "$AWK" ]; then
	AWK="awk"
fi
if [ -z "$GREP" ]; then
	GREP="egrep"
fi
if [ -z "$SED" ]; then
	SED="sed"
fi

# define what modules should be installed

# openser standard modules
STANDARD_MODULES="standard acc lcr domain group permissions 
                  registrar usrloc msilo alias_db uri_db 
                  speeddial avpops auth_db"

# openser extra modules
EXTRA_MODULES="imc cpl siptrace domainpolicy"
#################################################################


usage() {
COMMAND=`basename $0`
cat <<EOF
usage: $COMMAND create
       $COMMAND drop   (!!entirely deletes tables)
       $COMMAND reinit (!!entirely deletes and than re-creates tables
       $COMMAND backup (dumps current database to stdout)
       $COMMAND restore <file> (restores tables from a file)
       $COMMAND copy <new_db> (creates a new db from an existing one)
       $COMMAND migrate <old_db> <new_db> (migrates DB from 1.1 to 1.2)
       $COMMAND presence (adds the presence related tables)
       $COMMAND extra (adds the extra tables - imc,cpl,siptrace,domainpolicy)
       $COMMAND serweb (adds the SERWEB specific tables)

       if you want to manipulate database as other MySQL user than
       root, want to change database name from default value "$DBNAME",
       or want to use other values for users and password, edit the
       "config vars" section of the command $COMMAND

EOF
} #usage


# read password
prompt_pw()
{
	savetty=`stty -g`
	printf "MySQL password for $DBROOTUSER: "
	stty -echo
	read PW
	stty $savetty
	echo
	export PW
}

# execute sql command with optional db name 
# and password parameters given
sql_query()
{
	if [ $# -gt 1 ] ; then
		if [ -n "$1" ]; then
			DB="$1" # no quoting, mysql client don't like this
		else
			DB=""
		fi
		shift
		if [ -n "$PW" ]; then
			$CMD "-p$PW" $DB -e "$@"
		else
			$CMD $DB -e "$@"
		fi
	else
		if [ -n "$PW" ]; then
			$CMD "-p$PW" "$@"
		else
			$CMD "$@"
		fi
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
		echo "openser_backup dump failed"
		exit 1
	fi
	sql_query "" "create database $1_bak;"

	openser_restore $1_bak $BU
	if [ "$?" -ne 0 ]; then
		echo "openser backup/restore failed"
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

sql_query "" "drop database $1;"
}


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


db_charset_test()
{
	CURRCHARSET=`echo "show variables like '%character_set_server%'" | $CMD "-p$PW" | $AWK '{print $2}' | $SED -e 1d`
	ALLCHARSETS=`echo "show character set" | $CMD "-p$PW" | $AWK '{print $1}' | $SED -e 1d | $GREP -iv utf8\|ucs2`
	while [ `echo "$ALLCHARSETS" | $GREP -icw $CURRCHARSET`  = "0" ]
	do
		echo "Your current default mysql characters set cannot be used to create DB. Please choice another one from the following list:"
		echo "$ALLCHARSETS"
		echo -n "Enter character set name: "
		read CURRCHARSET
		if [ `echo $CURRCHARSET | $GREP -cE "\w+"` = "0" ]; then
			echo "can't continue: user break"
			exit 1
		fi
	done
	CHARSET=$CURRCHARSET
}


openser_create () # pars: <database name>
{
if [ $# -ne 1 ] ; then
	echo "openser_create function takes one param"
	exit 1
fi

db_charset_test

echo "creating database $1 ..."

# Users: ser is the regular user, serro only for reading
sql_query "" "create database $1 character set $CHARSET;
	      GRANT ALL PRIVILEGES ON $1.* TO $DBRWUSER IDENTIFIED  BY '$DBRWPW';
	      GRANT ALL PRIVILEGES ON $1.* TO ${DBRWUSER}@$DBHOST IDENTIFIED BY '$DBRWPW';
	      GRANT SELECT ON $1.* TO $DBROUSER IDENTIFIED BY '$DBROPW';
	      GRANT SELECT ON $1.* TO ${DBROUSER}@$DBHOST IDENTIFIED BY '$DBROPW';"


if [ $? -ne 0 ] ; then
	echo "Creating core database and grant privileges failed!"
	exit 1
fi

for TABLE in $STANDARD_MODULES; do
    echo "Creating core table: $TABLE"
    sql_query $1 < $DB_SCHEMA/$TABLE-create.sql
    if [ $? -ne 0 ] ; then
	echo "Creating core tables failed!"
	exit 1
    fi
done

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
	HAS_EXTRA="yes"
	extra_create $1
fi

echo -n "Install SERWEB related tables ?(y/n):"
read INPUT
if [ "$INPUT" = "y" ] || [ "$INPUT" = "Y" ]
then
	HAS_SERWEB="yes"
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

sql_query $1 < $DB_SCHEMA/presence-create.sql

if [ $? -ne 0 ] ; then
	echo "Failed to create presence tables!"
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

echo "creating extra tables into $1 ..."

for TABLE in $EXTRA_MODULES; do
    echo "Creating extra table: $TABLE"
    sql_query $1 < $DB_SCHEMA/$TABLE-create.sql
    if [ $? -ne 0 ] ; then
	echo "Creating extra tables failed!"
	exit 1
    fi
done

echo "Extra tables succesfully created."
}  # end extra_create


serweb_create () # pars: <database name>
{
if [ $# -ne 1 ] ; then
	echo "serweb_create function takes one param"
	exit 1
fi

echo "creating serweb tables into $1 ..."

sql_query $1 < $DB_SCHEMA/serweb-create.sql 

if [ $? -ne 0 ] ; then
	echo "Failed to create serweb tables!"
	exit 1
fi

sql_query $1 "ALTER TABLE subscriber
    ADD COLUMN (
    phplib_id varchar(32) NOT NULL default '',
    phone varchar(15) NOT NULL default '',
    datetime_modified datetime NOT NULL default '0000-00-00 00:00:00',
    confirmation varchar(64) NOT NULL default '',
    flag char(1) NOT NULL default 'o',
    sendnotification varchar(50) NOT NULL default '',
    greeting varchar(50) NOT NULL default '',
    allow_find char(1) NOT NULL default '0'),
    ADD UNIQUE KEY phplib_id (phplib_id);"

if [ $? -ne 0 ] ; then
	echo "Failed to alter subscriber table for serweb!"
	exit 1
fi

if [ -z "$NO_USER_INIT" ] ; then
	if [ -z "$SIP_DOMAIN" ] ; then
		prompt_realm
	fi
	credentials
	sql_query $1 "INSERT INTO subscriber 
	($USERCOL, password, first_name, last_name, phone,
	email_address, datetime_created, datetime_modified, confirmation,
	flag, sendnotification, greeting, ha1, domain, ha1b, phplib_id )
	VALUES ( 'admin', '$DBRWPW', 'Initial', 'Admin', '123',
	'root@localhost', '2002-09-04 19:37:45', '0000-00-00 00:00:00',
	'57DaSIPuCm52UNe54LF545750cfdL48OMZfroM53', 'o', '', '',
	'$HA1', '$SIP_DOMAIN', '$HA1B',	'$PHPLIB_ID' );
	INSERT INTO admin_privileges ($USERCOL, domain, priv_name, priv_value)
	VALUES ('admin', '$SIP_DOMAIN', 'is_admin', '1');
	INSERT INTO admin_privileges ($USERCOL, domain, priv_name, priv_value)
	VALUES ('admin', '$SIP_DOMAIN', 'change_privileges', '1');"
fi

if [ $? -ne 0 ] ; then
	echo "Failed to create serweb credentials tables!"
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
echo "!    password: $DBRWPW "
echo "!                                                 !"
echo "! Please change this password or remove this user !"
echo "! from the subscriber and admin_privileges table. !"
echo "!                                                 !"
echo "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!"
}  # end serweb_create



migrate_table () # 4 paremeters (dst_table, dst_cols, src_table, src_cols)
{
if [ $# -ne 4 ] ; then
	echo "migrate_table function takes 4 params $@"
	exit 1
fi

src_cols=`echo $4 | sed s/?/$3./g `

X=`sql_query "" "INSERT into $1 ($2) SELECT $src_cols from $3;" 2>&1`

if [ $? -ne 0 ] ; then
	echo $X | $GREP "ERROR 1146" > /dev/null
	if [ $? -eq 0 ] ; then 
		echo " -- Migrating $3 to $1.....SKIPPED (no source)"
		return 0
	fi
	echo "ERROR: failed to migrate $3 to $1!!!"
	echo -n "Skip it and continue (y/n)? "
	read INPUT
	if [ "$INPUT" = "y" ] || [ "$INPUT" = "Y" ]
	then
		return 0
	fi

	exit 1;
fi

echo " -- Migrating $3 to $1.....OK"

}


migrate_db () # 2 parameters (src_db, dst_db)
{
if [ $# -ne 2 ] ; then
	echo "migrate_db function takes 2 params"
	exit 1
fi

dst_db=$2
src_db=$1

migrate_table ${dst_db}.acc \
	"method,from_tag,to_tag,callid,sip_code,time" \
	${src_db}.acc \
	"?sip_method,?fromtag,?totag,?sip_callid,?sip_status,?time"

migrate_table ${dst_db}.missed_calls \
	"method,from_tag,to_tag,callid,sip_code,time" \
	${src_db}.missed_calls \
	"?sip_method,?fromtag,?totag,?sip_callid,?sip_status,?time"

migrate_table ${dst_db}.aliases \
	"username,domain,contact,expires,q,callid,cseq,last_modified,\
		flags,user_agent" \
	${src_db}.aliases \
	"?username,?domain,?contact,?expires,?q,?callid,?cseq,?last_modified,\
		?flags,?user_agent"

migrate_table ${dst_db}.dbaliases \
	"alias_username,alias_domain,username,domain" \
	${src_db}.dbaliases \
	"?alias_username,?alias_domain,?username,?domain"

migrate_table ${dst_db}.grp \
	"username,domain,grp,last_modified" \
	${src_db}.grp \
	"?username,?domain,?grp,?last_modified"

migrate_table ${dst_db}.re_grp \
	"reg_exp,group_id" \
	${src_db}.re_grp \
	"?reg_exp,?group_id"

migrate_table ${dst_db}.silo \
	"id,src_addr,dst_addr,username,domain,inc_time,exp_time,snd_time,\
		ctype,body" \
	${src_db}.silo \
	"?mid,?src_addr,?dst_addr,?username,?domain,?inc_time,?exp_time,?snd_time,\
		?ctype,?body"

migrate_table ${dst_db}.domain \
	"domain,last_modified" \
	${src_db}.domain \
	"?domain,?last_modified"

migrate_table ${dst_db}.uri \
	"username,domain,uri_user,last_modified" \
	${src_db}.uri \
	"?username,?domain,?uri_user,?last_modified"

migrate_table ${dst_db}.usr_preferences \
	"uuid,username,domain,attribute,type,value,last_modified" \
	${src_db}.usr_preferences \
	"?uuid,?username,?domain,?attribute,?type,?value,?modified"

migrate_table ${dst_db}.trusted \
	"src_ip,proto,from_pattern,tag" \
	${src_db}.trusted \
	"?src_ip,?proto,?from_pattern,?tag"

migrate_table ${dst_db}.speed_dial \
	"id,username,domain,sd_username,sd_domain,new_uri,\
		fname,lname,description" \
	${src_db}.speed_dial \
	"?uuid,?username,?domain,?sd_username,?sd_domain,?new_uri,\
		?fname,?lname,?description"

migrate_table ${dst_db}.gw \
	"gw_name,grp_id,ip_addr,port,uri_scheme,transport,strip,prefix" \
	${src_db}.gw \
	"?gw_name,?grp_id, INET_NTOA(((?ip_addr & 0x000000ff) << 24) + \
	((?ip_addr & 0x0000ff00) << 8) + ((?ip_addr & 0x00ff0000) >> 8) + \
	((?ip_addr & 0xff000000) >> 24)),?port,?uri_scheme,?transport,?strip,\
	?prefix"

migrate_table ${dst_db}.gw_grp \
	"grp_id,grp_name" \
	${src_db}.gw_grp \
	"?grp_id,?grp_name"

migrate_table ${dst_db}.lcr \
	"prefix,from_uri,grp_id,priority" \
	${src_db}.lcr \
	"?prefix,?from_uri,?grp_id,?priority"

migrate_table ${dst_db}.pdt \
	"sdomain,prefix,domain" \
	${src_db}.pdt \
	"?sdomain,?prefix,?domain"

if [ "$HAS_SERWEB" = "yes" ] ; then
	# migrate subscribers with serweb support
	migrate_table ${dst_db}.subscriber \
		"username,domain,password,first_name,last_name,email_address,\
		datetime_created,ha1,ha1b,timezone,rpid,phplib_id,phone,\
		datetime_modified,confirmation,flag,sendnotification,\
		greeting,allow_find" \
		${src_db}.subscriber \
		"?username,?domain,?password,?first_name,?last_name,?email_address,\
		?datetime_created,?ha1,?ha1b,?timezone,?rpid,?phplib_id,?phone,\
		?datetime_modified,?confirmation,?flag,?sendnotification,\
		?greeting,?allow_find"

	migrate_table ${dst_db}.active_sessions \
		"sid,name,val,changed" \
		${src_db}.active_sessions \
		"?sid,?name,?val,?changed"

	migrate_table ${dst_db}.pending \
		"username,domain,password,first_name,last_name,email_address,\
		datetime_created,ha1,ha1b,timezone,rpid,phplib_id,phone,\
		datetime_modified,confirmation,flag,sendnotification,\
		greeting,allow_find" \
		${src_db}.pending \
		"?username,?domain,?password,?first_name,?last_name,?email_address,\
		?datetime_created,?ha1,?ha1b,?timezone,?rpid,?phplib_id,?phone,\
		?datetime_modified,?confirmation,?flag,?sendnotification,\
		?greeting,?allow_find"

	migrate_table ${dst_db}.phonebook \
		"id,username,domain,fname,lname,sip_uri" \
		${src_db}.phonebook \
		"?id,?username,?domain,?fname,?lname,?sip_uri"

	migrate_table ${dst_db}.server_monitoring \
		"time,id,param,value,increment" \
		${src_db}.server_monitoring \
		"?time,?id,?param,?value,?increment"

	migrate_table ${dst_db}.usr_preferences_type \
		"att_name,att_rich_type,att_raw_type,att_type_spec,default_value" \
		${src_db}.usr_preferences_type \
		"?att_name,?att_rich_type,?att_raw_type,?att_type_spec,?default_value"

	migrate_table ${dst_db}.server_monitoring_agg \
		"param,s_value,s_increment,last_aggregated_increment,av,mv,ad,lv,\
		min_val,max_val,min_inc,max_inc,lastupdate" \
		${src_db}.server_monitoring_agg \
		"?param,?s_value,?s_increment,?last_aggregated_increment,?av,?mv,\
		?ad,?lv,?min_val,?max_val,?min_inc,?max_inc,?lastupdate"

	migrate_table ${dst_db}.admin_privileges \
		"username,domain,priv_name,priv_value" \
		${src_db}.admin_privileges \
		"?username,?domain,?priv_name,?priv_value"
else
	# migrate subscribers with no serweb support
	migrate_table ${dst_db}.subscriber \
		"username,domain,password,first_name,last_name,email_address,\
		datetime_created,ha1,ha1b,timezone,rpid" \
		${src_db}.subscriber \
		"?username,?domain,?password,?first_name,?last_name,?email_address,\
		?datetime_created,?ha1,?ha1b,?timezone,?rpid"
fi

if [ "$HAS_EXTRA" = "yes" ] ; then
	migrate_table ${dst_db}.cpl \
		"username,domain,cpl_xml,cpl_bin" \
		${src_db}.cpl \
		"?username,?domain,?cpl_xml,?cpl_bin"

	migrate_table ${dst_db}.siptrace \
		"id,date,callid,traced_user,msg,method,status,fromip,toip,\
		fromtag,direction" \
		${src_db}.siptrace \
		"?id,?date,?callid,?traced_user,?msg,?method,?status,?fromip,?toip,\
		?fromtag,?direction"
fi

}  #end migrate_db()


export PW
if [ "$#" -ne 0 ] && [ "$PW" = "" ]; then
	prompt_pw
fi




case $1 in
	migrate)
		if [ $# -ne 3 ] ; then
			echo "migrate requires 2 paramets: old and new database"
			exit 1
		fi
		# create new database
		echo "Creating new Database $3...."
		NO_USER_INIT="yes"
		openser_create $3
		if [ "$?" -ne 0 ] ; then
			echo "migrate: creating new database failed"
			exit 1
		fi
		# migrate data
		echo "Migrating data from $2 to $3...."
		migrate_db $2 $3
		echo "Migration successfully completed."
		exit 0;
		;;
	copy)
		# copy database to some other name
		shift
		if [ $# -ne 1 ]; then
			usage
			exit 1
		fi
		tmp_file=/tmp/openser_mysql.$$
		openser_dump $DBNAME > $tmp_file
		ret=$?
		if [ "$ret" -ne 0 ]; then
			echo "copy: dumping original db failed"
			rm $tmp_file
			exit $ret
		fi
		NO_USER_INIT="yes"
		openser_create $1
		ret=$?
		if [ "$ret" -ne 0 ]; then
			echo "copy: creating new db failed"
			rm $tmp_file
			exit $ret
		fi
		openser_restore $1 $tmp_file
		ret=$?
		rm -f $tmp_file
		if [ "$ret" -ne 0 ]; then
			echo "copy: restoring old db failed"
		fi
		exit $ret
		;;
	backup)
		# backup current database
		openser_dump $DBNAME
		exit $?
		;;
	restore)
		# restore database from a backup
		shift
		if [ $# -ne 1 ]; then
			usage
			exit 1
		fi
		openser_restore $DBNAME $1
		exit $?
		;;
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

