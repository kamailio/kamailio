-- MySQL dump 10.13  Distrib 5.5.43, for debian-linux-gnu (x86_64)
--
-- Host: localhost    Database: scscf_db
-- ------------------------------------------------------
-- Server version	5.5.43-0ubuntu0.14.04.1

/*!40101 SET @OLD_CHARACTER_SET_CLIENT=@@CHARACTER_SET_CLIENT */;
/*!40101 SET @OLD_CHARACTER_SET_RESULTS=@@CHARACTER_SET_RESULTS */;
/*!40101 SET @OLD_COLLATION_CONNECTION=@@COLLATION_CONNECTION */;
/*!40101 SET NAMES utf8 */;
/*!40103 SET @OLD_TIME_ZONE=@@TIME_ZONE */;
/*!40103 SET TIME_ZONE='+00:00' */;
/*!40014 SET @OLD_UNIQUE_CHECKS=@@UNIQUE_CHECKS, UNIQUE_CHECKS=0 */;
/*!40014 SET @OLD_FOREIGN_KEY_CHECKS=@@FOREIGN_KEY_CHECKS, FOREIGN_KEY_CHECKS=0 */;
/*!40101 SET @OLD_SQL_MODE=@@SQL_MODE, SQL_MODE='NO_AUTO_VALUE_ON_ZERO' */;
/*!40111 SET @OLD_SQL_NOTES=@@SQL_NOTES, SQL_NOTES=0 */;

--
-- Current Database: `scscf_db`
--

/*!40000 DROP DATABASE IF EXISTS `scscf_db`*/;

CREATE DATABASE /*!32312 IF NOT EXISTS*/ `scscf_db` /*!40100 DEFAULT CHARACTER SET latin1 */;

USE `scscf_db`;

--
-- Table structure for table `active_watchers`
--

DROP TABLE IF EXISTS `active_watchers`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `active_watchers` (
  `id` int(10) unsigned NOT NULL AUTO_INCREMENT,
  `presentity_uri` varchar(128) NOT NULL,
  `watcher_username` varchar(64) NOT NULL,
  `watcher_domain` varchar(64) NOT NULL,
  `to_user` varchar(64) NOT NULL,
  `to_domain` varchar(64) NOT NULL,
  `event` varchar(64) NOT NULL DEFAULT 'presence',
  `event_id` varchar(64) DEFAULT NULL,
  `to_tag` varchar(64) NOT NULL,
  `from_tag` varchar(64) NOT NULL,
  `callid` varchar(255) NOT NULL,
  `local_cseq` int(11) NOT NULL,
  `remote_cseq` int(11) NOT NULL,
  `contact` varchar(128) NOT NULL,
  `record_route` text,
  `expires` int(11) NOT NULL,
  `status` int(11) NOT NULL DEFAULT '2',
  `reason` varchar(64) NOT NULL,
  `version` int(11) NOT NULL DEFAULT '0',
  `socket_info` varchar(64) NOT NULL,
  `local_contact` varchar(128) NOT NULL,
  `from_user` varchar(64) NOT NULL,
  `from_domain` varchar(64) NOT NULL,
  `updated` int(11) NOT NULL,
  `updated_winfo` int(11) NOT NULL,
  PRIMARY KEY (`id`),
  UNIQUE KEY `active_watchers_idx` (`callid`,`to_tag`,`from_tag`),
  KEY `active_watchers_expires` (`expires`),
  KEY `active_watchers_pres` (`presentity_uri`,`event`),
  KEY `updated_idx` (`updated`),
  KEY `updated_winfo_idx` (`updated_winfo`,`presentity_uri`)
) ENGINE=InnoDB DEFAULT CHARSET=latin1;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `contact`
--

DROP TABLE IF EXISTS `contact`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `contact` (
  `id` int(11) unsigned NOT NULL AUTO_INCREMENT,
  `contact` char(255) NOT NULL,
  `params` varchar(255) DEFAULT NULL,
  `path` varchar(255) DEFAULT NULL,
  `received` varchar(255) DEFAULT NULL,
  `user_agent` varchar(255) DEFAULT NULL,
  `expires` datetime DEFAULT NULL,
  `callid` varchar(255) DEFAULT NULL,
  PRIMARY KEY (`id`),
  UNIQUE KEY `contact` (`contact`)
) ENGINE=InnoDB DEFAULT CHARSET=latin1;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `impu`
--

