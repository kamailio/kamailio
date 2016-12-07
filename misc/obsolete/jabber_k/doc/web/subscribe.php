<?php
# *** ---------------------------------------- ***
# IM Gateway subscription page
# contact daniel for anything related to it
# *** ---------------------------------------- ***
#
include ("libjab.php");
$jserver = "127.0.0.1";   # Jabber server address
$jport = "5222";     # Jabber server port
$jcid  = 0;      # Jabber communication ID
#
/* **************************************
# main database - users profile table - used for authentication
$sip_db_srv="127.0.0.1";  # database server
$sip_db_usr="openser";  # database user
$sip_db_pas="***";  # database user's password
$sip_db_db="openser";   # database name
$sip_db_tab="subscriber";  # name of users table
$sip_db_cusr="user"; # column name for username
$sip_db_cpas="password"; # column name for user's password
*************************************** */
#
# Jabber module database
$jab_db_srv="127.0.0.1";  # database server
$jab_db_usr="openser";  # database user
$jab_db_pas="***";  # database user's password
$jab_db_db="sip_jab";   # database name
#
function html_die($message)
{
    echo "$message </DIV></BODY></HTML>";
    exit();
}
#
function dbg_msg($message)
{
    # echo "$message";
}
?>
<HTML>
<HEAD>
<TITLE>IM Gateway registration</TITLE>
</HEAD>

