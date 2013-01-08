-- MySQL dump 10.9
--
-- Host: localhost    Database: icscf
-- ------------------------------------------------------
-- Server version	4.1.20-log

/*!40101 SET @OLD_CHARACTER_SET_CLIENT=@@CHARACTER_SET_CLIENT */;
/*!40101 SET @OLD_CHARACTER_SET_RESULTS=@@CHARACTER_SET_RESULTS */;
/*!40101 SET @OLD_COLLATION_CONNECTION=@@COLLATION_CONNECTION */;
/*!40101 SET NAMES utf8 */;
/*!40014 SET @OLD_UNIQUE_CHECKS=@@UNIQUE_CHECKS, UNIQUE_CHECKS=0 */;
/*!40014 SET @OLD_FOREIGN_KEY_CHECKS=@@FOREIGN_KEY_CHECKS, FOREIGN_KEY_CHECKS=0 */;
/*!40101 SET @OLD_SQL_MODE=@@SQL_MODE, SQL_MODE='NO_AUTO_VALUE_ON_ZERO' */;
/*!40111 SET @OLD_SQL_NOTES=@@SQL_NOTES, SQL_NOTES=0 */;

--
-- Current Database: `icscf`
--

/*!40000 DROP DATABASE IF EXISTS `icscf`*/;

CREATE DATABASE /*!32312 IF NOT EXISTS*/ `icscf` /*!40100 DEFAULT CHARACTER SET utf8 */;

USE `icscf`;

--
-- Table structure for table `nds_trusted_domains`
--

DROP TABLE IF EXISTS `nds_trusted_domains`;
CREATE TABLE `nds_trusted_domains` (
  `id` int(11) NOT NULL auto_increment,
  `trusted_domain` varchar(83) NOT NULL default '',
  PRIMARY KEY  (`id`)
) ENGINE=MyISAM DEFAULT CHARSET=utf8;

--
-- Table structure for table `s_cscf`
--

DROP TABLE IF EXISTS `s_cscf`;
CREATE TABLE `s_cscf` (
  `id` int(11) NOT NULL auto_increment,
  `name` varchar(83) NOT NULL default '',
  `s_cscf_uri` varchar(83) NOT NULL default '',
  PRIMARY KEY  (`id`)
) ENGINE=MyISAM DEFAULT CHARSET=utf8;

--
-- Table structure for table `s_cscf_capabilities`
--

DROP TABLE IF EXISTS `s_cscf_capabilities`;
CREATE TABLE `s_cscf_capabilities` (
  `id` int(11) NOT NULL auto_increment,
  `id_s_cscf` int(11) NOT NULL default '0',
  `capability` int(11) NOT NULL default '0',
  PRIMARY KEY  (`id`),
  KEY `idx_capability` (`capability`),
  KEY `idx_id_s_cscf` (`id_s_cscf`)
) ENGINE=MyISAM DEFAULT CHARSET=utf8;

/*!40101 SET SQL_MODE=@OLD_SQL_MODE */;
/*!40014 SET FOREIGN_KEY_CHECKS=@OLD_FOREIGN_KEY_CHECKS */;
/*!40014 SET UNIQUE_CHECKS=@OLD_UNIQUE_CHECKS */;
/*!40101 SET CHARACTER_SET_CLIENT=@OLD_CHARACTER_SET_CLIENT */;
/*!40101 SET CHARACTER_SET_RESULTS=@OLD_CHARACTER_SET_RESULTS */;
/*!40101 SET COLLATION_CONNECTION=@OLD_COLLATION_CONNECTION */;
/*!40111 SET SQL_NOTES=@OLD_SQL_NOTES */;

-- MySQL dump 10.9
--
-- Host: localhost    Database: icscf
-- ------------------------------------------------------
-- Server version	4.1.20-log

/*!40101 SET @OLD_CHARACTER_SET_CLIENT=@@CHARACTER_SET_CLIENT */;
/*!40101 SET @OLD_CHARACTER_SET_RESULTS=@@CHARACTER_SET_RESULTS */;
/*!40101 SET @OLD_COLLATION_CONNECTION=@@COLLATION_CONNECTION */;
/*!40101 SET NAMES utf8 */;
/*!40014 SET @OLD_UNIQUE_CHECKS=@@UNIQUE_CHECKS, UNIQUE_CHECKS=0 */;
/*!40014 SET @OLD_FOREIGN_KEY_CHECKS=@@FOREIGN_KEY_CHECKS, FOREIGN_KEY_CHECKS=0 */;
/*!40101 SET @OLD_SQL_MODE=@@SQL_MODE, SQL_MODE='NO_AUTO_VALUE_ON_ZERO' */;
/*!40111 SET @OLD_SQL_NOTES=@@SQL_NOTES, SQL_NOTES=0 */;

--
-- Current Database: `icscf`
--

CREATE DATABASE /*!32312 IF NOT EXISTS*/ `icscf` /*!40100 DEFAULT CHARACTER SET utf8 */;

USE `icscf`;

--
-- Dumping data for table `nds_trusted_domains`
--


/*!40000 ALTER TABLE `nds_trusted_domains` DISABLE KEYS */;
LOCK TABLES `nds_trusted_domains` WRITE;
INSERT INTO `nds_trusted_domains` VALUES (1,'intern.ng-voice.com');
UNLOCK TABLES;
/*!40000 ALTER TABLE `nds_trusted_domains` ENABLE KEYS */;

--
-- Dumping data for table `s_cscf`
--


/*!40000 ALTER TABLE `s_cscf` DISABLE KEYS */;
LOCK TABLES `s_cscf` WRITE;
INSERT INTO `s_cscf` VALUES (1,'First and only S-CSCF','sip:scscf.intern.ng-voice.com:5060');
UNLOCK TABLES;
/*!40000 ALTER TABLE `s_cscf` ENABLE KEYS */;

--
-- Dumping data for table `s_cscf_capabilities`
--


/*!40000 ALTER TABLE `s_cscf_capabilities` DISABLE KEYS */;
LOCK TABLES `s_cscf_capabilities` WRITE;
INSERT INTO `s_cscf_capabilities` VALUES (1,1,0),(2,1,1);
UNLOCK TABLES;
/*!40000 ALTER TABLE `s_cscf_capabilities` ENABLE KEYS */;

/*!40101 SET SQL_MODE=@OLD_SQL_MODE */;
/*!40014 SET FOREIGN_KEY_CHECKS=@OLD_FOREIGN_KEY_CHECKS */;
/*!40014 SET UNIQUE_CHECKS=@OLD_UNIQUE_CHECKS */;
/*!40101 SET CHARACTER_SET_CLIENT=@OLD_CHARACTER_SET_CLIENT */;
/*!40101 SET CHARACTER_SET_RESULTS=@OLD_CHARACTER_SET_RESULTS */;
/*!40101 SET COLLATION_CONNECTION=@OLD_COLLATION_CONNECTION */;
/*!40111 SET SQL_NOTES=@OLD_SQL_NOTES */;

# DB access rights
grant delete,insert,select,update on icscf.* to icscf@192.168.178.210 identified by 'heslo';
/* grant delete,insert,select,update on icscf.* to provisioning@localhost identified by 'provi'; */
