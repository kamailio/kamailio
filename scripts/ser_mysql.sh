#!/bin/sh
#
# $Id$
#
# Script for adding and dropping ser MySql tables
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


#################################################################
# config vars
#################################################################
DBNAME=ser
DBHOST=localhost
USERNAME=ser
DEFAULT_PW=heslo
ROUSER=serro
RO_PW=47serro11
SQL_USER="root"
CMD="mysql -h $DBHOST -u$SQL_USER "
DUMP_CMD="mysqldump -h $DBHOST -u$SQL_USER -c -t "
BACKUP_CMD="mysqldump -h $DBHOST -u$SQL_USER -c "
TABLE_TYPE="TYPE=MyISAM"
# user name column
USERCOL="username"

GENHA1='gen_ha1'

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
       $COMMAND backup (dumps current database to stdout)
	   $COMMAND restore <file> (restores tables from a file)
       $COMMAND copy <new_db> (creates a new db from an existing one)
       $COMMAND reinstall (updates to a new SER database)

       if you want to manipulate database as other MySql user than
       root, want to change database name from default value "$DBNAME",
       or want to use other values for users and password, edit the
       "config vars" section of the command $COMMAND

EOF
} #usage


# read password
prompt_pw()
{
	savetty=`stty -g`
	printf "MySql password for $SQL_USER: "
	stty -echo
	read PW
	stty $savetty
	echo
}

# execute sql command
sql_query()
{
	$CMD "-p$PW" "$@"
}

