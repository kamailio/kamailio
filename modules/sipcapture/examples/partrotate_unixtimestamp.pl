#!/usr/bin/perl
#
# partrotate_unixtimestamp - perl script for mySQL partition rotation
#
# Copyright (C) 2011-2014 Alexandr Dubovikov (alexandr.dubovikov@gmail.com)
#
# This file is part of webhomer, a free capture server.
#
# partrotate_unixtimestamp is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 3 of the License, or
# (at your option) any later version
#
# partrotate_unixtimestamp is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

use DBI;

$version = "0.3.0";
$mysql_table = "sip_capture";
$mysql_dbname = "homer_db";
$mysql_user = "mysql_login";
$mysql_password = "mysql_password";
$mysql_host = "localhost";
$maxparts = 6; #6 days How long keep the data in the DB
$newparts = 2; #new partitions for 2 days. Anyway, start this script daily!
@stepsvalues = (86400, 3600, 1800, 900); 
$partstep = 0; # 0 - Day, 1 - Hour, 2 - 30 Minutes, 3 - 15 Minutes 
$engine = "InnoDB"; #MyISAM or InnoDB
$compress = "ROW_FORMAT=COMPRESSED KEY_BLOCK_SIZE=8"; #Enable this if you want use barracuda format or set var to empty.
$sql_schema_version = 2;
$auth_column = "auth";
$check_table = 1; #Check if table exists. For PostgreSQL change creation schema!

#Check it
$partstep=0 if(!defined $stepsvalues[$partstep]);
#Mystep
$mystep = $stepsvalues[$partstep];
#Coof

# Optionally load override configuration. perl format
$rc = "/etc/sysconfig/partrotaterc";
if (-e $rc) {
  do $rc;
}

$coof=int(86400/$mystep);

#How much partitions
$maxparts*=$coof;
$newparts*=$coof;
$totalparts = ($maxparts+$newparts);

my $db = DBI->connect("DBI:mysql:$mysql_dbname:$mysql_host:3306", $mysql_user, $mysql_password);

$auth_column = "authorization" if($sql_schema_version == 1);

#$db->{PrintError} = 0;

$sql = "CREATE TABLE IF NOT EXISTS `".$mysql_table."` (
  `id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
  `date` timestamp NOT NULL DEFAULT '0000-00-00 00:00:00',
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
  `".$auth_column."` varchar(120) NOT NULL,
  `user_agent` varchar(256) NOT NULL,
  `source_ip` varchar(60) NOT NULL DEFAULT '',
  `source_port` int(10) NOT NULL,
  `destination_ip` varchar(60) NOT NULL DEFAULT '',
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
  `msg` text NOT NULL,
  PRIMARY KEY (`id`,`date`),
  KEY `ruri_user` (`ruri_user`),
  KEY `from_user` (`from_user`),
  KEY `to_user` (`to_user`),
  KEY `pid_user` (`pid_user`),
  KEY `auth_user` (`auth_user`),
  KEY `callid_aleg` (`callid_aleg`),
  KEY `date` (`date`),
  KEY `callid` (`callid`),
  KEY `method` (`method`),
  KEY `source_ip` (`source_ip`),
  KEY `destination_ip` (`destination_ip`)
) ENGINE=".$engine." DEFAULT CHARSET=utf8 $compress
PARTITION BY RANGE ( UNIX_TIMESTAMP(`date`)) (PARTITION pmax VALUES LESS THAN MAXVALUE ENGINE = ".$engine.")";

my $sth = $db->do($sql) if($check_table == 1);

#check if the table has partitions. If not, create one
my $query = "SHOW TABLE STATUS FROM ".$mysql_dbname. " WHERE Name='".$mysql_table."'";
$sth = $db->prepare($query);
$sth->execute();
my $tstatus = $sth->fetchrow_hashref()->{Create_options};
if ($tstatus !~ /partitioned/) {
   my $query = "ALTER TABLE ".$mysql_table. " PARTITION BY RANGE ( UNIX_TIMESTAMP(`date`)) (PARTITION pmax VALUES LESS THAN MAXVALUE)";
   $sth = $db->prepare($query);
   $sth->execute();
}

my $query = "SELECT UNIX_TIMESTAMP(CURDATE() - INTERVAL 1 DAY)";
$sth = $db->prepare($query);
$sth->execute();
my ($curtstamp) = $sth->fetchrow_array();
$curtstamp+=0; 
$todaytstamp+=0;


my %PARTS;
#Geting all partitions
$query = "SELECT PARTITION_NAME, PARTITION_DESCRIPTION"
             ."\n FROM INFORMATION_SCHEMA.PARTITIONS WHERE TABLE_NAME='".$mysql_table."'"
             ."\n AND TABLE_SCHEMA='".$mysql_dbname."' ORDER BY PARTITION_DESCRIPTION ASC;";
$sth = $db->prepare($query);
$sth->execute();
my ($partcount) = $sth->rows;
while(my ($minpart,$todaytstamp) = $sth->fetchrow_array()) {

    if($partcount <= $totalparts || $curtstamp <= $todaytstamp) {
          #Creating HASH of existing partitions  
          $PARTS{$minpart."_".$todaytstamp} = 1;
          next;
    }
    
    next if($minpart eq "pmax");
    
    $query = "ALTER TABLE ".$mysql_table." DROP PARTITION ".$minpart;
    $db->do($query);
    if (!$db->{Executed}) {
           print "Couldn't drop partition: $minpart\n";
           break;
    }

    $partcount--;      
}

# < condition
$curtstamp+=(86400);

#Create new partitions
for(my $i=0; $i<$newparts; $i++) {

    $oldstamp = $curtstamp;
    $curtstamp+=$mystep;   

    ($sec,$min,$hour,$mday,$mon,$year,$wday,$yday,$isdst) = localtime($oldstamp);

    my $newpartname = sprintf("p%04d%02d%02d%02d",($year+=1900),(++$mon),$mday,$hour);
    $newpartname.= sprintf("%02d", $min) if($partstep > 1);

    if(!defined $PARTS{$newpartname."_".$curtstamp}) {

        # Fix MAXVALUE. Thanks Dorn B. <djbinter@gmail.com> for report and fix.
        $query = "ALTER TABLE ".$mysql_table." REORGANIZE PARTITION pmax INTO (PARTITION ".$newpartname
                                ."\n VALUES LESS THAN (".$curtstamp.") ENGINE = ".$engine.", PARTITION pmax VALUES LESS THAN MAXVALUE ENGINE = ".$engine.")";
        $db->do($query);
        if (!$db->{Executed}) {
             print "Couldn't add partition: $newpartname\n";
        }
    }    
}

