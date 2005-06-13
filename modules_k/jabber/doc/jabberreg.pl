#!/usr/bin/perl -w
#
# Register a new user with Jabber gateway
#
# The parameters must be only the username part of the sip address
# Change the $sip_domain according with your SIP server domain name
#

use Socket;
use DBD::mysql;

if(@ARGV == 0)
{
	die ("Syntax: regjab.pl username1 username2 ...");
}

$db_driver = "mysql";
$sip_domain = "voip.org";

##### MySQL connection to JabberGW database
$jab_db_nm = "sip_jab";			#JabberGW's database name
$jab_db_hn = "127.0.0.1";		#JabberGW's database server address
$jab_db_pt = "3306";			#JabberGW's database server port
$jab_db_us = "user";			#JabberGW's database username
$jab_db_ps = "*********";		#JabberGW's database password
$jab_db_dsn = "DBI:$db_driver:database=$jab_db_nm;host=$jab_db_hn;port=$jab_db_pt";
##### users table
$jab_tb_nm = "jusers";			#JabberGW's subscriber table
##### username column
$jab_cl_usj = "jab_id";
$jab_cl_psj = "jab_passwd";
$jab_cl_uss = "sip_id";

$jab_server = "localhost";		#Jabber server address
$jab_srvid = "jabber.x.com";		#Jabber server id
$jab_port = 5222;			#Jabber server port
$iaddr = inet_aton($jab_server)   or die "no host: $jab_server";
$paddr = sockaddr_in($jab_port, $iaddr);

$proto = getprotobyname('tcp');

### Connect to MySQL database
$jab_dbh = DBI->connect($jab_db_dsn, $jab_db_us, $jab_db_ps);
if(!$jab_dbh )
{
	print ('ERROR: Jabber\'s MySQL server not responding!');
	exit;
}

foreach $jab_usr (@ARGV)
{

	print "Subscribe SIP user <$jab_usr> to Jabber gateway\n";
	#checkif user has already a Jabber ID
	$sip_uri = "sip:$jab_usr\@$sip_domain";
	$jab_pwd = "qw1$jab_usr#";

	$jab_sth = $jab_dbh->prepare("SELECT $jab_cl_usj FROM $jab_tb_nm WHERE $jab_cl_uss='$sip_uri'");

	if (!$jab_sth)
	{
		die($jab_dbh->errstr);
	}
	if (!$jab_sth->execute)
	{
		die($jab_sth->errstr);
	}
	if(!($jab_sth->fetchrow_hashref()))
	{
		# create Jabber account
		print "Register SIP user <$jab_usr> to Jabber server\n";
		socket(SOCK, PF_INET,SOCK_STREAM, $proto)  or die "socket: $!";
		connect(SOCK, $paddr) or die "connect: $!";
		$jcid = 0;
		$err_no = 0;
		$line = "<stream:stream to='$jab_srvid' xmlns='jabber:client' xmlns:stream='http://etherx.jabber.org/streams'>";
		send(SOCK, $line, 0);
		recv(SOCK, $line, 256, 0);
		$jid_p = index($line, "id=");
		if($jid_p > 0)
		{
			$jid_p = $jid_p+4;
			$jid_p1 = index($line, "'", $jid_p);
			if($jid_p1 > 0)
			{
				$jid = substr($line, $jid_p, $jid_p1-$jid_p);
				print("JID: $jid\n");
			}
		}
		$jcid =$jcid + 1;
		$line = "<iq id='$jcid' to='$jab_srvid' type='get'><query xmlns='jabber:iq:register'/></iq>";
		send(SOCK, $line, 0);
	
		recv(SOCK, $line, 512, 0);
		# here we should check what Jabber really wants
		# - I know for our srv, so skip it
		# 
		$jcid =$jcid + 1;
		$line = "<iq id='$jcid' to='$jab_srvid' type='set'><query xmlns='jabber:iq:register'><username>$jab_usr</username><password>$jab_pwd</password></query></iq>";
		send(SOCK, $line, 0);

		recv(SOCK, $line, 512, 0);
		if(index($line, " id='$jcid'")>0 && index($line, " type='error'")>0)
		{
			#error creating Jabber user
			print("Error: creating Jabber user <$jab_usr>\n$line\n");
			$err_no = 1;
		}
		$line = "</stream:stream>";
		send(SOCK, $line, 0);
		close(SOCK) or die "close: $!";
				
		# add to Jabber database
		if($err_no == 0)
		{
			$rows = $jab_dbh->do("INSERT INTO $jab_tb_nm ($jab_cl_usj, $jab_cl_psj, $jab_cl_uss) VALUES ('$jab_usr', '$jab_pwd', '$sip_uri')");
			if($rows == 1)
			{
				print("SIP user <$jab_usr> added to Jabber database\n");
			}
			else
			{
				print("Error: SIP user <$jab_usr> not added to Jabber database\n");
			}
		}
	}
	else
	{
		print("SIP user <$jab_usr> is already in Jabber database\n");
	}
}
$jab_sth->finish();

#### Disconnect from the database.
$jab_dbh->disconnect();