DROP TABLE IF EXISTS `impu`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `impu` (
  `id` int(11) unsigned NOT NULL AUTO_INCREMENT,
  `impu` char(64) NOT NULL,
  `barring` int(1) DEFAULT '0',
  `reg_state` int(11) DEFAULT '0',
  `ccf1` char(64) DEFAULT NULL,
  `ccf2` char(64) DEFAULT NULL,
  `ecf1` char(64) DEFAULT NULL,
  `ecf2` char(64) DEFAULT NULL,
  `ims_subscription_data` blob,
  PRIMARY KEY (`id`),
  UNIQUE KEY `impu` (`impu`)
) ENGINE=InnoDB DEFAULT CHARSET=latin1;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `impu_contact`
--

DROP TABLE IF EXISTS `impu_contact`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `impu_contact` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `impu_id` int(11) NOT NULL,
  `contact_id` int(11) NOT NULL,
  PRIMARY KEY (`id`),
  UNIQUE KEY `impu_id` (`impu_id`,`contact_id`)
) ENGINE=InnoDB DEFAULT CHARSET=latin1;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `impu_subscriber`
--

DROP TABLE IF EXISTS `impu_subscriber`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `impu_subscriber` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `impu_id` int(11) NOT NULL,
  `subscriber_id` int(11) NOT NULL,
  PRIMARY KEY (`id`),
  UNIQUE KEY `impu_id` (`impu_id`,`subscriber_id`)
) ENGINE=InnoDB DEFAULT CHARSET=latin1;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `presentity`
--

DROP TABLE IF EXISTS `presentity`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `presentity` (
  `id` int(10) unsigned NOT NULL AUTO_INCREMENT,
  `username` varchar(64) NOT NULL,
  `domain` varchar(64) NOT NULL,
  `event` varchar(64) NOT NULL,
  `etag` varchar(64) NOT NULL,
  `expires` int(11) NOT NULL,
  `received_time` int(11) NOT NULL,
  `body` blob NOT NULL,
  `sender` varchar(128) NOT NULL,
  `priority` int(11) NOT NULL DEFAULT '0',
  PRIMARY KEY (`id`),
  UNIQUE KEY `presentity_idx` (`username`,`domain`,`event`,`etag`),
  KEY `presentity_expires` (`expires`),
  KEY `account_idx` (`username`,`domain`,`event`)
) ENGINE=InnoDB DEFAULT CHARSET=latin1;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `pua`
--

DROP TABLE IF EXISTS `pua`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `pua` (
  `id` int(10) unsigned NOT NULL AUTO_INCREMENT,
  `pres_uri` varchar(128) NOT NULL,
  `pres_id` varchar(255) NOT NULL,
  `event` int(11) NOT NULL,
  `expires` int(11) NOT NULL,
  `desired_expires` int(11) NOT NULL,
  `flag` int(11) NOT NULL,
  `etag` varchar(64) NOT NULL,
  `tuple_id` varchar(64) DEFAULT NULL,
  `watcher_uri` varchar(128) NOT NULL,
  `call_id` varchar(255) NOT NULL,
  `to_tag` varchar(64) NOT NULL,
  `from_tag` varchar(64) NOT NULL,
  `cseq` int(11) NOT NULL,
  `record_route` text,
  `contact` varchar(128) NOT NULL,
  `remote_contact` varchar(128) NOT NULL,
  `version` int(11) NOT NULL,
  `extra_headers` text NOT NULL,
  PRIMARY KEY (`id`),
  UNIQUE KEY `pua_idx` (`etag`,`tuple_id`,`call_id`,`from_tag`),
  KEY `expires_idx` (`expires`),
  KEY `dialog1_idx` (`pres_id`,`pres_uri`),
  KEY `dialog2_idx` (`call_id`,`from_tag`),
  KEY `record_idx` (`pres_id`)
) ENGINE=InnoDB DEFAULT CHARSET=latin1;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `subscriber`
--

DROP TABLE IF EXISTS `subscriber`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `subscriber` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `watcher_uri` varchar(50) NOT NULL,
  `watcher_contact` varchar(50) NOT NULL,
  `presentity_uri` varchar(50) NOT NULL,
  `event` int(11) NOT NULL,
  `expires` datetime NOT NULL,
  `version` int(11) NOT NULL,
  `local_cseq` int(11) NOT NULL,
  `call_id` varchar(50) NOT NULL,
  `from_tag` varchar(50) NOT NULL,
  `to_tag` varchar(50) NOT NULL,
  `record_route` varchar(50) NOT NULL,
  `sockinfo_str` varchar(50) NOT NULL,
  PRIMARY KEY (`id`),
  UNIQUE KEY `watcher_uri` (`event`,`watcher_contact`,`presentity_uri`)
) ENGINE=InnoDB DEFAULT CHARSET=latin1;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `version`
--

