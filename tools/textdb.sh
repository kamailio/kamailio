#!/bin/sh
#
# Script for adding and dropping OpenSER DBTEXT tables
#
# USAGE: call the command without any parameters for info
#
#
# History:
# 2007-02-14  Branch from mysqldb.sh script and adapt minimal capabilities(Cesc Santasusana)
#

PATH=$PATH:/usr/local/sbin

### include resource files, if any
if [ -f /etc/openser/.openscdbtextrc ]; then
	. /etc/openser/.openscdbtextrc
fi
if [ -f /usr/local/etc/openser/.openscdbtextrc ]; then
	. /usr/local/etc/openser/.openscdbtextrc
fi
if [ -f ~/.openscdbtextrc ]; then
	. ~/.openscdbtextrc
fi

#################################################################
# config vars
#################################################################
# Default PATH to the DBTEXT folder where the files are
if [ -z "$DBTEXT_PATH" ]; then
	DBTEXT_PATH="/usr/local/etc/openser/dbtext"
fi
#################################################################
#################################################################

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
usage: $COMMAND create [DBTEXT_PATH]
       $COMMAND drop  [DBTEXT_PATH] (!!entirely deletes tables)
       $COMMAND presence [DBTEXT_PATH] (adds the presence related tables)
       $COMMAND extra (adds the extra tables - imc,cpl,siptrace,domainpolicy)
       $COMMAND reinit [DBTEXT_PATH] (!!entirely deletes and then re-creates tables
not implemented:
       $COMMAND backup (dumps current database to stdout)
       $COMMAND restore <file> (restores tables from a file)
       $COMMAND copy <new_db> (creates a new db from an existing one)
       $COMMAND reinstall (updates to a new OpenSER database)

EOF
} #usage


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
	echo "openser_drop function takes one param"
	exit 1
fi

DBTEXT_PATH=$1

