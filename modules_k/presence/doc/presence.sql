
use openser;

CREATE TABLE `presentity` (
  `id` int(10) NOT NULL auto_increment,
  `username` varchar(64) NOT NULL,
  `domain` varchar(124) NOT NULL,
  `event` varchar(64) NOT NULL,
  `etag` varchar(64) NOT NULL,
  `expires` int(11) NOT NULL,
  `received_time` int(11) NOT NULL,
  `body` text NOT NULL,
  UNIQUE KEY udee_presentity (`username`,`domain`,`event`,`etag`),
  PRIMARY KEY (id)
) ENGINE=MyISAM;


CREATE TABLE `active_watchers` (
  `id` int(10) NOT NULL auto_increment,
  `to_user` varchar(64) NOT NULL,
  `to_domain` varchar(128) NOT NULL,
  `from_user` varchar(64) NOT NULL,
  `from_domain` varchar(128) NOT NULL,
  `event` varchar(64) NOT NULL default 'presence',
  `event_id` varchar(64) NOT NULL,
  `to_tag` varchar(128) NOT NULL,
  `from_tag` varchar(128) NOT NULL,
  `callid` varchar(128) NOT NULL,
  `cseq` int(11) NOT NULL,
  `contact` varchar(128) NOT NULL,
  `record_route` varchar(255) NOT NULL,
  `expires` int(11) NOT NULL,
  `status` varchar(32) NOT NULL default 'pending',
  `version` int(11) NOT NULL default '0',
  UNIQUE KEY ft_watchers (`from_tag`),
  PRIMARY KEY (id)
) ENGINE=MyISAM;


CREATE TABLE `watchers` (
  `id` int(10) NOT NULL auto_increment,
  `p_user` varchar(64) NOT NULL,
  `p_domain` varchar(128) NOT NULL,
  `w_user` varchar(64) NOT NULL,
  `w_domain` varchar(128) NOT NULL,
  `subs_status` varchar(64) NOT NULL,
  `reason` varchar(64) NOT NULL,
  `inserted_time` int(11) NOT NULL,
  UNIQUE KEY udud_watchers (`p_user`,`p_domain`,`w_user`,`w_domain`),
  PRIMARY KEY (id)
) ENGINE=MyISAM;



CREATE TABLE `xcap_xml` (
  `id` int(10) NOT NULL auto_increment,
  `user` varchar(66) NOT NULL,
  `domain` varchar(128) NOT NULL,
  `xcap` text NOT NULL,
  `doc_type` varchar(64) NOT NULL,
  UNIQUE KEY udd_xcap (`user`,`domain`,`doc_type`),
  PRIMARY KEY (id)
) ENGINE=MyISAM;
