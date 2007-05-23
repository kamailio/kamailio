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

# type of mysql tables
if [ -z "$TABLE_TYPE" ]; then
	TABLE_TYPE="TYPE=MyISAM"
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
if [ -z "$GREP" ]; then
	GREP="egrep"
fi
if [ -z "$SED" ]; then
	SED="sed"
fi

FOREVER="2020-05-28 21:32:15"

DEFAULT_ALIASES_EXPIRES=$FOREVER
DEFAULT_Q="1.0"
DEFAULT_CALLID="Default-Call-ID"
DEFAULT_CSEQ="13"
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

# execute sql command
sql_query()
{
	if [ "X$PW" = "X" ] ; then
		$CMD "$@"
	else
		$CMD "-p$PW" "$@"
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
	sql_query <<EOF
	create database $1_bak;
EOF

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

sql_query << EOF
drop database $1;
EOF
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

sql_query <<EOF
create database $1 character set $CHARSET;
use $1;

# Users: ser is the regular user, serro only for reading
GRANT ALL PRIVILEGES ON $1.* TO $DBRWUSER IDENTIFIED  BY '$DBRWPW';
GRANT ALL PRIVILEGES ON $1.* TO ${DBRWUSER}@$DBHOST IDENTIFIED BY '$DBRWPW';
GRANT SELECT ON $1.* TO $DBROUSER IDENTIFIED BY '$DBROPW';
GRANT SELECT ON $1.* TO ${DBROUSER}@$DBHOST IDENTIFIED BY '$DBROPW';


#
# Table structure versions
#

CREATE TABLE version (
   table_name varchar(64) NOT NULL primary key,
   table_version smallint(5) DEFAULT '0' NOT NULL
) $TABLE_TYPE;

#
# Dumping data for table 'version'
#

INSERT INTO version VALUES ( 'subscriber', '6');
INSERT INTO version VALUES ( 'missed_calls', '3');
INSERT INTO version VALUES ( 'location', '1004');
INSERT INTO version VALUES ( 'aliases', '1004');
INSERT INTO version VALUES ( 'grp', '2');
INSERT INTO version VALUES ( 're_grp', '1');
INSERT INTO version VALUES ( 'acc', '4');
INSERT INTO version VALUES ( 'silo', '5');
INSERT INTO version VALUES ( 'domain', '1');
INSERT INTO version VALUES ( 'uri', '1');
INSERT INTO version VALUES ( 'trusted', '4');
INSERT INTO version VALUES ( 'usr_preferences', '2');
INSERT INTO version VALUES ( 'speed_dial', '2');
INSERT INTO version VALUES ( 'dbaliases', '1');
INSERT INTO version VALUES ( 'gw', '4');
INSERT INTO version VALUES ( 'gw_grp', '1');
INSERT INTO version VALUES ( 'lcr', '2');
INSERT INTO version VALUES ( 'address', '3');


#
# Table structure for table 'subscriber' -- user database
#
CREATE TABLE subscriber (
  id int(10) unsigned NOT NULL auto_increment,
  $USERCOL varchar(64) NOT NULL default '',
  domain varchar(128) NOT NULL default '',
  password varchar(25) NOT NULL default '',
  first_name varchar(25) NOT NULL default '',
  last_name varchar(45) NOT NULL default '',
  email_address varchar(50) NOT NULL default '',
  datetime_created datetime NOT NULL default '0000-00-00 00:00:00',
  ha1 varchar(128) NOT NULL default '',
  ha1b varchar(128) NOT NULL default '',
  timezone varchar(128) default NULL,
  rpid varchar(128) default NULL,
  PRIMARY KEY (id),
  UNIQUE KEY user_id ($USERCOL, domain),
  INDEX username_id ($USERCOL)
) $TABLE_TYPE;


#
# Table structure for table 'acc' -- accounted calls
#
CREATE TABLE acc (
  id int(10) unsigned NOT NULL auto_increment,
  method varchar(16) NOT NULL default '',
  from_tag varchar(64) NOT NULL default '',
  to_tag varchar(64) NOT NULL default '',
  callid varchar(128) NOT NULL default '',
  sip_code char(3) NOT NULL default '',
  sip_reason varchar(32) NOT NULL default '',
  time datetime NOT NULL,
  INDEX acc_callid (callid),
  PRIMARY KEY  (id)
) $TABLE_TYPE;


#
# Table structure for table 'missed_calls' -- acc-like table
# for keeping track of missed calls
#
CREATE TABLE missed_calls (
  id int(10) unsigned NOT NULL auto_increment,
  method varchar(16) NOT NULL default '',
  from_tag varchar(64) NOT NULL default '',
  to_tag varchar(64) NOT NULL default '',
  callid varchar(128) NOT NULL default '',
  sip_code char(3) NOT NULL default '',
  sip_reason varchar(32) NOT NULL default '',
  time datetime NOT NULL,
  INDEX acc_callid (callid),
  PRIMARY KEY  (id)
) $TABLE_TYPE;


#
# Table structure for table 'location' -- that is persistent UsrLoc
#
CREATE TABLE location (
  id int(10) unsigned NOT NULL auto_increment,
  $USERCOL varchar(64) NOT NULL default '',
  domain varchar(128) NOT NULL default '',
  contact varchar(255) NOT NULL default '',
  received varchar(255) default NULL,
  path varchar(255) default NULL,
  expires datetime NOT NULL default '$DEFAULT_LOCATION_EXPIRES',
  q float(10,2) NOT NULL default '$DEFAULT_Q',
  callid varchar(255) NOT NULL default '$DEFAULT_CALLID',
  cseq int(11) NOT NULL default '$DEFAULT_CSEQ',
  last_modified datetime NOT NULL default "1900-01-01 00:00",
  flags int(11) NOT NULL default '0',
  cflags int(11) NOT NULL default '0',
  user_agent varchar(255) NOT NULL default '',
  socket varchar(128) default NULL,
  methods int(11) default NULL,
  INDEX udc_loc ($USERCOL, domain, contact),
  PRIMARY KEY  (id)
) $TABLE_TYPE;


#
# Table structure for table 'aliases' -- location-like table
# (aliases_contact index makes lookup of missed calls much faster)
#
CREATE TABLE aliases (
  id int(10) unsigned NOT NULL auto_increment,
  $USERCOL varchar(64) NOT NULL default '',
  domain varchar(128) NOT NULL default '',
  contact varchar(255) NOT NULL default '',
  received varchar(255) default NULL,
  path varchar(255) default NULL,
  expires datetime NOT NULL default '$DEFAULT_ALIASES_EXPIRES',
  q float(10,2) NOT NULL default '$DEFAULT_Q',
  callid varchar(255) NOT NULL default '$DEFAULT_CALLID',
  cseq int(11) NOT NULL default '$DEFAULT_CSEQ',
  last_modified datetime NOT NULL default "1900-01-01 00:00",
  flags int(11) NOT NULL default '0',
  cflags int(11) NOT NULL default '0',
  user_agent varchar(255) NOT NULL default '',
  socket varchar(128) default NULL,
  methods int(11) default NULL,
  INDEX udc_als($USERCOL, domain, contact),
  PRIMARY KEY  (id)
) $TABLE_TYPE;


#
# DB aliases
#
CREATE TABLE dbaliases (
  id int(10) unsigned NOT NULL auto_increment,
  alias_username varchar(64) NOT NULL default '',
  alias_domain varchar(128) NOT NULL default '',
  $USERCOL varchar(64) NOT NULL default '',
  domain varchar(128) NOT NULL default '',
  UNIQUE KEY alias_key (alias_username,alias_domain),
  INDEX alias_user ($USERCOL, domain),
  PRIMARY KEY  (id)
) $TABLE_TYPE;


#
# Table structure for table 'grp' -- group membership
# table; used primarily for ACLs
#
CREATE TABLE grp (
  id int(10) unsigned NOT NULL auto_increment,
  $USERCOL varchar(64) NOT NULL default '',
  domain varchar(128) NOT NULL default '',
  grp varchar(50) NOT NULL default '',
  last_modified datetime NOT NULL default '0000-00-00 00:00:00',
  UNIQUE KEY udg_grp($USERCOL, domain, grp),
  PRIMARY KEY  (id)
) $TABLE_TYPE;


#
# Table structure for table 're_grp' -- group membership
# based on regular expressions
#
CREATE TABLE re_grp (
  id int(10) unsigned NOT NULL auto_increment,
  reg_exp varchar(128) NOT NULL default '',
  group_id int(11) NOT NULL default '0',
  INDEX gid_grp (group_id),
  PRIMARY KEY  (id)
) $TABLE_TYPE;


#
# "instant" message silo
#
CREATE TABLE silo(
  id int(10) unsigned NOT NULL auto_increment,
  src_addr VARCHAR(255) NOT NULL DEFAULT "",
  dst_addr VARCHAR(255) NOT NULL DEFAULT "",
  $USERCOL VARCHAR(64) NOT NULL DEFAULT "",
  domain VARCHAR(128) NOT NULL DEFAULT "",
  inc_time INTEGER NOT NULL DEFAULT 0,
  exp_time INTEGER NOT NULL DEFAULT 0,
  snd_time INTEGER NOT NULL DEFAULT 0,
  ctype VARCHAR(32) NOT NULL DEFAULT "text/plain",
  body BLOB NOT NULL DEFAULT "",
  INDEX ud_silo ($USERCOL, domain),
  PRIMARY KEY  (id)
) $TABLE_TYPE;


#
# Table structure for table 'domain' -- domains proxy is responsible for
#
CREATE TABLE domain (
  id int(10) unsigned NOT NULL auto_increment,
  domain varchar(128) NOT NULL default '',
  last_modified datetime NOT NULL default '0000-00-00 00:00:00',
  UNIQUE KEY d_domain (domain),
  PRIMARY KEY  (id)
) $TABLE_TYPE;


#
# Table structure for table 'uri' -- uri user parts users are allowed to use
#
CREATE TABLE uri (
  id int(10) unsigned NOT NULL auto_increment,
  $USERCOL varchar(64) NOT NULL default '',
  domain varchar(128) NOT NULL default '',
  uri_user varchar(50) NOT NULL default '',
  last_modified datetime NOT NULL default '0000-00-00 00:00:00',
  UNIQUE KEY udu_uri ($USERCOL, domain, uri_user),
  PRIMARY KEY  (id)
) $TABLE_TYPE;


#
# Table structure for table 'usr_preferences'
#
CREATE TABLE usr_preferences (
  id int(10) NOT NULL auto_increment,
  uuid varchar(64) NOT NULL default '',
  $USERCOL varchar(64) NOT NULL default '0',
  domain varchar(128) NOT NULL default '',
  attribute varchar(32) NOT NULL default '',
  type int(11) NOT NULL default '0',
  value varchar(128) NOT NULL default '',
  last_modified timestamp(14) NOT NULL default '0000-00-00 00:00:00',
  INDEX ua_idx  (uuid,attribute),
  INDEX uda_idx ($USERCOL,domain,attribute),
  PRIMARY KEY (id)
) $TABLE_TYPE;


#
# Table structure for table trusted
#
CREATE TABLE trusted (
  id int(10) NOT NULL auto_increment,
  src_ip varchar(39) NOT NULL,
  proto varchar(4) NOT NULL,
  from_pattern varchar(64) DEFAULT NULL,
  tag varchar(32) DEFAULT NULL,
  KEY Key1 (src_ip),
  PRIMARY KEY (id)
) $TABLE_TYPE;


#
# Table structure for table 'speed_dial'
#
CREATE TABLE speed_dial (
  id int(10) NOT NULL auto_increment,
  $USERCOL varchar(64) NOT NULL default '',
  domain varchar(128) NOT NULL default '',
  sd_username varchar(64) NOT NULL default '',
  sd_domain varchar(128) NOT NULL default '',
  new_uri varchar(192) NOT NULL default '',
  fname varchar(128) NOT NULL default '',
  lname varchar(128) NOT NULL default '',
  description varchar(64) NOT NULL default '',
  UNIQUE KEY udss_sd ($USERCOL,domain,sd_domain,sd_username),
  PRIMARY KEY (id)
) $TABLE_TYPE;


#
# Table structure for table 'gw'
#
CREATE TABLE gw (
  id int(10) NOT NULL auto_increment,
  gw_name VARCHAR(128) NOT NULL,
  grp_id INT UNSIGNED NOT NULL,
  ip_addr varchar(15) NOT NULL,
  port SMALLINT UNSIGNED,
  uri_scheme TINYINT UNSIGNED,
  transport TINYINT UNSIGNED,
  strip TINYINT UNSIGNED,
  prefix varchar(16) default NULL,
  UNIQUE KEY name_gw (gw_name),
  KEY gid_gw (grp_id),
  PRIMARY KEY (id)
) $TABLE_TYPE;


#
# Table structure for table 'gw_grp'
#
CREATE TABLE gw_grp (
  grp_id INT UNSIGNED NOT NULL auto_increment,
  grp_name VARCHAR(64) NOT NULL,
  PRIMARY KEY (grp_id)
) $TABLE_TYPE;


#
# Table structure for table 'lcr'
#
CREATE TABLE lcr (
  id int(10) NOT NULL auto_increment,
  prefix varchar(16) NOT NULL,
  from_uri varchar(128) DEFAULT NULL,
  grp_id INT UNSIGNED NOT NULL,
  priority TINYINT UNSIGNED NOT NULL,
  KEY (prefix),
  KEY (from_uri),
  KEY (grp_id),
  PRIMARY KEY (id)
) $TABLE_TYPE;


#
# Table structure for table 'address'
#
CREATE TABLE address (
  id int(10) NOT NULL auto_increment,
  grp smallint(5) unsigned NOT NULL default '0',
  ip_addr varchar(15) NOT NULL,
  mask tinyint NOT NULL default 32,
  port smallint(5) unsigned NOT NULL default '0',
  PRIMARY KEY (id)
) $TABLE_TYPE;


/*
 * Table structure for table 'pdt'
 */
CREATE TABLE pdt (
  id int(10) NOT NULL auto_increment,
  sdomain varchar(255) NOT NULL,
  prefix varchar(32) NOT NULL,
  domain varchar(255) NOT NULL DEFAULT '',
  UNIQUE KEY sp_pdt (sdomain, prefix),
  PRIMARY KEY (id)
) $TABLE_TYPE;
EOF

if [ $? -ne 0 ] ; then
	echo "Creating core tables failed!"
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

sql_query <<EOF
use $1;

INSERT INTO version VALUES ( 'presentity', '1');
INSERT INTO version VALUES ( 'active_watchers', '4');
INSERT INTO version VALUES ( 'watchers', '1');
INSERT INTO version VALUES ( 'xcap_xml', '1');
INSERT INTO version VALUES ( 'pua', '3');

#
# Table structure for table 'presentity'
# 
# used by presence module
#
CREATE TABLE presentity (
  id int(10) NOT NULL auto_increment,
  username varchar(64) NOT NULL,
  domain varchar(124) NOT NULL,
  event varchar(64) NOT NULL,
  etag varchar(64) NOT NULL,
  expires int(11) NOT NULL,
  received_time int(11) NOT NULL,
  body text NOT NULL,
  UNIQUE KEY udee_presentity (username,domain,event,etag),
  PRIMARY KEY (id)
) $TABLE_TYPE;


#
# Table structure for table 'active_watchers'
# 
# used by presence module
#

CREATE TABLE active_watchers (
  id int(10) NOT NULL auto_increment,
  pres_user varchar(64) NOT NULL,
  pres_domain varchar(128) NOT NULL,
  to_user varchar(64) NOT NULL,
  to_domain varchar(128) NOT NULL,
  from_user varchar(64) NOT NULL,
  from_domain varchar(128) NOT NULL,
  event varchar(64) NOT NULL default 'presence',
  event_id varchar(64),
  to_tag varchar(128) NOT NULL,
  from_tag varchar(128) NOT NULL,
  callid varchar(128) NOT NULL,
  local_cseq int(11) NOT NULL,
  remote_cseq int(11) NOT NULL,
  contact varchar(128) NOT NULL,
  record_route text,
  expires int(11) NOT NULL,
  status varchar(32) NOT NULL default 'pending',
  version int(11) default '0',
  socket_info varchar(128) NOT NULL,
  local_contact varchar(255) NOT NULL,

  PRIMARY KEY  (id),
  UNIQUE KEY tt_watchers (to_tag),
  KEY due_activewatchers (to_domain,to_user,event)
) $TABLE_TYPE;
#
# Table structure for table 'watchers'
# 
# used by presence module
#

CREATE TABLE watchers (
  id int(10) NOT NULL auto_increment,
  p_user varchar(64) NOT NULL,
  p_domain varchar(128) NOT NULL,
  w_user varchar(64) NOT NULL,
  w_domain varchar(128) NOT NULL,
  subs_status varchar(64) NOT NULL,
  reason varchar(64), 
  inserted_time int(11) NOT NULL,
  UNIQUE KEY udud_watchers (p_user,p_domain,w_user,w_domain),
  PRIMARY KEY (id)
) $TABLE_TYPE;


#
# Table structure for table 'xcap_xml'
# 
# used by presence module
#

CREATE TABLE xcap_xml (
  id int(10) NOT NULL auto_increment,
  username varchar(66) NOT NULL,
  domain varchar(128) NOT NULL,
  xcap text NOT NULL,
  doc_type int(11) NOT NULL,
  UNIQUE KEY udd_xcap (username,domain,doc_type),
  PRIMARY KEY (id)
) $TABLE_TYPE;

#
# Table structure for table 'pua'
# 
# used by pua module
#
CREATE TABLE pua (
  id int(10) unsigned NOT NULL auto_increment,
  pres_uri varchar(128) NOT NULL,
  pres_id varchar(128) NOT NULL,
  event int(11) NOT NULL,  
  expires int(11) NOT NULL,
  flag int(11) NOT NULL,
  etag varchar(128) NOT NULL,
  tuple_id varchar(128) NOT NULL,
  watcher_uri varchar(128) NOT NULL,
  call_id varchar(128) NOT NULL,
  to_tag varchar(128) NOT NULL,
  from_tag varchar(128) NOT NULL,
  cseq int(11) NOT NULL,
  record_route text NULL,
  version int(11) NOT NULL,
  PRIMARY KEY  (id)
) $TABLE_TYPE;
EOF

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

sql_query <<EOF
use $1;

INSERT INTO version VALUES ( 'cpl', '1');
INSERT INTO version VALUES ( 'imc_members', '1');
INSERT INTO version VALUES ( 'imc_rooms', '1');
INSERT INTO version VALUES ( 'sip_trace', '1');
INSERT INTO version VALUES ( 'domainpolicy', '2');


#
# Table structure for table 'cpl'
#
# used by cpl-c module
#
CREATE TABLE cpl (
  id int(10) unsigned NOT NULL auto_increment,
  username varchar(64) NOT NULL,
  domain varchar(64) NOT NULL default '',
  cpl_xml text,
  cpl_bin text,
  UNIQUE KEY ud_cpl (username, domain),
  PRIMARY KEY (id)
) $TABLE_TYPE;


#
# Table structure for table 'imc_members'
#
# used by imc module
#
CREATE TABLE imc_members (
  id int(10) unsigned NOT NULL auto_increment,
  username varchar(128) NOT NULL,
  domain varchar(128) NOT NULL,
  room varchar(64) NOT NULL,
  flag int(11) NOT NULL,
  UNIQUE KEY ndr_imc (username,domain,room),
  PRIMARY KEY (id)
) $TABLE_TYPE;


#
# Table structure for table 'imc_rooms'
#
# used by imc module
#
CREATE TABLE imc_rooms (
  id int(10) unsigned NOT NULL auto_increment,
  name varchar(128) NOT NULL,
  domain varchar(128) NOT NULL,
  flag int(11) NOT NULL,
  UNIQUE KEY nd_imc (name,domain),
  PRIMARY KEY (id)
) $TABLE_TYPE;


#
# Table structure for table 'siptrace'
#
CREATE TABLE sip_trace (
  id int(10) NOT NULL auto_increment,
  date datetime NOT NULL default '0000-00-00 00:00:00',
  callid varchar(254) NOT NULL default '',
  traced_user varchar(128) NOT NULL default '',
  msg text NOT NULL,
  method varchar(50) NOT NULL default '',
  status varchar(254) NOT NULL default '',
  fromip varchar(50) NOT NULL default '',
  toip varchar(50) NOT NULL default '',
  fromtag varchar(64) NOT NULL default '',
  direction varchar(4) NOT NULL default '',
  INDEX user_idx (traced_user),
  INDEX date_id (date),
  INDEX ip_idx (fromip),
  KEY call_id (callid),
  PRIMARY KEY  (id)
) $TABLE_TYPE;


#
# domainpolicy table (see README domainpolicy module)
#
CREATE TABLE domainpolicy (
  id INT(10) NOT NULL AUTO_INCREMENT,
  rule VARCHAR(255) NOT NULL,
  type VARCHAR(255) NOT NULL,
  att VARCHAR(255),
  val VARCHAR(255),
  comment VARCHAR(255),
  UNIQUE (rule, att, val),
  INDEX  rule_idx (rule),
  PRIMARY KEY (id)
) $TABLE_TYPE;

EOF

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

if [ -z "$NO_USER_INIT" ] ; then
	if [ -z "$SIP_DOMAIN" ] ; then
		prompt_realm
	fi
	credentials
	INITIAL_INSERT="
		INSERT INTO subscriber
			($USERCOL, password, first_name, last_name, phone,
			email_address, datetime_created, datetime_modified, confirmation,
			flag, sendnotification, greeting, ha1, domain, ha1b, phplib_id )
			VALUES ( 'admin', '$DBRWPW', 'Initial', 'Admin', '123',
			'root@localhost', '2002-09-04 19:37:45', '0000-00-00 00:00:00',
			'57DaSIPuCm52UNe54LF545750cfdL48OMZfroM53', 'o', '', '',
			'$HA1', '$SIP_DOMAIN', '$HA1B',
			'$PHPLIB_ID' );
		INSERT INTO admin_privileges ($USERCOL, domain, priv_name, priv_value)
		VALUES ('admin', '$SIP_DOMAIN', 'is_admin', '1');
		INSERT INTO admin_privileges ($USERCOL, domain, priv_name, priv_value)
		VALUES ('admin', '$SIP_DOMAIN', 'change_privileges', '1');"
else
	INITIAL_INSERT=""
fi

echo "creating serweb tables into $1 ..."

sql_query <<EOF
use $1;

INSERT INTO version VALUES ( 'phonebook', '1');
INSERT INTO version VALUES ( 'pending', '6');
INSERT INTO version VALUES ( 'active_sessions', '1');
INSERT INTO version VALUES ( 'server_monitoring', '1');
INSERT INTO version VALUES ( 'server_monitoring_agg', '1');
INSERT INTO version VALUES ( 'usr_preferences_types', '1');
INSERT INTO version VALUES ( 'admin_privileges', '1');


#
# Extend table 'subscriber' with serweb specific columns
#
ALTER TABLE subscriber 
  ADD COLUMN (
    phplib_id varchar(32) NOT NULL default '',
    phone varchar(15) NOT NULL default '',
    datetime_modified datetime NOT NULL default '0000-00-00 00:00:00',
    confirmation varchar(64) NOT NULL default '',
    flag char(1) NOT NULL default 'o',
    sendnotification varchar(50) NOT NULL default '',
    greeting varchar(50) NOT NULL default '',
    allow_find char(1) NOT NULL default '0'
  ),
  ADD UNIQUE KEY phplib_id (phplib_id)
;


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
# Table structure for table 'pending' -- unconfirmed subscribtion
# requests
#
CREATE TABLE pending (
  id int(10) unsigned NOT NULL auto_increment,
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
  PRIMARY KEY (id),
  UNIQUE KEY user_id ($USERCOL, domain),
  UNIQUE KEY phplib_id (phplib_id),
  INDEX username_id ($USERCOL)
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
# Table structure for table 'server_monitoring'
#
CREATE TABLE server_monitoring (
  time datetime NOT NULL default '0000-00-00 00:00:00',
  id int(10) unsigned NOT NULL default '0',
  param varchar(32) NOT NULL default '',
  value int(10) NOT NULL default '0',
  increment int(10) NOT NULL default '0',
  PRIMARY KEY  (id,param)
) $TABLE_TYPE;


#
# Table structure for table 'usr_preferences_type'
#
CREATE TABLE usr_preferences_types (
  att_name varchar(32) NOT NULL default '',
  att_rich_type varchar(32) NOT NULL default 'string',
  att_raw_type int NOT NULL default '2',
  att_type_spec text,
  default_value varchar(100) NOT NULL default '',
  PRIMARY KEY  (att_name)
) $TABLE_TYPE;


#
# Table structure for table 'server_monitoring_agg'
#
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
# Table structure for table 'admin_privileges' 
# for multidomain serweb ACL control
#
CREATE TABLE admin_privileges (
  $USERCOL varchar(64) NOT NULL default '',
  domain varchar(128) NOT NULL default '',
  priv_name varchar(64) NOT NULL default '',
  priv_value varchar(64) NOT NULL default '',
  PRIMARY KEY  ($USERCOL,priv_name,priv_value,domain)
) $TABLE_TYPE;
$INITIAL_INSERT
EOF

if [ $? -ne 0 ] ; then
	echo "Failed to create serweb tables!"
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

X=`sql_query 2>&1 <<EOF
INSERT into $1 ($2) SELECT $src_cols from $3;
EOF`

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

