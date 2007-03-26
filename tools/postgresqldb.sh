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

CMD="psql -h $DBHOST -d template1 -U $DBROOTUSER "
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


# noot needed for postgresql
## read password
#prompt_pw()
#{
#	savetty=`stty -g`
#	printf "Postgres password for $DBROOTUSER: "
#	stty -echo
#	read PW
#	stty $savetty
#	echo
#}

# execute sql command
sql_query()
{
	$CMD "$@"
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

# define constants for database definition
USE_CMD='\connect'
GRANT_CMD="CREATE USER $DBRWUSER WITH PASSWORD '$DBRWPW';
	CREATE USER $DBROUSER WITH PASSWORD '$DBROPW';
	GRANT ALL PRIVILEGES ON TABLE 
		version, 
		acc, acc_id_seq, 
		address, address_id_seq, 
		aliases, aliases_id_seq,
		dbaliases, dbaliases_id_seq, 
		domain, domain_id_seq, 
		grp, grp_id_seq,
		gw, gw_id_seq, 
		gw_grp, gw_grp_grp_id_seq, 
		lcr, lcr_id_seq, 
		location, location_id_seq, 
		missed_calls, missed_calls_id_seq, 
		pdt, pdt_id_seq, 
		re_grp, re_grp_id_seq, 
		silo, silo_id_seq, 
		speed_dial, speed_dial_id_seq, 
		subscriber, subscriber_id_seq, 
		trusted, trusted_id_seq, 
		uri, uri_id_seq, 
		usr_preferences, usr_preferences_id_seq
		TO $DBRWUSER;
	GRANT SELECT ON TABLE 
		version, 
		acc, 
		address, 
		aliases, 
		dbaliases, 
		domain, 
		grp, 
		gw, 
		gw_grp, 
		lcr, 
		location, 
		missed_calls, 
		pdt, 
		re_grp, 
		silo, 
		speed_dial, 
		subscriber, 
		trusted, 
		uri, 
		usr_preferences
		TO $DBROUSER;"
TIMESTAMP="timestamp NOT NULL DEFAULT NOW()"
DATETIME="TIMESTAMP WITHOUT TIME ZONE NOT NULL default '$DUMMY_DATE'"
DATETIMEALIAS="TIMESTAMP WITHOUT TIME ZONE NOT NULL default '$DEFAULT_ALIASES_EXPIRES'"
DATETIMELOCATION="TIMESTAMP WITHOUT TIME ZONE NOT NULL default '$DEFAULT_LOCATION_EXPIRES'"
FLOAT="NUMERIC(10,2)"
TINYINT="NUMERIC(4,0)"
AUTO_INCREMENT="SERIAL PRIMARY KEY"

echo "creating database $1 ..."

#cat <<EOF
sql_query <<EOF
create database $1;
$USE_CMD $1;

/*
 * Table structure versions
 */

CREATE TABLE version (
   table_name varchar(64) NOT NULL primary key,
   table_version smallint DEFAULT '0' NOT NULL
) $TABLE_TYPE;

/*
 * Dumping data for table 'version'
 */

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
INSERT INTO version VALUES ( 'address', '2');

/*
 * Table structure for table 'subscriber' -- user database
 */
CREATE TABLE subscriber (
  id $AUTO_INCREMENT,
  $USERCOL varchar(64) NOT NULL default '',
  domain varchar(128) NOT NULL default '',
  password varchar(25) NOT NULL default '',
  first_name varchar(25) NOT NULL default '',
  last_name varchar(45) NOT NULL default '',
  email_address varchar(50) NOT NULL default '',
  datetime_created $TIMESTAMP,
  ha1 varchar(128) NOT NULL default '',
  ha1b varchar(128) NOT NULL default '',
  timezone varchar(128) default NULL,
  rpid varchar(128) default NULL,
  UNIQUE ($USERCOL, domain)
) $TABLE_TYPE;
CREATE INDEX username_subs_indx ON subscriber ($USERCOL);


/*
 * Table structure for table 'acc' -- accounted calls
 */
CREATE TABLE acc (
  id $AUTO_INCREMENT,
  method varchar(16) NOT NULL default '',
  from_tag varchar(64) NOT NULL default '',
  to_tag varchar(64) NOT NULL default '',
  callid varchar(128) NOT NULL default '',
  sip_code char(3) NOT NULL default '',
  sip_reason varchar(32) NOT NULL default '',
  time $DATETIME
) $TABLE_TYPE;
CREATE INDEX acc_callid_indx ON acc (callid);


/* 
 * Table structure for table 'missed_calls' -- acc-like table
 * for keeping track of missed calls
 */ 
CREATE TABLE missed_calls (
  id $AUTO_INCREMENT,
  method varchar(16) NOT NULL default '',
  from_tag varchar(64) NOT NULL default '',
  to_tag varchar(64) NOT NULL default '',
  callid varchar(128) NOT NULL default '',
  sip_code char(3) NOT NULL default '',
  sip_reason varchar(32) NOT NULL default '',
  time $DATETIME
) $TABLE_TYPE;
CREATE INDEX mc_callid_indx ON missed_calls (callid);



/*
 * Table structure for table 'location' -- that is persistent UsrLoc
 */
CREATE TABLE location (
  id $AUTO_INCREMENT,
  $USERCOL varchar(64) NOT NULL default '',
  domain varchar(128) NOT NULL default '',
  contact varchar(255) NOT NULL default '',
  received varchar(255) default NULL,
  path varchar(255) default NULL,
  expires $DATETIMELOCATION,
  q $FLOAT NOT NULL default '$DEFAULT_Q',
  callid varchar(255) NOT NULL default '$DEFAULT_CALLID',
  cseq int NOT NULL default '$DEFAULT_CSEQ',
  last_modified $TIMESTAMP,
  flags int NOT NULL default '0',
  cflags int NOT NULL default '0',
  user_agent varchar(255) NOT NULL default '',
  socket varchar(128) default NULL,
  methods int default NULL
) $TABLE_TYPE;
CREATE INDEX location_udc_indx ON location ($USERCOL, domain, contact);


/*
 * Table structure for table 'aliases' -- location-like table
 * (aliases_contact index makes lookup of missed calls much faster)
 */
CREATE TABLE aliases (
  id $AUTO_INCREMENT,
  $USERCOL varchar(64) NOT NULL default '',
  domain varchar(128) NOT NULL default '',
  contact varchar(255) NOT NULL default '',
  received varchar(255) default NULL,
  path varchar(255) default NULL,
  expires $DATETIMEALIAS,
  q $FLOAT NOT NULL default '$DEFAULT_Q',
  callid varchar(255) NOT NULL default '$DEFAULT_CALLID',
  cseq int NOT NULL default '$DEFAULT_CSEQ',
  last_modified $TIMESTAMP,
  flags int NOT NULL default '0',
  cflags int NOT NULL default '0',
  user_agent varchar(255) NOT NULL default '',
  socket varchar(128) default NULL,
  methods int default NULL
) $TABLE_TYPE;
CREATE INDEX aliases_udc_indx ON aliases ($USERCOL, domain, contact);


/*
 * DB aliases
 */
CREATE TABLE dbaliases (
  id $AUTO_INCREMENT,
  alias_username varchar(64) NOT NULL default '',
  alias_domain varchar(128) NOT NULL default '',
  $USERCOL varchar(64) NOT NULL default '',
  domain varchar(128) NOT NULL default '',
  UNIQUE (alias_username,alias_domain)
) $TABLE_TYPE;
CREATE INDEX alias_user_indx ON dbaliases ($USERCOL, domain);


/*
 * Table structure for table 'grp' -- group membership
 * table; used primarily for ACLs
 */
CREATE TABLE grp (
  id $AUTO_INCREMENT,
  $USERCOL varchar(64) NOT NULL default '',
  domain varchar(128) NOT NULL default '',
  grp varchar(50) NOT NULL default '',
  last_modified $TIMESTAMP,
  UNIQUE ($USERCOL, domain, grp)
) $TABLE_TYPE;


/*
 * Table structure for table 're_grp' -- group membership
 * based on regular expressions
 */
CREATE TABLE re_grp (
  id $AUTO_INCREMENT,
  reg_exp varchar(128) NOT NULL default '',
  group_id int NOT NULL default '0'
) $TABLE_TYPE;
CREATE INDEX gid_grp_indx ON re_grp (group_id);


/*
 * "instant" message silo
 */
CREATE TABLE silo(
  id $AUTO_INCREMENT,
  src_addr varchar(255) NOT NULL DEFAULT '',
  dst_addr varchar(255) NOT NULL DEFAULT '',
  $USERCOL varchar(64) NOT NULL DEFAULT '',
  domain varchar(128) NOT NULL DEFAULT '',
  inc_time int NOT NULL DEFAULT 0,
  exp_time int NOT NULL DEFAULT 0,
  snd_time int NOT NULL DEFAULT 0,
  ctype varchar(32) NOT NULL DEFAULT 'text/plain',
  body TEXT NOT NULL DEFAULT ''
) $TABLE_TYPE;
CREATE INDEX silo_idx ON silo($USERCOL, domain);

/*
 * Table structure for table 'domain' -- domains proxy is responsible for
 */
CREATE TABLE domain (
  id $AUTO_INCREMENT,
  domain varchar(128) NOT NULL default '',
  last_modified $TIMESTAMP,
  UNIQUE (domain)
) $TABLE_TYPE;


/*
 * Table structure for table 'uri' -- uri user parts users are allowed to use
 */
CREATE TABLE uri (
  id $AUTO_INCREMENT,
  $USERCOL varchar(64) NOT NULL default '',
  domain varchar(128) NOT NULL default '',
  uri_user varchar(50) NOT NULL default '',
  last_modified $TIMESTAMP,
  UNIQUE ($USERCOL, domain, uri_user)
) $TABLE_TYPE;


/*
 * Table structure for table 'usr_preferences'
 */
CREATE TABLE usr_preferences (
  id $AUTO_INCREMENT,
  uuid varchar(64) NOT NULL default '',
  $USERCOL varchar(100) NOT NULL default '0',
  domain varchar(128) NOT NULL default '',
  attribute varchar(32) NOT NULL default '',
  value varchar(128) NOT NULL default '',
  type int NOT NULL default '0',
  last_modified $TIMESTAMP
) $TABLE_TYPE;
CREATE INDEX ua_idx ON usr_preferences(uuid,attribute);
CREATE INDEX uda_idx ON usr_preferences($USERCOL,domain,attribute);


/*
 * Table structure for table trusted
 */
CREATE TABLE trusted (
  id $AUTO_INCREMENT,
  src_ip varchar(39) NOT NULL,
  proto varchar(4) NOT NULL,
  from_pattern varchar(64) default NULL,
  tag varchar(32) default NULL
) $TABLE_TYPE;
CREATE INDEX ip_addr ON trusted(src_ip);


/*
 * Table structure for table 'speed_dial'
 */
CREATE TABLE speed_dial (
  id $AUTO_INCREMENT,
  $USERCOL varchar(64) NOT NULL default '',
  domain varchar(128) NOT NULL default '',
  sd_username varchar(64) NOT NULL default '',
  sd_domain varchar(128) NOT NULL default '',
  new_uri varchar(192) NOT NULL default '',
  fname varchar(128) NOT NULL default '',
  lname varchar(128) NOT NULL default '',
  description varchar(64) NOT NULL default '',
  UNIQUE ($USERCOL,domain,sd_domain,sd_username)
) $TABLE_TYPE;


/*
 * Table structure for table 'gw' (lcr module)
 */
CREATE TABLE gw (
  id $AUTO_INCREMENT,
  gw_name varchar(128) NOT NULL,
  grp_id INT CHECK (grp_id > 0) NOT NULL,
  ip_addr varchar(15) NOT NULL,
  port INT CHECK (port > 0 AND port < 65536),
  uri_scheme SMALLINT CHECK (uri_scheme >= 0 and uri_scheme < 256),
  transport SMALLINT CHECK (transport >= 0 and transport < 256),
  strip SMALLINT CHECK (strip >= 0),
  prefix varchar(16) default NULL,
  UNIQUE (gw_name)
);
CREATE INDEX gw_grp_id_indx ON gw (grp_id);


/*
 * Table structure for table 'gw_grp' (lcr module)
 */
CREATE TABLE gw_grp (
  grp_id SERIAL PRIMARY KEY,
  grp_name varchar(64) NOT NULL
);


/*
 * Table structure for table 'lcr' (lcr module)
 */
CREATE TABLE lcr (
  id $AUTO_INCREMENT,
  prefix varchar(16) NOT NULL,
  from_uri varchar(128) DEFAULT NULL,
  grp_id INT CHECK (grp_id > 0) NOT NULL,
  priority SMALLINT CHECK (priority >= 0 and priority < 256) NOT NULL
);
CREATE INDEX lcr_prefix_indx ON lcr (prefix);
CREATE INDEX lcr_from_uri_indx ON lcr (from_uri);
CREATE INDEX lcr_grp_id_indx ON lcr (grp_id);


/*
 * emulate mysql proprietary functions used by the lcr module
 * in postgresql
 *
 */
CREATE FUNCTION "concat" (text,text) RETURNS text AS 'SELECT \$1 || \$2;' LANGUAGE 'sql';
CREATE FUNCTION "rand" () RETURNS double precision AS 'SELECT random();' LANGUAGE 'sql';


/*
* Table structure for table 'address'
*/
CREATE TABLE address (
  id $AUTO_INCREMENT,
  grp smallint NOT NULL default '0',
  ip_addr varchar(15) NOT NULL default '',
  mask $TINYINT NOT NULL default 32,
  port smallint NOT NULL default '0'
) $TABLE_TYPE; 


/*
 * Table structure for table 'pdt'
 */
CREATE TABLE pdt (
  id $AUTO_INCREMENT,
  sdomain varchar(255) NOT NULL,
  prefix varchar(32) NOT NULL,
  domain varchar(255) NOT NULL DEFAULT '',
  UNIQUE (sdomain, prefix)
) $TABLE_TYPE;


/*
 * GRANT permissions
 */

$GRANT_CMD

EOF

if [ $? -ne 0 ] ; then
	echo "Creating core tables failed!"
	exit 1;
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

GRANT_PRESENCE_CMD="
	GRANT ALL PRIVILEGES ON TABLE 
		active_watchers, active_watchers_id_seq, 
		presentity, presentity_id_seq, 
		watchers, watchers_id_seq, 
		xcap_xml, xcap_xml_id_seq,
		pua, pua_id_seq
		TO $DBRWUSER;
	GRANT SELECT ON TABLE 
		active_watchers, 
		presentity, 
		watchers, 
		xcap_xml,
		pua
		TO $DBROUSER;" 

echo "creating presence tables into $1 ..."

sql_query <<EOF
$USE_CMD $1;

INSERT INTO version VALUES ( 'presentity', '1');
INSERT INTO version VALUES ( 'active_watchers', '1');
INSERT INTO version VALUES ( 'watchers', '1');
INSERT INTO version VALUES ( 'xcap_xml', '1');
INSERT INTO version VALUES ( 'pua', '1');

/*
 * Table structure for table 'presentity'
 * 
 * used by presence module
 */
CREATE TABLE presentity (
  id $AUTO_INCREMENT,
  username varchar(64) NOT NULL,
  domain varchar(124) NOT NULL,
  event varchar(64) NOT NULL,
  etag varchar(64) NOT NULL,
  expires int NOT NULL,
  received_time int NOT NULL,
  body bytea NOT NULL,
  UNIQUE (username, domain, event, etag)
) $TABLE_TYPE;



/*
 * Table structure for table 'active_watchers'
 * 
 * used by presence module
 */
CREATE TABLE active_watchers (
  id $AUTO_INCREMENT,
  to_user varchar(64) NOT NULL,
  to_domain varchar(128) NOT NULL,
  from_user varchar(64) NOT NULL,
  from_domain varchar(128) NOT NULL,
  event varchar(64) NOT NULL default 'presence',
  event_id varchar(64) NOT NULL,
  to_tag varchar(128) NOT NULL,
  from_tag varchar(128) NOT NULL,
  callid varchar(128) NOT NULL,
  local_cseq int NOT NULL,
  remote_cseq int NOT NULL,
  contact varchar(128) NOT NULL,
  record_route varchar(255) NULL,
  expires int NOT NULL,
  status varchar(32) NOT NULL default 'pending',
  version int NOT NULL default '0',
  socket_info varchar(128) NOT NULL,
  local_contact varchar(255) NOT NULL,
  UNIQUE (to_tag)
) $TABLE_TYPE;
CREATE INDEX due_activewatchers ON active_watchers (to_domain,to_user,event);


/*
 * Table structure for table 'watchers'
 * 
 * used by presence module
 */
CREATE TABLE watchers (
  id $AUTO_INCREMENT,
  p_user varchar(64) NOT NULL,
  p_domain varchar(128) NOT NULL,
  w_user varchar(64) NOT NULL,
  w_domain varchar(128) NOT NULL,
  subs_status varchar(64) NOT NULL,
  reason varchar(64) NULL,
  inserted_time int NOT NULL,
  UNIQUE (p_user, p_domain, w_user, w_domain)
) $TABLE_TYPE;



/*
 * Table structure for table 'xcap_xml'
 * 
 * used by presence module
 */
CREATE TABLE xcap_xml (
  id $AUTO_INCREMENT,
  username varchar(66) NOT NULL,
  domain varchar(128) NOT NULL,
  xcap text NOT NULL,
  doc_type int NOT NULL,
  UNIQUE (username, domain, doc_type)
) $TABLE_TYPE;


/*
 * Table structure for table 'pua'
 * 
 * used by pua module
 */

CREATE TABLE pua (
  id $AUTO_INCREMENT,
  pres_uri varchar(128) NOT NULL,
  pres_id varchar(128) NOT NULL,
  expires int NOT NULL,
  flag int NOT NULL,
  etag varchar(128) NOT NULL,
  tuple_id varchar(128) NOT NULL,
  watcher_uri varchar(128) NOT NULL,
  call_id varchar(128) NOT NULL,
  to_tag varchar(128) NOT NULL,
  from_tag varchar(128) NOT NULL,
  cseq int NOT NULL
) $TABLE_TYPE;


$GRANT_PRESENCE_CMD

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

GRANT_EXTRA_CMD="
	GRANT ALL PRIVILEGES ON TABLE 
		cpl, cpl_id_seq,
		imc_members, imc_members_id_seq,
		imc_rooms, imc_rooms_id_seq,
		sip_trace, sip_trace_id_seq,
		domainpolicy, domainpolicy_id_seq
		TO $DBRWUSER;
	GRANT SELECT ON TABLE 
		cpl,
		imc_members,
		imc_rooms,
		sip_trace,
		domainpolicy
		TO $DBROUSER;"

echo "creating extra tables into $1 ..."

sql_query <<EOF
$USE_CMD $1;

INSERT INTO version VALUES ( 'cpl', '1');
INSERT INTO version VALUES ( 'imc_members', '1');
INSERT INTO version VALUES ( 'imc_rooms', '1');
INSERT INTO version VALUES ( 'sip_trace', '1');
INSERT INTO version VALUES ( 'domainpolicy', '2');

/*
 * Table structure for table 'cpl'
 * 
 * used by cpl-c module
 */
CREATE TABLE cpl (
  id $AUTO_INCREMENT,
  username varchar(64) NOT NULL,
  domain varchar(64) NOT NULL default '',
  cpl_xml text,
  cpl_bin text,
  UNIQUE (username, domain)
) $TABLE_TYPE;


/*
 * Table structure for table 'imc_members'
 * 
 * used by imc module
 */
CREATE TABLE imc_members (
  id $AUTO_INCREMENT,
  username varchar(128) NOT NULL,
  domain varchar(128) NOT NULL,
  room varchar(64) NOT NULL,
  flag int NOT NULL,
  UNIQUE (username,domain,room)
) $TABLE_TYPE;


/*
 * Table structure for table 'imc_rooms'
 * 
 * used by imc module
 */
CREATE TABLE imc_rooms (
  id $AUTO_INCREMENT,
  name varchar(128) NOT NULL,
  domain varchar(128) NOT NULL,
  flag int NOT NULL,
  UNIQUE (name,domain)
) $TABLE_TYPE;


/*
 * Table structure for table 'siptrace'
 */
CREATE TABLE sip_trace (
  id $AUTO_INCREMENT,
  date $TIMESTAMP,
  callid varchar(254) NOT NULL default '',
  traced_user varchar(128) NOT NULL default '',
  msg text NOT NULL,
  method varchar(50) NOT NULL default '',
  status varchar(254) NOT NULL default '',
  fromip varchar(50) NOT NULL default '',
  toip varchar(50) NOT NULL default '',
  fromtag varchar(64) NOT NULL default '',
  direction varchar(4) NOT NULL default ''
) $TABLE_TYPE;
CREATE INDEX user_idx ON sip_trace (traced_user);
CREATE INDEX date_idx ON sip_trace (date);
CREATE INDEX fromip_idx ON sip_trace (fromip);
CREATE INDEX callid_idx ON sip_trace (callid);


/*
 * domainpolicy table (see README domainpolicy module)
 */
CREATE TABLE domainpolicy (
  id $AUTO_INCREMENT,
  rule VARCHAR(255) NOT NULL,
  type VARCHAR(255) NOT NULL,
  att VARCHAR(255),
  val VARCHAR(255),
  comment VARCHAR(255),
  UNIQUE ( rule, att, val )
);
CREATE INDEX domainpolicy_rule_idx ON domainpolicy(rule);

$GRANT_EXTRA_CMD

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

GRANT_SERWEB_CMD="
	GRANT ALL PRIVILEGES ON TABLE phonebook, phonebook_id_seq, 
		pending, pending_id_seq, active_sessions,
		server_monitoring, server_monitoring_agg,
		usr_preferences_types, admin_privileges to $DBRWUSER; 
	GRANT SELECT ON TABLE phonebook, pending, active_sessions,
		server_monitoring, server_monitoring_agg,
		usr_preferences_types, admin_privileges to $DBROUSER;" 

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
			'root@localhost', '2002-09-04 19:37:45', '$DUMMY_DATE',
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
$USE_CMD $1;

INSERT INTO version VALUES ( 'phonebook', '1');
INSERT INTO version VALUES ( 'pending', '6');
INSERT INTO version VALUES ( 'active_sessions', '1');
INSERT INTO version VALUES ( 'server_monitoring', '1');
INSERT INTO version VALUES ( 'server_monitoring_agg', '1');
INSERT INTO version VALUES ( 'usr_preferences_types', '1');
INSERT INTO version VALUES ( 'admin_privileges', '1');

/*
 * Extend table 'subscriber' with serweb specific columns
 * It would be easier to drop the table and create a new one,
 * but in case someone want to add serweb and has already
 * a populated subscriber table, we show here how this
 * can be done without deleting the existing data.
 * Tested with postgres 7.4.7
 */
ALTER TABLE subscriber ADD COLUMN phplib_id varchar(32);
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
                      greeting = DEFAULT, allow_find = DEFAULT ;

/*
 * Table structure for table 'active_sessions' -- web stuff
 */
CREATE TABLE active_sessions (
  sid varchar(32) NOT NULL default '',
  name varchar(32) NOT NULL default '',
  val text,
  changed varchar(14) NOT NULL default '',
  PRIMARY KEY  (name,sid)
) $TABLE_TYPE;
CREATE INDEX ch_active_sess_indx ON active_sessions (changed);


/*
 * Table structure for table 'pending' -- unconfirmed subscribtion
 * requests
 */
CREATE TABLE pending (
  id $AUTO_INCREMENT,
  phplib_id varchar(32) NOT NULL default '',
  $USERCOL varchar(64) NOT NULL default '',
  domain varchar(128) NOT NULL default '',
  password varchar(25) NOT NULL default '',
  first_name varchar(25) NOT NULL default '',
  last_name varchar(45) NOT NULL default '',
  phone varchar(15) NOT NULL default '',
  email_address varchar(50) NOT NULL default '',
  datetime_created $TIMESTAMP,
  datetime_modified $TIMESTAMP,
  confirmation varchar(64) NOT NULL default '',
  flag char(1) NOT NULL default 'o',
  sendnotification varchar(50) NOT NULL default '',
  greeting varchar(50) NOT NULL default '',
  ha1 varchar(128) NOT NULL default '',
  ha1b varchar(128) NOT NULL default '',
  allow_find char(1) NOT NULL default '0',
  timezone varchar(128) default NULL,
  rpid varchar(128) default NULL,
  UNIQUE ($USERCOL, domain),
  UNIQUE (phplib_id)
) $TABLE_TYPE;
CREATE INDEX username_pend_indx ON pending ($USERCOL);


/*
 * Table structure for table 'phonebook' -- user's phonebook
 */
CREATE TABLE phonebook (
  id $AUTO_INCREMENT,
  $USERCOL varchar(64) NOT NULL default '',
  domain varchar(128) NOT NULL default '',
  fname varchar(32) NOT NULL default '',
  lname varchar(32) NOT NULL default '',
  sip_uri varchar(128) NOT NULL default ''
) $TABLE_TYPE;


/*
 * Table structure for table 'server_monitoring'
 */
CREATE TABLE server_monitoring (
  time $TIMESTAMP,
  id bigint NOT NULL default '0',
  param varchar(32) NOT NULL default '',
  value int NOT NULL default '0',
  increment int NOT NULL default '0',
  PRIMARY KEY  (id,param)
) $TABLE_TYPE;


/*
 * Table structure for table 'usr_preferences_types' -- types of atributes 
 * in preferences
 */
CREATE TABLE usr_preferences_types (
  att_name varchar(32) NOT NULL default '',
  att_rich_type varchar(32) NOT NULL default 'string',
  att_raw_type int NOT NULL default '2',
  att_type_spec text,
  default_value varchar(100) NOT NULL default '',
  PRIMARY KEY  (att_name)
) $TABLE_TYPE;


/*
 * Table structure for table 'server_monitoring_agg'
 */
CREATE TABLE server_monitoring_agg (
  param varchar(32) NOT NULL default '',
  s_value int NOT NULL default '0',
  s_increment int NOT NULL default '0',
  last_aggregated_increment int NOT NULL default '0',
  av real NOT NULL default '0',
  mv int NOT NULL default '0',
  ad real NOT NULL default '0',
  lv int NOT NULL default '0',
  min_val int NOT NULL default '0',
  max_val int NOT NULL default '0',
  min_inc int NOT NULL default '0',
  max_inc int NOT NULL default '0',
  lastupdate $TIMESTAMP,
  PRIMARY KEY  (param)
) $TABLE_TYPE;


/*
 * Table structure for table 'admin_privileges' -- multidomain serweb
 * ACL control
 */

CREATE TABLE admin_privileges (
  $USERCOL varchar(64) NOT NULL default '',
  domain varchar(128) NOT NULL default '',
  priv_name varchar(64) NOT NULL default '',
  priv_value varchar(64) NOT NULL default '',
  PRIMARY KEY  ($USERCOL,priv_name,priv_value,domain)
) $TABLE_TYPE;


/*
 * emulate mysql proprietary functions used by the serweb
 * in postgresql
 */
CREATE FUNCTION "truncate" (numeric,int) RETURNS numeric AS 'SELECT trunc(\$1,\$2);' LANGUAGE 'sql';
create function unix_timestamp(timestamp) returns integer as 'select date_part(''epoch'', \$1)::int4 as result' language 'sql';

$INITIAL_INSERT

$GRANT_SERWEB_CMD

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

