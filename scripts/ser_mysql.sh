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


#################################################################
# config vars
#################################################################
DBNAME=ser
USERNAME=ser
DEFAULT_PW=heslo
ROUSER=serro
RO_PW=47serro11
CMD="mysql -p -u "
TABLE_TYPE="TYPE=MyISAM"
SQL_USER="root"

#################################################################


usage() {
COMMAND=`basename $0`
cat <<EOF
usage: $COMMAND create
       $COMMAND drop   (!!entirely deletes tables)
       $COMMAND reinit (!!entirely deletes and than re-creates tables)

       if you want to manipulate database as other MySql user than
       root, want to change database name from default value "$DBNAME",
       or want to use other values for users and password, edit the
       "config vars" section of the command $COMMAND

EOF
} #usage

ser_drop()  # pars: <database name> <sql_user>
{
if [ $# -ne 2 ] ; then
	echo "ser_drop function takes two params"
	exit 1
fi

$CMD $2 << EOF
drop database $1;
EOF
} #ser_drop

ser_create () # pars: <database name> <sql_user>
{

#test
#cat > /tmp/sss <<EOF

if [ $# -ne 2 ] ; then
	echo "ser_create function takes two params"
	exit 1
fi

$CMD $2 <<EOF
create database $1;
use $1;

# Users: ser is the regular user, serro only for reading
GRANT ALL PRIVILEGES ON $1.* TO $USERNAME IDENTIFIED  BY '$DEFAULT_PW';
GRANT SELECT ON $1.* TO $ROUSER IDENTIFIED BY '$RO_PW';

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
  sip_callid varchar(128) NOT NULL default '',
  user varchar(64) NOT NULL default '',
  time datetime NOT NULL default '0000-00-00 00:00:00',
  timestamp timestamp(14) NOT NULL
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
#


CREATE TABLE aliases (
  user varchar(50) NOT NULL default '',
  contact varchar(255) NOT NULL default '',
  expires datetime default NULL,
  q float(10,2) default NULL,
  callid varchar(255) default NULL,
  cseq int(11) default NULL,
  last_modified timestamp(14) NOT NULL,
  KEY user (user,contact)
) $TABLE_TYPE;




#
# Table structure for table 'event' -- track of predefined
# events to which a user subscribed
#


CREATE TABLE event (
  id int(10) unsigned NOT NULL auto_increment,
  user varchar(50) NOT NULL default '',
  uri varchar(255) NOT NULL default '',
  description varchar(255) NOT NULL default '',
  PRIMARY KEY  (id),
  UNIQUE KEY id (id)
) $TABLE_TYPE;




#
# Table structure for table 'grp' -- group membership
# table; used primarily for ACLs
#


CREATE TABLE grp (
  user varchar(50) NOT NULL default '',
  grp varchar(50) NOT NULL default '',
  last_modified datetime NOT NULL default '0000-00-00 00:00:00'
) $TABLE_TYPE;




#
# Table structure for table 'location' -- that is persistent UsrLoc
#


CREATE TABLE location (
  user varchar(50) NOT NULL default '',
  contact varchar(255) NOT NULL default '',
  expires datetime default NULL,
  q float(10,2) default NULL,
  callid varchar(255) default NULL,
  cseq int(11) default NULL,
  last_modified timestamp(14) NOT NULL,
  KEY user (user,contact)
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
  sip_callid varchar(128) NOT NULL default '',
  user varchar(64) NOT NULL default '',
  time datetime NOT NULL default '0000-00-00 00:00:00',
  timestamp timestamp(14) NOT NULL
) $TABLE_TYPE;




#
# Table structure for table 'pending' -- unconfirmed subscribtion
# requests
#


CREATE TABLE pending (
  USER_ID varchar(100) NOT NULL default '',
  PASSWORD varchar(25) NOT NULL default '',
  FIRST_NAME varchar(25) NOT NULL default '',
  LAST_NAME varchar(45) NOT NULL default '',
  PHONE varchar(15) NOT NULL default '',
  EMAIL_ADDRESS varchar(50) NOT NULL default '',
  DATETIME_CREATED datetime NOT NULL default '0000-00-00 00:00:00',
  DATETIME_MODIFIED datetime NOT NULL default '0000-00-00 00:00:00',
  confirmation varchar(64) NOT NULL default '',
  flag char(1) NOT NULL default 'o',
  SendNotification varchar(50) NOT NULL default '',
  Greeting varchar(50) NOT NULL default '',
  HA1 varchar(128) NOT NULL default '',
  REALM varchar(128) NOT NULL default '',
  ha1b varchar(128) NOT NULL default '',
  UNIQUE KEY USER_ID (USER_ID),
  KEY USER_ID_2 (USER_ID)
) $TABLE_TYPE;




#
# Table structure for table 'phonebook' -- user's phonebook
#


CREATE TABLE phonebook (
  id int(10) unsigned NOT NULL default '0',
  user varchar(50) NOT NULL default '',
  fname varchar(32) NOT NULL default '',
  lname varchar(32) NOT NULL default '',
  sip_uri varchar(128) NOT NULL default '',
  PRIMARY KEY  (id),
  UNIQUE KEY id (id)
) $TABLE_TYPE;




#
# Table structure for table 'reserved' -- reserved username
# which should be never allowed for subscription
#


CREATE TABLE reserved (
  user_id char(100) NOT NULL default '',
  UNIQUE KEY user_id (user_id)
) $TABLE_TYPE;




#
# Table structure for table 'subscriber' -- user database
# (note: realm is only informational -- it is defined
# in ser scripts)
#


CREATE TABLE subscriber (
  phplib_id varchar(32) NOT NULL default '',
  USER_ID varchar(100) NOT NULL default '',
  PASSWORD varchar(25) NOT NULL default '',
  FIRST_NAME varchar(25) NOT NULL default '',
  LAST_NAME varchar(45) NOT NULL default '',
  PHONE varchar(15) NOT NULL default '',
  EMAIL_ADDRESS varchar(50) NOT NULL default '',
  DATETIME_CREATED datetime NOT NULL default '0000-00-00 00:00:00',
  DATETIME_MODIFIED datetime NOT NULL default '0000-00-00 00:00:00',
  confirmation varchar(64) NOT NULL default '',
  flag char(1) NOT NULL default 'o',
  SendNotification varchar(50) NOT NULL default '',
  Greeting varchar(50) NOT NULL default '',
  HA1 varchar(128) NOT NULL default '',
  REALM varchar(128) NOT NULL default '',
  ha1b varchar(128) NOT NULL default '',
  UNIQUE KEY phplib_id (phplib_id),
  UNIQUE KEY USER_ID (USER_ID),
  KEY USER_ID_2 (USER_ID)
) $TABLE_TYPE;

EOF

} # ser_create


case $1 in
	create)
		ser_create $DBNAME $SQL_USER
		exit $?
		;;
	drop)
		ser_drop $DBNAME $SQL_USER
		exit $?
		;;
	reinit)
		ser_drop $DBNAME $SQL_USER
		ret=$?
		if [ "$ret" -ne 0 ]; then
			exit $ret
		fi
		ser_create $DBNAME $SQL_USER
		exit $?
		;;
	*)
		usage
		exit 1;
		;;
esac

