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
$fifo_cmd=":t_uac:".$myfilename."\n".
    "REFER\n".$caller."\n".
    "p-version: ".$signature."\n".
    "Contact: ".$web_contact."\n".
    "Referred-By: ".$web_contact."\n".
	"Refer-To: ".$callee."\n".
    "\n". /* EoHeader */
    ".\n\n"; /* EoFifoRequest */

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

