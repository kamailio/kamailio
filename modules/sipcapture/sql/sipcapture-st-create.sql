-- MySQL dump 10.13  Distrib 5.1.41, for debian-linux-gnu (x86_64)
--
-- Host: localhost    Database: sip_capture_test
-- ------------------------------------------------------
-- Server version	5.1.41-3ubuntu12.10

--
-- Table structure for table `sip_capture_01_00`
--

DROP TABLE IF EXISTS `sip_capture_01_00`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `sip_capture_01_00` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
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
  PRIMARY KEY (`id`),
  KEY `ruri_user` (`ruri_user`),
  KEY `from_user` (`from_user`),
  KEY `to_user` (`to_user`),
  KEY `pid_user` (`pid_user`),
  KEY `auth_user` (`auth_user`),
  KEY `callid_aleg` (`callid_aleg`),
  KEY `date` (`date`),
  KEY `callid` (`callid`)
) ENGINE=MyISAM AUTO_INCREMENT=1 DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `sip_capture_01_01`
--

DROP TABLE IF EXISTS `sip_capture_01_01`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `sip_capture_01_01` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
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
  `direction` varchar(4) NOT NULL DEFAULT '',
  PRIMARY KEY (`id`),
  KEY `ruri_user` (`ruri_user`),
  KEY `from_user` (`from_user`),
  KEY `to_user` (`to_user`),
  KEY `pid_user` (`pid_user`),
  KEY `auth_user` (`auth_user`),
  KEY `callid_aleg` (`callid_aleg`),
  KEY `date` (`date`),
  KEY `callid` (`callid`)
) ENGINE=MyISAM AUTO_INCREMENT=1 DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `sip_capture_01_02`
--

DROP TABLE IF EXISTS `sip_capture_01_02`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `sip_capture_01_02` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
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
  `direction` varchar(4) NOT NULL DEFAULT '',
  PRIMARY KEY (`id`),
  KEY `ruri_user` (`ruri_user`),
  KEY `from_user` (`from_user`),
  KEY `to_user` (`to_user`),
  KEY `pid_user` (`pid_user`),
  KEY `auth_user` (`auth_user`),
  KEY `callid_aleg` (`callid_aleg`),
  KEY `date` (`date`),
  KEY `callid` (`callid`)
) ENGINE=MyISAM AUTO_INCREMENT=1 DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `sip_capture_01_03`
--

DROP TABLE IF EXISTS `sip_capture_01_03`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `sip_capture_01_03` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
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
  `direction` varchar(4) NOT NULL DEFAULT '',
  PRIMARY KEY (`id`),
  KEY `ruri_user` (`ruri_user`),
  KEY `from_user` (`from_user`),
  KEY `to_user` (`to_user`),
  KEY `pid_user` (`pid_user`),
  KEY `auth_user` (`auth_user`),
  KEY `callid_aleg` (`callid_aleg`),
  KEY `date` (`date`),
  KEY `callid` (`callid`)
) ENGINE=MyISAM AUTO_INCREMENT=1 DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `sip_capture_01_04`
--

DROP TABLE IF EXISTS `sip_capture_01_04`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `sip_capture_01_04` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
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
  `direction` varchar(4) NOT NULL DEFAULT '',
  PRIMARY KEY (`id`),
  KEY `ruri_user` (`ruri_user`),
  KEY `from_user` (`from_user`),
  KEY `to_user` (`to_user`),
  KEY `pid_user` (`pid_user`),
  KEY `auth_user` (`auth_user`),
  KEY `callid_aleg` (`callid_aleg`),
  KEY `date` (`date`),
  KEY `callid` (`callid`)
) ENGINE=MyISAM AUTO_INCREMENT=1 DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `sip_capture_01_05`
--

DROP TABLE IF EXISTS `sip_capture_01_05`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `sip_capture_01_05` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
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
  `direction` varchar(4) NOT NULL DEFAULT '',
  PRIMARY KEY (`id`),
  KEY `ruri_user` (`ruri_user`),
  KEY `from_user` (`from_user`),
  KEY `to_user` (`to_user`),
  KEY `pid_user` (`pid_user`),
  KEY `auth_user` (`auth_user`),
  KEY `callid_aleg` (`callid_aleg`),
  KEY `date` (`date`),
  KEY `callid` (`callid`)
) ENGINE=MyISAM AUTO_INCREMENT=1 DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `sip_capture_01_06`
--

DROP TABLE IF EXISTS `sip_capture_01_06`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `sip_capture_01_06` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
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
  `direction` varchar(4) NOT NULL DEFAULT '',
  PRIMARY KEY (`id`),
  KEY `ruri_user` (`ruri_user`),
  KEY `from_user` (`from_user`),
  KEY `to_user` (`to_user`),
  KEY `pid_user` (`pid_user`),
  KEY `auth_user` (`auth_user`),
  KEY `callid_aleg` (`callid_aleg`),
  KEY `date` (`date`),
  KEY `callid` (`callid`)
) ENGINE=MyISAM AUTO_INCREMENT=1 DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `sip_capture_01_07`
--

DROP TABLE IF EXISTS `sip_capture_01_07`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `sip_capture_01_07` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
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
  `direction` varchar(4) NOT NULL DEFAULT '',
  PRIMARY KEY (`id`),
  KEY `ruri_user` (`ruri_user`),
  KEY `from_user` (`from_user`),
  KEY `to_user` (`to_user`),
  KEY `pid_user` (`pid_user`),
  KEY `auth_user` (`auth_user`),
  KEY `callid_aleg` (`callid_aleg`),
  KEY `date` (`date`),
  KEY `callid` (`callid`)
) ENGINE=MyISAM AUTO_INCREMENT=1 DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `sip_capture_01_08`
--

DROP TABLE IF EXISTS `sip_capture_01_08`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `sip_capture_01_08` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
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
  `direction` varchar(4) NOT NULL DEFAULT '',
  PRIMARY KEY (`id`),
  KEY `ruri_user` (`ruri_user`),
  KEY `from_user` (`from_user`),
  KEY `to_user` (`to_user`),
  KEY `pid_user` (`pid_user`),
  KEY `auth_user` (`auth_user`),
  KEY `callid_aleg` (`callid_aleg`),
  KEY `date` (`date`),
  KEY `callid` (`callid`)
) ENGINE=MyISAM AUTO_INCREMENT=1 DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `sip_capture_01_09`
--

DROP TABLE IF EXISTS `sip_capture_01_09`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `sip_capture_01_09` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
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
  `direction` varchar(4) NOT NULL DEFAULT '',
  PRIMARY KEY (`id`),
  KEY `ruri_user` (`ruri_user`),
  KEY `from_user` (`from_user`),
  KEY `to_user` (`to_user`),
  KEY `pid_user` (`pid_user`),
  KEY `auth_user` (`auth_user`),
  KEY `callid_aleg` (`callid_aleg`),
  KEY `date` (`date`),
  KEY `callid` (`callid`)
) ENGINE=MyISAM AUTO_INCREMENT=1 DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `sip_capture_01_10`
--

DROP TABLE IF EXISTS `sip_capture_01_10`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `sip_capture_01_10` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
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
  `direction` varchar(4) NOT NULL DEFAULT '',
  PRIMARY KEY (`id`),
  KEY `ruri_user` (`ruri_user`),
  KEY `from_user` (`from_user`),
  KEY `to_user` (`to_user`),
  KEY `pid_user` (`pid_user`),
  KEY `auth_user` (`auth_user`),
  KEY `callid_aleg` (`callid_aleg`),
  KEY `date` (`date`),
  KEY `callid` (`callid`)
) ENGINE=MyISAM AUTO_INCREMENT=1 DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `sip_capture_01_11`
--

DROP TABLE IF EXISTS `sip_capture_01_11`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `sip_capture_01_11` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
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
  `direction` varchar(4) NOT NULL DEFAULT '',
  PRIMARY KEY (`id`),
  KEY `ruri_user` (`ruri_user`),
  KEY `from_user` (`from_user`),
  KEY `to_user` (`to_user`),
  KEY `pid_user` (`pid_user`),
  KEY `auth_user` (`auth_user`),
  KEY `callid_aleg` (`callid_aleg`),
  KEY `date` (`date`),
  KEY `callid` (`callid`)
) ENGINE=MyISAM AUTO_INCREMENT=1 DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `sip_capture_01_12`
--

DROP TABLE IF EXISTS `sip_capture_01_12`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `sip_capture_01_12` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
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
  `direction` varchar(4) NOT NULL DEFAULT '',
  PRIMARY KEY (`id`),
  KEY `ruri_user` (`ruri_user`),
  KEY `from_user` (`from_user`),
  KEY `to_user` (`to_user`),
  KEY `pid_user` (`pid_user`),
  KEY `auth_user` (`auth_user`),
  KEY `callid_aleg` (`callid_aleg`),
  KEY `date` (`date`),
  KEY `callid` (`callid`)
) ENGINE=MyISAM AUTO_INCREMENT=1 DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `sip_capture_01_13`
--

DROP TABLE IF EXISTS `sip_capture_01_13`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `sip_capture_01_13` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
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
  `direction` varchar(4) NOT NULL DEFAULT '',
  PRIMARY KEY (`id`),
  KEY `ruri_user` (`ruri_user`),
  KEY `from_user` (`from_user`),
  KEY `to_user` (`to_user`),
  KEY `pid_user` (`pid_user`),
  KEY `auth_user` (`auth_user`),
  KEY `callid_aleg` (`callid_aleg`),
  KEY `date` (`date`),
  KEY `callid` (`callid`)
) ENGINE=MyISAM AUTO_INCREMENT=1 DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `sip_capture_01_14`
--

DROP TABLE IF EXISTS `sip_capture_01_14`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `sip_capture_01_14` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
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
  `direction` varchar(4) NOT NULL DEFAULT '',
  PRIMARY KEY (`id`),
  KEY `ruri_user` (`ruri_user`),
  KEY `from_user` (`from_user`),
  KEY `to_user` (`to_user`),
  KEY `pid_user` (`pid_user`),
  KEY `auth_user` (`auth_user`),
  KEY `callid_aleg` (`callid_aleg`),
  KEY `date` (`date`),
  KEY `callid` (`callid`)
) ENGINE=MyISAM AUTO_INCREMENT=1 DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `sip_capture_01_15`
--

DROP TABLE IF EXISTS `sip_capture_01_15`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `sip_capture_01_15` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
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
  `direction` varchar(4) NOT NULL DEFAULT '',
  PRIMARY KEY (`id`),
  KEY `ruri_user` (`ruri_user`),
  KEY `from_user` (`from_user`),
  KEY `to_user` (`to_user`),
  KEY `pid_user` (`pid_user`),
  KEY `auth_user` (`auth_user`),
  KEY `callid_aleg` (`callid_aleg`),
  KEY `date` (`date`),
  KEY `callid` (`callid`)
) ENGINE=MyISAM AUTO_INCREMENT=1 DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `sip_capture_01_16`
--

DROP TABLE IF EXISTS `sip_capture_01_16`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `sip_capture_01_16` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
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
  `direction` varchar(4) NOT NULL DEFAULT '',
  PRIMARY KEY (`id`),
  KEY `ruri_user` (`ruri_user`),
  KEY `from_user` (`from_user`),
  KEY `to_user` (`to_user`),
  KEY `pid_user` (`pid_user`),
  KEY `auth_user` (`auth_user`),
  KEY `callid_aleg` (`callid_aleg`),
  KEY `date` (`date`),
  KEY `callid` (`callid`)
) ENGINE=MyISAM AUTO_INCREMENT=1 DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `sip_capture_01_17`
--

DROP TABLE IF EXISTS `sip_capture_01_17`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `sip_capture_01_17` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
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
  `direction` varchar(4) NOT NULL DEFAULT '',
  PRIMARY KEY (`id`),
  KEY `ruri_user` (`ruri_user`),
  KEY `from_user` (`from_user`),
  KEY `to_user` (`to_user`),
  KEY `pid_user` (`pid_user`),
  KEY `auth_user` (`auth_user`),
  KEY `callid_aleg` (`callid_aleg`),
  KEY `date` (`date`),
  KEY `callid` (`callid`)
) ENGINE=MyISAM AUTO_INCREMENT=1 DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `sip_capture_01_18`
--

DROP TABLE IF EXISTS `sip_capture_01_18`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `sip_capture_01_18` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
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
  `direction` varchar(4) NOT NULL DEFAULT '',
  PRIMARY KEY (`id`),
  KEY `ruri_user` (`ruri_user`),
  KEY `from_user` (`from_user`),
  KEY `to_user` (`to_user`),
  KEY `pid_user` (`pid_user`),
  KEY `auth_user` (`auth_user`),
  KEY `callid_aleg` (`callid_aleg`),
  KEY `date` (`date`),
  KEY `callid` (`callid`)
) ENGINE=MyISAM AUTO_INCREMENT=1 DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `sip_capture_01_19`
--

DROP TABLE IF EXISTS `sip_capture_01_19`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `sip_capture_01_19` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
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
  `direction` varchar(4) NOT NULL DEFAULT '',
  PRIMARY KEY (`id`),
  KEY `ruri_user` (`ruri_user`),
  KEY `from_user` (`from_user`),
  KEY `to_user` (`to_user`),
  KEY `pid_user` (`pid_user`),
  KEY `auth_user` (`auth_user`),
  KEY `callid_aleg` (`callid_aleg`),
  KEY `date` (`date`),
  KEY `callid` (`callid`)
) ENGINE=MyISAM AUTO_INCREMENT=1 DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `sip_capture_01_20`
--

