
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
  `event_id` varchar(64),
  `to_tag` varchar(128) NOT NULL,
  `from_tag` varchar(128) NOT NULL,
  `callid` varchar(128) NOT NULL,
  `local_cseq` int(11) NOT NULL,
  `remote_cseq` int(11) NOT NULL,
  `contact` varchar(128) NOT NULL,
  `record_route` text,
  `expires` int(11) NOT NULL,
  `status` varchar(32) NOT NULL default 'pending',
  `version` int(11) default '0',
  `socket_info` varchar(128) NOT NULL,
  `local_contact` varchar(255) NOT NULL,
  PRIMARY KEY  (`id`),
  UNIQUE KEY `tt_watchers` (`to_tag`),
  KEY `due_activewatchers` (`to_domain`,`to_user`,`event`)
) ENGINE=MyISAM DEFAULT CHARSET=latin1 AUTO_INCREMENT=1 ;


CREATE TABLE `watchers` (
  `id` int(10) NOT NULL auto_increment,
  `p_user` varchar(64) NOT NULL,
  `p_domain` varchar(128) NOT NULL,
  `w_user` varchar(64) NOT NULL,
  `w_domain` varchar(128) NOT NULL,
  `subs_status` varchar(64) NOT NULL,
  `reason` varchar(64),
  `inserted_time` int(11) NOT NULL,
  UNIQUE KEY udud_watchers (`p_user`,`p_domain`,`w_user`,`w_domain`),
  PRIMARY KEY (id)
) ENGINE=MyISAM;