echo "DBTEXT ... erasing all files at: $DBTEXT_PATH"
rm -f $DBTEXT_PATH/*
}

openser_create () # pars: <database name>
{
if [ $# -ne 1 ] ; then
	echo "openser_create function takes one param (DBTEXT_PATH)"
	exit 1
fi

DBTEXT_PATH=$1

echo "creating DBTEXT database at: $DBTEXT_PATH ..."

#
# Table structure versions
#

CURRENT_PWD=$PWD
mkdir -p $DBTEXT_PATH
cd $DBTEXT_PATH

touch version
echo "DBTEXT Add Table: version"
echo "table_name(str) table_version(int)" >> version;
#
# Dumping data for table 'version'
#
echo "subscriber:6" 		>> version;
echo "missed_calls:3" 		>> version;
echo "location:1004" 		>> version;
echo "aliases:1004" 		>> version;
echo "grp:2" 				>> version;
echo "re_grp:1" 			>> version;
echo "acc:4" 				>> version;
echo "silo:5" 				>> version;
echo "domain:1" 			>> version;
echo "uri:1" 				>> version;
echo "trusted:4" 			>> version;
echo "usr_preferences:2" 	>> version;
echo "speed_dial:2" 		>> version;
echo "dbaliases:1" 			>> version;
echo "gw:4" 				>> version;
echo "gw_grp:1" 			>> version;
echo "lcr:2" 				>> version;
echo "address:3" 			>> version;


#
# Table structure for table 'subscriber' -- user database
#
touch subscriber;
echo "DBTEXT Add Table: subscriber"
echo "id(int,auto) $USERCOL(str) domain(str) password(str) first_name(str,null) last_name(str,null) email_address(str) datetime_created(int) ha1(str) ha1b(str) timezone(str,null) rpid(str,null)" >> subscriber;

#
# Table structure for table 'acc' -- accounted calls
#
touch acc;
echo "DBTEXT Add Table: acc"
echo "id(int,auto) method(str) from_tag(str) to_tag(str) callid(str) sip_code(str) sip_reason(str) time(int)" >> acc;

#
# Table structure for table 'missed_calls' -- acc-like table
# for keeping track of missed calls
#
touch missed_calls;
echo "DBTEXT Add Table: missed_calls"
echo "id(int,auto) method(str) from_tag(str) to_tag(str) callid(str) sip_code(str) sip_reason(str) time(int)" >> missed_calls;

#
# Table structure for table 'location' -- that is persistent UsrLoc
#
touch location;
echo "DBTEXT Add Table: location"
echo "id(int,auto) $USERCOL(str) domain(str) contact(str) received(str,null) path(str,null) expires(int) q(double) callid(str) cseq(int) last_modified(int) flags(int) cflags(int) user_agent(str) socket(str,null) methods(int,null)" >> location;

#
# Table structure for table 'aliases' -- location-like table
# (aliases_contact index makes lookup of missed calls much faster)
#
touch aliases;
echo "DBTEXT Add Table: aliases"
echo "id(int,auto) $USERCOL(str) domain(str) contact(str) received(str,null) path(str,null) expires(int) q(double) callid(str) cseq(int) last_modified(int) flags(int) cflags(int) user_agent(str) socket(str,null) methods(int,null)" >> aliases;

#
# DB aliases
#
touch dbaliases;
echo "DBTEXT Add Table: dbaliases"
echo "id(int,auto) alias_username(str) alias_domain(str) $USERCOL(str) domain(str)" >> dbaliases;

#
# Table structure for table 'grp' -- group membership
# table; used primarily for ACLs
#
touch grp;
echo "DBTEXT Add Table: grp"
echo "id(int,auto) $USERCOL(str) domain(str) grp(str) last_modified(int)" >> grp;

#
# Table structure for table 're_grp' -- group membership
# based on regular expressions
#
touch re_grp;
echo "DBTEXT Add Table: re_grp"
echo "id(int,auto) reg_exp(str) group_id(str)" >> re_grp;

#
# "instant" message silo
#
touch silo;
echo "DBTEXT Add Table: silo"
echo "WARNING: the body column is declared BLOB in MySQL ... here we do as str!!! Correct?"
echo "id(int,auto) src_addr(str) dst_addr(str) $USERCOL(str) domain(str) inc_time(int) exp_time(int) snd_time(int) ctype(str) body(str)" >> silo;

#
# Table structure for table 'domain' -- domains proxy is responsible for
#
touch domain;
echo "DBTEXT Add Table: domain"
echo "id(int,auto) domain(str) last_modified(int)" >> domain;

#
# Table structure for table 'uri' -- uri user parts users are allowed to use
#
touch uri;
echo "DBTEXT Add Table: uri"
echo "id(int,auto) $USERCOL(str) domain(str) uri_user(str) last_modified(int)" >> uri;

#
# Table structure for table 'usr_preferences'
#
touch usr_preferences;
echo "DBTEXT Add Table: usr_preferences"
echo "id(int,auto) uuid(str) $USERCOL(str) domain(str) attribute(str) type(int) value(str) modified(int)" >> usr_preferences;

#
# Table structure for table trusted
#
touch trusted;
echo "DBTEXT Add Table: trusted"
echo "id(int,auto) src_ip(str) proto(str) from_pattern(str,null) tag(str,null)" >> trusted;

#
# Table structure for table 'speed_dial'
#
touch speed_dial;
echo "DBTEXT Add Table: speed_dial"
echo "id(int,auto) $USERCOL(str) domain(str) sd_username(str) sd_domain(str) new_uri(str) fname(str) lname(str) description(str)" >> speed_dial;

#
# Table structure for table 'gw'
#
touch gw;
echo "DBTEXT Add Table: gw"
echo "id(int,auto) gw_name(str) grp_id(int) ip_addr(str) port(int) uri_scheme(int) transport(int) strip(int) prefix(str,null)" >> gw;

#
# Table structure for table 'gw_grp'
#
touch gw_grp;
echo "DBTEXT Add Table: gw_grp"
echo "grp_id(int,auto) grp_name(str)" >> gw_grp;

#
# Table structure for table 'lcr'
#
touch lcr;
echo "DBTEXT Add Table: lcr"
echo "id(int,auto) prefix(str) from_uri(str,null) grp_id(int) priority(int)" >> lcr;

#
# Table structure for table 'address'
#
touch address;
echo "DBTEXT Add Table: addresses"
echo "id(int,auto) grp(int) ip_addr(str) mask(int) port(int)" >> address;

#
# Table structure for table 'pdt'
# 
touch pdt;
echo "DBTEXT Add Table: pdt"
echo "id(int,auto) sdomain(str) prefix(str) domain(str)" >> pdt;

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

cd $CURRENT_PWD

} # openser_create


presence_create () # pars: <database name>
{
if [ $# -ne 1 ] ; then
	echo "presence_create function takes one param (DBTEXT_PATH)"
	exit 1
fi

DBTEXT_PATH=$1

echo "creating DBTEXT presence database at: $DBTEXT_PATH ..."

#
# Table structure versions
#

CURRENT_PWD=$PWD
mkdir -p $DBTEXT_PATH
cd $DBTEXT_PATH

echo "creating presence tables into $1 ..."

echo "presentity:1" 		>> version;
echo "active_watchers:2" 	>> version;
echo "watchers:1" 			>> version;
echo "xcap_xml:1" 			>> version;
echo "pua:3" 				>> version;

#
# Table structure for table 'presentity'
# 
# used by presence module
#
touch presentity;
echo "DBTEXT Add Table: presentity"
echo "id(int,auto) username(str) domain(str) event(str) etag(str) expires(int) received_time(int) body(str)" >> presentity;

#
# Table structure for table 'active_watchers'
# 
# used by presence module
#
touch active_watchers;
echo "DBTEXT Add Table: active_watchers"
echo "id(int,auto) to_user(str) to_domain(str) from_user(str) from_domain(str) event(str) event_id(str,null) to_tag(str) from_tag(str) callid (str) local_cseq(int) remote_cseq(int) contact(str) record_route(str,null) expires(int)  status(str) version(int) socket_info(str) local_contact(str)" >> active_watchers;

#
# Table structure for table 'watchers'
# 
# used by presence module
#
touch watchers;
echo "DBTEXT Add Table: watchers"
echo "id(int,auto) p_user(str) p_domain(str) w_user(str) w_domain(str) subs_status(str) reason(str,null) inserted_time(int)" >> watchers;

#
# Table structure for table 'xcap_xml'
# 
# used by presence module
#
touch xcap_xml;
echo "DBTEXT Add Table: xcap_xml"
echo "WARNING: Creating xcap_xml table, with column xcap originally BLOB type ... now STR"
echo "id(int,auto) username(str) domain(str) xcap(str) doc_type(str)" >> xcap_xml;

#
# Table structure for table 'pua'
# 
# used by pua module
#
touch pua;
echo "DBTEXT Add Table: pua"
echo "id(int,auto) pres_uri(str) pres_id(str) event(int) expires(int) flag(int) etag(str) tuple_id(str) watcher_uri(str) call_id(str) to_tag(str) from_tag(str) cseq(int) record_route(str) version(int)" >> pua;

cd $CURRENT_PWD
}  # end presence_create


extra_create () # pars: <database name>
{
		echo "extra_create NOT IMPLEMENTED ... quitting";
		return 0;

if [ $# -ne 1 ] ; then
	echo "extra_create function takes one param"
	exit 1
fi

echo "creating extra tables into $1 ..."

echo "cpl:1" 			>> version;
echo "imc_members:1" 	>> version;
echo "imc_rooms:1" 		>> version;
echo "sip_trace:1" 		>> version;
echo "domainpolicy:2" 	>> version;

#
# Table structure for table 'cpl'
#
# used by cpl-c module
#
touch cpl;
echo "DBTEXT Add Table: cpl"
echo "id(int,auto) username(str) domain(str) cpl_xml(str) cpl_bin(str)" >> cpl;


#
# Table structure for table 'imc_members'
#
# used by imc module
#
touch imc_members;
echo "DBTEXT Add Table: imc_members"
echo "id(int,auto) username(str) domain(str) room(str) flag(int)" >> imc_members;


#
# Table structure for table 'imc_rooms'
#
# used by imc module
#
touch imc_rooms;
echo "DBTEXT Add Table: imc_rooms"
echo "id(int,auto) name(str) domain(str) flag(int)" >> imc_rooms;


#
# Table structure for table 'siptrace'
#
touch sip_trace;
echo "DBTEXT Add Table: sip_trace"
echo "id(int,auto) date(int) callid(str) traced_user(str) msg(str) method(str) status(str) fromip(str) toip(str) fromtag(str) direction(str)" >> sip_trace;


#
# domainpolicy table (see README domainpolicy module)
#
touch domainpolicy;
echo "DBTEXT Add Table: domainpolicy"
echo "id(int,auto) rule(str) type(str) att(str) val(str) comment(str)" >> domainpolicy;

	if [ $? -eq 0 ] ; then
		echo "...extra tables created"
	fi
}  # end extra_create


case $1 in
	create)
		# create new database structures
		shift
		if [ $# -eq 1 ] ; then
			DBTEXT_PATH="$1"
		fi
		openser_create $DBTEXT_PATH
		exit $?
		;;
	presence)
		shift
		if [ $# -eq 1 ] ; then
			DBTEXT_PATH="$1"
		fi
		presence_create $DBTEXT_PATH
		exit $?
		;;
	extra)
		shift
		if [ $# -eq 1 ] ; then
			DBTEXT_PATH="$1"
		fi
		extra_create $DBNAME
		exit $?
		;;
	drop)
		shift
		# delete openser database
		if [ $# -eq 1 ] ; then
			DBTEXT_PATH="$1"
		fi
		openser_drop $DBTEXT_PATH
		exit $?
		;;
	reinstall)
		echo "NOT IMPLEMENTED ... quitting";
		exit 0;
		#1 create a backup database (named *_bak)
		echo "creating backup database"
		openser_backup $DBNAME
		if [ "$?" -ne 0 ] ; then
			echo "reinstall: creating backup db failed"
			exit 1
		fi
		#2 dump original database and change names in it
		echo "dumping table content ($DBNAME)"
		tmp_file=/tmp/openser_mysql.$$
		openser_dump "$DBNAME --ignore-table=$DBNAME.version" > $tmp_file
		if [ "$?" -ne 0 ] ; then
			echo "reinstall: dumping original db failed"
			exit 1
		fi
		sed "s/[sS][rR][cC]\($\|[^_]\)/src_leg\1/g" $tmp_file |
			sed "s/[dD][sS][tT]\($\|[^_]\)/dst_leg\1/g"> ${tmp_file}.2
		#3 drop original database
		echo "dropping table ($DBNAME)"
		openser_drop $DBNAME
		if [ "$?" -ne 0 ] ; then
			echo "reinstall: dropping table failed"
			rm $tmp_file*
			exit 1
		fi
		#4 change names in table definition and restore
		echo "creating new structures"
		NO_USER_INIT="yes"
		openser_create $DBNAME
		if [ "$?" -ne 0 ] ; then
			echo "reinstall: creating new table failed"
			rm $tmp_file*
			exit 1
		fi
		#5 restoring table content
		echo "restoring table content"
		openser_restore $DBNAME ${tmp_file}.2
		if [ "$?" -ne 0 ] ; then
			echo "reinstall: restoring table failed"
			rm $tmp_file*
			exit 1
		fi
		# done
		rm -f $tmp_file*
		exit 0
		;;
	copy)
		echo "NOT IMPLEMENTED ... quitting";
		exit 0;
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
		exit $ret
		;;
	backup)
		echo "NOT IMPLEMENTED ... quitting";
		exit 0;
		# backup current database
		openser_dump $DBNAME
		exit $?
		;;
	restore)
		echo "NOT IMPLEMENTED ... quitting";
		exit 0;
		# restore database from a backup
		shift
		if [ $# -ne 1 ]; then
			usage
			exit 1
		fi
		openser_restore $DBNAME $1
		exit $?
		;;
	reinit)
		shift
		if [ $# -eq 1 ] ; then
			DBTEXT_PATH="$1"
		fi
		# delete database and create a new one
		openser_drop $DBTEXT_PATH
		ret=$?
		if [ "$ret" -ne 0 ]; then
			exit $ret
		fi
		openser_create $DBTEXT_PATH
		exit $?
		;;
	*)
		usage
		exit 1;
		;;
esac

