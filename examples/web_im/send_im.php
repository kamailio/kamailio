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
$web_contact="sip:daemon@iptel.org";
$fifo="/tmp/ser_fifo";
$signature="web_test_0.0.0";

/* open reply fifo */
$myfilename="webfifo_".rand();
$mypath="/tmp/".$myfilename;

echo "Initiating your request...<p>";

/* open fifo now */
$fifo_handle=fopen( $fifo, "w" );
if (!$fifo_handle) {
    exit ("Sorry -- cannot open fifo: ".$fifo);
}

/* construct FIFO command */
$fifo_cmd=":t_uac_dlg:".$myfilename."\n".
    "MESSAGE\n".$sip_address."\n.\n".
	"From: sip:sender@foo.bar\n".
	"To: ".$sip_address."\n".
    "p-version: ".$signature."\n".
    "Contact: ".$web_contact."\n".
    "Content-Type: text/plain; charset=UTF-8\n.\n".
    $instant_message."\n.\n";

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

