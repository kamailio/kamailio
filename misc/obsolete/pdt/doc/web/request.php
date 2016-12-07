<?php
session_start();

if(!$domain_name)
{
    include("index.php");
    die;
}    
?>

<html>
<head>
</head>
<body bgcolor=#0099cc>
<br>
<center><h1>Prefix Domain Translation -- UserInterface</h1></center>
<br>

<?php
$database="pdt";
$table="admin";
$input_file = "/tmp/ser_fifo";

#datbase hostname
$host="127.0.0.1";

# database user
$user="root";

# database user password
$pass="";

		
	$authorized="0";
	
	if(!strcasecmp($admin, "Anonymous") || !strcmp($admin,""))
	    $authorized = "0";
	else
	{
	    if(!strcmp($passwd,""))
	    {
		echo "<h2>No password supplied</h2>";
		exit;
	    }
	    
	    $link = mysql_connect($host, $user, $pass)
		or die("Could not connect to mysql");

	    mysql_select_db($database) or die("Could not select database");       
	
	    $query = "SELECT * FROM ".$table." WHERE name=\"".$admin."\" and passwd=\"".$passwd."\"";
	
	    $result = mysql_query($query) or die("Query failed: ".mysql_error());	
	
	    $num_rows = mysql_num_rows($result);
	    if($num_rows>0)
		$authorized="1";
	    else
    		echo "<h2>Authentication failed. No right to register a new domain.</h2>";	
	
	    mysql_free_result($result);

	    mysql_close($link);
	    
	}    	
	    
	$response_file = "rf".session_id();
	$reply = "/tmp/".$response_file;
	@system("mkfifo -m 666 ".$reply);
	
	$new_line ="\n";	
	$fifo_command = ":get_domainprefix:";	
	$fifo_command = $fifo_command.$response_file.$new_line;
	$fifo_command = $fifo_command.$domain_name;
	if($domain_port)
	    $fifo_command = $fifo_command.":".$domain_port;
	$fifo_command = $fifo_command.$new_line;
	$fifo_command = $fifo_command.$authorized.$new_line.$new_line;

	$fp = fopen($input_file, "w");
	if(!$fp)
	{
	    echo "Cannot open fifo<br>";
	    exit;
	}
	    
	if( fwrite($fp, $fifo_command) == -1)
	{
	    @unlink($reply);
	    @fclose($fp);
	    echo "fifo writing error<br>";
	    exit;
	}
	fclose($fp);

	$fr = fopen($reply, "r");
	if(!$fr)
	{
	    @unlink($reply);
	    echo "Cannot open reply file";
	    exit;
	}
	$count = 1000;
	$str = fread($fr, $count);
	if(!$str)
	{
	    @fclose($fr);
	    @unlink($reply);
	    echo "response fifo reading error";
	    exit;
	}
	$domain_code = "";
	list($return_code, $description) = explode("|", $str);
	if(!strcmp("$return_code","400 "))	
	{
	    echo "<h2>ERROR: Cannot read from fifo. Try again.</h2>";
	    exit;
	}
	list($garbage1, $garbage2, $domain_code) = explode("=", $str);
	list($domain_code, $garbage3) = explode("\n", $domain_code);
	fclose($fr);
	@unlink("/tmp/".$response_file);
	
	
	if(!strcmp("$return_code","204 "))
	{
	    $domain_code = "registration failed"; 
	}
	
	if(!strcmp("$return_code","203 "))
	{
	    $domain_code = "not registered";
	}
		
?>

<table border=1 align="center" cellspacing="40" cellpadding="10">
<tr>
	<td>
		<font size=4>Domain Name</font>
	</td>
	<td>
		<b><font size=5><?php echo $domain_name;?></font></b>
	</td>
</tr>
<tr>
	<td>
		<font size=4>Domain Code</font>
	</td>
	<td>
		<b><font size=5><?php echo $domain_code;?></font></b>
	</td>
</tr>
</table>

</body>
</html>
