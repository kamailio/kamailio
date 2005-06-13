<?php
session_start();
?>
<html>
<head>
</head>
<body bgcolor=#0099cc>
<br>
<center>
<h1>Prefix Domain Translation -- User Interface</h1>
</center>
<br>
<?php
#    echo session_id();
?>


<form action="request.php">
<table border=1 align="center" cellspacing="5" cellpadding="20" bgcolor="black">
<tr>
	<td bgcolor="#0077cc">
	        <font size="4">User</font>
	</td>
	<td bgcolor="#0077cc">
		<input type="text" name="admin" value="Anonymous">
	</td>
</tr>
<tr>
	<td bgcolor="#0077cc">
		<font size="4">Password</font>
	</td>
	<td bgcolor="#0077cc">
		<input type="password" name="passwd">
	</td>
</tr>
<tr>
	<td bgcolor="#0077cc">
		<font size="4">Domain Name</font>
	</td>
	<td bgcolor="#0077cc">
		<input type="text" name="domain_name">
	</td>
</tr>
<tr>
	<td bgcolor="#0077cc">
		<font size="4">Domain Port</font>
	</td>
	<td bgcolor="#0077cc">
		<input type="text" name="domain_port">
	</td>
</tr>
<tr>
	<td bgcolor="#0077cc">
		<input type="submit" value="Submit">
	</td>
	<td bgcolor="#0077cc">
		<input type="reset" value="Cancel">
	</td>
</tr>
</table>
</form>
</body>
</html>