DROP TABLE IF EXISTS `sip_capture_01_20`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `sip_capture_01_20` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
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
  `direction` varchar(4) NOT NULL DEFAULT '',
  PRIMARY KEY (`id`),
  KEY `ruri_user` (`ruri_user`),
  KEY `from_user` (`from_user`),
  KEY `to_user` (`to_user`),
  KEY `pid_user` (`pid_user`),
  KEY `auth_user` (`auth_user`),
  KEY `callid_aleg` (`callid_aleg`),
  KEY `date` (`date`),
  KEY `callid` (`callid`)
) ENGINE=MyISAM AUTO_INCREMENT=1 DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `sip_capture_01_21`
--

DROP TABLE IF EXISTS `sip_capture_01_21`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `sip_capture_01_21` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
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
  `direction` varchar(4) NOT NULL DEFAULT '',
  PRIMARY KEY (`id`),
  KEY `ruri_user` (`ruri_user`),
  KEY `from_user` (`from_user`),
  KEY `to_user` (`to_user`),
  KEY `pid_user` (`pid_user`),
  KEY `auth_user` (`auth_user`),
  KEY `callid_aleg` (`callid_aleg`),
  KEY `date` (`date`),
  KEY `callid` (`callid`)
) ENGINE=MyISAM AUTO_INCREMENT=1 DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `sip_capture_01_22`
--

DROP TABLE IF EXISTS `sip_capture_01_22`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `sip_capture_01_22` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
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
  `direction` varchar(4) NOT NULL DEFAULT '',
  PRIMARY KEY (`id`),
  KEY `ruri_user` (`ruri_user`),
  KEY `from_user` (`from_user`),
  KEY `to_user` (`to_user`),
  KEY `pid_user` (`pid_user`),
  KEY `auth_user` (`auth_user`),
  KEY `callid_aleg` (`callid_aleg`),
  KEY `date` (`date`),
  KEY `callid` (`callid`)
) ENGINE=MyISAM AUTO_INCREMENT=1 DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `sip_capture_01_23`
--

DROP TABLE IF EXISTS `sip_capture_01_23`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `sip_capture_01_23` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
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
  `direction` varchar(4) NOT NULL DEFAULT '',
  PRIMARY KEY (`id`),
  KEY `ruri_user` (`ruri_user`),
  KEY `from_user` (`from_user`),
  KEY `to_user` (`to_user`),
  KEY `pid_user` (`pid_user`),
  KEY `auth_user` (`auth_user`),
  KEY `callid_aleg` (`callid_aleg`),
  KEY `date` (`date`),
  KEY `callid` (`callid`)
) ENGINE=MyISAM AUTO_INCREMENT=1 DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `sip_capture_02_00`
--

DROP TABLE IF EXISTS `sip_capture_02_00`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `sip_capture_02_00` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
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
  `direction` varchar(4) NOT NULL DEFAULT '',
  PRIMARY KEY (`id`),
  KEY `ruri_user` (`ruri_user`),
  KEY `from_user` (`from_user`),
  KEY `to_user` (`to_user`),
  KEY `pid_user` (`pid_user`),
  KEY `auth_user` (`auth_user`),
  KEY `callid_aleg` (`callid_aleg`),
  KEY `date` (`date`),
  KEY `callid` (`callid`)
) ENGINE=MyISAM AUTO_INCREMENT=1 DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `sip_capture_02_01`
--

DROP TABLE IF EXISTS `sip_capture_02_01`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `sip_capture_02_01` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
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
  `direction` varchar(4) NOT NULL DEFAULT '',
  PRIMARY KEY (`id`),
  KEY `ruri_user` (`ruri_user`),
  KEY `from_user` (`from_user`),
  KEY `to_user` (`to_user`),
  KEY `pid_user` (`pid_user`),
  KEY `auth_user` (`auth_user`),
  KEY `callid_aleg` (`callid_aleg`),
  KEY `date` (`date`),
  KEY `callid` (`callid`)
) ENGINE=MyISAM AUTO_INCREMENT=1 DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `sip_capture_02_02`
--

DROP TABLE IF EXISTS `sip_capture_02_02`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `sip_capture_02_02` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
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
  `direction` varchar(4) NOT NULL DEFAULT '',
  PRIMARY KEY (`id`),
  KEY `ruri_user` (`ruri_user`),
  KEY `from_user` (`from_user`),
  KEY `to_user` (`to_user`),
  KEY `pid_user` (`pid_user`),
  KEY `auth_user` (`auth_user`),
  KEY `callid_aleg` (`callid_aleg`),
  KEY `date` (`date`),
  KEY `callid` (`callid`)
) ENGINE=MyISAM AUTO_INCREMENT=1 DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `sip_capture_02_03`
--

DROP TABLE IF EXISTS `sip_capture_02_03`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `sip_capture_02_03` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
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
  `direction` varchar(4) NOT NULL DEFAULT '',
  PRIMARY KEY (`id`),
  KEY `ruri_user` (`ruri_user`),
  KEY `from_user` (`from_user`),
  KEY `to_user` (`to_user`),
  KEY `pid_user` (`pid_user`),
  KEY `auth_user` (`auth_user`),
  KEY `callid_aleg` (`callid_aleg`),
  KEY `date` (`date`),
  KEY `callid` (`callid`)
) ENGINE=MyISAM AUTO_INCREMENT=1 DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `sip_capture_02_04`
--

DROP TABLE IF EXISTS `sip_capture_02_04`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `sip_capture_02_04` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
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
  `direction` varchar(4) NOT NULL DEFAULT '',
  PRIMARY KEY (`id`),
  KEY `ruri_user` (`ruri_user`),
  KEY `from_user` (`from_user`),
  KEY `to_user` (`to_user`),
  KEY `pid_user` (`pid_user`),
  KEY `auth_user` (`auth_user`),
  KEY `callid_aleg` (`callid_aleg`),
  KEY `date` (`date`),
  KEY `callid` (`callid`)
) ENGINE=MyISAM AUTO_INCREMENT=1 DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `sip_capture_02_05`
--

DROP TABLE IF EXISTS `sip_capture_02_05`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `sip_capture_02_05` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
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
  `direction` varchar(4) NOT NULL DEFAULT '',
  PRIMARY KEY (`id`),
  KEY `ruri_user` (`ruri_user`),
  KEY `from_user` (`from_user`),
  KEY `to_user` (`to_user`),
  KEY `pid_user` (`pid_user`),
  KEY `auth_user` (`auth_user`),
  KEY `callid_aleg` (`callid_aleg`),
  KEY `date` (`date`),
  KEY `callid` (`callid`)
) ENGINE=MyISAM AUTO_INCREMENT=1 DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `sip_capture_02_06`
--

DROP TABLE IF EXISTS `sip_capture_02_06`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `sip_capture_02_06` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
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
  `direction` varchar(4) NOT NULL DEFAULT '',
  PRIMARY KEY (`id`),
  KEY `ruri_user` (`ruri_user`),
  KEY `from_user` (`from_user`),
  KEY `to_user` (`to_user`),
  KEY `pid_user` (`pid_user`),
  KEY `auth_user` (`auth_user`),
  KEY `callid_aleg` (`callid_aleg`),
  KEY `date` (`date`),
  KEY `callid` (`callid`)
) ENGINE=MyISAM AUTO_INCREMENT=1 DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `sip_capture_02_07`
--

DROP TABLE IF EXISTS `sip_capture_02_07`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `sip_capture_02_07` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
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
  `direction` varchar(4) NOT NULL DEFAULT '',
  PRIMARY KEY (`id`),
  KEY `ruri_user` (`ruri_user`),
  KEY `from_user` (`from_user`),
  KEY `to_user` (`to_user`),
  KEY `pid_user` (`pid_user`),
  KEY `auth_user` (`auth_user`),
  KEY `callid_aleg` (`callid_aleg`),
  KEY `date` (`date`),
  KEY `callid` (`callid`)
) ENGINE=MyISAM AUTO_INCREMENT=1 DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `sip_capture_02_08`
--

DROP TABLE IF EXISTS `sip_capture_02_08`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `sip_capture_02_08` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
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
  `direction` varchar(4) NOT NULL DEFAULT '',
  PRIMARY KEY (`id`),
  KEY `ruri_user` (`ruri_user`),
  KEY `from_user` (`from_user`),
  KEY `to_user` (`to_user`),
  KEY `pid_user` (`pid_user`),
  KEY `auth_user` (`auth_user`),
  KEY `callid_aleg` (`callid_aleg`),
  KEY `date` (`date`),
  KEY `callid` (`callid`)
) ENGINE=MyISAM AUTO_INCREMENT=1 DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `sip_capture_02_09`
--

DROP TABLE IF EXISTS `sip_capture_02_09`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `sip_capture_02_09` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
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
  `direction` varchar(4) NOT NULL DEFAULT '',
  PRIMARY KEY (`id`),
  KEY `ruri_user` (`ruri_user`),
  KEY `from_user` (`from_user`),
  KEY `to_user` (`to_user`),
  KEY `pid_user` (`pid_user`),
  KEY `auth_user` (`auth_user`),
  KEY `callid_aleg` (`callid_aleg`),
  KEY `date` (`date`),
  KEY `callid` (`callid`)
) ENGINE=MyISAM AUTO_INCREMENT=1 DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `sip_capture_02_10`
--

DROP TABLE IF EXISTS `sip_capture_02_10`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `sip_capture_02_10` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
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
  `direction` varchar(4) NOT NULL DEFAULT '',
  PRIMARY KEY (`id`),
  KEY `ruri_user` (`ruri_user`),
  KEY `from_user` (`from_user`),
  KEY `to_user` (`to_user`),
  KEY `pid_user` (`pid_user`),
  KEY `auth_user` (`auth_user`),
  KEY `callid_aleg` (`callid_aleg`),
  KEY `date` (`date`),
  KEY `callid` (`callid`)
) ENGINE=MyISAM AUTO_INCREMENT=1 DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `sip_capture_02_11`
--

DROP TABLE IF EXISTS `sip_capture_02_11`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `sip_capture_02_11` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
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
  `direction` varchar(4) NOT NULL DEFAULT '',
  PRIMARY KEY (`id`),
  KEY `ruri_user` (`ruri_user`),
  KEY `from_user` (`from_user`),
  KEY `to_user` (`to_user`),
  KEY `pid_user` (`pid_user`),
  KEY `auth_user` (`auth_user`),
  KEY `callid_aleg` (`callid_aleg`),
  KEY `date` (`date`),
  KEY `callid` (`callid`)
) ENGINE=MyISAM AUTO_INCREMENT=1 DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `sip_capture_02_12`
--

DROP TABLE IF EXISTS `sip_capture_02_12`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `sip_capture_02_12` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
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
  `direction` varchar(4) NOT NULL DEFAULT '',
  PRIMARY KEY (`id`),
  KEY `ruri_user` (`ruri_user`),
  KEY `from_user` (`from_user`),
  KEY `to_user` (`to_user`),
  KEY `pid_user` (`pid_user`),
  KEY `auth_user` (`auth_user`),
  KEY `callid_aleg` (`callid_aleg`),
  KEY `date` (`date`),
  KEY `callid` (`callid`)
) ENGINE=MyISAM AUTO_INCREMENT=1 DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `sip_capture_02_13`
--

DROP TABLE IF EXISTS `sip_capture_02_13`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `sip_capture_02_13` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
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
  `direction` varchar(4) NOT NULL DEFAULT '',
  PRIMARY KEY (`id`),
  KEY `ruri_user` (`ruri_user`),
  KEY `from_user` (`from_user`),
  KEY `to_user` (`to_user`),
  KEY `pid_user` (`pid_user`),
  KEY `auth_user` (`auth_user`),
  KEY `callid_aleg` (`callid_aleg`),
  KEY `date` (`date`),
  KEY `callid` (`callid`)
) ENGINE=MyISAM AUTO_INCREMENT=1 DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `sip_capture_02_14`
--

DROP TABLE IF EXISTS `sip_capture_02_14`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `sip_capture_02_14` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
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
  `direction` varchar(4) NOT NULL DEFAULT '',
  PRIMARY KEY (`id`),
  KEY `ruri_user` (`ruri_user`),
  KEY `from_user` (`from_user`),
  KEY `to_user` (`to_user`),
  KEY `pid_user` (`pid_user`),
  KEY `auth_user` (`auth_user`),
  KEY `callid_aleg` (`callid_aleg`),
  KEY `date` (`date`),
  KEY `callid` (`callid`)
) ENGINE=MyISAM AUTO_INCREMENT=1 DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `sip_capture_02_15`
--

DROP TABLE IF EXISTS `sip_capture_02_15`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `sip_capture_02_15` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
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
  `direction` varchar(4) NOT NULL DEFAULT '',
  PRIMARY KEY (`id`),
  KEY `ruri_user` (`ruri_user`),
  KEY `from_user` (`from_user`),
  KEY `to_user` (`to_user`),
  KEY `pid_user` (`pid_user`),
  KEY `auth_user` (`auth_user`),
  KEY `callid_aleg` (`callid_aleg`),
  KEY `date` (`date`),
  KEY `callid` (`callid`)
) ENGINE=MyISAM AUTO_INCREMENT=1 DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `sip_capture_02_16`
--

DROP TABLE IF EXISTS `sip_capture_02_16`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `sip_capture_02_16` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
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
  `direction` varchar(4) NOT NULL DEFAULT '',
  PRIMARY KEY (`id`),
  KEY `ruri_user` (`ruri_user`),
  KEY `from_user` (`from_user`),
  KEY `to_user` (`to_user`),
  KEY `pid_user` (`pid_user`),
  KEY `auth_user` (`auth_user`),
  KEY `callid_aleg` (`callid_aleg`),
  KEY `date` (`date`),
  KEY `callid` (`callid`)
) ENGINE=MyISAM AUTO_INCREMENT=1 DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `sip_capture_02_17`
--

