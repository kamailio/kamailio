-- MySQL dump 10.13  Distrib 5.1.41, for debian-linux-gnu (x86_64)
--
-- Host: localhost    Database: homer_data
-- ------------------------------------------------------
-- Server version	5.1.41-3ubuntu12.10
--
-- Table structure for table `sip_capture`
--

DROP TABLE IF EXISTS `sip_capture`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `sip_capture` (
  `id` int(20) NOT NULL AUTO_INCREMENT,
  `date` datetime NOT NULL DEFAULT '0000-00-00 00:00:00',
  `micro_ts` bigint(18) NOT NULL DEFAULT '0',
  `method` varchar(50) NOT NULL DEFAULT '',
  `reply_reason` varchar(100) NOT NULL,
  `ruri` varchar(200) NOT NULL DEFAULT '',
  `ruri_user` varchar(100) NOT NULL DEFAULT '',
  `from_user` varchar(100) NOT NULL DEFAULT '',
  `from_tag` varchar(64) NOT NULL DEFAULT '',
  `to_user` varchar(100) NOT NULL DEFAULT '',
  `to_tag` varchar(64) NOT NULL,
  `pid_user` varchar(100) NOT NULL DEFAULT '',
  `contact_user` varchar(120) NOT NULL,
  `auth_user` varchar(120) NOT NULL,
  `callid` varchar(100) NOT NULL DEFAULT '',
  `callid_aleg` varchar(100) NOT NULL DEFAULT '',
  `via_1` varchar(256) NOT NULL,
  `via_1_branch` varchar(80) NOT NULL,
  `cseq` varchar(25) NOT NULL,
  `diversion` varchar(256) NOT NULL,
  `reason` varchar(200) NOT NULL,
  `content_type` varchar(256) NOT NULL,
  `authorization` varchar(256) NOT NULL,
  `user_agent` varchar(256) NOT NULL,
  `source_ip` varchar(50) NOT NULL DEFAULT '',
  `source_port` int(10) NOT NULL,
  `destination_ip` varchar(50) NOT NULL DEFAULT '',
  `destination_port` int(10) NOT NULL,
  `contact_ip` varchar(60) NOT NULL,
  `contact_port` int(10) NOT NULL,
  `originator_ip` varchar(60) NOT NULL DEFAULT '',
  `originator_port` int(10) NOT NULL,
  `proto` int(5) NOT NULL,
  `family` int(1) DEFAULT NULL,
  `rtp_stat` varchar(256) NOT NULL,
  `type` int(2) NOT NULL,
  `node` varchar(125) NOT NULL,
  `msg` longblob NOT NULL,
  PRIMARY KEY (`id`,`date`),
  KEY `ruri_user` (`ruri_user`),
  KEY `from_user` (`from_user`),
  KEY `to_user` (`to_user`),
  KEY `pid_user` (`pid_user`),
  KEY `auth_user` (`auth_user`),
  KEY `callid_aleg` (`callid_aleg`),
  KEY `date` (`date`),
  KEY `callid` (`callid`)
) ENGINE=MyISAM AUTO_INCREMENT=1 DEFAULT CHARSET=utf8
/*!50100 PARTITION BY RANGE ( DAYOFWEEK(`date`))
SUBPARTITION BY HASH ( HOUR(`date`))
SUBPARTITIONS 24
(PARTITION p0 VALUES LESS THAN (2) ENGINE = MyISAM,
 PARTITION p1 VALUES LESS THAN (3) ENGINE = MyISAM,
 PARTITION p2 VALUES LESS THAN (4) ENGINE = MyISAM,
 PARTITION p3 VALUES LESS THAN (5) ENGINE = MyISAM,
 PARTITION p4 VALUES LESS THAN (6) ENGINE = MyISAM,
 PARTITION p5 VALUES LESS THAN (7) ENGINE = MyISAM,
 PARTITION p6 VALUES LESS THAN (8) ENGINE = MyISAM) */;
/*!40101 SET character_set_client = @saved_cs_client */;
/*!40103 SET TIME_ZONE=@OLD_TIME_ZONE */;

-- Dump completed on 2011-08-18 13:54:21
