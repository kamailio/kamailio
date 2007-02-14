use openser;

CREATE TABLE `imc_members` (
  `id` int(10) unsigned NOT NULL auto_increment,
  `username` varchar(128) NOT NULL,
  `domain` varchar(128) NOT NULL,
  `room` varchar(64) NOT NULL,
  `flag` int(11) NOT NULL,
  UNIQUE KEY ndr_imc (`username`,`domain`,`room`),
  PRIMARY KEY (id)
) ENGINE=MyISAM;

CREATE TABLE `imc_rooms` (
  `id` int(10) unsigned NOT NULL auto_increment,
  `name` varchar(128) NOT NULL,
  `domain` varchar(128) NOT NULL,
  `flag` int(11) NOT NULL,
  UNIQUE KEY nd_imc (`name`,`domain`),
  PRIMARY KEY (id)
) ENGINE=MyISAM;