DROP TABLE IF EXISTS `sip_capture_02_17`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `sip_capture_02_17` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
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
  `direction` varchar(4) NOT NULL DEFAULT '',
  PRIMARY KEY (`id`),
  KEY `ruri_user` (`ruri_user`),
  KEY `from_user` (`from_user`),
  KEY `to_user` (`to_user`),
  KEY `pid_user` (`pid_user`),
  KEY `auth_user` (`auth_user`),
  KEY `callid_aleg` (`callid_aleg`),
  KEY `date` (`date`),
  KEY `callid` (`callid`)
) ENGINE=MyISAM AUTO_INCREMENT=1 DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `sip_capture_02_18`
--

DROP TABLE IF EXISTS `sip_capture_02_18`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `sip_capture_02_18` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
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
  `direction` varchar(4) NOT NULL DEFAULT '',
  PRIMARY KEY (`id`),
  KEY `ruri_user` (`ruri_user`),
  KEY `from_user` (`from_user`),
  KEY `to_user` (`to_user`),
  KEY `pid_user` (`pid_user`),
  KEY `auth_user` (`auth_user`),
  KEY `callid_aleg` (`callid_aleg`),
  KEY `date` (`date`),
  KEY `callid` (`callid`)
) ENGINE=MyISAM AUTO_INCREMENT=1 DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `sip_capture_02_19`
--

DROP TABLE IF EXISTS `sip_capture_02_19`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `sip_capture_02_19` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
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
  `direction` varchar(4) NOT NULL DEFAULT '',
  PRIMARY KEY (`id`),
  KEY `ruri_user` (`ruri_user`),
  KEY `from_user` (`from_user`),
  KEY `to_user` (`to_user`),
  KEY `pid_user` (`pid_user`),
  KEY `auth_user` (`auth_user`),
  KEY `callid_aleg` (`callid_aleg`),
  KEY `date` (`date`),
  KEY `callid` (`callid`)
) ENGINE=MyISAM AUTO_INCREMENT=1 DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `sip_capture_02_20`
--

DROP TABLE IF EXISTS `sip_capture_02_20`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `sip_capture_02_20` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
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
  `direction` varchar(4) NOT NULL DEFAULT '',
  PRIMARY KEY (`id`),
  KEY `ruri_user` (`ruri_user`),
  KEY `from_user` (`from_user`),
  KEY `to_user` (`to_user`),
  KEY `pid_user` (`pid_user`),
  KEY `auth_user` (`auth_user`),
  KEY `callid_aleg` (`callid_aleg`),
  KEY `date` (`date`),
  KEY `callid` (`callid`)
) ENGINE=MyISAM AUTO_INCREMENT=1 DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `sip_capture_02_21`
--

DROP TABLE IF EXISTS `sip_capture_02_21`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `sip_capture_02_21` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
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
  `direction` varchar(4) NOT NULL DEFAULT '',
  PRIMARY KEY (`id`),
  KEY `ruri_user` (`ruri_user`),
  KEY `from_user` (`from_user`),
  KEY `to_user` (`to_user`),
  KEY `pid_user` (`pid_user`),
  KEY `auth_user` (`auth_user`),
  KEY `callid_aleg` (`callid_aleg`),
  KEY `date` (`date`),
  KEY `callid` (`callid`)
) ENGINE=MyISAM AUTO_INCREMENT=1 DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `sip_capture_02_22`
--

DROP TABLE IF EXISTS `sip_capture_02_22`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `sip_capture_02_22` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
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
  `direction` varchar(4) NOT NULL DEFAULT '',
  PRIMARY KEY (`id`),
  KEY `ruri_user` (`ruri_user`),
  KEY `from_user` (`from_user`),
  KEY `to_user` (`to_user`),
  KEY `pid_user` (`pid_user`),
  KEY `auth_user` (`auth_user`),
  KEY `callid_aleg` (`callid_aleg`),
  KEY `date` (`date`),
  KEY `callid` (`callid`)
) ENGINE=MyISAM AUTO_INCREMENT=1 DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `sip_capture_02_23`
--

DROP TABLE IF EXISTS `sip_capture_02_23`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `sip_capture_02_23` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
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
  `direction` varchar(4) NOT NULL DEFAULT '',
  PRIMARY KEY (`id`),
  KEY `ruri_user` (`ruri_user`),
  KEY `from_user` (`from_user`),
  KEY `to_user` (`to_user`),
  KEY `pid_user` (`pid_user`),
  KEY `auth_user` (`auth_user`),
  KEY `callid_aleg` (`callid_aleg`),
  KEY `date` (`date`),
  KEY `callid` (`callid`)
) ENGINE=MyISAM AUTO_INCREMENT=1 DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `sip_capture_03_00`
--

DROP TABLE IF EXISTS `sip_capture_03_00`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `sip_capture_03_00` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
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
  `direction` varchar(4) NOT NULL DEFAULT '',
  PRIMARY KEY (`id`),
  KEY `ruri_user` (`ruri_user`),
  KEY `from_user` (`from_user`),
  KEY `to_user` (`to_user`),
  KEY `pid_user` (`pid_user`),
  KEY `auth_user` (`auth_user`),
  KEY `callid_aleg` (`callid_aleg`),
  KEY `date` (`date`),
  KEY `callid` (`callid`)
) ENGINE=MyISAM AUTO_INCREMENT=1 DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `sip_capture_03_01`
--

DROP TABLE IF EXISTS `sip_capture_03_01`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `sip_capture_03_01` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
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
  `direction` varchar(4) NOT NULL DEFAULT '',
  PRIMARY KEY (`id`),
  KEY `ruri_user` (`ruri_user`),
  KEY `from_user` (`from_user`),
  KEY `to_user` (`to_user`),
  KEY `pid_user` (`pid_user`),
  KEY `auth_user` (`auth_user`),
  KEY `callid_aleg` (`callid_aleg`),
  KEY `date` (`date`),
  KEY `callid` (`callid`)
) ENGINE=MyISAM AUTO_INCREMENT=1 DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `sip_capture_03_02`
--

DROP TABLE IF EXISTS `sip_capture_03_02`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `sip_capture_03_02` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
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
  `direction` varchar(4) NOT NULL DEFAULT '',
  PRIMARY KEY (`id`),
  KEY `ruri_user` (`ruri_user`),
  KEY `from_user` (`from_user`),
  KEY `to_user` (`to_user`),
  KEY `pid_user` (`pid_user`),
  KEY `auth_user` (`auth_user`),
  KEY `callid_aleg` (`callid_aleg`),
  KEY `date` (`date`),
  KEY `callid` (`callid`)
) ENGINE=MyISAM DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `sip_capture_03_03`
--

DROP TABLE IF EXISTS `sip_capture_03_03`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `sip_capture_03_03` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
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
  `direction` varchar(4) NOT NULL DEFAULT '',
  PRIMARY KEY (`id`),
  KEY `ruri_user` (`ruri_user`),
  KEY `from_user` (`from_user`),
  KEY `to_user` (`to_user`),
  KEY `pid_user` (`pid_user`),
  KEY `auth_user` (`auth_user`),
  KEY `callid_aleg` (`callid_aleg`),
  KEY `date` (`date`),
  KEY `callid` (`callid`)
) ENGINE=MyISAM DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `sip_capture_03_04`
--

DROP TABLE IF EXISTS `sip_capture_03_04`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `sip_capture_03_04` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
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
  `direction` varchar(4) NOT NULL DEFAULT '',
  PRIMARY KEY (`id`),
  KEY `ruri_user` (`ruri_user`),
  KEY `from_user` (`from_user`),
  KEY `to_user` (`to_user`),
  KEY `pid_user` (`pid_user`),
  KEY `auth_user` (`auth_user`),
  KEY `callid_aleg` (`callid_aleg`),
  KEY `date` (`date`),
  KEY `callid` (`callid`)
) ENGINE=MyISAM DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `sip_capture_03_05`
--

DROP TABLE IF EXISTS `sip_capture_03_05`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `sip_capture_03_05` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
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
  `direction` varchar(4) NOT NULL DEFAULT '',
  PRIMARY KEY (`id`),
  KEY `ruri_user` (`ruri_user`),
  KEY `from_user` (`from_user`),
  KEY `to_user` (`to_user`),
  KEY `pid_user` (`pid_user`),
  KEY `auth_user` (`auth_user`),
  KEY `callid_aleg` (`callid_aleg`),
  KEY `date` (`date`),
  KEY `callid` (`callid`)
) ENGINE=MyISAM DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `sip_capture_03_06`
--

DROP TABLE IF EXISTS `sip_capture_03_06`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `sip_capture_03_06` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
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
  `direction` varchar(4) NOT NULL DEFAULT '',
  PRIMARY KEY (`id`),
  KEY `ruri_user` (`ruri_user`),
  KEY `from_user` (`from_user`),
  KEY `to_user` (`to_user`),
  KEY `pid_user` (`pid_user`),
  KEY `auth_user` (`auth_user`),
  KEY `callid_aleg` (`callid_aleg`),
  KEY `date` (`date`),
  KEY `callid` (`callid`)
) ENGINE=MyISAM DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `sip_capture_03_07`
--

DROP TABLE IF EXISTS `sip_capture_03_07`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `sip_capture_03_07` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
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
  `direction` varchar(4) NOT NULL DEFAULT '',
  PRIMARY KEY (`id`),
  KEY `ruri_user` (`ruri_user`),
  KEY `from_user` (`from_user`),
  KEY `to_user` (`to_user`),
  KEY `pid_user` (`pid_user`),
  KEY `auth_user` (`auth_user`),
  KEY `callid_aleg` (`callid_aleg`),
  KEY `date` (`date`),
  KEY `callid` (`callid`)
) ENGINE=MyISAM DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `sip_capture_03_08`
--

DROP TABLE IF EXISTS `sip_capture_03_08`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `sip_capture_03_08` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
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
  `direction` varchar(4) NOT NULL DEFAULT '',
  PRIMARY KEY (`id`),
  KEY `ruri_user` (`ruri_user`),
  KEY `from_user` (`from_user`),
  KEY `to_user` (`to_user`),
  KEY `pid_user` (`pid_user`),
  KEY `auth_user` (`auth_user`),
  KEY `callid_aleg` (`callid_aleg`),
  KEY `date` (`date`),
  KEY `callid` (`callid`)
) ENGINE=MyISAM DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `sip_capture_03_09`
--

DROP TABLE IF EXISTS `sip_capture_03_09`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `sip_capture_03_09` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
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
  `direction` varchar(4) NOT NULL DEFAULT '',
  PRIMARY KEY (`id`),
  KEY `ruri_user` (`ruri_user`),
  KEY `from_user` (`from_user`),
  KEY `to_user` (`to_user`),
  KEY `pid_user` (`pid_user`),
  KEY `auth_user` (`auth_user`),
  KEY `callid_aleg` (`callid_aleg`),
  KEY `date` (`date`),
  KEY `callid` (`callid`)
) ENGINE=MyISAM DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `sip_capture_03_10`
--

DROP TABLE IF EXISTS `sip_capture_03_10`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `sip_capture_03_10` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
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
  `direction` varchar(4) NOT NULL DEFAULT '',
  PRIMARY KEY (`id`),
  KEY `ruri_user` (`ruri_user`),
  KEY `from_user` (`from_user`),
  KEY `to_user` (`to_user`),
  KEY `pid_user` (`pid_user`),
  KEY `auth_user` (`auth_user`),
  KEY `callid_aleg` (`callid_aleg`),
  KEY `date` (`date`),
  KEY `callid` (`callid`)
) ENGINE=MyISAM DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `sip_capture_03_11`
--

DROP TABLE IF EXISTS `sip_capture_03_11`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `sip_capture_03_11` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
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
  `direction` varchar(4) NOT NULL DEFAULT '',
  PRIMARY KEY (`id`),
  KEY `ruri_user` (`ruri_user`),
  KEY `from_user` (`from_user`),
  KEY `to_user` (`to_user`),
  KEY `pid_user` (`pid_user`),
  KEY `auth_user` (`auth_user`),
  KEY `callid_aleg` (`callid_aleg`),
  KEY `date` (`date`),
  KEY `callid` (`callid`)
) ENGINE=MyISAM DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `sip_capture_03_12`
--

DROP TABLE IF EXISTS `sip_capture_03_12`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `sip_capture_03_12` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
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
  `direction` varchar(4) NOT NULL DEFAULT '',
  PRIMARY KEY (`id`),
  KEY `ruri_user` (`ruri_user`),
  KEY `from_user` (`from_user`),
  KEY `to_user` (`to_user`),
  KEY `pid_user` (`pid_user`),
  KEY `auth_user` (`auth_user`),
  KEY `callid_aleg` (`callid_aleg`),
  KEY `date` (`date`),
  KEY `callid` (`callid`)
) ENGINE=MyISAM DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `sip_capture_03_13`
--

DROP TABLE IF EXISTS `sip_capture_03_13`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `sip_capture_03_13` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
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
  `direction` varchar(4) NOT NULL DEFAULT '',
  PRIMARY KEY (`id`),
  KEY `ruri_user` (`ruri_user`),
  KEY `from_user` (`from_user`),
  KEY `to_user` (`to_user`),
  KEY `pid_user` (`pid_user`),
  KEY `auth_user` (`auth_user`),
  KEY `callid_aleg` (`callid_aleg`),
  KEY `date` (`date`),
  KEY `callid` (`callid`)
) ENGINE=MyISAM DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `sip_capture_03_14`
--

DROP TABLE IF EXISTS `sip_capture_03_14`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `sip_capture_03_14` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
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
  `direction` varchar(4) NOT NULL DEFAULT '',
  PRIMARY KEY (`id`),
  KEY `ruri_user` (`ruri_user`),
  KEY `from_user` (`from_user`),
  KEY `to_user` (`to_user`),
  KEY `pid_user` (`pid_user`),
  KEY `auth_user` (`auth_user`),
  KEY `callid_aleg` (`callid_aleg`),
  KEY `date` (`date`),
  KEY `callid` (`callid`)
) ENGINE=MyISAM DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `sip_capture_03_15`
--

