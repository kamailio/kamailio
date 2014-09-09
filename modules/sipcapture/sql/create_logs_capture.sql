CREATE TABLE IF NOT EXISTS `logs_capture` (
  `id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
  `date` timestamp NOT NULL DEFAULT '0000-00-00 00:00:00',
  `micro_ts` bigint(18) NOT NULL DEFAULT '0',
  `correlation_id` varchar(256) NOT NULL DEFAULT '',
  `source_ip` varchar(60) NOT NULL DEFAULT '',
  `source_port` int(10) NOT NULL,
  `destination_ip` varchar(60) NOT NULL DEFAULT '',
  `destination_port` int(10) NOT NULL,
  `proto` int(5) NOT NULL,
  `family` int(1) DEFAULT NULL,
  `type` int(2) NOT NULL,
  `node` varchar(125) NOT NULL,
  `msg` varchar(2500) NOT NULL,
  PRIMARY KEY (`id`,`date`),
  KEY `date` (`date`),
  KEY `correlationid` (`correlation_id`(255))
) ENGINE=InnoDB  DEFAULT CHARSET=utf8
/*!50100 PARTITION BY RANGE ( UNIX_TIMESTAMP(`date`))
(PARTITION p2013082901 VALUES LESS THAN (1377734400) ENGINE = InnoDB,
 PARTITION p2013090821 VALUES LESS THAN (1378670400) ENGINE = InnoDB,
 PARTITION p2013090822 VALUES LESS THAN (1378674000) ENGINE = InnoDB,
 PARTITION p2013090823 VALUES LESS THAN (1378677600) ENGINE = InnoDB,
 PARTITION pmax VALUES LESS THAN MAXVALUE ENGINE = InnoDB) */