# dump all rows
ser_dump()  # pars: <database name>
{
	if [ $# -ne 1 ] ; then
		echo "ser_dump function takes one param"
		exit 1
	fi
	$DUMP_CMD "-p$PW" $1
}


# copy a database to database_bak
ser_backup() # par: <database name>
{
	if [ $# -ne 1 ] ; then
		echo  "ser_backup function takes one param"
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

	ser_restore $1_bak $BU
	if [ "$?" -ne 0 ]; then
		echo "ser backup/restore failed"
		rm $BU
		exit 1
	fi
}

ser_restore() #pars: <database name> <filename>
{
if [ $# -ne 2 ] ; then
	echo "ser_restore function takes two params"
	exit 1
fi
sql_query $1 < $2
}

ser_drop()  # pars: <database name>
{
if [ $# -ne 1 ] ; then
	echo "ser_drop function takes two params"
	exit 1
fi

sql_query << EOF
drop database $1;
EOF
} #ser_drop

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
	HA1=`$GENHA1 admin $SIP_DOMAIN heslo`
	if [ $? -ne 0 ] ; then
		echo "HA1 calculation failed"
		exit 1
	fi
	HA1B=`$GENHA1 "admin@$SIP_DOMAIN" $SIP_DOMAIN heslo`
	if [ $? -ne 0 ] ; then
		echo "HA1B calculation failed"
		exit 1
	fi

  #PHPLIB_ID of users should be difficulty to guess for security reasons
  NOW=`date`;
  PHPLIB_ID=`$GENHA1 "$RANDOM" "$NOW" $SIP_DOMAIN`
	if [ $? -ne 0 ] ; then
    echo "PHPLIB_ID calculation failed"
		exit 1
	fi
}

ser_create () # pars: <database name> [<no_init_user>]
{
if [ $# -eq 1 ] ; then
	if [ -z "$SIP_DOMAIN" ] ; then
		prompt_realm
	fi
	credentials
	# by default we create initial user
	INITIAL_USER="INSERT INTO subscriber
		($USERCOL, password, first_name, last_name, phone,
		email_address, datetime_created, datetime_modified, confirmation,
    flag, sendnotification, greeting, ha1, domain, ha1b, phplib_id )
		VALUES ( 'admin', 'heslo', 'Initial', 'Admin', '123',
		'root@localhost', '2002-09-04 19:37:45', '0000-00-00 00:00:00',
		'57DaSIPuCm52UNe54LF545750cfdL48OMZfroM53', 'o', '', '',
		'$HA1', '$SIP_DOMAIN', '$HA1B',
    '$PHPLIB_ID' );

    INSERT INTO admin_privileges ($USERCOL, domain, priv_name, priv_value)
    VALUES ('admin', '$SIP_DOMAIN', 'is_admin', '1');

    INSERT INTO admin_privileges ($USERCOL, domain, priv_name, priv_value)
    VALUES ('admin', '$SIP_DOMAIN', 'change_privileges', '1');"
elif [ $# -eq 2 ] ; then
	# if 3rd param set, don't create any initial user
	INITIAL_USER=""
else
	echo "ser_create function takes one or two params"
	exit 1
fi

echo "creating database $1 ..."

sql_query <<EOF
create database $1;
use $1;

# Users: ser is the regular user, serro only for reading
GRANT ALL PRIVILEGES ON $1.* TO $USERNAME IDENTIFIED  BY '$DEFAULT_PW';
GRANT ALL PRIVILEGES ON $1.* TO ${USERNAME}@$DBHOST IDENTIFIED BY '$DEFAULT_PW';
GRANT SELECT ON $1.* TO $ROUSER IDENTIFIED BY '$RO_PW';
GRANT SELECT ON $1.* TO ${ROUSER}@$DBHOST IDENTIFIED BY '$RO_PW';


#
# Table structure versions
#

CREATE TABLE version (
   table_name varchar(64) NOT NULL,
   table_version smallint(5) DEFAULT '0' NOT NULL
) $TABLE_TYPE;

#
# Dumping data for table 'version'
#

INSERT INTO version VALUES ( 'subscriber', '5');
INSERT INTO version VALUES ( 'reserved', '1');
INSERT INTO version VALUES ( 'phonebook', '1');
INSERT INTO version VALUES ( 'pending', '4');
INSERT INTO version VALUES ( 'missed_calls', '2');
INSERT INTO version VALUES ( 'location', '6');
INSERT INTO version VALUES ( 'grp', '2');
INSERT INTO version VALUES ( 'event', '1');
INSERT INTO version VALUES ( 'aliases', '6');
INSERT INTO version VALUES ( 'active_sessions', '1');
INSERT INTO version VALUES ( 'acc', '2');
INSERT INTO version VALUES ( 'config', '1');
INSERT INTO version VALUES ( 'silo', '3');
INSERT INTO version VALUES ( 'realm', '1');
INSERT INTO version VALUES ( 'domain', '1');
INSERT INTO version VALUES ( 'uri', '1');
INSERT INTO version VALUES ( 'server_monitoring', '1');
INSERT INTO version VALUES ( 'server_monitoring_agg', '1');
INSERT INTO version VALUES ( 'trusted', '1');
INSERT INTO version VALUES ( 'usr_preferences', '1');
INSERT INTO version VALUES ( 'preferences_types', '1');
INSERT INTO version VALUES ( 'admin_privileges', '1');
INSERT INTO version VALUES ( 'calls_forwarding', '1');
INSERT INTO version VALUES ( 'speed_dial', '1');

#
# Table structure for table 'acc' -- accounted calls
#


CREATE TABLE acc (
  sip_from varchar(128) NOT NULL default '',
  sip_to varchar(128) NOT NULL default '',
  sip_status varchar(128) NOT NULL default '',
  sip_method varchar(16) NOT NULL default '',
  i_uri varchar(128) NOT NULL default '',
  o_uri varchar(128) NOT NULL default '',
  from_uri varchar(128) NOT NULL default '',
  to_uri varchar(128) NOT NULL default '',
  sip_callid varchar(128) NOT NULL default '',
  $USERCOL varchar(64) NOT NULL default '',
  domain varchar(128) NOT NULL default '',
  fromtag varchar(128) NOT NULL default '',
  totag varchar(128) NOT NULL default '',
  time datetime NOT NULL default '0000-00-00 00:00:00',
  timestamp timestamp(14) NOT NULL,
  INDEX acc_user ($USERCOL, domain),
  KEY sip_callid (sip_callid)
) $TABLE_TYPE;




#
# Table structure for table 'active_sessions' -- web stuff
#


CREATE TABLE active_sessions (
  sid varchar(32) NOT NULL default '',
  name varchar(32) NOT NULL default '',
  val text,
  changed varchar(14) NOT NULL default '',
  PRIMARY KEY  (name,sid),
  KEY changed (changed)
) $TABLE_TYPE;



#
# Table structure for table 'aliases' -- location-like table
# (aliases_contact index makes lookup of missed calls much faster)
#

CREATE TABLE aliases (
  $USERCOL varchar(64) NOT NULL default '',
  domain varchar(128) NOT NULL default '',
  contact varchar(255) NOT NULL default '',
  received varchar(255) default NULL,
  expires datetime NOT NULL default '$DEFAULT_ALIASES_EXPIRES',
  q float(10,2) NOT NULL default '$DEFAULT_Q',
  callid varchar(255) NOT NULL default '$DEFAULT_CALLID',
  cseq int(11) NOT NULL default '$DEFAULT_CSEQ',
  last_modified timestamp(14) NOT NULL,
  replicate int(10) unsigned NOT NULL default '0',
  state tinyint(1) unsigned NOT NULL default '0',
  flags int(11) NOT NULL default '0',
  user_agent varchar(50) NOT NULL default '',
  PRIMARY KEY($USERCOL, domain, contact),
  INDEX aliases_contact (contact)
) $TABLE_TYPE;


#
# Table structure for table 'event' -- track of predefined
# events to which a user subscribed
#


CREATE TABLE event (
  id int(10) unsigned NOT NULL auto_increment,
  $USERCOL varchar(64) NOT NULL default '',
  domain varchar(128) NOT NULL default '',
  uri varchar(255) NOT NULL default '',
  description varchar(255) NOT NULL default '',
  PRIMARY KEY (id)
) $TABLE_TYPE;




#
# Table structure for table 'grp' -- group membership
# table; used primarily for ACLs
#


CREATE TABLE grp (
  $USERCOL varchar(64) NOT NULL default '',
  domain varchar(128) NOT NULL default '',
  grp varchar(50) NOT NULL default '',
  last_modified datetime NOT NULL default '0000-00-00 00:00:00',
  PRIMARY KEY($USERCOL, domain, grp)
) $TABLE_TYPE;




#
# Table structure for table 'location' -- that is persistent UsrLoc
#
CREATE TABLE location (
  $USERCOL varchar(64) NOT NULL default '',
  domain varchar(128) NOT NULL default '',
  contact varchar(255) NOT NULL default '',
  received varchar(255) default NULL,
  expires datetime NOT NULL default '$DEFAULT_LOCATION_EXPIRES',
  q float(10,2) NOT NULL default '$DEFAULT_Q',
  callid varchar(255) NOT NULL default '$DEFAULT_CALLID',
  cseq int(11) NOT NULL default '$DEFAULT_CSEQ',
  last_modified timestamp(14) NOT NULL,
  replicate int(10) unsigned NOT NULL default '0',
  state tinyint(1) unsigned NOT NULL default '0',
  flags int(11) NOT NULL default '0',
  user_agent varchar(50) NOT NULL default '',
  PRIMARY KEY($USERCOL, domain, contact)
) $TABLE_TYPE;




#
# Table structure for table 'missed_calls' -- acc-like table
# for keeping track of missed calls
#


CREATE TABLE missed_calls (
  sip_from varchar(128) NOT NULL default '',
  sip_to varchar(128) NOT NULL default '',
  sip_status varchar(128) NOT NULL default '',
  sip_method varchar(16) NOT NULL default '',
  i_uri varchar(128) NOT NULL default '',
  o_uri varchar(128) NOT NULL default '',
  from_uri varchar(128) NOT NULL default '',
  to_uri varchar(128) NOT NULL default '',
  sip_callid varchar(128) NOT NULL default '',
  $USERCOL varchar(64) NOT NULL default '',
  domain varchar(128) NOT NULL default '',
  fromtag varchar(128) NOT NULL default '',
  totag varchar(128) NOT NULL default '',
  time datetime NOT NULL default '0000-00-00 00:00:00',
  timestamp timestamp(14) NOT NULL,
  INDEX mc_user ($USERCOL, domain)
) $TABLE_TYPE;




#
# Table structure for table 'pending' -- unconfirmed subscribtion
# requests
#


CREATE TABLE pending (
  phplib_id varchar(32) NOT NULL default '',
  $USERCOL varchar(64) NOT NULL default '',
  domain varchar(128) NOT NULL default '',
  password varchar(25) NOT NULL default '',
  first_name varchar(25) NOT NULL default '',
  last_name varchar(45) NOT NULL default '',
  phone varchar(15) NOT NULL default '',
  email_address varchar(50) NOT NULL default '',
  datetime_created datetime NOT NULL default '0000-00-00 00:00:00',
  datetime_modified datetime NOT NULL default '0000-00-00 00:00:00',
  confirmation varchar(64) NOT NULL default '',
  flag char(1) NOT NULL default 'o',
  sendnotification varchar(50) NOT NULL default '',
  greeting varchar(50) NOT NULL default '',
  ha1 varchar(128) NOT NULL default '',
  ha1b varchar(128) NOT NULL default '',
  allow_find char(1) NOT NULL default '0',
  timezone varchar(128) default NULL,
  rpid varchar(128) default NULL,
  domn int(10) default NULL,
  uuid varchar(64) default NULL,
  PRIMARY KEY ($USERCOL, domain),
  KEY user_2 ($USERCOL),
  UNIQUE KEY phplib_id (phplib_id)
) $TABLE_TYPE;




#
# Table structure for table 'phonebook' -- user's phonebook
#


CREATE TABLE phonebook (
  id int(10) unsigned NOT NULL auto_increment,
  $USERCOL varchar(64) NOT NULL default '',
  domain varchar(128) NOT NULL default '',
  fname varchar(32) NOT NULL default '',
  lname varchar(32) NOT NULL default '',
  sip_uri varchar(128) NOT NULL default '',
  PRIMARY KEY  (id)
) $TABLE_TYPE;




#
# Table structure for table 'reserved' -- reserved username
# which should be never allowed for subscription
#


CREATE TABLE reserved (
  $USERCOL char(64) NOT NULL default '',
  UNIQUE KEY user2(username)
) $TABLE_TYPE;




#
# Table structure for table 'subscriber' -- user database
#


CREATE TABLE subscriber (
  phplib_id varchar(32) NOT NULL default '',
  $USERCOL varchar(64) NOT NULL default '',
  domain varchar(128) NOT NULL default '',
  password varchar(25) NOT NULL default '',
  first_name varchar(25) NOT NULL default '',
  last_name varchar(45) NOT NULL default '',
  phone varchar(15) NOT NULL default '',
  email_address varchar(50) NOT NULL default '',
  datetime_created datetime NOT NULL default '0000-00-00 00:00:00',
  datetime_modified datetime NOT NULL default '0000-00-00 00:00:00',
  confirmation varchar(64) NOT NULL default '',
  flag char(1) NOT NULL default 'o',
  sendnotification varchar(50) NOT NULL default '',
  greeting varchar(50) NOT NULL default '',
  ha1 varchar(128) NOT NULL default '',
  ha1b varchar(128) NOT NULL default '',
  allow_find char(1) NOT NULL default '0',
  timezone varchar(128) default NULL,
  rpid varchar(128) default NULL,
  domn int(10) default NULL,
  uuid varchar(64) default NULL,
  UNIQUE KEY phplib_id (phplib_id),
  PRIMARY KEY ($USERCOL, domain),
  KEY user_2 ($USERCOL)
) $TABLE_TYPE;

# hook-table for all posssible future config values
# (currently unused)

CREATE TABLE config (
   attribute varchar(32) NOT NULL,
   value varchar(128) NOT NULL,
   $USERCOL varchar(64) NOT NULL default '',
   domain varchar(128) NOT NULL default '',
   modified timestamp(14)
) $TABLE_TYPE;

# "instant" message silo

CREATE TABLE silo(
    mid INTEGER NOT NULL AUTO_INCREMENT PRIMARY KEY,
    src_addr VARCHAR(255) NOT NULL DEFAULT "",
    dst_addr VARCHAR(255) NOT NULL DEFAULT "",
    r_uri VARCHAR(255) NOT NULL DEFAULT "",
    username VARCHAR(64) NOT NULL DEFAULT "",
    domain VARCHAR(128) NOT NULL DEFAULT "",
    inc_time INTEGER NOT NULL DEFAULT 0,
    exp_time INTEGER NOT NULL DEFAULT 0,
    ctype VARCHAR(32) NOT NULL DEFAULT "text/plain",
    body BLOB NOT NULL DEFAULT ""
) $TABLE_TYPE;

#
# Table structure for table 'domain' -- domains proxy is responsible for
#

CREATE TABLE domain (
  domain varchar(128) NOT NULL default '',
  last_modified datetime NOT NULL default '0000-00-00 00:00:00',
  PRIMARY KEY  (domain)
) $TABLE_TYPE;


#
# Table structure for table 'uri' -- uri user parts users are allowed to use
#
CREATE TABLE uri (
  $USERCOL varchar(64) NOT NULL default '',
  domain varchar(128) NOT NULL default '',
  uri_user varchar(50) NOT NULL default '',
  last_modified datetime NOT NULL default '0000-00-00 00:00:00',
  PRIMARY KEY ($USERCOL, domain, uri_user)
) $TABLE_TYPE;

#
# Table structure for table 'server_monitoring'
#


DROP TABLE IF EXISTS server_monitoring;
CREATE TABLE server_monitoring (
  time datetime NOT NULL default '0000-00-00 00:00:00',
  id int(10) unsigned NOT NULL default '0',
  param varchar(32) NOT NULL default '',
  value int(10) NOT NULL default '0',
  increment int(10) NOT NULL default '0',
  PRIMARY KEY  (id,param)
) $TABLE_TYPE;

#
# Table structure for table 'usr_preferences'
#

DROP TABLE IF EXISTS usr_preferences;
CREATE TABLE usr_preferences (
  uuid varchar(64) NOT NULL default '',
  $USERCOL varchar(100) NOT NULL default '0',
  domain varchar(128) NOT NULL default '',
  attribute varchar(32) NOT NULL default '',
  value varchar(128) NOT NULL default '',
  modified timestamp(14) NOT NULL,
  PRIMARY KEY  (attribute,$USERCOL,domain)
) $TABLE_TYPE;



#
# Table structure for table 'preferences_types' -- types of atributes in preferences
#

CREATE TABLE preferences_types (
  att_name varchar(50) NOT NULL default '',
  att_rich_type varchar(32) NOT NULL default 'string',
  att_raw_type int(11) unsigned NOT NULL default '2',
  att_type_spec text,
  default_value varchar(100) NOT NULL default '',
  PRIMARY KEY  (att_name)
) $TABLE_TYPE;

#
# Table structure for table trusted
CREATE TABLE trusted (
  src_ip varchar(39) NOT NULL,
  proto varchar(4) NOT NULL,
  from_pattern varchar(64) NOT NULL,
  PRIMARY KEY (src_ip, proto, from_pattern)
) $TABLE_TYPE;


#
# Table structure for table 'server_monitoring_agg'
#
DROP TABLE IF EXISTS server_monitoring_agg;
CREATE TABLE server_monitoring_agg (
  param varchar(32) NOT NULL default '',
  s_value int(10) NOT NULL default '0',
  s_increment int(10) NOT NULL default '0',
  last_aggregated_increment int(10) NOT NULL default '0',
  av float NOT NULL default '0',
  mv int(10) NOT NULL default '0',
  ad float NOT NULL default '0',
  lv int(10) NOT NULL default '0',
  min_val int(10) NOT NULL default '0',
  max_val int(10) NOT NULL default '0',
  min_inc int(10) NOT NULL default '0',
  max_inc int(10) NOT NULL default '0',
  lastupdate datetime NOT NULL default '0000-00-00 00:00:00',
  PRIMARY KEY  (param)
) $TABLE_TYPE;

#
# Table structure for table 'admin_privileges' -- multidomain serweb ACL control
#

CREATE TABLE admin_privileges (
  $USERCOL varchar(64) NOT NULL default '',
  domain varchar(128) NOT NULL default '',
  priv_name varchar(64) NOT NULL default '',
  priv_value varchar(64) NOT NULL default '',
  PRIMARY KEY  ($USERCOL,priv_name,priv_value,domain)
) $TABLE_TYPE;

#
# Table structure for table 'calls_forwarding'  -- curently used only for caller screening
#

CREATE TABLE calls_forwarding (
  $USERCOL varchar(64) NOT NULL default '',
  domain varchar(128) NOT NULL default '',
  uri_re varchar(128) NOT NULL default '',
  purpose varchar(32) NOT NULL default '',
  action varchar(32) NOT NULL default '',
  param1 varchar(128) default NULL,
  param2 varchar(128) default NULL,
  PRIMARY KEY  ($USERCOL,domain,uri_re,purpose)
) $TABLE_TYPE;

#
# Table structure for table 'speed_dial'
#

CREATE TABLE speed_dial (
  $USERCOL varchar(64) NOT NULL default '',
  domain varchar(128) NOT NULL default '',
  username_from_req_uri varchar(128) NOT NULL default '',
  domain_from_req_uri varchar(128) NOT NULL default '',
  new_request_uri varchar(128) NOT NULL default '',
  PRIMARY KEY  ($USERCOL,domain,domain_from_req_uri,username_from_req_uri)
) $TABLE_TYPE;



# add an admin user "admin" with password==heslo,
# so that one can try it out on quick start

$INITIAL_USER



EOF

} # ser_create


export PW
if [ "$#" -ne 0 ]; then
  prompt_pw
fi

case $1 in
	reinstall)

		#1 create a backup database (named *_bak)
		echo "creating backup database"
		ser_backup $DBNAME
		if [ "$?" -ne 0 ] ; then
			echo "reinstall: ser_backup failed"
			exit 1
		fi
		#2 dump original database and change names in it
		echo "dumping table content ($DBNAME)"
		tmp_file=/tmp/ser_mysql.$$
		ser_dump $DBNAME  > $tmp_file
		if [ "$?" -ne 0 ] ; then
			echo "reinstall: dumping original db failed"
			exit 1
		fi
		sed "s/[uU][sS][eE][rR]_[iI][dD]/user/g" $tmp_file |
			sed "s/[uU][sS][eE][rR]\($\|[^a-zA-Z]\)/$USERCOL\1/g" |
			sed "s/[rR][eE][aA][lL][mM]/domain/g"> ${tmp_file}.2
		#3 drop original database
		echo "dropping table ($DBNAME)"
		ser_drop $DBNAME
		if [ "$?" -ne 0 ] ; then
			echo "reinstall: dropping table failed"
			rm $tmp_file*
			exit 1
		fi
		#4 change names in table definition and restore
		echo "creating new structures"
		ser_create $DBNAME no_init_user
		if [ "$?" -ne 0 ] ; then
			echo "reinstall: creating new table failed"
			rm $tmp_file*
			exit 1
		fi
		#5 restoring table content
		echo "restoring table content"

		# Recreate perms column here so that subsequent
		# restore succeeds

    sql_query $DBNAME << EOF
    ALTER TABLE subscriber ADD perms VARCHAR(32)  AFTER ha1b;
    ALTER TABLE pending ADD perms VARCHAR(32)  AFTER ha1b;
EOF


		ser_restore $DBNAME ${tmp_file}.2
		if [ "$?" -ne 0 ] ; then
			echo "reinstall: restoring table failed"
			rm $tmp_file*
			exit 1
		fi


    sql_query $DBNAME << EOF

    # Move perms from subscriber to admin_privileges
    INSERT INTO admin_privileges ($USERCOL, domain, priv_name, priv_value) SELECT $USERCOL, domain, 'is_admin', '1' FROM subscriber WHERE perms='admin';

		# Drop perms column here
    ALTER TABLE subscriber DROP perms;
    ALTER TABLE pending DROP perms;

EOF

#XX
#		rm $tmp_file*
		exit 0
		;;
	copy)
		# copy database to some other name
		shift
		if [ $# -ne 1 ]; then
			usage
			exit 1
		fi
		tmp_file=/tmp/ser_mysql.$$
		ser_dump $DBNAME  > $tmp_file
		ret=$?
		if [ "$ret" -ne 0 ]; then
			rm $tmp_file
			exit $ret
		fi
		ser_create $1 no_init_user
		ret=$?
		if [ "$ret" -ne 0 ]; then
			rm $tmp_file
			exit $ret
		fi
		ser_restore $1 $tmp_file
		ret=$?
		rm $tmp_file
		exit $ret
		;;
	backup)
		# backup current database
		ser_dump $DBNAME
		exit $?
		;;
	restore)
		# restore database from a backup
		shift
		if [ $# -ne 1 ]; then
			usage
			exit 1
		fi
		ser_restore $DBNAME $1
		exit $?
		;;
	create)
		# create new database structures
		shift
		if [ $# -eq 1 ] ; then
			DBNAME="$1"
		fi
		ser_create $DBNAME
		exit $?
		;;
	drop)
		# delete ser database
		ser_drop $DBNAME
		exit $?
		;;
	reinit)
		# delete database and create a new one
		ser_drop $DBNAME
		ret=$?
		if [ "$ret" -ne 0 ]; then
			exit $ret
		fi
		ser_create $DBNAME
		exit $?
		;;
	*)
		usage
		exit 1;
		;;
esac

