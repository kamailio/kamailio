DROP TABLE IF EXISTS stats_method;
CREATE TABLE IF NOT EXISTS `stats_method` (
  `id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
  `from_date` timestamp NOT NULL DEFAULT '0000-00-00 00:00:00',
  `to_date` timestamp NOT NULL DEFAULT '0000-00-00 00:00:00',
  `method` varchar(50) NOT NULL DEFAULT '',
  `total` int(10) NOT NULL DEFAULT '0',
  `auth` int(10) NOT NULL DEFAULT '0',
  `completed` int(10) NOT NULL DEFAULT '0',
  `uncompleted` int(10) NOT NULL DEFAULT '0',
  `rejected` int(10) NOT NULL DEFAULT '0',
  `asr` int(10) NOT NULL DEFAULT '0',
  `ner` int(10) NOT NULL DEFAULT '0',
  PRIMARY KEY (`id`),
  UNIQUE KEY `datemethod` (`to_date`,`method`),
  KEY `from_date` (`from_date`),
  KEY `to_date` (`to_date`),
  KEY `method` (`method`),
  KEY `total` (`total`),
  KEY `completed` (`completed`),
  KEY `uncompleted` (`uncompleted`)
) ENGINE=MyISAM AUTO_INCREMENT=0 DEFAULT CHARSET=latin1;

DROP TABLE IF EXISTS stats_useragent;
CREATE TABLE IF NOT EXISTS `stats_useragent` (
`id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
  `from_date` timestamp NOT NULL DEFAULT '0000-00-00 00:00:00',
  `to_date` timestamp NOT NULL DEFAULT '0000-00-00 00:00:00',
  `useragent` varchar(100) NOT NULL DEFAULT '',
  `method` varchar(50) NOT NULL DEFAULT '',
  `total` int(10) NOT NULL DEFAULT '0',
  PRIMARY KEY (`id`),
  UNIQUE KEY `datemethodua` (`to_date`,`method`,`useragent`),
  KEY `from_date` (`from_date`),
  KEY `to_date` (`to_date`),
  KEY `useragent` (`useragent`),
  KEY `method` (`method`),
  KEY `total` (`total`)
) ENGINE=MyISAM AUTO_INCREMENT=2771 DEFAULT CHARSET=latin1;
