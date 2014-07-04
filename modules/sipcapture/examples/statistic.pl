#!/usr/bin/perl
#
# statistics.pl - perl script for Homer statistic
#
# Copyright (C) 2011 Alexandr Dubovikov (QSC AG) (alexandr.dubovikov@gmail.com)
#
# This file is part of webhomer, a free capture server.
#
# statistics is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version
#
# statistics is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA

use DBI;

$version = "0.0.2";
$mysql_table = "sip_capture";
$mysql_dbname = "homer_db";
$mysql_user = "homer_user";
$mysql_password = "homer_password";
$mysql_host = "localhost";
$statsmethod = "stats_method";
$statsuseragent = "stats_useragent";
$keepdays = 100; #How long statistic must be keeped in DB
$step = 300; # in seconds! for 5 minutes statistic. Script must start each 5 minutes 
#Crontab:
#*/5 * * * * statistic.pl 2>&1 > /dev/null

my $db = DBI->connect("DBI:mysql:$mysql_dbname:$mysql_host:3306", $mysql_user, $mysql_password);

#$db->{PrintError} = 0;

@nowtime = localtime();
@oldtime = localtime(time()-$step);

#($sec,$min,$hour,$mday,$mon,$year,$wday,$yday,$isdst) = localtime();

my $to_date = sprintf("'%04d-%02d-%02d %02d:%02d:00'",($nowtime[5]+=1900),(++$nowtime[4]),$nowtime[3],$nowtime[2], $nowtime[1]);
my $from_date = sprintf("'%04d-%02d-%02d %02d:%02d:00'",($oldtime[5]+=1900),(++$oldtime[4]),$oldtime[3],$oldtime[2], $oldtime[1]);

#QUERY
my $mainquery = "FROM ".$mysql_table.$wheredata." WHERE `date` BETWEEN $from_date AND $to_date";
#My ASR AND NER == 0 at this time;
$ner = 0;
$asr = 0;

#statistic
#ALL AND CURRENT PACKETS. ALL = ALL MESSAGES IN DB. CURRENT = MESSAGES IN THIS INTERVALL
##############################################################################################
$all = loadResult("SELECT COUNT(*) FROM ".$mysql_table);
insertStat("stats_method","method='ALL',total='".$all."'");

$current = loadResult("SELECT COUNT(*) ".$mainquery);
insertStat("stats_method","method='CURRENT',total='".$current."'");

##################################   INVITE ########################################################
#ALL INVITES
$invites = loadResult("SELECT COUNT(*) ".$mainquery." AND method = 'INVITE'");
#ALL 407 for INVITE
$auth = loadResult("SELECT COUNT(*) ".$mainquery." AND method = '407' AND cseq like '% INVITE'");
#Total invites without AUTH
$totalinvites = $invites - $auth;

#ASR/NER
#Ansered INVITES
$answered = loadResult("SELECT COUNT(*) ".$mainquery." AND method='200' AND cseq like '% INVITE'");
#NER
$bad486 = loadResult("SELECT COUNT(*) ".$mainquery." AND method IN ('486','487','603') AND cseq like '% INVITE'");
#UNSANSWERED
$unanswered = $totalinvites - $answered;

#fix for retransmitions
if($unanswered < 0) { $unanswered = 0; }
if($answered > $totalinvites) { $answered=$totalinvites; }

if($totalinvites > 0) {
  $ner = sprintf( "%.0f", ($answered+$bad486)/$totalinvites*100);
  $asr = sprintf( "%.0f", $answered/$totalinvites*100);
}

$value="method='INVITE',total='".$invites."',auth='".$auth."',completed='"
    .$answered."',uncompleted='".$unanswered."',rejected='".$bad486."',asr='".$asr."',ner='".$ner."'";
insertStat($statsmethod,$value);

##################################  REGISTERED ########################################################
#ALL REGISTRATION
$registers = loadResult("SELECT COUNT(*) ".$mainquery." AND method = 'REGISTER'");
#ALL 401 for REGISTER
$regiserauth = loadResult("SELECT COUNT(*) ".$mainquery." AND method = '401' AND cseq like '% REGISTER'");
#REGISTERED! for expire =0 will be also calculate as registered. Bad idea but no chance :-/
$registered = loadResult("SELECT COUNT(*) ".$mainquery." AND method='200' AND cseq like '% REGISTER'");

#Bad register
$badregister=0;
$badregister = $regiserauth - $registered if($registered < $regiserauth);
insertStat($statsmethod,"method='REGISTER', uncompleted='".$badregister."', total='".$registers."',auth='".$regiserauth."',completed='".$registered."'");

#USER AGENT
#REGISTRATION
$query = "SELECT user_agent, COUNT(*) as cnt ".$mainquery." AND method = 'REGISTER' GROUP BY user_agent";
$sth = $db->prepare($query);
$sth->execute();
while ( @row = $sth->fetchrow_array ) {
    $myuas = $db->quote($row[0]);
    insertStat($statsuseragent,"method='REGISTER',useragent=".$myuas.",total=".$row[1]);
}

#INVITES
$query = "SELECT user_agent, COUNT(*) as cnt ".$mainquery." AND method = 'INVITE' GROUP BY user_agent";
$sth = $db->prepare($query);
$sth->execute();
while ( @row = $sth->fetchrow_array ) {
    $myuas = $db->quote($row[0]);
    insertStat($statsuseragent,"method='INVITE',useragent=".$myuas.",total=".$row[1]);
}

#Time for CLEAR OLD records;
clearOldData() if($nowtime[2] == 0 && $nowtime[1] == 0) ;

sub loadResult() {
    my $query = shift;
    $sth = $db->prepare($query);
    $sth->execute();
    my $result = $sth->fetchrow_array();
    return $result;
}

sub insertStat() {
    my $table = shift;
    my $values = shift;
    my $query = "INSERT INTO ".$table." SET `from_date`=".$from_date.", `to_date`=".$to_date.",".$values;
    $sth2 = $db->prepare($query);
    $sth2->execute();
    return 1;
}

sub clearOldData() {
  $query = "DELETE FROM ".$statsmethod." WHERE `from_date` < UNIX_TIMESTAMP(CURDATE() - INTERVAL ".$keepdays." DAY)";
  $sth = $db->prepare($query);
  $sth->execute();

  $query = "DELETE FROM ".$statsuseragent." WHERE `from_date` < UNIX_TIMESTAMP(CURDATE() - INTERVAL ".$keepdays." DAY)";
  $sth = $db->prepare($query);
  $sth->execute();
}