DROP TABLE IF EXISTS `sip_capture_03_15`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `sip_capture_03_15` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
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
  `direction` varchar(4) NOT NULL DEFAULT '',
  PRIMARY KEY (`id`),
  KEY `ruri_user` (`ruri_user`),
  KEY `from_user` (`from_user`),
  KEY `to_user` (`to_user`),
  KEY `pid_user` (`pid_user`),
  KEY `auth_user` (`auth_user`),
  KEY `callid_aleg` (`callid_aleg`),
  KEY `date` (`date`),
  KEY `callid` (`callid`)
) ENGINE=MyISAM DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `sip_capture_03_16`
--

DROP TABLE IF EXISTS `sip_capture_03_16`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `sip_capture_03_16` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
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
  `direction` varchar(4) NOT NULL DEFAULT '',
  PRIMARY KEY (`id`),
  KEY `ruri_user` (`ruri_user`),
  KEY `from_user` (`from_user`),
  KEY `to_user` (`to_user`),
  KEY `pid_user` (`pid_user`),
  KEY `auth_user` (`auth_user`),
  KEY `callid_aleg` (`callid_aleg`),
  KEY `date` (`date`),
  KEY `callid` (`callid`)
) ENGINE=MyISAM DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `sip_capture_03_17`
--

DROP TABLE IF EXISTS `sip_capture_03_17`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `sip_capture_03_17` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
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
  `direction` varchar(4) NOT NULL DEFAULT '',
  PRIMARY KEY (`id`),
  KEY `ruri_user` (`ruri_user`),
  KEY `from_user` (`from_user`),
  KEY `to_user` (`to_user`),
  KEY `pid_user` (`pid_user`),
  KEY `auth_user` (`auth_user`),
  KEY `callid_aleg` (`callid_aleg`),
  KEY `date` (`date`),
  KEY `callid` (`callid`)
) ENGINE=MyISAM AUTO_INCREMENT=1 DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `sip_capture_03_18`
--

DROP TABLE IF EXISTS `sip_capture_03_18`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `sip_capture_03_18` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
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
  `direction` varchar(4) NOT NULL DEFAULT '',
  PRIMARY KEY (`id`),
  KEY `ruri_user` (`ruri_user`),
  KEY `from_user` (`from_user`),
  KEY `to_user` (`to_user`),
  KEY `pid_user` (`pid_user`),
  KEY `auth_user` (`auth_user`),
  KEY `callid_aleg` (`callid_aleg`),
  KEY `date` (`date`),
  KEY `callid` (`callid`)
) ENGINE=MyISAM AUTO_INCREMENT=1 DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `sip_capture_03_19`
--

DROP TABLE IF EXISTS `sip_capture_03_19`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `sip_capture_03_19` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
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
  `direction` varchar(4) NOT NULL DEFAULT '',
  PRIMARY KEY (`id`),
  KEY `ruri_user` (`ruri_user`),
  KEY `from_user` (`from_user`),
  KEY `to_user` (`to_user`),
  KEY `pid_user` (`pid_user`),
  KEY `auth_user` (`auth_user`),
  KEY `callid_aleg` (`callid_aleg`),
  KEY `date` (`date`),
  KEY `callid` (`callid`)
) ENGINE=MyISAM DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `sip_capture_03_20`
--

DROP TABLE IF EXISTS `sip_capture_03_20`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `sip_capture_03_20` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
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
  `direction` varchar(4) NOT NULL DEFAULT '',
  PRIMARY KEY (`id`),
  KEY `ruri_user` (`ruri_user`),
  KEY `from_user` (`from_user`),
  KEY `to_user` (`to_user`),
  KEY `pid_user` (`pid_user`),
  KEY `auth_user` (`auth_user`),
  KEY `callid_aleg` (`callid_aleg`),
  KEY `date` (`date`),
  KEY `callid` (`callid`)
) ENGINE=MyISAM DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `sip_capture_03_21`
--

DROP TABLE IF EXISTS `sip_capture_03_21`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `sip_capture_03_21` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
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
  `direction` varchar(4) NOT NULL DEFAULT '',
  PRIMARY KEY (`id`),
  KEY `ruri_user` (`ruri_user`),
  KEY `from_user` (`from_user`),
  KEY `to_user` (`to_user`),
  KEY `pid_user` (`pid_user`),
  KEY `auth_user` (`auth_user`),
  KEY `callid_aleg` (`callid_aleg`),
  KEY `date` (`date`),
  KEY `callid` (`callid`)
) ENGINE=MyISAM DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `sip_capture_03_22`
--

DROP TABLE IF EXISTS `sip_capture_03_22`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `sip_capture_03_22` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
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
  `direction` varchar(4) NOT NULL DEFAULT '',
  PRIMARY KEY (`id`),
  KEY `ruri_user` (`ruri_user`),
  KEY `from_user` (`from_user`),
  KEY `to_user` (`to_user`),
  KEY `pid_user` (`pid_user`),
  KEY `auth_user` (`auth_user`),
  KEY `callid_aleg` (`callid_aleg`),
  KEY `date` (`date`),
  KEY `callid` (`callid`)
) ENGINE=MyISAM DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `sip_capture_03_23`
--

DROP TABLE IF EXISTS `sip_capture_03_23`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `sip_capture_03_23` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
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
  `direction` varchar(4) NOT NULL DEFAULT '',
  PRIMARY KEY (`id`),
  KEY `ruri_user` (`ruri_user`),
  KEY `from_user` (`from_user`),
  KEY `to_user` (`to_user`),
  KEY `pid_user` (`pid_user`),
  KEY `auth_user` (`auth_user`),
  KEY `callid_aleg` (`callid_aleg`),
  KEY `date` (`date`),
  KEY `callid` (`callid`)
) ENGINE=MyISAM DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `sip_capture_04_00`
--

DROP TABLE IF EXISTS `sip_capture_04_00`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `sip_capture_04_00` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
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
  `direction` varchar(4) NOT NULL DEFAULT '',
  PRIMARY KEY (`id`),
  KEY `ruri_user` (`ruri_user`),
  KEY `from_user` (`from_user`),
  KEY `to_user` (`to_user`),
  KEY `pid_user` (`pid_user`),
  KEY `auth_user` (`auth_user`),
  KEY `callid_aleg` (`callid_aleg`),
  KEY `date` (`date`),
  KEY `callid` (`callid`)
) ENGINE=MyISAM DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `sip_capture_04_01`
--

DROP TABLE IF EXISTS `sip_capture_04_01`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `sip_capture_04_01` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
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
  `direction` varchar(4) NOT NULL DEFAULT '',
  PRIMARY KEY (`id`),
  KEY `ruri_user` (`ruri_user`),
  KEY `from_user` (`from_user`),
  KEY `to_user` (`to_user`),
  KEY `pid_user` (`pid_user`),
  KEY `auth_user` (`auth_user`),
  KEY `callid_aleg` (`callid_aleg`),
  KEY `date` (`date`),
  KEY `callid` (`callid`)
) ENGINE=MyISAM DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `sip_capture_04_02`
--

DROP TABLE IF EXISTS `sip_capture_04_02`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `sip_capture_04_02` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
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
  `direction` varchar(4) NOT NULL DEFAULT '',
  PRIMARY KEY (`id`),
  KEY `ruri_user` (`ruri_user`),
  KEY `from_user` (`from_user`),
  KEY `to_user` (`to_user`),
  KEY `pid_user` (`pid_user`),
  KEY `auth_user` (`auth_user`),
  KEY `callid_aleg` (`callid_aleg`),
  KEY `date` (`date`),
  KEY `callid` (`callid`)
) ENGINE=MyISAM DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `sip_capture_04_03`
--

DROP TABLE IF EXISTS `sip_capture_04_03`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `sip_capture_04_03` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
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
  `direction` varchar(4) NOT NULL DEFAULT '',
  PRIMARY KEY (`id`),
  KEY `ruri_user` (`ruri_user`),
  KEY `from_user` (`from_user`),
  KEY `to_user` (`to_user`),
  KEY `pid_user` (`pid_user`),
  KEY `auth_user` (`auth_user`),
  KEY `callid_aleg` (`callid_aleg`),
  KEY `date` (`date`),
  KEY `callid` (`callid`)
) ENGINE=MyISAM DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `sip_capture_04_04`
--

DROP TABLE IF EXISTS `sip_capture_04_04`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `sip_capture_04_04` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
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
  `direction` varchar(4) NOT NULL DEFAULT '',
  PRIMARY KEY (`id`),
  KEY `ruri_user` (`ruri_user`),
  KEY `from_user` (`from_user`),
  KEY `to_user` (`to_user`),
  KEY `pid_user` (`pid_user`),
  KEY `auth_user` (`auth_user`),
  KEY `callid_aleg` (`callid_aleg`),
  KEY `date` (`date`),
  KEY `callid` (`callid`)
) ENGINE=MyISAM DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `sip_capture_04_05`
--

DROP TABLE IF EXISTS `sip_capture_04_05`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `sip_capture_04_05` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
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
  `direction` varchar(4) NOT NULL DEFAULT '',
  PRIMARY KEY (`id`),
  KEY `ruri_user` (`ruri_user`),
  KEY `from_user` (`from_user`),
  KEY `to_user` (`to_user`),
  KEY `pid_user` (`pid_user`),
  KEY `auth_user` (`auth_user`),
  KEY `callid_aleg` (`callid_aleg`),
  KEY `date` (`date`),
  KEY `callid` (`callid`)
) ENGINE=MyISAM DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `sip_capture_04_06`
--

DROP TABLE IF EXISTS `sip_capture_04_06`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `sip_capture_04_06` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
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
  `direction` varchar(4) NOT NULL DEFAULT '',
  PRIMARY KEY (`id`),
  KEY `ruri_user` (`ruri_user`),
  KEY `from_user` (`from_user`),
  KEY `to_user` (`to_user`),
  KEY `pid_user` (`pid_user`),
  KEY `auth_user` (`auth_user`),
  KEY `callid_aleg` (`callid_aleg`),
  KEY `date` (`date`),
  KEY `callid` (`callid`)
) ENGINE=MyISAM DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `sip_capture_04_07`
--

DROP TABLE IF EXISTS `sip_capture_04_07`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `sip_capture_04_07` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
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
  `direction` varchar(4) NOT NULL DEFAULT '',
  PRIMARY KEY (`id`),
  KEY `ruri_user` (`ruri_user`),
  KEY `from_user` (`from_user`),
  KEY `to_user` (`to_user`),
  KEY `pid_user` (`pid_user`),
  KEY `auth_user` (`auth_user`),
  KEY `callid_aleg` (`callid_aleg`),
  KEY `date` (`date`),
  KEY `callid` (`callid`)
) ENGINE=MyISAM DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `sip_capture_04_08`
--

DROP TABLE IF EXISTS `sip_capture_04_08`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `sip_capture_04_08` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
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
  `direction` varchar(4) NOT NULL DEFAULT '',
  PRIMARY KEY (`id`),
  KEY `ruri_user` (`ruri_user`),
  KEY `from_user` (`from_user`),
  KEY `to_user` (`to_user`),
  KEY `pid_user` (`pid_user`),
  KEY `auth_user` (`auth_user`),
  KEY `callid_aleg` (`callid_aleg`),
  KEY `date` (`date`),
  KEY `callid` (`callid`)
) ENGINE=MyISAM DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `sip_capture_04_09`
--

DROP TABLE IF EXISTS `sip_capture_04_09`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `sip_capture_04_09` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
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
  `direction` varchar(4) NOT NULL DEFAULT '',
  PRIMARY KEY (`id`),
  KEY `ruri_user` (`ruri_user`),
  KEY `from_user` (`from_user`),
  KEY `to_user` (`to_user`),
  KEY `pid_user` (`pid_user`),
  KEY `auth_user` (`auth_user`),
  KEY `callid_aleg` (`callid_aleg`),
  KEY `date` (`date`),
  KEY `callid` (`callid`)
) ENGINE=MyISAM DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `sip_capture_04_10`
--

DROP TABLE IF EXISTS `sip_capture_04_10`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `sip_capture_04_10` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
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
  `direction` varchar(4) NOT NULL DEFAULT '',
  PRIMARY KEY (`id`),
  KEY `ruri_user` (`ruri_user`),
  KEY `from_user` (`from_user`),
  KEY `to_user` (`to_user`),
  KEY `pid_user` (`pid_user`),
  KEY `auth_user` (`auth_user`),
  KEY `callid_aleg` (`callid_aleg`),
  KEY `date` (`date`),
  KEY `callid` (`callid`)
) ENGINE=MyISAM DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `sip_capture_04_11`
--

DROP TABLE IF EXISTS `sip_capture_04_11`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `sip_capture_04_11` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
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
  `direction` varchar(4) NOT NULL DEFAULT '',
  PRIMARY KEY (`id`),
  KEY `ruri_user` (`ruri_user`),
  KEY `from_user` (`from_user`),
  KEY `to_user` (`to_user`),
  KEY `pid_user` (`pid_user`),
  KEY `auth_user` (`auth_user`),
  KEY `callid_aleg` (`callid_aleg`),
  KEY `date` (`date`),
  KEY `callid` (`callid`)
) ENGINE=MyISAM DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `sip_capture_04_12`
--

