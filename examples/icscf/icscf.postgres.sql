--
-- Generated from mysql2pgsql.perl
-- http://gborg.postgresql.org/project/mysql2psql/
-- (c) 2001 - 2007 Jose M. Duarte, Joseph Speigle
--

-- warnings are printed for drop tables if they do not exist
-- please see http://archives.postgresql.org/pgsql-novice/2004-10/msg00158.php

-- ##############################################################
/*!40101 SET @OLD_CHARACTER_SET_CLIENT=@@CHARACTER_SET_CLIENT */;
/*!40101 SET @OLD_CHARACTER_SET_RESULTS=@@CHARACTER_SET_RESULTS */;
/*!40101 SET @OLD_COLLATION_CONNECTION=@@COLLATION_CONNECTION */;
/*!40101 SET NAMES utf8 */;



DROP TABLE IF EXISTS "nds_trusted_domains" CASCADE\g
DROP SEQUENCE IF EXISTS "nds_trusted_domains_id_seq" CASCADE ;

CREATE SEQUENCE "nds_trusted_domains_id_seq" ;

CREATE TABLE  "nds_trusted_domains" (
   "id" integer DEFAULT nextval('"nds_trusted_domains_id_seq"') NOT NULL,
   "trusted_domain"   varchar(83) NOT NULL DEFAULT '', 
   primary key ("id")
)   ;

INSERT INTO "nds_trusted_domains" VALUES (1, E'ims.ng-voice.com'); 

DROP TABLE IF EXISTS "s_cscf" CASCADE\g
DROP SEQUENCE IF EXISTS "s_cscf_id_seq" CASCADE ;

CREATE SEQUENCE "s_cscf_id_seq" ;

CREATE TABLE  "s_cscf" (
   "id" integer DEFAULT nextval('"s_cscf_id_seq"') NOT NULL,
   "name"   varchar(83) NOT NULL DEFAULT '', 
   "s_cscf_uri"   varchar(83) NOT NULL DEFAULT '', 
   primary key ("id")
)   ;

INSERT INTO "s_cscf" VALUES (1, E'First and only S-CSCF', E'sip:scscf.ims.ng-voice.com:5060'); 

DROP TABLE IF EXISTS "s_cscf_capabilities" CASCADE\g
DROP SEQUENCE IF EXISTS "s_cscf_capabilities_id_seq" CASCADE ;

CREATE SEQUENCE "s_cscf_capabilities_id_seq" ;

CREATE TABLE  "s_cscf_capabilities" (
   "id" integer DEFAULT nextval('"s_cscf_capabilities_id_seq"') NOT NULL,
   "id_s_cscf"   int NOT NULL DEFAULT '0', 
   "capability"   int NOT NULL DEFAULT '0', 
   primary key ("id")
)   ;

INSERT INTO "s_cscf_capabilities" VALUES (1, 1, 0); 
INSERT INTO "s_cscf_capabilities" VALUES (2, 1, 1); 
INSERT INTO "s_cscf_capabilities" VALUES (4, 1, 2); 
/*!40101 SET CHARACTER_SET_CLIENT=@OLD_CHARACTER_SET_CLIENT */;
/*!40101 SET CHARACTER_SET_RESULTS=@OLD_CHARACTER_SET_RESULTS */;
/*!40101 SET COLLATION_CONNECTION=@OLD_COLLATION_CONNECTION */;
CREATE INDEX "s_cscf_capabilities_capability_idx" ON "s_cscf_capabilities" USING btree ("capability");
CREATE INDEX "s_cscf_capabilities_id_s_cscf_idx" ON "s_cscf_capabilities" USING btree ("id_s_cscf");
