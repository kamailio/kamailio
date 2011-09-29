#!/usr/bin/perl
use DBI;

$table = "sip_capture";
$dbname = "homer_db";
$maxparts = 20; #20 days
$newparts = 1; #new partitions for 1 day. Script must start daily!

my $db = DBI->connect("DBI:mysql:$dbname:localhost:3306", "mysql_login", "mysql_password");

#$db->{PrintError} = 0;

my $query = "SELECT TO_DAYS(NOW()),UNIX_TIMESTAMP(NOW() + INTERVAL 1 DAY)";
$sth = $db->prepare($query);
$sth->execute();
my ($curtodays,$curtstamp) = $sth->fetchrow_array();
$curtodays+=0; 
$curtstamp+=0; 


my $query = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.PARTITIONS"
            ."\n WHERE TABLE_NAME='".$table."' AND TABLE_SCHEMA='".$dbname."'";
$sth = $db->prepare($query);
$sth->execute();
my ($partcount) = $sth->fetchrow_array();

while($partcount > ($maxparts + $newparts)) {

    $query = "SELECT PARTITION_NAME, MIN(PARTITION_DESCRIPTION)"
             ."\n FROM INFORMATION_SCHEMA.PARTITIONS WHERE TABLE_NAME='".$table."'"
             ."\n AND TABLE_SCHEMA='".$dbname."';";
             
    $sth = $db->prepare($query);
    $sth->execute();
    my ($minpart,$todays) = $sth->fetchrow_array();
    $todays+=0;
    
    #Dont' delete the partition for the current day or for future. Bad idea!
    if($curtodays <= $todays) {    
            $partcount=0;
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
$curtodays+=1;

#Create new partitions 
for(my $i=0; $i<$newparts; $i++) {

    $curtodays+=1;
    
    ($sec,$min,$hour,$mday,$mon,$year,$wday,$yday,$isdst) = localtime($curtstamp);

    my $newpartname = sprintf("p%04d%02d%02d",($year+=1900),(++$mon),$mday);    
    
    $query = "SELECT COUNT(*) "
             ."\n FROM INFORMATION_SCHEMA.PARTITIONS WHERE TABLE_NAME='".$table."'"
             ."\n AND TABLE_SCHEMA='".$dbname."' AND PARTITION_NAME='".$newpartname."'"
             ."\n AND PARTITION_DESCRIPTION = '".$curtodays."'";
             
    $sth = $db->prepare($query);
    $sth->execute();
    my ($exist) = $sth->fetchrow_array();
    $exist+=0;
    
    if(!$exist) {
    
        # Fix MAXVALUE. Thanks Dorn B. <djbinter@gmail.com> for report and fix.
        $query = "ALTER TABLE ".$table." REORGANIZE PARTITION pmax INTO (PARTITION ".$newpartname
                                        ."\n VALUES LESS THAN (".$curtodays.") ENGINE = MyISAM, PARTITION pmax VALUES LESS THAN MAXVALUE ENGINE = MyISAM)";                                                         
        $db->do($query);
        if (!$db->{Executed}) {
             print "Couldn't add partition: $newpartname\n";
        }
    }
    
    $curtstamp += 86400;
}