DROP TABLE IF EXISTS `sip_capture_04_12`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `sip_capture_04_12` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
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
  `direction` varchar(4) NOT NULL DEFAULT '',
  PRIMARY KEY (`id`),
  KEY `ruri_user` (`ruri_user`),
  KEY `from_user` (`from_user`),
  KEY `to_user` (`to_user`),
  KEY `pid_user` (`pid_user`),
  KEY `auth_user` (`auth_user`),
  KEY `callid_aleg` (`callid_aleg`),
  KEY `date` (`date`),
  KEY `callid` (`callid`)
) ENGINE=MyISAM DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `sip_capture_04_13`
--

DROP TABLE IF EXISTS `sip_capture_04_13`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `sip_capture_04_13` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
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
  `direction` varchar(4) NOT NULL DEFAULT '',
  PRIMARY KEY (`id`),
  KEY `ruri_user` (`ruri_user`),
  KEY `from_user` (`from_user`),
  KEY `to_user` (`to_user`),
  KEY `pid_user` (`pid_user`),
  KEY `auth_user` (`auth_user`),
  KEY `callid_aleg` (`callid_aleg`),
  KEY `date` (`date`),
  KEY `callid` (`callid`)
) ENGINE=MyISAM DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `sip_capture_04_14`
--

DROP TABLE IF EXISTS `sip_capture_04_14`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `sip_capture_04_14` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
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
  `direction` varchar(4) NOT NULL DEFAULT '',
  PRIMARY KEY (`id`),
  KEY `ruri_user` (`ruri_user`),
  KEY `from_user` (`from_user`),
  KEY `to_user` (`to_user`),
  KEY `pid_user` (`pid_user`),
  KEY `auth_user` (`auth_user`),
  KEY `callid_aleg` (`callid_aleg`),
  KEY `date` (`date`),
  KEY `callid` (`callid`)
) ENGINE=MyISAM DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `sip_capture_04_15`
--

DROP TABLE IF EXISTS `sip_capture_04_15`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `sip_capture_04_15` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
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
  `direction` varchar(4) NOT NULL DEFAULT '',
  PRIMARY KEY (`id`),
  KEY `ruri_user` (`ruri_user`),
  KEY `from_user` (`from_user`),
  KEY `to_user` (`to_user`),
  KEY `pid_user` (`pid_user`),
  KEY `auth_user` (`auth_user`),
  KEY `callid_aleg` (`callid_aleg`),
  KEY `date` (`date`),
  KEY `callid` (`callid`)
) ENGINE=MyISAM DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `sip_capture_04_16`
--

DROP TABLE IF EXISTS `sip_capture_04_16`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `sip_capture_04_16` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
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
  `direction` varchar(4) NOT NULL DEFAULT '',
  PRIMARY KEY (`id`),
  KEY `ruri_user` (`ruri_user`),
  KEY `from_user` (`from_user`),
  KEY `to_user` (`to_user`),
  KEY `pid_user` (`pid_user`),
  KEY `auth_user` (`auth_user`),
  KEY `callid_aleg` (`callid_aleg`),
  KEY `date` (`date`),
  KEY `callid` (`callid`)
) ENGINE=MyISAM DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `sip_capture_04_17`
--

DROP TABLE IF EXISTS `sip_capture_04_17`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `sip_capture_04_17` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
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
  `direction` varchar(4) NOT NULL DEFAULT '',
  PRIMARY KEY (`id`),
  KEY `ruri_user` (`ruri_user`),
  KEY `from_user` (`from_user`),
  KEY `to_user` (`to_user`),
  KEY `pid_user` (`pid_user`),
  KEY `auth_user` (`auth_user`),
  KEY `callid_aleg` (`callid_aleg`),
  KEY `date` (`date`),
  KEY `callid` (`callid`)
) ENGINE=MyISAM DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `sip_capture_04_18`
--

DROP TABLE IF EXISTS `sip_capture_04_18`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `sip_capture_04_18` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
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
  `direction` varchar(4) NOT NULL DEFAULT '',
  PRIMARY KEY (`id`),
  KEY `ruri_user` (`ruri_user`),
  KEY `from_user` (`from_user`),
  KEY `to_user` (`to_user`),
  KEY `pid_user` (`pid_user`),
  KEY `auth_user` (`auth_user`),
  KEY `callid_aleg` (`callid_aleg`),
  KEY `date` (`date`),
  KEY `callid` (`callid`)
) ENGINE=MyISAM DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `sip_capture_04_19`
--

DROP TABLE IF EXISTS `sip_capture_04_19`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `sip_capture_04_19` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
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
  `direction` varchar(4) NOT NULL DEFAULT '',
  PRIMARY KEY (`id`),
  KEY `ruri_user` (`ruri_user`),
  KEY `from_user` (`from_user`),
  KEY `to_user` (`to_user`),
  KEY `pid_user` (`pid_user`),
  KEY `auth_user` (`auth_user`),
  KEY `callid_aleg` (`callid_aleg`),
  KEY `date` (`date`),
  KEY `callid` (`callid`)
) ENGINE=MyISAM DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `sip_capture_04_20`
--

DROP TABLE IF EXISTS `sip_capture_04_20`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `sip_capture_04_20` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
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
  `direction` varchar(4) NOT NULL DEFAULT '',
  PRIMARY KEY (`id`),
  KEY `ruri_user` (`ruri_user`),
  KEY `from_user` (`from_user`),
  KEY `to_user` (`to_user`),
  KEY `pid_user` (`pid_user`),
  KEY `auth_user` (`auth_user`),
  KEY `callid_aleg` (`callid_aleg`),
  KEY `date` (`date`),
  KEY `callid` (`callid`)
) ENGINE=MyISAM DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `sip_capture_04_21`
--

DROP TABLE IF EXISTS `sip_capture_04_21`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `sip_capture_04_21` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
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
  `direction` varchar(4) NOT NULL DEFAULT '',
  PRIMARY KEY (`id`),
  KEY `ruri_user` (`ruri_user`),
  KEY `from_user` (`from_user`),
  KEY `to_user` (`to_user`),
  KEY `pid_user` (`pid_user`),
  KEY `auth_user` (`auth_user`),
  KEY `callid_aleg` (`callid_aleg`),
  KEY `date` (`date`),
  KEY `callid` (`callid`)
) ENGINE=MyISAM DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `sip_capture_04_22`
--

DROP TABLE IF EXISTS `sip_capture_04_22`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `sip_capture_04_22` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
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
  `direction` varchar(4) NOT NULL DEFAULT '',
  PRIMARY KEY (`id`),
  KEY `ruri_user` (`ruri_user`),
  KEY `from_user` (`from_user`),
  KEY `to_user` (`to_user`),
  KEY `pid_user` (`pid_user`),
  KEY `auth_user` (`auth_user`),
  KEY `callid_aleg` (`callid_aleg`),
  KEY `date` (`date`),
  KEY `callid` (`callid`)
) ENGINE=MyISAM DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `sip_capture_04_23`
--

DROP TABLE IF EXISTS `sip_capture_04_23`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `sip_capture_04_23` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
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
  `direction` varchar(4) NOT NULL DEFAULT '',
  PRIMARY KEY (`id`),
  KEY `ruri_user` (`ruri_user`),
  KEY `from_user` (`from_user`),
  KEY `to_user` (`to_user`),
  KEY `pid_user` (`pid_user`),
  KEY `auth_user` (`auth_user`),
  KEY `callid_aleg` (`callid_aleg`),
  KEY `date` (`date`),
  KEY `callid` (`callid`)
) ENGINE=MyISAM DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `sip_capture_05_00`
--

DROP TABLE IF EXISTS `sip_capture_05_00`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `sip_capture_05_00` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
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
  `direction` varchar(4) NOT NULL DEFAULT '',
  PRIMARY KEY (`id`),
  KEY `ruri_user` (`ruri_user`),
  KEY `from_user` (`from_user`),
  KEY `to_user` (`to_user`),
  KEY `pid_user` (`pid_user`),
  KEY `auth_user` (`auth_user`),
  KEY `callid_aleg` (`callid_aleg`),
  KEY `date` (`date`),
  KEY `callid` (`callid`)
) ENGINE=MyISAM AUTO_INCREMENT=1 DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `sip_capture_05_01`
--

DROP TABLE IF EXISTS `sip_capture_05_01`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `sip_capture_05_01` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
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
  `direction` varchar(4) NOT NULL DEFAULT '',
  PRIMARY KEY (`id`),
  KEY `ruri_user` (`ruri_user`),
  KEY `from_user` (`from_user`),
  KEY `to_user` (`to_user`),
  KEY `pid_user` (`pid_user`),
  KEY `auth_user` (`auth_user`),
  KEY `callid_aleg` (`callid_aleg`),
  KEY `date` (`date`),
  KEY `callid` (`callid`)
) ENGINE=MyISAM AUTO_INCREMENT=1 DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `sip_capture_05_02`
--

DROP TABLE IF EXISTS `sip_capture_05_02`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `sip_capture_05_02` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
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
  `direction` varchar(4) NOT NULL DEFAULT '',
  PRIMARY KEY (`id`),
  KEY `ruri_user` (`ruri_user`),
  KEY `from_user` (`from_user`),
  KEY `to_user` (`to_user`),
  KEY `pid_user` (`pid_user`),
  KEY `auth_user` (`auth_user`),
  KEY `callid_aleg` (`callid_aleg`),
  KEY `date` (`date`),
  KEY `callid` (`callid`)
) ENGINE=MyISAM AUTO_INCREMENT=1 DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `sip_capture_05_03`
--

DROP TABLE IF EXISTS `sip_capture_05_03`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `sip_capture_05_03` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
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
  `direction` varchar(4) NOT NULL DEFAULT '',
  PRIMARY KEY (`id`),
  KEY `ruri_user` (`ruri_user`),
  KEY `from_user` (`from_user`),
  KEY `to_user` (`to_user`),
  KEY `pid_user` (`pid_user`),
  KEY `auth_user` (`auth_user`),
  KEY `callid_aleg` (`callid_aleg`),
  KEY `date` (`date`),
  KEY `callid` (`callid`)
) ENGINE=MyISAM AUTO_INCREMENT=1 DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `sip_capture_05_04`
--

DROP TABLE IF EXISTS `sip_capture_05_04`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `sip_capture_05_04` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
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
  `direction` varchar(4) NOT NULL DEFAULT '',
  PRIMARY KEY (`id`),
  KEY `ruri_user` (`ruri_user`),
  KEY `from_user` (`from_user`),
  KEY `to_user` (`to_user`),
  KEY `pid_user` (`pid_user`),
  KEY `auth_user` (`auth_user`),
  KEY `callid_aleg` (`callid_aleg`),
  KEY `date` (`date`),
  KEY `callid` (`callid`)
) ENGINE=MyISAM AUTO_INCREMENT=1 DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `sip_capture_05_05`
--

DROP TABLE IF EXISTS `sip_capture_05_05`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `sip_capture_05_05` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
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
  `direction` varchar(4) NOT NULL DEFAULT '',
  PRIMARY KEY (`id`),
  KEY `ruri_user` (`ruri_user`),
  KEY `from_user` (`from_user`),
  KEY `to_user` (`to_user`),
  KEY `pid_user` (`pid_user`),
  KEY `auth_user` (`auth_user`),
  KEY `callid_aleg` (`callid_aleg`),
  KEY `date` (`date`),
  KEY `callid` (`callid`)
) ENGINE=MyISAM AUTO_INCREMENT=1 DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `sip_capture_05_06`
--

DROP TABLE IF EXISTS `sip_capture_05_06`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `sip_capture_05_06` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
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
  `direction` varchar(4) NOT NULL DEFAULT '',
  PRIMARY KEY (`id`),
  KEY `ruri_user` (`ruri_user`),
  KEY `from_user` (`from_user`),
  KEY `to_user` (`to_user`),
  KEY `pid_user` (`pid_user`),
  KEY `auth_user` (`auth_user`),
  KEY `callid_aleg` (`callid_aleg`),
  KEY `date` (`date`),
  KEY `callid` (`callid`)
) ENGINE=MyISAM AUTO_INCREMENT=1 DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `sip_capture_05_07`
--

DROP TABLE IF EXISTS `sip_capture_05_07`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `sip_capture_05_07` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
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
  `direction` varchar(4) NOT NULL DEFAULT '',
  PRIMARY KEY (`id`),
  KEY `ruri_user` (`ruri_user`),
  KEY `from_user` (`from_user`),
  KEY `to_user` (`to_user`),
  KEY `pid_user` (`pid_user`),
  KEY `auth_user` (`auth_user`),
  KEY `callid_aleg` (`callid_aleg`),
  KEY `date` (`date`),
  KEY `callid` (`callid`)
) ENGINE=MyISAM AUTO_INCREMENT=1 DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `sip_capture_05_08`
--

DROP TABLE IF EXISTS `sip_capture_05_08`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `sip_capture_05_08` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
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
  `direction` varchar(4) NOT NULL DEFAULT '',
  PRIMARY KEY (`id`),
  KEY `ruri_user` (`ruri_user`),
  KEY `from_user` (`from_user`),
  KEY `to_user` (`to_user`),
  KEY `pid_user` (`pid_user`),
  KEY `auth_user` (`auth_user`),
  KEY `callid_aleg` (`callid_aleg`),
  KEY `date` (`date`),
  KEY `callid` (`callid`)
) ENGINE=MyISAM AUTO_INCREMENT=1 DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `sip_capture_05_09`
--

DROP TABLE IF EXISTS `sip_capture_05_09`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `sip_capture_05_09` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
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
  `direction` varchar(4) NOT NULL DEFAULT '',
  PRIMARY KEY (`id`),
  KEY `ruri_user` (`ruri_user`),
  KEY `from_user` (`from_user`),
  KEY `to_user` (`to_user`),
  KEY `pid_user` (`pid_user`),
  KEY `auth_user` (`auth_user`),
  KEY `callid_aleg` (`callid_aleg`),
  KEY `date` (`date`),
  KEY `callid` (`callid`)
) ENGINE=MyISAM AUTO_INCREMENT=1 DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `sip_capture_05_10`
--

