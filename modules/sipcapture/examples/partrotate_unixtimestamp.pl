#!/usr/bin/perl

use DBI;

$table = "sip_capture";
$dbname = "homer_db";
$maxparts = 6; #6 days
$newparts = 1; #new partitions for 1 day. Script must start daily!

#Hours
$maxparts*=24;
$newparts*=24;

my $db = DBI->connect("DBI:mysql:$dbname:localhost:3306", "mysql_login", "mysql_password");

#$db->{PrintError} = 0;

my $query = "SELECT UNIX_TIMESTAMP(CURDATE() - INTERVAL 1 DAY)";
$sth = $db->prepare($query);
$sth->execute();
my ($curtstamp) = $sth->fetchrow_array();
$curtstamp+=0; 

#print "ZZ: $curtstamp: $maxparts, $newparts\n";
#exit;

my $query = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.PARTITIONS"
            ."\n WHERE TABLE_NAME='".$table."' AND TABLE_SCHEMA='".$dbname."'";
$sth = $db->prepare($query);
$sth->execute();
my ($partcount) = $sth->fetchrow_array();

#print "$query\nZ: $partcount\n";

while($partcount > ($maxparts + $newparts)) {

    $query = "SELECT PARTITION_NAME, MIN(PARTITION_DESCRIPTION)"
             ."\n FROM INFORMATION_SCHEMA.PARTITIONS WHERE TABLE_NAME='".$table."'"
             ."\n AND TABLE_SCHEMA='".$dbname."';";

    $sth = $db->prepare($query);
    $sth->execute();
    my ($minpart,$todaytstamp) = $sth->fetchrow_array();
    $todaytstamp+=0;
    
    #Dont' delete the partition for the current day or for future. Bad idea!
    if($curtstamp <= $todaytstamp) {    
          $partcount = 0;
          next;
    }
           
    #Delete
    $query = "ALTER TABLE ".$table." DROP PARTITION ".$minpart;
    $db->do($query);
    if (!$db->{Executed}) {
           print "Couldn't drop partition: $minpart\n";
           break;
    }
}

# < condition
$curtstamp+=(86400);

#Create new partitions 
for(my $i=0; $i<$newparts; $i++) {

    $oldstamp = $curtstamp;
    $curtstamp+=3600;
    
    ($sec,$min,$hour,$mday,$mon,$year,$wday,$yday,$isdst) = localtime($oldstamp);

    my $newpartname = sprintf("p%04d%02d%02d%02d",($year+=1900),(++$mon),$mday,$hour);    
    
    $query = "SELECT COUNT(*) "
             ."\n FROM INFORMATION_SCHEMA.PARTITIONS WHERE TABLE_NAME='".$table."'"
             ."\n AND TABLE_SCHEMA='".$dbname."' AND PARTITION_NAME='".$newpartname."'"
             ."\n AND PARTITION_DESCRIPTION = '".$curtstamp."'";
             
    $sth = $db->prepare($query);
    $sth->execute();
    my ($exist) = $sth->fetchrow_array();
    $exist+=0;
    
    if(!$exist) {
    
        $query = "ALTER TABLE ".$table." ADD PARTITION (PARTITION ".$newpartname
             ."\n VALUES LESS THAN (".$curtstamp.") ENGINE = MyISAM)";
        $db->do($query);
        if (!$db->{Executed}) {
             print "Couldn't add partition: $newpartname\n";
        }
    }    
}
