# $Id$

create database ser;
use ser;

CREATE TABLE grp (
   user varchar(50) NOT NULL,
   grp varchar(50) NOT NULL
);

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
   key(user_id,realm)
);