DROP TABLE IF EXISTS `sip_capture_05_10`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `sip_capture_05_10` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
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
  `direction` varchar(4) NOT NULL DEFAULT '',
  PRIMARY KEY (`id`),
  KEY `ruri_user` (`ruri_user`),
  KEY `from_user` (`from_user`),
  KEY `to_user` (`to_user`),
  KEY `pid_user` (`pid_user`),
  KEY `auth_user` (`auth_user`),
  KEY `callid_aleg` (`callid_aleg`),
  KEY `date` (`date`),
  KEY `callid` (`callid`)
) ENGINE=MyISAM AUTO_INCREMENT=1 DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `sip_capture_05_11`
--

DROP TABLE IF EXISTS `sip_capture_05_11`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `sip_capture_05_11` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
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
  `direction` varchar(4) NOT NULL DEFAULT '',
  PRIMARY KEY (`id`),
  KEY `ruri_user` (`ruri_user`),
  KEY `from_user` (`from_user`),
  KEY `to_user` (`to_user`),
  KEY `pid_user` (`pid_user`),
  KEY `auth_user` (`auth_user`),
  KEY `callid_aleg` (`callid_aleg`),
  KEY `date` (`date`),
  KEY `callid` (`callid`)
) ENGINE=MyISAM AUTO_INCREMENT=1 DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `sip_capture_05_12`
--

DROP TABLE IF EXISTS `sip_capture_05_12`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `sip_capture_05_12` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
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
  `direction` varchar(4) NOT NULL DEFAULT '',
  PRIMARY KEY (`id`),
  KEY `ruri_user` (`ruri_user`),
  KEY `from_user` (`from_user`),
  KEY `to_user` (`to_user`),
  KEY `pid_user` (`pid_user`),
  KEY `auth_user` (`auth_user`),
  KEY `callid_aleg` (`callid_aleg`),
  KEY `date` (`date`),
  KEY `callid` (`callid`)
) ENGINE=MyISAM AUTO_INCREMENT=1 DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `sip_capture_05_13`
--

DROP TABLE IF EXISTS `sip_capture_05_13`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `sip_capture_05_13` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
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
  `direction` varchar(4) NOT NULL DEFAULT '',
  PRIMARY KEY (`id`),
  KEY `ruri_user` (`ruri_user`),
  KEY `from_user` (`from_user`),
  KEY `to_user` (`to_user`),
  KEY `pid_user` (`pid_user`),
  KEY `auth_user` (`auth_user`),
  KEY `callid_aleg` (`callid_aleg`),
  KEY `date` (`date`),
  KEY `callid` (`callid`)
) ENGINE=MyISAM AUTO_INCREMENT=1 DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `sip_capture_05_14`
--

DROP TABLE IF EXISTS `sip_capture_05_14`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `sip_capture_05_14` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
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
  `direction` varchar(4) NOT NULL DEFAULT '',
  PRIMARY KEY (`id`),
  KEY `ruri_user` (`ruri_user`),
  KEY `from_user` (`from_user`),
  KEY `to_user` (`to_user`),
  KEY `pid_user` (`pid_user`),
  KEY `auth_user` (`auth_user`),
  KEY `callid_aleg` (`callid_aleg`),
  KEY `date` (`date`),
  KEY `callid` (`callid`)
) ENGINE=MyISAM AUTO_INCREMENT=1 DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `sip_capture_05_15`
--

DROP TABLE IF EXISTS `sip_capture_05_15`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `sip_capture_05_15` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
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
  `direction` varchar(4) NOT NULL DEFAULT '',
  PRIMARY KEY (`id`),
  KEY `ruri_user` (`ruri_user`),
  KEY `from_user` (`from_user`),
  KEY `to_user` (`to_user`),
  KEY `pid_user` (`pid_user`),
  KEY `auth_user` (`auth_user`),
  KEY `callid_aleg` (`callid_aleg`),
  KEY `date` (`date`),
  KEY `callid` (`callid`)
) ENGINE=MyISAM AUTO_INCREMENT=1 DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `sip_capture_05_16`
--

DROP TABLE IF EXISTS `sip_capture_05_16`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `sip_capture_05_16` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
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
  `direction` varchar(4) NOT NULL DEFAULT '',
  PRIMARY KEY (`id`),
  KEY `ruri_user` (`ruri_user`),
  KEY `from_user` (`from_user`),
  KEY `to_user` (`to_user`),
  KEY `pid_user` (`pid_user`),
  KEY `auth_user` (`auth_user`),
  KEY `callid_aleg` (`callid_aleg`),
  KEY `date` (`date`),
  KEY `callid` (`callid`)
) ENGINE=MyISAM AUTO_INCREMENT=1 DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `sip_capture_05_17`
--

DROP TABLE IF EXISTS `sip_capture_05_17`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `sip_capture_05_17` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
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
  `direction` varchar(4) NOT NULL DEFAULT '',
  PRIMARY KEY (`id`),
  KEY `ruri_user` (`ruri_user`),
  KEY `from_user` (`from_user`),
  KEY `to_user` (`to_user`),
  KEY `pid_user` (`pid_user`),
  KEY `auth_user` (`auth_user`),
  KEY `callid_aleg` (`callid_aleg`),
  KEY `date` (`date`),
  KEY `callid` (`callid`)
) ENGINE=MyISAM AUTO_INCREMENT=1 DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `sip_capture_05_18`
--

DROP TABLE IF EXISTS `sip_capture_05_18`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `sip_capture_05_18` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
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
  `direction` varchar(4) NOT NULL DEFAULT '',
  PRIMARY KEY (`id`),
  KEY `ruri_user` (`ruri_user`),
  KEY `from_user` (`from_user`),
  KEY `to_user` (`to_user`),
  KEY `pid_user` (`pid_user`),
  KEY `auth_user` (`auth_user`),
  KEY `callid_aleg` (`callid_aleg`),
  KEY `date` (`date`),
  KEY `callid` (`callid`)
) ENGINE=MyISAM AUTO_INCREMENT=1 DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `sip_capture_05_19`
--

DROP TABLE IF EXISTS `sip_capture_05_19`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `sip_capture_05_19` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
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
  `direction` varchar(4) NOT NULL DEFAULT '',
  PRIMARY KEY (`id`),
  KEY `ruri_user` (`ruri_user`),
  KEY `from_user` (`from_user`),
  KEY `to_user` (`to_user`),
  KEY `pid_user` (`pid_user`),
  KEY `auth_user` (`auth_user`),
  KEY `callid_aleg` (`callid_aleg`),
  KEY `date` (`date`),
  KEY `callid` (`callid`)
) ENGINE=MyISAM AUTO_INCREMENT=1 DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `sip_capture_05_20`
--

DROP TABLE IF EXISTS `sip_capture_05_20`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `sip_capture_05_20` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
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
  `direction` varchar(4) NOT NULL DEFAULT '',
  PRIMARY KEY (`id`),
  KEY `ruri_user` (`ruri_user`),
  KEY `from_user` (`from_user`),
  KEY `to_user` (`to_user`),
  KEY `pid_user` (`pid_user`),
  KEY `auth_user` (`auth_user`),
  KEY `callid_aleg` (`callid_aleg`),
  KEY `date` (`date`),
  KEY `callid` (`callid`)
) ENGINE=MyISAM AUTO_INCREMENT=1 DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `sip_capture_05_21`
--

DROP TABLE IF EXISTS `sip_capture_05_21`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `sip_capture_05_21` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
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
  `direction` varchar(4) NOT NULL DEFAULT '',
  PRIMARY KEY (`id`),
  KEY `ruri_user` (`ruri_user`),
  KEY `from_user` (`from_user`),
  KEY `to_user` (`to_user`),
  KEY `pid_user` (`pid_user`),
  KEY `auth_user` (`auth_user`),
  KEY `callid_aleg` (`callid_aleg`),
  KEY `date` (`date`),
  KEY `callid` (`callid`)
) ENGINE=MyISAM AUTO_INCREMENT=1 DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `sip_capture_05_22`
--

DROP TABLE IF EXISTS `sip_capture_05_22`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `sip_capture_05_22` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
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
  `direction` varchar(4) NOT NULL DEFAULT '',
  PRIMARY KEY (`id`),
  KEY `ruri_user` (`ruri_user`),
  KEY `from_user` (`from_user`),
  KEY `to_user` (`to_user`),
  KEY `pid_user` (`pid_user`),
  KEY `auth_user` (`auth_user`),
  KEY `callid_aleg` (`callid_aleg`),
  KEY `date` (`date`),
  KEY `callid` (`callid`)
) ENGINE=MyISAM AUTO_INCREMENT=1 DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `sip_capture_05_23`
--

DROP TABLE IF EXISTS `sip_capture_05_23`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `sip_capture_05_23` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
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
  `direction` varchar(4) NOT NULL DEFAULT '',
  PRIMARY KEY (`id`),
  KEY `ruri_user` (`ruri_user`),
  KEY `from_user` (`from_user`),
  KEY `to_user` (`to_user`),
  KEY `pid_user` (`pid_user`),
  KEY `auth_user` (`auth_user`),
  KEY `callid_aleg` (`callid_aleg`),
  KEY `date` (`date`),
  KEY `callid` (`callid`)
) ENGINE=MyISAM AUTO_INCREMENT=1 DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `sip_capture_06_00`
--

DROP TABLE IF EXISTS `sip_capture_06_00`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `sip_capture_06_00` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
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
  `direction` varchar(4) NOT NULL DEFAULT '',
  PRIMARY KEY (`id`),
  KEY `ruri_user` (`ruri_user`),
  KEY `from_user` (`from_user`),
  KEY `to_user` (`to_user`),
  KEY `pid_user` (`pid_user`),
  KEY `auth_user` (`auth_user`),
  KEY `callid_aleg` (`callid_aleg`),
  KEY `date` (`date`),
  KEY `callid` (`callid`)
) ENGINE=MyISAM AUTO_INCREMENT=1 DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `sip_capture_06_01`
--

DROP TABLE IF EXISTS `sip_capture_06_01`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `sip_capture_06_01` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
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
  `direction` varchar(4) NOT NULL DEFAULT '',
  PRIMARY KEY (`id`),
  KEY `ruri_user` (`ruri_user`),
  KEY `from_user` (`from_user`),
  KEY `to_user` (`to_user`),
  KEY `pid_user` (`pid_user`),
  KEY `auth_user` (`auth_user`),
  KEY `callid_aleg` (`callid_aleg`),
  KEY `date` (`date`),
  KEY `callid` (`callid`)
) ENGINE=MyISAM AUTO_INCREMENT=1 DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `sip_capture_06_02`
--

DROP TABLE IF EXISTS `sip_capture_06_02`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `sip_capture_06_02` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
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
  `direction` varchar(4) NOT NULL DEFAULT '',
  PRIMARY KEY (`id`),
  KEY `ruri_user` (`ruri_user`),
  KEY `from_user` (`from_user`),
  KEY `to_user` (`to_user`),
  KEY `pid_user` (`pid_user`),
  KEY `auth_user` (`auth_user`),
  KEY `callid_aleg` (`callid_aleg`),
  KEY `date` (`date`),
  KEY `callid` (`callid`)
) ENGINE=MyISAM AUTO_INCREMENT=1 DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `sip_capture_06_03`
--

DROP TABLE IF EXISTS `sip_capture_06_03`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `sip_capture_06_03` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
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
  `direction` varchar(4) NOT NULL DEFAULT '',
  PRIMARY KEY (`id`),
  KEY `ruri_user` (`ruri_user`),
  KEY `from_user` (`from_user`),
  KEY `to_user` (`to_user`),
  KEY `pid_user` (`pid_user`),
  KEY `auth_user` (`auth_user`),
  KEY `callid_aleg` (`callid_aleg`),
  KEY `date` (`date`),
  KEY `callid` (`callid`)
) ENGINE=MyISAM AUTO_INCREMENT=1 DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `sip_capture_06_04`
--

DROP TABLE IF EXISTS `sip_capture_06_04`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `sip_capture_06_04` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
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
  `direction` varchar(4) NOT NULL DEFAULT '',
  PRIMARY KEY (`id`),
  KEY `ruri_user` (`ruri_user`),
  KEY `from_user` (`from_user`),
  KEY `to_user` (`to_user`),
  KEY `pid_user` (`pid_user`),
  KEY `auth_user` (`auth_user`),
  KEY `callid_aleg` (`callid_aleg`),
  KEY `date` (`date`),
  KEY `callid` (`callid`)
) ENGINE=MyISAM AUTO_INCREMENT=1 DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `sip_capture_06_05`
--

DROP TABLE IF EXISTS `sip_capture_06_05`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `sip_capture_06_05` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
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
  `direction` varchar(4) NOT NULL DEFAULT '',
  PRIMARY KEY (`id`),
  KEY `ruri_user` (`ruri_user`),
  KEY `from_user` (`from_user`),
  KEY `to_user` (`to_user`),
  KEY `pid_user` (`pid_user`),
  KEY `auth_user` (`auth_user`),
  KEY `callid_aleg` (`callid_aleg`),
  KEY `date` (`date`),
  KEY `callid` (`callid`)
) ENGINE=MyISAM AUTO_INCREMENT=1 DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `sip_capture_06_06`
--

DROP TABLE IF EXISTS `sip_capture_06_06`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `sip_capture_06_06` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
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
  `direction` varchar(4) NOT NULL DEFAULT '',
  PRIMARY KEY (`id`),
  KEY `ruri_user` (`ruri_user`),
  KEY `from_user` (`from_user`),
  KEY `to_user` (`to_user`),
  KEY `pid_user` (`pid_user`),
  KEY `auth_user` (`auth_user`),
  KEY `callid_aleg` (`callid_aleg`),
  KEY `date` (`date`),
  KEY `callid` (`callid`)
) ENGINE=MyISAM AUTO_INCREMENT=1 DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `sip_capture_06_07`
--

