<html>
<!-- $Id$ -->
<header>
<title>
Click-To-Dial
</title>
</header>

<body>
<h1>
Click-To-Dial
</h1>

<?php

/* config values */
$web_contact="sip:daemon@mydomain.net";
$fifo="/tmp/kamailio_fifo";
$signature="web_dialer_0.1.0";

/* open reply fifo */
$myfilename="webfifo_".rand();
$mypath="/tmp/".$myfilename;
$outbound_proxy=".";


$caller = $_POST['caller'];
$callee = $_POST['callee'];

echo "Initiating your request...<p>";
/* open fifo now */
$fifo_handle=fopen( $fifo, "w" );
if (!$fifo_handle) {
    exit ("Sorry -- cannot open fifo: ".$fifo);
}

/* construct FIFO command */

$fifo_cmd=":t_uac_dlg:".$myfilename."\n".
    "REFER\n".
     $caller."\n".
	 $outbound_proxy."\n".
	 ".\n".
     "\"From: ".$web_contact."\r\n".
     "To: ".$callee."\r\n".
     "p-version: ".$signature."\r\n".
    "Contact: ".$web_contact."\r\n".
    "Referred-By: ".$web_contact."\r\n".
	"Refer-To: ".$callee."\r\n".
	"\"\n\n";
	
    
/* create fifo for replies */
system("mkfifo -m 666 ".$mypath );

/* write fifo command */
if (fwrite( $fifo_handle, $fifo_cmd)==-1) {
    unlink($mypath);
    fclose($fifo_handle);
    exit("Sorry -- fifo writing error");
}
fclose($fifo_handle);

/* read output now */
if (readfile( $mypath )==-1) {
    unlink($mypath);
	exit("Sorry -- fifo reading error");
}
unlink($mypath);
echo "<p>Thank you for using click-to-dial<p>";

?>

</body>
</html>

