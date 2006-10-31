use openser;

CREATE TABLE `imc_members` (
  `user` varchar(128) NOT NULL,
  `domain` varchar(128) NOT NULL,
  `room` varchar(64) NOT NULL,
  `flag` int(11) NOT NULL,
  PRIMARY KEY  (`user`,`domain`,`room`)
) ENGINE=MyISAM;

CREATE TABLE `imc_rooms` (
  `name` varchar(128) NOT NULL,
  `domain` varchar(128) NOT NULL,
  `flag` int(11) NOT NULL,
  PRIMARY KEY  (`name`,`domain`)
) ENGINE=MyISAM; 