DROP TABLE IF EXISTS `version`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `version` (
  `table_name` varchar(32) NOT NULL,
  `table_version` int(10) unsigned NOT NULL DEFAULT '0',
  UNIQUE KEY `table_name_idx` (`table_name`)
) ENGINE=InnoDB DEFAULT CHARSET=latin1;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `watchers`
--

DROP TABLE IF EXISTS `watchers`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `watchers` (
  `id` int(10) unsigned NOT NULL AUTO_INCREMENT,
  `presentity_uri` varchar(128) NOT NULL,
  `watcher_username` varchar(64) NOT NULL,
  `watcher_domain` varchar(64) NOT NULL,
  `event` varchar(64) NOT NULL DEFAULT 'presence',
  `status` int(11) NOT NULL,
  `reason` varchar(64) DEFAULT NULL,
  `inserted_time` int(11) NOT NULL,
  PRIMARY KEY (`id`),
  UNIQUE KEY `watcher_idx` (`presentity_uri`,`watcher_username`,`watcher_domain`,`event`)
) ENGINE=InnoDB DEFAULT CHARSET=latin1;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `xcap`
--

DROP TABLE IF EXISTS `xcap`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `xcap` (
  `id` int(10) unsigned NOT NULL AUTO_INCREMENT,
  `username` varchar(64) NOT NULL,
  `domain` varchar(64) NOT NULL,
  `doc` mediumblob NOT NULL,
  `doc_type` int(11) NOT NULL,
  `etag` varchar(64) NOT NULL,
  `source` int(11) NOT NULL,
  `doc_uri` varchar(255) NOT NULL,
  `port` int(11) NOT NULL,
  PRIMARY KEY (`id`),
  UNIQUE KEY `doc_uri_idx` (`doc_uri`),
  KEY `account_doc_type_idx` (`username`,`domain`,`doc_type`),
  KEY `account_doc_type_uri_idx` (`username`,`domain`,`doc_type`,`doc_uri`),
  KEY `account_doc_uri_idx` (`username`,`domain`,`doc_uri`)
) ENGINE=InnoDB DEFAULT CHARSET=latin1;
/*!40101 SET character_set_client = @saved_cs_client */;
/*!40103 SET TIME_ZONE=@OLD_TIME_ZONE */;

/*!40101 SET SQL_MODE=@OLD_SQL_MODE */;
/*!40014 SET FOREIGN_KEY_CHECKS=@OLD_FOREIGN_KEY_CHECKS */;
/*!40014 SET UNIQUE_CHECKS=@OLD_UNIQUE_CHECKS */;
/*!40101 SET CHARACTER_SET_CLIENT=@OLD_CHARACTER_SET_CLIENT */;
/*!40101 SET CHARACTER_SET_RESULTS=@OLD_CHARACTER_SET_RESULTS */;
/*!40101 SET COLLATION_CONNECTION=@OLD_COLLATION_CONNECTION */;
/*!40111 SET SQL_NOTES=@OLD_SQL_NOTES */;

-- Dump completed on 2015-05-08 11:38:00
-- MySQL dump 10.13  Distrib 5.5.43, for debian-linux-gnu (x86_64)
--
-- Host: localhost    Database: scscf_db
-- ------------------------------------------------------
-- Server version	5.5.43-0ubuntu0.14.04.1

/*!40101 SET @OLD_CHARACTER_SET_CLIENT=@@CHARACTER_SET_CLIENT */;
/*!40101 SET @OLD_CHARACTER_SET_RESULTS=@@CHARACTER_SET_RESULTS */;
/*!40101 SET @OLD_COLLATION_CONNECTION=@@COLLATION_CONNECTION */;
/*!40101 SET NAMES utf8 */;
/*!40103 SET @OLD_TIME_ZONE=@@TIME_ZONE */;
/*!40103 SET TIME_ZONE='+00:00' */;
/*!40014 SET @OLD_UNIQUE_CHECKS=@@UNIQUE_CHECKS, UNIQUE_CHECKS=0 */;
/*!40014 SET @OLD_FOREIGN_KEY_CHECKS=@@FOREIGN_KEY_CHECKS, FOREIGN_KEY_CHECKS=0 */;
/*!40101 SET @OLD_SQL_MODE=@@SQL_MODE, SQL_MODE='NO_AUTO_VALUE_ON_ZERO' */;
/*!40111 SET @OLD_SQL_NOTES=@@SQL_NOTES, SQL_NOTES=0 */;

