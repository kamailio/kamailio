
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
create table presentity (
	presid    int(10) unsigned NOT NULL auto_increment,
	uri       varchar(128) NOT NULL,
	basic     varchar(32) NOT NULL default 'offline',
	status    varchar(32) NOT NULL default '',
	location  varchar(128) NOT NULL default '',
	site      varchar(32),
	floor     varchar(32),
	room      varchar(64),
	x         float(5,2),
	y         float(5,2),
	radius    float(5,2),
	primary key(presid),
	index uri_index (uri),
	index locaiton_index (location)
);

drop table if exists watcher;
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
	primary key(s_id),
	index r_uri_index (r_uri),
	index w_uri_index (w_uri)
);

insert into watcherinfo (package, r_uri, w_uri, display_name, s_id, status, expires, event) values
	('presence', 'bob@example.com', 'alice@example.com',null, 'foo22', 'pending', 0, 'subscribe'),
	('presence', 'bob@example.com', 'joe@example.com', 'Joe F', 'foo23', 'active', 2600, 'approved');