<BODY>
<DIV ALIGN="center">
<?php
if(!isset($action) || $action=="" || !isset($sipname) || $sipname=="" || !isset($imtype) || $imtype=="" || ($action=="Subscribe" && (!isset($imname) || $imname=="")))
{
?>
	<b>Subscription page for Instant Messaging gateway</b>
	<br>
	You MUST have a SIP account
	<br><hr size="1" width="60%"><br>
	<TABLE>
	<FORM action="/im/subscribe.php" method="post">
		<TR>
		<TD>SIP username:</TD><TD><INPUT type="text" value="<?php echo $sipname;?>" name="sipname" size="32"></TD>
		</TR>
		<TR>
		<TD>SIP password:</TD><TD><INPUT type="password" name="sippasswd" size="32"></TD>
		</TR>

		<TR>
		<TD COLSPAN="2"><b><i><hr></i></b></TD>
		</TR>
		<TR>
		<TD>My Jabber account:</TD>
		<TD align="right">
		    <INPUT type="submit" name="action" value="Enable">
			&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;
			<INPUT type="submit" name="action" value="Disable">
		</TD>
		</TR>
		<TR>
		<TD COLSPAN="2"><b><i><hr></i></b></TD>
		</TR>

		<TR>
		<TD>IM service:</TD>
		<TD>
			<SELECT name="imtype">
				<OPTION value="aim">AIM </OPTION>
				<OPTION value="icq">ICQ </OPTION>
				<OPTION value="msn">MSN </OPTION>
				<OPTION value="yahoo">Yahoo </OPTION>
			</SELECT>
		</TD>
    		</TR>
		<TR>
		<TD>IM nickname:</TD><TD><INPUT type="text" value="<?php echo $imnick;?>" name="imnick" size="32"></TD>
		</TR>
		<TR>
		<TD>IM account:</TD><TD><INPUT type="text" value="<?php echo $imname;?>" name="imname" size="32"></TD>
		</TR>
		<TR>
		<TD>IM password:</TD><TD><INPUT type="password" name="impasswd" size="32"></TD>
		</TR>

		<TR>
		<TD COLSPAN="2"><b><i><hr></i></b></TD>
		</TR>

		<TR>
		<TD><INPUT type="reset" value="Reset"></TD>
		<TD>
		    <INPUT type="submit" name="action" value="Subscribe">
		    &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;
		    <INPUT type="submit" name="action" value="Unsubscribe">
		</TD>
		</TR>
	</FORM>
	</TABLE>
    <DIV ALIGN="left">
    <br><br>
    <h4>User info</h4>
    - for any operation you MUST provide your username and password
    <br>
    <br>
    <b><i>AIM Gateway subscription</i></b>
    <br>
    - choose 'AIM' as 'IM service'<br>
    - 'IM nickname' is your display name for AIM network<br>
    - 'IM account' is your AIM account name (ex: 'alpha')<br>
    - 'IM password' is the password of your AIM account<br>
    - click on 'Subscribe'<br>
    <b><i>ICQ Gateway subscription</i></b>
    <br>
    - choose 'ICQ' as 'IM service'<br>
    - 'IM nickname' is your display name for ICQ network<br>
    - 'IM account' is your ICQ number (ex: '158251040')<br>
    - 'IM password' is the password of your ICQ account<br>
    - click on 'Subscribe'<br>
    <b><i>MSN Gateway subscription</i></b>
    <br>
    - choose 'MSN' as 'IM service'<br>
    - 'IM nickname' is your display name for MSN network<br>
    - 'IM account' is your MSN account (ex: 'alpha@hotmail.com' or 'alpha@msn.com')<br>
    - 'IM password' is the password of your MSN account<br>
    - click on 'Subscribe'<br>
    <b><i>Yahoo Gateway subscription</i></b>
    <br>
    - choose 'Yahoo' as 'IM service'<br>
    - 'IM nickname' is your display name for Yahoo network<br>
    - 'IM account' is your Yahoo account (ex: 'alpha')<br>
    - 'IM password' is the password of your Yahoo account<br>
    - click on 'Subscribe'<br>
    <br>
    <b><i>IM Gateway unsubscription</i></b>
    <br>
    - choose the 'IM service' from which you want to unsubscribe<br>
    - click on 'Unsubscribe'<br>
    </DIV>
<?php
}
else
{
	# -----
	# AUTHENTICATION - verify username and password
	# -----
	/* ****************************************
	echo "<br><h2>Instant Messaging Gateway</h2><hr size=\"1\" width=\"60%\"><br>";
	$dblink = mysql_connect($sip_db_srv, $sip_db_usr, $sip_db_pas) or
		html_die("Could not connect to SIP database server");
	mysql_select_db($sip_db_db, $dblink) 
		or html_die("Could not select SIP database");
	$query = "SELECT $sip_db_cusr FROM $sip_db_tab WHERE $sip_db_cusr='$sipname' AND $sip_db_cpas='$sippasswd'";
	dbg_msg("$query <BR>");
	$result = mysql_query($query) or html_die("Invalid SQL query");
	if(mysql_num_rows($result) == 0)
		html_die("Invalid SIP username or password");
	mysql_close($dblink);
	***************************************** */
	#
	#------------------------------------------------------
	#
	# -----
	# check if is already registered to Jabber gateway
	# -----
	$sipuri = "sip:".$sipname."@example.org";
	$dblink = mysql_connect($jab_db_srv, $jab_db_usr, $jab_db_pas) or html_die("Could not connect to Jabber database");
	mysql_select_db($jab_db_db, $dblink) or html_die("Could not use Jabber database");
	# ----
	if($action == "Disable")
	{
		$query = "UPDATE jusers SET tyep=1 WHERE sip_id='$sipuri'";
		$result = mysql_query($query, $dblink);
		if(mysql_affected_rows() != 1)
		{
			mysql_close($dblink);
			html_die("<br>Cannot find Jabber ID of '$sipname' in database");
		}
		mysql_close($dblink);
		html_die("<br>Your IM account was updated");
	}
	# ----
	$query = "SELECT jab_id FROM jusers WHERE sip_id='$sipuri'";
	$result = mysql_query($query, $dblink) or html_die("Invalid SQL query");
	if(mysql_num_rows($result) == 0)
	{ // no Jabber account - create one
		$fd = jab_connect($jserver, $jport);
		if(!$fd)
			html_die("Could not connect to Jabber server");
		$buf_recv = fread($fd, 2048);
		while(!$buf_recv)
		{
			usleep(100);
			$buf_recv = fread($fd, 2048);
		}
		$jid1 = stristr($buf_recv, "id='");
		$jid1 = substr($jid1, 4);
		if($jid1)
		{
			$jid2 = strstr($jid1, "'");
			if($jid2)
			{
				$jid = substr($jid1, 0, strlen($jid1)-strlen($jid2));
				dbg_msg("JID: $jid<BR>");
			}
		}
		$jcid = $jcid + 1;
		jab_get_reg($fd, $jcid, $jserver);
		$buf_recv = fread($fd, 2048);
		while(!$buf_recv)
		{
			usleep(100);
			$buf_recv = fread($fd, 2048);
		}
		$jcid = $jcid + 1;
		$new_passwd = "#".$sipname."%";
		jab_set_reg($fd, $jcid, $jserver, $sipname, $new_passwd);
		$buf_recv = fread($fd, 2048);
		while(!$buf_recv)
		{
			usleep(100);
			$buf_recv = fread($fd, 2048);
		}
		if(stristr($buf_recv, " id='$jcid'") && stristr($buf_recv, " type='error'"))
		{
			mysql_close($dblink);
			jab_disconnect($fd);
			html_die("<br>Something bizarre with account '$sipname'");
		}
		# -----
		# Add user in database
		# -----
		$query = "INSERT INTO jusers (jab_id, jab_passwd, sip_id) VALUES ('$sipname', '$new_passwd', '$sipuri')";
		$result = mysql_query($query, $dblink);
		if(mysql_affected_rows() != 1)
		{
			mysql_close($dblink);
			jab_disconnect($fd);
			html_die("<br>Can not insert '$sipname' in database");
		}
		jab_disconnect($fd);
	}
	# -----
	if($action == "Enable")
	{
		$query = "UPDATE jusers SET type=0 WHERE sip_id='$sipuri'";
		$result = mysql_query($query, $dblink);
		if(mysql_affected_rows() != 1)
		{
			mysql_close($dblink);
			html_die("<br>Cannot find Jabber ID of '$sipname' in database");
		}
		mysql_close($dblink);
		html_die("<br>Your IM account was updated");
	}
	# -----
	$query="SELECT juid,jab_id,jab_passwd,type FROM jusers WHERE
	sip_id='$sipuri' and type=0";
	$result = mysql_query($query, $dblink) or html_die("Invalid SQL query");
	if(mysql_num_rows($result) != 1 || (!($row = mysql_fetch_array($result))))
	{
		mysql_close($dblink);
		html_die("<br>You do not have an associated Jabber account or it is
		disabled!<br>Press 'Enable' in order to create a new one or to activate an
		old one.<br>If error persists, please inform the administrator.");
	}

	$juid = $row[0];
	$jab_id = $row[1];
	$jab_passwd = $row[2];
	$jab_type = $row[3];
	dbg_msg("Jabber User ID: $juid<BR>");
	$fd = jab_connect($jserver, $jport);
	if(!$fd)
		html_die("Could not connect to Jabber server");
	$buf_recv = fread($fd, 2048);
	while(!$buf_recv)
	{
		usleep(100);
		$buf_recv = fread($fd, 2048);
	}
	$jid1 = stristr($buf_recv, "id='");
	$jid1 = substr($jid1, 4);
	if($jid1)
	{
		$jid2 = strstr($jid1, "'");
		if($jid2)
		{
			$jid = substr($jid1, 0, strlen($jid1)-strlen($jid2));
			dbg_msg("JID: $jid<BR>");
		}
	}
	$jcid = $jcid + 1;
	jab_get_auth($fd, $jcid, $jab_id);
	$buf_recv = fread($fd, 2048);
	while(!$buf_recv)
	{
		usleep(100);
		$buf_recv = fread($fd, 2048);
	}
	$jcid = $jcid + 1;
	jab_set_auth($fd, $jcid, $jab_id, $jab_passwd);

	$buf_recv = fread($fd, 2048);
	while(!$buf_recv)
	{
		usleep(100);
		$buf_recv = fread($fd, 2048);
	}
	if(stristr($buf_recv, " id='$jcid'") && stristr($buf_recv, " type='error'"))
	{
		jab_disconnect($fd);
		html_die("<br>Wrong username or password at Jabber authentication");
	}
	# -----
	# browse agents
	# -----
	$jcid = $jcid + 1;
	jab_get_agents($fd, $jcid, $jserver);
	$buf_agents = fread($fd, 4096);
	while(!$buf_agents)
	{
		usleep(100);
		$buf_agents = fread($fd, 4096);
	}
	# dbg_msg("\n<!-- $buf_agents -->");
	# -----
	$imag1 = stristr($buf_agents, "<agent jid='".$imtype.".");
	$imag1 = substr($imag1, strlen("<agent jid='"));
	if($imag1)
	{
		$imag2 = strstr($imag1, "'");
		if($imag2)
		{
			$imag = substr($imag1, 0, strlen($imag1)-strlen($imag2));
			dbg_msg("IMAgent: $imag<BR>");
		}
	}
	# -----
	if(isset($imag))
	{
		if($action == "Subscribe" && isset($imname) && $imname != "")
		{
			echo "<h3><i>IM ($imtype) subscription</i></h3><BR>";
			# -----
			# unsubscribe the previous IM account (if exists)
			# -----
			$jcid = $jcid + 1;
			jab_set_unreg($fd, $jcid, $icqag);
			$buf_recv = fread($fd, 2048);
			while(!$buf_recv)
			{
				usleep(100);
				$buf_recv = fread($fd, 2048);
			}
			sleep(1);
			# -----
			# subscription
			# -----
			$jcid = $jcid + 1;
			jab_get_reg($fd, $jcid, $imag);
			$buf_recv = fread($fd, 2048);
			while(!$buf_recv)
			{
				usleep(100);
				$buf_recv = fread($fd, 2048);
			}
			$imkey1 = stristr($buf_recv, "<key>");
			$imkey1 = substr($imkey1, 5);
			if($imkey1)
			{
				$imkey2 = strstr($imkey1, "</key>");
				if($imkey2)
				{
					$imkey = substr($imkey1, 0, strlen($imkey1)-strlen($imkey2));
					dbg_msg("IM key: $imkey<BR>");
				}
			}
			if(!isset($imkey))
			{
				jab_disconnect($fd);
				mysql_close($dblink);
				html_die("<br>Session key for IM ($imtype) Transport not found");
			}
			$jcid = $jcid + 1;
			jab_set_regk($fd, $jcid, $imag, $imname, $impasswd, $imnick, $imkey);
			$buf_recv = fread($fd, 2048);
			while(!$buf_recv)
			{
				usleep(100);
				$buf_recv = fread($fd, 2048);
			}
			if(stristr($buf_recv, " id='$jcid'") && stristr($buf_recv, " type='error'"))
			{
				$err1 = stristr($buf_recv, "<error ");
				$err1 = substr($err1, 7);
				$err1 = strstr($err1, ">");
				$err1 = substr($err1, 1);
				if($err1)
				{
					$err2 = strstr($err1, "</error>");
					if($err2)
						$err = substr($err1, 0, strlen($err1)-strlen($err2));
				}
				jab_disconnect($fd);
				mysql_close($dblink);
				html_die("<br><b>Error registering your IM ($imtype) account: <i>$err</i></BODY></HTML>");
			}
			jab_send_presence($fd, $imag."/registered", "subscribed");
			# -----
			# Update database
			$query = "SELECT ".$imtype."_id FROM ".$imtype." WHERE juid='$juid'";
			$result = mysql_query($query, $dblink) or html_die("Invalid SQL query");
			if(mysql_num_rows($result) == 0)
			{ # INSERT
				$query = "INSERT INTO ".$imtype." (juid, ".$imtype."_id, ".$imtype."_passwd, ".$imtype."_nick) VALUES ('$juid', '$imname', '$impasswd', '$imnick')";
				dbg_msg("$query <br>");
				$result = mysql_query($query, $dblink);
				if(mysql_affected_rows() != 1)
				{
					echo "<br><b>Can not register '$sipname'/'$imname'</b></br>";
				}
				else
				{
					echo "<b>Your IM ($imtype) account was successfully registered</b><BR>";
				}
			}
			else
			{ # UPDATE
				$query = "UPDATE ".$imtype." SET ".$imtype."_id='$imname', ".$imtype."_passwd='$impasswd', ".$imtype."_nick='$imnick' WHERE juid='$juid'";
				dbg_msg("$query <br>");
				$result = mysql_query($query, $dblink);
				if(!$result)
				{
					echo "<br>Can not update '$sipname'/'$imname'<br>";
				}
				else
				{
				        if(mysql_affected_rows() == 1)
					{
						echo "Your IM ($imtype) account was successfully updated<BR>";
				        }
				        else
				        {
						echo "No modification in your IM ($imtype) account<BR>";
				        }
				}
			}
		}
		else
		{
			echo "<h3><i>IM ($imtype) unsubscription</i></h3><BR>";
			# -----
			# unsubscribe the IM account
			# -----
			$jcid = $jcid + 1;
			jab_set_unreg($fd, $jcid, $icqag);
			$buf_recv = fread($fd, 2048);
			while(!$buf_recv)
			{
				usleep(100);
				$buf_recv = fread($fd, 2048);
			}
			sleep(1);
			$query = "DELETE FROM ".$imtype." WHERE juid='$juid'";
			dbg_msg("$query <br>");
			$result = mysql_query($query, $dblink);
			if(!$result)
			{
				echo "<br>Can not remove IM ($imtype) information from database<br>";
			}
			else
			{
				echo "<br>Unsubscription from IM ($imtype) completed<br>";
			}
		}
	}
	sleep(1);
	jab_disconnect($fd);
	mysql_close($dblink);
}
?>
</DIV>

</BODY>
</HTML>
