# phpMyAdmin MySQL-Dump
# http://phpwizard.net/phpMyAdmin/
#
# $Id$
#

create database ser;
use ser;

# Users: ser is the regular user, serro only for reading
GRANT ALL PRIVILEGES ON ser.* TO ser IDENTIFIED  BY 'heslo';
GRANT SELECT ON ser.* TO serro IDENTIFIED BY '47serro11';

# --------------------------------------------------------
#
# Table structure for table 'aliases' -- the same as UsrLoc
#

CREATE TABLE aliases (
   user varchar(50) NOT NULL,
   contact varchar(255) NOT NULL,
   expires datetime,
   q float(10,2),
   callid varchar(255),
   cseq int(11),
   last_modified timestamp(14),
   KEY user (user, contact)
);


# --------------------------------------------------------
#
# Table structure for table 'grp' -- ACLs
#

CREATE TABLE grp (
   user varchar(50) NOT NULL,
   grp varchar(50) NOT NULL,
   last_modified datetime DEFAULT '0000-00-00 00:00:00' NOT NULL
);


# --------------------------------------------------------
#
# Table structure for table 'location'
#

CREATE TABLE location (
   user varchar(50) NOT NULL,
   contact varchar(255) NOT NULL,
   expires datetime,
   q float(10,2),
   callid varchar(255),
   cseq int(11),
   last_modified timestamp(14),
   KEY user (user, contact)
);


# --------------------------------------------------------
#
# Table structure for table 'pending'
#

CREATE TABLE pending (
   USER_ID varchar(100) NOT NULL,
   PASSWORD varchar(25) NOT NULL,
   FIRST_NAME varchar(25) NOT NULL,
   LAST_NAME varchar(45) NOT NULL,
   PHONE varchar(15) NOT NULL,
   EMAIL_ADDRESS varchar(50) NOT NULL,
   DATETIME_CREATED datetime DEFAULT '0000-00-00 00:00:00' NOT NULL,
   DATETIME_MODIFIED datetime DEFAULT '0000-00-00 00:00:00' NOT NULL,
   confirmation varchar(64) NOT NULL,
   flag char(1) DEFAULT 'o' NOT NULL,
   SendNotification varchar(50) NOT NULL,
   Greeting varchar(50) NOT NULL,
   HA1 varchar(128) NOT NULL,
   REALM varchar(128) NOT NULL,
   ha1b varchar(128) NOT NULL,
   UNIQUE USER_ID (USER_ID),
   KEY USER_ID_2 (USER_ID)
);


# --------------------------------------------------------
#
# Table structure for table 'reserved' -- reserved usernames
#

CREATE TABLE reserved (
   user_id char(100) NOT NULL,
   UNIQUE user_id (user_id)
);


# --------------------------------------------------------
#
# Table structure for table 'subscriber'
#

CREATE TABLE subscriber (
   USER_ID varchar(100) NOT NULL,
   PASSWORD varchar(25) NOT NULL,
   FIRST_NAME varchar(25) NOT NULL,
   LAST_NAME varchar(45) NOT NULL,
   PHONE varchar(15) NOT NULL,
   EMAIL_ADDRESS varchar(50) NOT NULL,
   DATETIME_CREATED datetime DEFAULT '0000-00-00 00:00:00' NOT NULL,
   DATETIME_MODIFIED datetime DEFAULT '0000-00-00 00:00:00' NOT NULL,
   confirmation varchar(64) NOT NULL,
   flag char(1) DEFAULT 'o' NOT NULL,
   SendNotification varchar(50) NOT NULL,
   Greeting varchar(50) NOT NULL,
   HA1 varchar(128) NOT NULL,
   REALM varchar(128) NOT NULL,
   ha1b varchar(128) NOT NULL,
   UNIQUE USER_ID (USER_ID),
   KEY USER_ID_2 (USER_ID)
);