DROP TABLE IF EXISTS `sip_capture_06_07`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `sip_capture_06_07` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
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
  `direction` varchar(4) NOT NULL DEFAULT '',
  PRIMARY KEY (`id`),
  KEY `ruri_user` (`ruri_user`),
  KEY `from_user` (`from_user`),
  KEY `to_user` (`to_user`),
  KEY `pid_user` (`pid_user`),
  KEY `auth_user` (`auth_user`),
  KEY `callid_aleg` (`callid_aleg`),
  KEY `date` (`date`),
  KEY `callid` (`callid`)
) ENGINE=MyISAM AUTO_INCREMENT=1 DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `sip_capture_06_08`
--

DROP TABLE IF EXISTS `sip_capture_06_08`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `sip_capture_06_08` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
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
  `direction` varchar(4) NOT NULL DEFAULT '',
  PRIMARY KEY (`id`),
  KEY `ruri_user` (`ruri_user`),
  KEY `from_user` (`from_user`),
  KEY `to_user` (`to_user`),
  KEY `pid_user` (`pid_user`),
  KEY `auth_user` (`auth_user`),
  KEY `callid_aleg` (`callid_aleg`),
  KEY `date` (`date`),
  KEY `callid` (`callid`)
) ENGINE=MyISAM AUTO_INCREMENT=1 DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `sip_capture_06_09`
--

DROP TABLE IF EXISTS `sip_capture_06_09`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `sip_capture_06_09` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
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
  `direction` varchar(4) NOT NULL DEFAULT '',
  PRIMARY KEY (`id`),
  KEY `ruri_user` (`ruri_user`),
  KEY `from_user` (`from_user`),
  KEY `to_user` (`to_user`),
  KEY `pid_user` (`pid_user`),
  KEY `auth_user` (`auth_user`),
  KEY `callid_aleg` (`callid_aleg`),
  KEY `date` (`date`),
  KEY `callid` (`callid`)
) ENGINE=MyISAM AUTO_INCREMENT=1 DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `sip_capture_06_10`
--

DROP TABLE IF EXISTS `sip_capture_06_10`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `sip_capture_06_10` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
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
  `direction` varchar(4) NOT NULL DEFAULT '',
  PRIMARY KEY (`id`),
  KEY `ruri_user` (`ruri_user`),
  KEY `from_user` (`from_user`),
  KEY `to_user` (`to_user`),
  KEY `pid_user` (`pid_user`),
  KEY `auth_user` (`auth_user`),
  KEY `callid_aleg` (`callid_aleg`),
  KEY `date` (`date`),
  KEY `callid` (`callid`)
) ENGINE=MyISAM AUTO_INCREMENT=1 DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `sip_capture_06_11`
--

DROP TABLE IF EXISTS `sip_capture_06_11`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `sip_capture_06_11` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
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
  `direction` varchar(4) NOT NULL DEFAULT '',
  PRIMARY KEY (`id`),
  KEY `ruri_user` (`ruri_user`),
  KEY `from_user` (`from_user`),
  KEY `to_user` (`to_user`),
  KEY `pid_user` (`pid_user`),
  KEY `auth_user` (`auth_user`),
  KEY `callid_aleg` (`callid_aleg`),
  KEY `date` (`date`),
  KEY `callid` (`callid`)
) ENGINE=MyISAM AUTO_INCREMENT=1 DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `sip_capture_06_12`
--

DROP TABLE IF EXISTS `sip_capture_06_12`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `sip_capture_06_12` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
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
  `direction` varchar(4) NOT NULL DEFAULT '',
  PRIMARY KEY (`id`),
  KEY `ruri_user` (`ruri_user`),
  KEY `from_user` (`from_user`),
  KEY `to_user` (`to_user`),
  KEY `pid_user` (`pid_user`),
  KEY `auth_user` (`auth_user`),
  KEY `callid_aleg` (`callid_aleg`),
  KEY `date` (`date`),
  KEY `callid` (`callid`)
) ENGINE=MyISAM AUTO_INCREMENT=1 DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `sip_capture_06_13`
--

DROP TABLE IF EXISTS `sip_capture_06_13`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `sip_capture_06_13` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
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
  `direction` varchar(4) NOT NULL DEFAULT '',
  PRIMARY KEY (`id`),
  KEY `ruri_user` (`ruri_user`),
  KEY `from_user` (`from_user`),
  KEY `to_user` (`to_user`),
  KEY `pid_user` (`pid_user`),
  KEY `auth_user` (`auth_user`),
  KEY `callid_aleg` (`callid_aleg`),
  KEY `date` (`date`),
  KEY `callid` (`callid`)
) ENGINE=MyISAM AUTO_INCREMENT=1 DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `sip_capture_06_14`
--

DROP TABLE IF EXISTS `sip_capture_06_14`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `sip_capture_06_14` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
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
  `direction` varchar(4) NOT NULL DEFAULT '',
  PRIMARY KEY (`id`),
  KEY `ruri_user` (`ruri_user`),
  KEY `from_user` (`from_user`),
  KEY `to_user` (`to_user`),
  KEY `pid_user` (`pid_user`),
  KEY `auth_user` (`auth_user`),
  KEY `callid_aleg` (`callid_aleg`),
  KEY `date` (`date`),
  KEY `callid` (`callid`)
) ENGINE=MyISAM AUTO_INCREMENT=1 DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `sip_capture_06_15`
--

DROP TABLE IF EXISTS `sip_capture_06_15`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `sip_capture_06_15` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
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
  `direction` varchar(4) NOT NULL DEFAULT '',
  PRIMARY KEY (`id`),
  KEY `ruri_user` (`ruri_user`),
  KEY `from_user` (`from_user`),
  KEY `to_user` (`to_user`),
  KEY `pid_user` (`pid_user`),
  KEY `auth_user` (`auth_user`),
  KEY `callid_aleg` (`callid_aleg`),
  KEY `date` (`date`),
  KEY `callid` (`callid`)
) ENGINE=MyISAM AUTO_INCREMENT=1 DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `sip_capture_06_16`
--

DROP TABLE IF EXISTS `sip_capture_06_16`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `sip_capture_06_16` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
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
  `direction` varchar(4) NOT NULL DEFAULT '',
  PRIMARY KEY (`id`),
  KEY `ruri_user` (`ruri_user`),
  KEY `from_user` (`from_user`),
  KEY `to_user` (`to_user`),
  KEY `pid_user` (`pid_user`),
  KEY `auth_user` (`auth_user`),
  KEY `callid_aleg` (`callid_aleg`),
  KEY `date` (`date`),
  KEY `callid` (`callid`)
) ENGINE=MyISAM AUTO_INCREMENT=1 DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `sip_capture_06_17`
--

DROP TABLE IF EXISTS `sip_capture_06_17`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `sip_capture_06_17` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
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
  `direction` varchar(4) NOT NULL DEFAULT '',
  PRIMARY KEY (`id`),
  KEY `ruri_user` (`ruri_user`),
  KEY `from_user` (`from_user`),
  KEY `to_user` (`to_user`),
  KEY `pid_user` (`pid_user`),
  KEY `auth_user` (`auth_user`),
  KEY `callid_aleg` (`callid_aleg`),
  KEY `date` (`date`),
  KEY `callid` (`callid`)
) ENGINE=MyISAM AUTO_INCREMENT=1 DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `sip_capture_06_18`
--

DROP TABLE IF EXISTS `sip_capture_06_18`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `sip_capture_06_18` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
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
  `direction` varchar(4) NOT NULL DEFAULT '',
  PRIMARY KEY (`id`),
  KEY `ruri_user` (`ruri_user`),
  KEY `from_user` (`from_user`),
  KEY `to_user` (`to_user`),
  KEY `pid_user` (`pid_user`),
  KEY `auth_user` (`auth_user`),
  KEY `callid_aleg` (`callid_aleg`),
  KEY `date` (`date`),
  KEY `callid` (`callid`)
) ENGINE=MyISAM AUTO_INCREMENT=1 DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `sip_capture_06_19`
--

DROP TABLE IF EXISTS `sip_capture_06_19`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `sip_capture_06_19` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
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
  `direction` varchar(4) NOT NULL DEFAULT '',
  PRIMARY KEY (`id`),
  KEY `ruri_user` (`ruri_user`),
  KEY `from_user` (`from_user`),
  KEY `to_user` (`to_user`),
  KEY `pid_user` (`pid_user`),
  KEY `auth_user` (`auth_user`),
  KEY `callid_aleg` (`callid_aleg`),
  KEY `date` (`date`),
  KEY `callid` (`callid`)
) ENGINE=MyISAM AUTO_INCREMENT=1 DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `sip_capture_06_20`
--

DROP TABLE IF EXISTS `sip_capture_06_20`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `sip_capture_06_20` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
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
  `direction` varchar(4) NOT NULL DEFAULT '',
  PRIMARY KEY (`id`),
  KEY `ruri_user` (`ruri_user`),
  KEY `from_user` (`from_user`),
  KEY `to_user` (`to_user`),
  KEY `pid_user` (`pid_user`),
  KEY `auth_user` (`auth_user`),
  KEY `callid_aleg` (`callid_aleg`),
  KEY `date` (`date`),
  KEY `callid` (`callid`)
) ENGINE=MyISAM AUTO_INCREMENT=1 DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `sip_capture_06_21`
--

DROP TABLE IF EXISTS `sip_capture_06_21`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `sip_capture_06_21` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
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
  `direction` varchar(4) NOT NULL DEFAULT '',
  PRIMARY KEY (`id`),
  KEY `ruri_user` (`ruri_user`),
  KEY `from_user` (`from_user`),
  KEY `to_user` (`to_user`),
  KEY `pid_user` (`pid_user`),
  KEY `auth_user` (`auth_user`),
  KEY `callid_aleg` (`callid_aleg`),
  KEY `date` (`date`),
  KEY `callid` (`callid`)
) ENGINE=MyISAM AUTO_INCREMENT=1 DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `sip_capture_06_22`
--

DROP TABLE IF EXISTS `sip_capture_06_22`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `sip_capture_06_22` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
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
  `direction` varchar(4) NOT NULL DEFAULT '',
  PRIMARY KEY (`id`),
  KEY `ruri_user` (`ruri_user`),
  KEY `from_user` (`from_user`),
  KEY `to_user` (`to_user`),
  KEY `pid_user` (`pid_user`),
  KEY `auth_user` (`auth_user`),
  KEY `callid_aleg` (`callid_aleg`),
  KEY `date` (`date`),
  KEY `callid` (`callid`)
) ENGINE=MyISAM AUTO_INCREMENT=1 DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `sip_capture_06_23`
--

DROP TABLE IF EXISTS `sip_capture_06_23`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `sip_capture_06_23` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
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
  `direction` varchar(4) NOT NULL DEFAULT '',
  PRIMARY KEY (`id`),
  KEY `ruri_user` (`ruri_user`),
  KEY `from_user` (`from_user`),
  KEY `to_user` (`to_user`),
  KEY `pid_user` (`pid_user`),
  KEY `auth_user` (`auth_user`),
  KEY `callid_aleg` (`callid_aleg`),
  KEY `date` (`date`),
  KEY `callid` (`callid`)
) ENGINE=MyISAM AUTO_INCREMENT=1 DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `sip_capture_07_00`
--

DROP TABLE IF EXISTS `sip_capture_07_00`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `sip_capture_07_00` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
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
  `direction` varchar(4) NOT NULL DEFAULT '',
  PRIMARY KEY (`id`),
  KEY `ruri_user` (`ruri_user`),
  KEY `from_user` (`from_user`),
  KEY `to_user` (`to_user`),
  KEY `pid_user` (`pid_user`),
  KEY `auth_user` (`auth_user`),
  KEY `callid_aleg` (`callid_aleg`),
  KEY `date` (`date`),
  KEY `callid` (`callid`)
) ENGINE=MyISAM AUTO_INCREMENT=1 DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `sip_capture_07_01`
--

DROP TABLE IF EXISTS `sip_capture_07_01`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `sip_capture_07_01` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
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
  `direction` varchar(4) NOT NULL DEFAULT '',
  PRIMARY KEY (`id`),
  KEY `ruri_user` (`ruri_user`),
  KEY `from_user` (`from_user`),
  KEY `to_user` (`to_user`),
  KEY `pid_user` (`pid_user`),
  KEY `auth_user` (`auth_user`),
  KEY `callid_aleg` (`callid_aleg`),
  KEY `date` (`date`),
  KEY `callid` (`callid`)
) ENGINE=MyISAM AUTO_INCREMENT=1 DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `sip_capture_07_02`
--

DROP TABLE IF EXISTS `sip_capture_07_02`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `sip_capture_07_02` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
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
  `direction` varchar(4) NOT NULL DEFAULT '',
  PRIMARY KEY (`id`),
  KEY `ruri_user` (`ruri_user`),
  KEY `from_user` (`from_user`),
  KEY `to_user` (`to_user`),
  KEY `pid_user` (`pid_user`),
  KEY `auth_user` (`auth_user`),
  KEY `callid_aleg` (`callid_aleg`),
  KEY `date` (`date`),
  KEY `callid` (`callid`)
) ENGINE=MyISAM AUTO_INCREMENT=1 DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `sip_capture_07_03`
--

