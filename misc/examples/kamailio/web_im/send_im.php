<html>
<!-- $Id$ -->
<header>
<title>
Send IM Status
</title>
</header>

<body>
<h1>
Send IM Status
</h1>

<?php

/* config values */
$web_contact="sip:daemon@mydomain.net";
$fifo="/tmp/kamailio_fifo";
$signature="web_im_0.1.0";

/* open reply fifo */
$myfilename="webfifo_".rand();
$mypath="/tmp/".$myfilename;
$outbound_proxy=".";


$sip_address = $_POST['sip_address'];
$instant_message = $_POST['instant_message'];


echo "Initiating your request...<p>";

/* open fifo now */
$fifo_handle=fopen( $fifo, "w" );
if (!$fifo_handle) {
    exit ("Sorry -- cannot open fifo: ".$fifo);
}

/* construct FIFO command */
$fifo_cmd=":t_uac_dlg:".$myfilename."\n".
    "MESSAGE\n".
    $sip_address."\n".
	$outbound_proxy."\n".
    ".\n".
    "\"From: ".$web_contact."\r\n".
	"To: ".$sip_address."\r\n".
	"p-version: ".$signature."\r\n".
    "Contact: ".$web_contact."\r\n".
    "Content-Type: text/plain; charset=UTF-8\r\n".
    "\"\n".
    "\"".$instant_message."\"".
  	"\n\n";  

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
echo "<p>Thank you for using IM<p>";

?>

</body>
</html>

