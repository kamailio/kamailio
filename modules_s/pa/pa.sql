
-- Copyright (c) 2003, Hewlett-Packard Company
-- All rights reserved.

-- Redistribution and use in source and binary forms, with or without
-- modification, are permitted provided that the following conditions are
-- met:

--  * Redistributions of source code must retain the above copyright
--    notice, this list of conditions and the following disclaimer.

--  * Redistributions in binary form must reproduce the above copyright
--    notice, this list of conditions and the following disclaimer in the
--    documentation and/or other materials provided with the
--    distribution.
   
--  * Neither the name of the Hewlett-Packard Company nor the names of
--    its contributors nor Internet2 may be used to endorse or promote
--    products derived from this software without explicit prior written
--    permission.

-- You are under no obligation whatsoever to provide any enhancements to
-- Hewlett-Packard Company, its contributors, or Internet2.  If you
-- choose to provide your enhancements, or if you choose to otherwise
-- publish or distribute your enhancements, in source code form without
-- contemporaneously requiring end users to enter into a separate written
-- license agreement for such enhancements, then you thereby grant
-- Hewlett-Packard Company, its contributors, and Internet2 a
-- non-exclusive, royalty-free, perpetual license to install, use,
-- modify, prepare derivative works, incorporate into the software or
-- other computer software, distribute, and sublicense your enhancements
-- or derivative works thereof, in binary and source code form.

-- THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
-- "AS IS" AND WITH ALL FAULTS.  ANY EXPRESS OR IMPLIED WARRANTIES,
-- INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
-- MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, AND
-- NON-INFRINGEMENT ARE DISCLAIMED AND the entire risk of satisfactory
-- quality, performance, accuracy, and effort is with LICENSEE. IN NO
-- EVENT SHALL THE COPYRIGHT OWNER, CONTRIBUTORS, OR THE UNIVERSITY
-- CORPORATION FOR ADVANCED INTERNET DEVELOPMENT, INC. BE LIABLE FOR ANY
-- DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
-- DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
-- GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
-- INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
-- IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
-- OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OR DISTRIBUTION OF THIS
-- SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
-- 

drop table if exists presentity;
CREATE TABLE presentity (
  presid int(10) unsigned NOT NULL auto_increment,
  uri varchar(128) NOT NULL,
  pdomain varchar(128) NOT NULL,
  PRIMARY KEY  (presid),
  KEY uri_index (uri)
) TYPE=MyISAM;

drop table if exists presentity_contact;
CREATE TABLE presentity_contact (
  contactid int(10) unsigned NOT NULL auto_increment,
  presid int(10) unsigned NOT NULL,
  basic varchar(32) NOT NULL default 'offline',
  status varchar(32) NOT NULL default '',
  location varchar(128) NOT NULL default '',
  expires datetime not null default '2020-05-28 21:32:15',
  placeid int(10) default NULL,
  priority float(5,2) not null default '0.5';
  contact varchar(128) default NULL,
  PRIMARY KEY  (contactid),
  KEY presid_index (presid),
  KEY location_index (location),
  KEY placeid_index (placeid)
) TYPE=MyISAM;

drop table if exists watcherinfo;
create table watcherinfo (
	r_uri        varchar(128) NOT NULL,
	w_uri        varchar(128) NOT NULL,
	display_name varchar(128) NOT NULL,
	s_id         varchar(32) NOT NULL,
	package      varchar(32) NOT NULL default 'presence',
	status       varchar(32) NOT NULL default 'pending',
	event        varchar(32),
	expires      int,
	accepts      varchar(64),
	primary key(s_id),
	index r_uri_index (r_uri),
	index w_uri_index (w_uri)
);

drop table if exists firstseen;

create table firstseen (
	username varchar(64) not null primary key,
	timestamp timestamp
);

drop table if exists eventlist;
create table eventlist (
	elid         int(10) unsigned NOT NULL auto_increment,
	o_uri        varchar(128) NOT NULL, -- owner uri
	l_uri        varchar(128) NOT NULL, -- list uri
	name         varchar(128) NOT NULL,
	parent_elid  int(10) unsigned NOT NULL,
	l_pos        int(10) unsigned,
	version      varchar(32),
	subscribeable varchar(32),
	primary key(elid),
	index o_uri_index (o_uri),
	index l_uri_index (l_uri)
);

drop table if exists eventlistitem;
create table eventlistitem (
	itemid       int(10) unsigned NOT NULL auto_increment,
	elid	     int(10) unsigned NOT NULL,
	r_uri        varchar(128) NOT NULL,
	r_name       varchar(128),
	r_id         varchar(128),
	display_name varchar(128),
	cid          varchar(128),
	state	     varchar(32),
	primary key(itemid),
	index elid_index (elid)
);

drop table if exists place;
create table place(
	placeid                 int(10) unsigned NOT NULL auto_increment,
	room                    varchar(40) not null,
	room_number             int(10),
	floor                   int(10),
	site                    varchar(40),
	nestedid                int(10),
	contains                varchar(255),
	bugged                  int(10),
	session_name            varchar(128),
	session_end             datetime,
	upstream_packet_loss    float,
	downstream_packet_loss  float,
	primary key(placeid)
);
