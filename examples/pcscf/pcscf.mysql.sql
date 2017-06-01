-- MySQL dump 10.13  Distrib 5.5.43, for debian-linux-gnu (x86_64)
--
-- Host: localhost    Database: pcscf_db
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
-- Current Database: `pcscf_db`
--

/*!40000 DROP DATABASE IF EXISTS `pcscf_db`*/;

CREATE DATABASE /*!32312 IF NOT EXISTS*/ `pcscf_db` /*!40100 DEFAULT CHARACTER SET latin1 */;

USE `pcscf_db`;

--
-- Table structure for table `location`
--

DROP TABLE IF EXISTS `location`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `location` (
  `id` int(10) unsigned NOT NULL AUTO_INCREMENT,
  `domain` varchar(64) DEFAULT NULL,
  `aor` varchar(255) NOT NULL,
  `contact` varchar(255) DEFAULT NULL,
  `received` varchar(128) DEFAULT NULL,
  `received_port` int(10) unsigned DEFAULT NULL,
  `received_proto` int(10) unsigned DEFAULT NULL,
  `path` varchar(512) DEFAULT NULL,
  `rx_session_id` varchar(256) DEFAULT NULL,
  `reg_state` tinyint(4) DEFAULT NULL,
  `expires` datetime DEFAULT '2030-05-28 21:32:15',
  `service_routes` varchar(2048) DEFAULT NULL,
  `socket` varchar(64) DEFAULT NULL,
  `public_ids` varchar(2048) DEFAULT NULL,
  `security_type` int(11) DEFAULT NULL,
  `protocol` char(5) DEFAULT NULL,
  `mode` char(10) DEFAULT NULL,
  `ck` varchar(100) DEFAULT NULL,
  `ik` varchar(100) DEFAULT NULL,
  `ealg` char(20) DEFAULT NULL,
  `ialg` char(20) DEFAULT NULL,
  `port_uc` int(11) unsigned DEFAULT NULL,
  `port_us` int(11) unsigned DEFAULT NULL,
  `spi_pc` int(11) unsigned DEFAULT NULL,
  `spi_ps` int(11) unsigned DEFAULT NULL,
  `spi_uc` int(11) unsigned DEFAULT NULL,
  `spi_us` int(11) unsigned DEFAULT NULL,
  `t_security_type` int(11) DEFAULT NULL,
  `t_port_uc` int(11) unsigned DEFAULT NULL,
  `t_port_us` int(11) unsigned DEFAULT NULL,
  `t_spi_pc` int(11) unsigned DEFAULT NULL,
  `t_spi_ps` int(11) unsigned DEFAULT NULL,
  `t_spi_uc` int(11) unsigned DEFAULT NULL,
  `t_spi_us` int(11) unsigned DEFAULT NULL,
  `t_protocol` char(5) DEFAULT NULL,
  `t_mode` char(10) DEFAULT NULL,
  `t_ck` varchar(100) DEFAULT NULL,
  `t_ik` varchar(100) DEFAULT NULL,
  `t_ealg` char(20) DEFAULT NULL,
  `t_ialg` char(20) DEFAULT NULL,
  PRIMARY KEY (`id`),
  KEY `aor` (`aor`)
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
  `pres_id` varchar(128) NOT NULL,
  `event` int(11) NOT NULL,
  `expires` int(11) NOT NULL,
  `flag` int(11) NOT NULL,
  `etag` varchar(128) NOT NULL,
  `tuple_id` varchar(128) NOT NULL,
  `watcher_uri` varchar(128) NOT NULL,
  `call_id` varchar(128) NOT NULL,
  `to_tag` varchar(128) NOT NULL,
  `from_tag` varchar(128) NOT NULL,
  `cseq` int(11) NOT NULL,
  `record_route` text,
  `contact` varchar(128) NOT NULL,
  `remote_contact` varchar(128) NOT NULL,
  `extra_headers` text,
  `desired_expires` int(11) NOT NULL,
  `version` int(11) NOT NULL,
  PRIMARY KEY (`id`)
) ENGINE=MyISAM DEFAULT CHARSET=latin1;
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
/*!40103 SET TIME_ZONE=@OLD_TIME_ZONE */;

/*!40101 SET SQL_MODE=@OLD_SQL_MODE */;
/*!40014 SET FOREIGN_KEY_CHECKS=@OLD_FOREIGN_KEY_CHECKS */;
/*!40014 SET UNIQUE_CHECKS=@OLD_UNIQUE_CHECKS */;
/*!40101 SET CHARACTER_SET_CLIENT=@OLD_CHARACTER_SET_CLIENT */;
/*!40101 SET CHARACTER_SET_RESULTS=@OLD_CHARACTER_SET_RESULTS */;
/*!40101 SET COLLATION_CONNECTION=@OLD_COLLATION_CONNECTION */;
/*!40111 SET SQL_NOTES=@OLD_SQL_NOTES */;

-- Dump completed on 2015-05-08 11:37:40
-- MySQL dump 10.13  Distrib 5.5.43, for debian-linux-gnu (x86_64)
--
-- Host: localhost    Database: pcscf_db
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
-- Current Database: `pcscf_db`
--

CREATE DATABASE /*!32312 IF NOT EXISTS*/ `pcscf_db` /*!40100 DEFAULT CHARACTER SET latin1 */;

USE `pcscf_db`;

--
-- Dumping data for table `location`
--

LOCK TABLES `location` WRITE;
/*!40000 ALTER TABLE `location` DISABLE KEYS */;
/*!40000 ALTER TABLE `location` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Dumping data for table `version`
--

LOCK TABLES `version` WRITE;
/*!40000 ALTER TABLE `version` DISABLE KEYS */;
INSERT INTO `version` VALUES ('location',6);
INSERT INTO `version` VALUES ('pua',7);
/*!40000 ALTER TABLE `version` ENABLE KEYS */;
UNLOCK TABLES;
/*!40103 SET TIME_ZONE=@OLD_TIME_ZONE */;

/*!40101 SET SQL_MODE=@OLD_SQL_MODE */;
/*!40014 SET FOREIGN_KEY_CHECKS=@OLD_FOREIGN_KEY_CHECKS */;
/*!40014 SET UNIQUE_CHECKS=@OLD_UNIQUE_CHECKS */;
/*!40101 SET CHARACTER_SET_CLIENT=@OLD_CHARACTER_SET_CLIENT */;
/*!40101 SET CHARACTER_SET_RESULTS=@OLD_CHARACTER_SET_RESULTS */;
/*!40101 SET COLLATION_CONNECTION=@OLD_COLLATION_CONNECTION */;
/*!40111 SET SQL_NOTES=@OLD_SQL_NOTES */;

-- Dump completed on 2015-05-08 11:37:40
# DB access rights
grant delete,insert,select,update on pcscf_db.* to pcscf@localhost identified by 'pcscf';
grant delete,insert,select,update on pcscf_db.* to provisioning@localhost identified by 'provi';