--
-- Current Database: `scscf_db`
--

CREATE DATABASE /*!32312 IF NOT EXISTS*/ `scscf_db` /*!40100 DEFAULT CHARACTER SET latin1 */;

USE `scscf_db`;

--
-- Dumping data for table `active_watchers`
--

LOCK TABLES `active_watchers` WRITE;
/*!40000 ALTER TABLE `active_watchers` DISABLE KEYS */;
/*!40000 ALTER TABLE `active_watchers` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Dumping data for table `contact`
--

LOCK TABLES `contact` WRITE;
/*!40000 ALTER TABLE `contact` DISABLE KEYS */;
/*!40000 ALTER TABLE `contact` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Dumping data for table `impu`
--

LOCK TABLES `impu` WRITE;
/*!40000 ALTER TABLE `impu` DISABLE KEYS */;
/*!40000 ALTER TABLE `impu` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Dumping data for table `impu_contact`
--

LOCK TABLES `impu_contact` WRITE;
/*!40000 ALTER TABLE `impu_contact` DISABLE KEYS */;
/*!40000 ALTER TABLE `impu_contact` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Dumping data for table `impu_subscriber`
--

LOCK TABLES `impu_subscriber` WRITE;
/*!40000 ALTER TABLE `impu_subscriber` DISABLE KEYS */;
/*!40000 ALTER TABLE `impu_subscriber` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Dumping data for table `presentity`
--

LOCK TABLES `presentity` WRITE;
/*!40000 ALTER TABLE `presentity` DISABLE KEYS */;
/*!40000 ALTER TABLE `presentity` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Dumping data for table `pua`
--

LOCK TABLES `pua` WRITE;
/*!40000 ALTER TABLE `pua` DISABLE KEYS */;
/*!40000 ALTER TABLE `pua` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Dumping data for table `subscriber`
--

LOCK TABLES `subscriber` WRITE;
/*!40000 ALTER TABLE `subscriber` DISABLE KEYS */;
/*!40000 ALTER TABLE `subscriber` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Dumping data for table `version`
--

LOCK TABLES `version` WRITE;
/*!40000 ALTER TABLE `version` DISABLE KEYS */;
INSERT INTO `version` VALUES ('active_watchers',11);
INSERT INTO `version` VALUES ('contact',6);
INSERT INTO `version` VALUES ('impu',6);
INSERT INTO `version` VALUES ('impu_contact',6);
INSERT INTO `version` VALUES ('impu_subscriber',6);
INSERT INTO `version` VALUES ('presentity',4);
INSERT INTO `version` VALUES ('pua',7);
INSERT INTO `version` VALUES ('subscriber',6);
INSERT INTO `version` VALUES ('watchers',3);
INSERT INTO `version` VALUES ('xcap',4);
/*!40000 ALTER TABLE `version` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Dumping data for table `watchers`
--

LOCK TABLES `watchers` WRITE;
/*!40000 ALTER TABLE `watchers` DISABLE KEYS */;
/*!40000 ALTER TABLE `watchers` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Dumping data for table `xcap`
--

LOCK TABLES `xcap` WRITE;
/*!40000 ALTER TABLE `xcap` DISABLE KEYS */;
/*!40000 ALTER TABLE `xcap` ENABLE KEYS */;
UNLOCK TABLES;
/*!40103 SET TIME_ZONE=@OLD_TIME_ZONE */;

/*!40101 SET SQL_MODE=@OLD_SQL_MODE */;
/*!40014 SET FOREIGN_KEY_CHECKS=@OLD_FOREIGN_KEY_CHECKS */;
/*!40014 SET UNIQUE_CHECKS=@OLD_UNIQUE_CHECKS */;
/*!40101 SET CHARACTER_SET_CLIENT=@OLD_CHARACTER_SET_CLIENT */;
/*!40101 SET CHARACTER_SET_RESULTS=@OLD_CHARACTER_SET_RESULTS */;
/*!40101 SET COLLATION_CONNECTION=@OLD_COLLATION_CONNECTION */;
/*!40111 SET SQL_NOTES=@OLD_SQL_NOTES */;

-- Dump completed on 2015-05-08 11:38:01
# DB access rights
grant delete,insert,select,update on scscf_db.* to scscf@localhost identified by 'scscf';
grant delete,insert,select,update on scscf_db.* to provisioning@localhost identified by 'provi';
