#!/usr/bin/perl

#Crontab script, to clear table/partition for the next day
#set crontab at 23:50

my $mysqlstring = "/usr/bin/mysql -uhomer_user -phomer_password -hlocalhost homer_db";

#homer node
my $wday = (localtime())[6] + 1;

#uncomment if you use separate tables
#Separate tables
#for(my $i=0; $i < 24; $i++) {
#  my $query = sprintf("TRUNCATE TABLE sip_capture_%02d_%02d", $wday, $i);
# `echo \"$query\"| $mysqlstring`;
#}

#uncomment if you use partitioning table and  MySQL >= 5.5
#my $query = sprintf("ALTER TABLE sip_capture TRUNCATE PARTITION p%d", $wday);
#`echo \"$query\"| $mysqlstring`;

#or Mysql < 5.5
my ($sec,$min,$hour,$mday,$mon,$year,$wday,$yday,$isdst) = localtime(time() - (86400*6));
$year+=1900;
$mon++;

my $query = sprintf("DELETE FROM sip_capture WHERE `date` < '%d-%02d-%02d 00:00:00' ", $year, $mon, $mday);
`echo \"$query\"| $mysqlstring`;
