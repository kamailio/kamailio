--
-- structure for table `alarm_data`
--
DROP TABLE IF EXISTS alarm_data;
CREATE TABLE IF NOT EXISTS `alarm_data` (
  `id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
  `create_date` timestamp NOT NULL DEFAULT '0000-00-00 00:00:00',
  `type` varchar(50) NOT NULL DEFAULT '',
  `total` int(20) NOT NULL,
  `source_ip` varchar(150) NOT NULL DEFAULT '0.0.0.0',
  `description` varchar(256) NOT NULL,
  `status` int(1) NOT NULL DEFAULT '1',
  PRIMARY KEY (`id`),
  KEY `to_date` (`create_date`),
  KEY `method` (`type`)
) ENGINE=MyISAM  DEFAULT CHARSET=latin1 AUTO_INCREMENT=1 ;

-- --------------------------------------------------------

--
-- Table structure for table `alarm_data_mem`
--
DROP TABLE IF EXISTS alarm_data_mem;
CREATE TABLE IF NOT EXISTS `alarm_data_mem` (
  `id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
  `create_date` timestamp NOT NULL DEFAULT '0000-00-00 00:00:00',
  `type` varchar(50) NOT NULL DEFAULT '',
  `total` int(20) NOT NULL,
  `source_ip` varchar(150) NOT NULL DEFAULT '0.0.0.0',
  `description` varchar(256) NOT NULL,
  `status` int(1) NOT NULL DEFAULT '1',
  PRIMARY KEY (`id`),
  UNIQUE KEY `type` (`type`,`source_ip`),
  KEY `to_date` (`create_date`),
  KEY `method` (`type`)
) ENGINE=MEMORY DEFAULT CHARSET=latin1 AUTO_INCREMENT=1 ;

-- --------------------------------------------------------

--
-- Table structure for table `stats_data`
--
DROP TABLE IF EXISTS stats_data;

CREATE TABLE IF NOT EXISTS `stats_data` (
  `id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
  `from_date` timestamp NOT NULL DEFAULT '0000-00-00 00:00:00',
  `to_date` timestamp NOT NULL DEFAULT '0000-00-00 00:00:00',
  `type` varchar(50) NOT NULL DEFAULT '',
  `total` int(20) NOT NULL,
  PRIMARY KEY (`id`),
  KEY `from_date` (`from_date`),
  KEY `to_date` (`to_date`),
  KEY `method` (`type`)
) ENGINE=MyISAM  DEFAULT CHARSET=latin1 AUTO_INCREMENT=1 ;

-- --------------------------------------------------------

--
-- Table structure for table `stats_ip`
--
DROP TABLE IF EXISTS stats_ip;
CREATE TABLE IF NOT EXISTS `stats_ip` (
  `id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
  `from_date` timestamp NOT NULL DEFAULT '0000-00-00 00:00:00',
  `to_date` timestamp NOT NULL DEFAULT '0000-00-00 00:00:00',
  `method` varchar(50) NOT NULL DEFAULT '',
  `source_ip` varchar(255) NOT NULL DEFAULT '0.0.0.0',
  `total` int(20) NOT NULL,
  PRIMARY KEY (`id`),
  UNIQUE KEY `datemethod` (`to_date`,`method`,`source_ip`),
  KEY `from_date` (`from_date`),
  KEY `to_date` (`to_date`),
  KEY `method` (`method`)
) ENGINE=MyISAM  DEFAULT CHARSET=latin1 AUTO_INCREMENT=1 ;

-- --------------------------------------------------------

--
-- Table structure for table `stats_ip_mem`
--
DROP TABLE IF EXISTS stats_ip_mem;
CREATE TABLE IF NOT EXISTS `stats_ip_mem` (
  `id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
  `create_date` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  `method` varchar(50) NOT NULL DEFAULT '',
  `source_ip` varchar(255) NOT NULL DEFAULT '0.0.0.0',
  `total` int(20) NOT NULL,
  PRIMARY KEY (`id`),
  UNIQUE KEY `datemethod` (`method`,`source_ip`)
) ENGINE=MEMORY DEFAULT CHARSET=latin1 AUTO_INCREMENT=1 ;

-- --------------------------------------------------------

--
-- Table structure for table `stats_method`
--
DROP TABLE IF EXISTS stats_method;
CREATE TABLE IF NOT EXISTS `stats_method` (
  `id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
  `from_date` timestamp NOT NULL DEFAULT '0000-00-00 00:00:00',
  `to_date` timestamp NOT NULL DEFAULT '0000-00-00 00:00:00',
  `method` varchar(50) NOT NULL DEFAULT '',
  `auth` tinyint(1) NOT NULL DEFAULT '0',
  `cseq` varchar(100) NOT NULL,
  `totag` tinyint(1) NOT NULL,
  `total` int(20) NOT NULL,
  PRIMARY KEY (`id`),
  UNIQUE KEY `datemethod` (`to_date`,`method`,`auth`,`totag`,`cseq`),
  KEY `from_date` (`from_date`),
  KEY `to_date` (`to_date`),
  KEY `method` (`method`),
  KEY `completed` (`cseq`)
) ENGINE=MyISAM  DEFAULT CHARSET=latin1 AUTO_INCREMENT=1 ;

-- --------------------------------------------------------

--
-- Table structure for table `stats_method_mem`
--
DROP TABLE IF EXISTS stats_method_mem;
CREATE TABLE IF NOT EXISTS `stats_method_mem` (
  `id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
  `create_date` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  `method` varchar(50) NOT NULL DEFAULT '',
  `auth` tinyint(1) NOT NULL DEFAULT '0',
  `cseq` varchar(100) NOT NULL,
  `totag` tinyint(1) NOT NULL,
  `total` int(20) NOT NULL,
  PRIMARY KEY (`id`),
  UNIQUE KEY `datemethod` (`method`,`auth`,`totag`, `cseq`),
  KEY `from_date` (`create_date`),
  KEY `method` (`method`),
  KEY `completed` (`cseq`)
) ENGINE=MEMORY DEFAULT CHARSET=latin1 AUTO_INCREMENT=1 ;

-- --------------------------------------------------------

--
-- Table structure for table `stats_useragent`
--
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
) ENGINE=MyISAM  DEFAULT CHARSET=latin1 AUTO_INCREMENT=1 ;

-- --------------------------------------------------------

--
-- Table structure for table `stats_useragent_mem`
--
DROP TABLE IF EXISTS stats_useragent_mem;
CREATE TABLE IF NOT EXISTS `stats_useragent_mem` (
  `id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
  `create_date` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  `useragent` varchar(100) NOT NULL DEFAULT '',
  `method` varchar(50) NOT NULL DEFAULT '',
  `total` int(10) NOT NULL DEFAULT '0',
  PRIMARY KEY (`id`),
  UNIQUE KEY `useragent` (`useragent`,`method`)
) ENGINE=MEMORY DEFAULT CHARSET=latin1 AUTO_INCREMENT=1 ;