DROP TABLE IF EXISTS `sip_capture_07_03`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `sip_capture_07_03` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
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
  `direction` varchar(4) NOT NULL DEFAULT '',
  PRIMARY KEY (`id`),
  KEY `ruri_user` (`ruri_user`),
  KEY `from_user` (`from_user`),
  KEY `to_user` (`to_user`),
  KEY `pid_user` (`pid_user`),
  KEY `auth_user` (`auth_user`),
  KEY `callid_aleg` (`callid_aleg`),
  KEY `date` (`date`),
  KEY `callid` (`callid`)
) ENGINE=MyISAM AUTO_INCREMENT=1 DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `sip_capture_07_04`
--

DROP TABLE IF EXISTS `sip_capture_07_04`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `sip_capture_07_04` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
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
  `direction` varchar(4) NOT NULL DEFAULT '',
  PRIMARY KEY (`id`),
  KEY `ruri_user` (`ruri_user`),
  KEY `from_user` (`from_user`),
  KEY `to_user` (`to_user`),
  KEY `pid_user` (`pid_user`),
  KEY `auth_user` (`auth_user`),
  KEY `callid_aleg` (`callid_aleg`),
  KEY `date` (`date`),
  KEY `callid` (`callid`)
) ENGINE=MyISAM AUTO_INCREMENT=1 DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `sip_capture_07_05`
--

DROP TABLE IF EXISTS `sip_capture_07_05`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `sip_capture_07_05` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
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
  `direction` varchar(4) NOT NULL DEFAULT '',
  PRIMARY KEY (`id`),
  KEY `ruri_user` (`ruri_user`),
  KEY `from_user` (`from_user`),
  KEY `to_user` (`to_user`),
  KEY `pid_user` (`pid_user`),
  KEY `auth_user` (`auth_user`),
  KEY `callid_aleg` (`callid_aleg`),
  KEY `date` (`date`),
  KEY `callid` (`callid`)
) ENGINE=MyISAM AUTO_INCREMENT=1 DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `sip_capture_07_06`
--

DROP TABLE IF EXISTS `sip_capture_07_06`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `sip_capture_07_06` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
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
  `direction` varchar(4) NOT NULL DEFAULT '',
  PRIMARY KEY (`id`),
  KEY `ruri_user` (`ruri_user`),
  KEY `from_user` (`from_user`),
  KEY `to_user` (`to_user`),
  KEY `pid_user` (`pid_user`),
  KEY `auth_user` (`auth_user`),
  KEY `callid_aleg` (`callid_aleg`),
  KEY `date` (`date`),
  KEY `callid` (`callid`)
) ENGINE=MyISAM AUTO_INCREMENT=1 DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `sip_capture_07_07`
--

DROP TABLE IF EXISTS `sip_capture_07_07`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `sip_capture_07_07` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
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
  `direction` varchar(4) NOT NULL DEFAULT '',
  PRIMARY KEY (`id`),
  KEY `ruri_user` (`ruri_user`),
  KEY `from_user` (`from_user`),
  KEY `to_user` (`to_user`),
  KEY `pid_user` (`pid_user`),
  KEY `auth_user` (`auth_user`),
  KEY `callid_aleg` (`callid_aleg`),
  KEY `date` (`date`),
  KEY `callid` (`callid`)
) ENGINE=MyISAM AUTO_INCREMENT=1 DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `sip_capture_07_08`
--

DROP TABLE IF EXISTS `sip_capture_07_08`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `sip_capture_07_08` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
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
  `direction` varchar(4) NOT NULL DEFAULT '',
  PRIMARY KEY (`id`),
  KEY `ruri_user` (`ruri_user`),
  KEY `from_user` (`from_user`),
  KEY `to_user` (`to_user`),
  KEY `pid_user` (`pid_user`),
  KEY `auth_user` (`auth_user`),
  KEY `callid_aleg` (`callid_aleg`),
  KEY `date` (`date`),
  KEY `callid` (`callid`)
) ENGINE=MyISAM AUTO_INCREMENT=1 DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `sip_capture_07_09`
--

DROP TABLE IF EXISTS `sip_capture_07_09`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `sip_capture_07_09` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
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
  `direction` varchar(4) NOT NULL DEFAULT '',
  PRIMARY KEY (`id`),
  KEY `ruri_user` (`ruri_user`),
  KEY `from_user` (`from_user`),
  KEY `to_user` (`to_user`),
  KEY `pid_user` (`pid_user`),
  KEY `auth_user` (`auth_user`),
  KEY `callid_aleg` (`callid_aleg`),
  KEY `date` (`date`),
  KEY `callid` (`callid`)
) ENGINE=MyISAM AUTO_INCREMENT=1 DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `sip_capture_07_10`
--

DROP TABLE IF EXISTS `sip_capture_07_10`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `sip_capture_07_10` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
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
  `direction` varchar(4) NOT NULL DEFAULT '',
  PRIMARY KEY (`id`),
  KEY `ruri_user` (`ruri_user`),
  KEY `from_user` (`from_user`),
  KEY `to_user` (`to_user`),
  KEY `pid_user` (`pid_user`),
  KEY `auth_user` (`auth_user`),
  KEY `callid_aleg` (`callid_aleg`),
  KEY `date` (`date`),
  KEY `callid` (`callid`)
) ENGINE=MyISAM AUTO_INCREMENT=1 DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `sip_capture_07_11`
--

DROP TABLE IF EXISTS `sip_capture_07_11`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `sip_capture_07_11` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
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
  `direction` varchar(4) NOT NULL DEFAULT '',
  PRIMARY KEY (`id`),
  KEY `ruri_user` (`ruri_user`),
  KEY `from_user` (`from_user`),
  KEY `to_user` (`to_user`),
  KEY `pid_user` (`pid_user`),
  KEY `auth_user` (`auth_user`),
  KEY `callid_aleg` (`callid_aleg`),
  KEY `date` (`date`),
  KEY `callid` (`callid`)
) ENGINE=MyISAM AUTO_INCREMENT=1 DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `sip_capture_07_12`
--

DROP TABLE IF EXISTS `sip_capture_07_12`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `sip_capture_07_12` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
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
  `direction` varchar(4) NOT NULL DEFAULT '',
  PRIMARY KEY (`id`),
  KEY `ruri_user` (`ruri_user`),
  KEY `from_user` (`from_user`),
  KEY `to_user` (`to_user`),
  KEY `pid_user` (`pid_user`),
  KEY `auth_user` (`auth_user`),
  KEY `callid_aleg` (`callid_aleg`),
  KEY `date` (`date`),
  KEY `callid` (`callid`)
) ENGINE=MyISAM AUTO_INCREMENT=1 DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `sip_capture_07_13`
--

DROP TABLE IF EXISTS `sip_capture_07_13`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `sip_capture_07_13` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
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
  `direction` varchar(4) NOT NULL DEFAULT '',
  PRIMARY KEY (`id`),
  KEY `ruri_user` (`ruri_user`),
  KEY `from_user` (`from_user`),
  KEY `to_user` (`to_user`),
  KEY `pid_user` (`pid_user`),
  KEY `auth_user` (`auth_user`),
  KEY `callid_aleg` (`callid_aleg`),
  KEY `date` (`date`),
  KEY `callid` (`callid`)
) ENGINE=MyISAM AUTO_INCREMENT=1 DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `sip_capture_07_14`
--

DROP TABLE IF EXISTS `sip_capture_07_14`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `sip_capture_07_14` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
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
  `direction` varchar(4) NOT NULL DEFAULT '',
  PRIMARY KEY (`id`),
  KEY `ruri_user` (`ruri_user`),
  KEY `from_user` (`from_user`),
  KEY `to_user` (`to_user`),
  KEY `pid_user` (`pid_user`),
  KEY `auth_user` (`auth_user`),
  KEY `callid_aleg` (`callid_aleg`),
  KEY `date` (`date`),
  KEY `callid` (`callid`)
) ENGINE=MyISAM AUTO_INCREMENT=1 DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `sip_capture_07_15`
--

DROP TABLE IF EXISTS `sip_capture_07_15`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `sip_capture_07_15` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
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
  `direction` varchar(4) NOT NULL DEFAULT '',
  PRIMARY KEY (`id`),
  KEY `ruri_user` (`ruri_user`),
  KEY `from_user` (`from_user`),
  KEY `to_user` (`to_user`),
  KEY `pid_user` (`pid_user`),
  KEY `auth_user` (`auth_user`),
  KEY `callid_aleg` (`callid_aleg`),
  KEY `date` (`date`),
  KEY `callid` (`callid`)
) ENGINE=MyISAM AUTO_INCREMENT=1 DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `sip_capture_07_16`
--

DROP TABLE IF EXISTS `sip_capture_07_16`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `sip_capture_07_16` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
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
  `direction` varchar(4) NOT NULL DEFAULT '',
  PRIMARY KEY (`id`),
  KEY `ruri_user` (`ruri_user`),
  KEY `from_user` (`from_user`),
  KEY `to_user` (`to_user`),
  KEY `pid_user` (`pid_user`),
  KEY `auth_user` (`auth_user`),
  KEY `callid_aleg` (`callid_aleg`),
  KEY `date` (`date`),
  KEY `callid` (`callid`)
) ENGINE=MyISAM AUTO_INCREMENT=1 DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `sip_capture_07_17`
--

DROP TABLE IF EXISTS `sip_capture_07_17`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `sip_capture_07_17` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
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
  `direction` varchar(4) NOT NULL DEFAULT '',
  PRIMARY KEY (`id`),
  KEY `ruri_user` (`ruri_user`),
  KEY `from_user` (`from_user`),
  KEY `to_user` (`to_user`),
  KEY `pid_user` (`pid_user`),
  KEY `auth_user` (`auth_user`),
  KEY `callid_aleg` (`callid_aleg`),
  KEY `date` (`date`),
  KEY `callid` (`callid`)
) ENGINE=MyISAM AUTO_INCREMENT=1 DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `sip_capture_07_18`
--

DROP TABLE IF EXISTS `sip_capture_07_18`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `sip_capture_07_18` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
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
  `direction` varchar(4) NOT NULL DEFAULT '',
  PRIMARY KEY (`id`),
  KEY `ruri_user` (`ruri_user`),
  KEY `from_user` (`from_user`),
  KEY `to_user` (`to_user`),
  KEY `pid_user` (`pid_user`),
  KEY `auth_user` (`auth_user`),
  KEY `callid_aleg` (`callid_aleg`),
  KEY `date` (`date`),
  KEY `callid` (`callid`)
) ENGINE=MyISAM AUTO_INCREMENT=1 DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `sip_capture_07_19`
--

DROP TABLE IF EXISTS `sip_capture_07_19`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `sip_capture_07_19` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
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
  `direction` varchar(4) NOT NULL DEFAULT '',
  PRIMARY KEY (`id`),
  KEY `ruri_user` (`ruri_user`),
  KEY `from_user` (`from_user`),
  KEY `to_user` (`to_user`),
  KEY `pid_user` (`pid_user`),
  KEY `auth_user` (`auth_user`),
  KEY `callid_aleg` (`callid_aleg`),
  KEY `date` (`date`),
  KEY `callid` (`callid`)
) ENGINE=MyISAM AUTO_INCREMENT=1 DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `sip_capture_07_20`
--

DROP TABLE IF EXISTS `sip_capture_07_20`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `sip_capture_07_20` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
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
  `direction` varchar(4) NOT NULL DEFAULT '',
  PRIMARY KEY (`id`),
  KEY `ruri_user` (`ruri_user`),
  KEY `from_user` (`from_user`),
  KEY `to_user` (`to_user`),
  KEY `pid_user` (`pid_user`),
  KEY `auth_user` (`auth_user`),
  KEY `callid_aleg` (`callid_aleg`),
  KEY `date` (`date`),
  KEY `callid` (`callid`)
) ENGINE=MyISAM AUTO_INCREMENT=1 DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `sip_capture_07_21`
--

DROP TABLE IF EXISTS `sip_capture_07_21`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `sip_capture_07_21` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
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
  `direction` varchar(4) NOT NULL DEFAULT '',
  PRIMARY KEY (`id`),
  KEY `ruri_user` (`ruri_user`),
  KEY `from_user` (`from_user`),
  KEY `to_user` (`to_user`),
  KEY `pid_user` (`pid_user`),
  KEY `auth_user` (`auth_user`),
  KEY `callid_aleg` (`callid_aleg`),
  KEY `date` (`date`),
  KEY `callid` (`callid`)
) ENGINE=MyISAM AUTO_INCREMENT=1 DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `sip_capture_07_22`
--

DROP TABLE IF EXISTS `sip_capture_07_22`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `sip_capture_07_22` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
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
  `direction` varchar(4) NOT NULL DEFAULT '',
  PRIMARY KEY (`id`),
  KEY `ruri_user` (`ruri_user`),
  KEY `from_user` (`from_user`),
  KEY `to_user` (`to_user`),
  KEY `pid_user` (`pid_user`),
  KEY `auth_user` (`auth_user`),
  KEY `callid_aleg` (`callid_aleg`),
  KEY `date` (`date`),
  KEY `callid` (`callid`)
) ENGINE=MyISAM AUTO_INCREMENT=1 DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `sip_capture_07_23`
--

DROP TABLE IF EXISTS `sip_capture_07_23`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `sip_capture_07_23` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
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
  `direction` varchar(4) NOT NULL DEFAULT '',
  PRIMARY KEY (`id`),
  KEY `ruri_user` (`ruri_user`),
  KEY `from_user` (`from_user`),
  KEY `to_user` (`to_user`),
  KEY `pid_user` (`pid_user`),
  KEY `auth_user` (`auth_user`),
  KEY `callid_aleg` (`callid_aleg`),
  KEY `date` (`date`),
  KEY `callid` (`callid`)
) ENGINE=MyISAM AUTO_INCREMENT=1 DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;
/*!40103 SET TIME_ZONE=@OLD_TIME_ZONE */;

-- Dump completed on 2011-08-18 13:55:33
