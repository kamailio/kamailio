USE openser;

CREATE TABLE `cpl` (
  `id` int(10) NOT NULL auto_increment,
  `username` varchar(64) NOT NULL,
  `domain` varchar(64) NOT NULL default '',
  `cpl_xml` blob,
  `cpl_bin` blob,
  UNIQUE KEY ud_cpl (`username`,`domain`),
  PRIMARY KEY  (`id`)
) ENGINE=MyISAM;
