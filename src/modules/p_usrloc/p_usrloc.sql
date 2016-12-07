--
-- Table structure for table `locdb`
--

DROP TABLE IF EXISTS `locdb`;
CREATE TABLE `locdb` (
  `id` int(11) NOT NULL default '0',
  `no` int(11) NOT NULL default '0',
  `url` varchar(255) NOT NULL default '',
  `status` tinyint(4) NOT NULL default '1',
  `errors` int(11) NOT NULL default '0',
  `failover` datetime NOT NULL default '1900-01-01 00:00:01',
  `spare` tinyint(4) NOT NULL default '0',
  `rg` int(11) NOT NULL default '0',
  PRIMARY KEY  (`id`,`no`),
  KEY `no` (`no`)
) ENGINE=InnoDB DEFAULT CHARSET=latin1;