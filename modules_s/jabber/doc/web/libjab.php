<?php

function jab_connect($server, $port)
{
	global $errfile;
	if(!isset($errfile))
	    $errfile = "/tmp/php_error.log";
	$fd = fsockopen($server, $port, $errno, $errstr, 30);
	if(!$fd)
	{
		$errmsg = "Error: $errno - $errstr\n";
		error_log($errmsg, 3, $errfile);
		return FALSE;
	}
	$fdp = socket_set_blocking($fd, 0);
	$stream = "<stream:stream to='$server' xmlns='jabber:client' xmlns:stream='http://etherx.jabber.org/streams'>";
	fputs ($fd,$stream);
	return $fd;
}

function jab_disconnect($fd)
{
	$stream = "</stream:stream>";
	fputs ($fd,$stream);
	fclose($fd);
}

function jab_get_reg($fd, $id, $server)
{
	$str = "<iq id='$id' to='$server' type='get'><query xmlns='jabber:iq:register'/></iq>";
	fputs ($fd,$str);
}

function jab_set_reg($fd, $id, $server, $username, $password)
{
	$str = "<iq id='$id' to='$server' type='set'><query xmlns='jabber:iq:register'><username>$username</username><password>$password</password></query></iq>";
	fputs($fd, $str);

}

function jab_set_regk($fd, $id, $server, $username, $password, $nick, $key)
{
	$str = "<iq id='$id' to='$server' type='set'><query xmlns='jabber:iq:register'><username>$username</username><password>$password</password><nick>$nick</nick><key>$key</key></query></iq>";
	fputs($fd, $str);
}

function jab_set_unreg($fd, $id, $server)
{
	$str = "<iq id='$id' to='$server' type='set'><query xmlns='jabber:iq:register'><remove/></query></iq>";
	fputs($fd, $str);
}

function jab_get_agents($fd, $id, $server)
{
	$str = "<iq id='j86' to='$server' type='get'><query xmlns='jabber:iq:agents'/></iq>";
	fputs($fd, $str);
}

function jab_get_auth($fd, $id, $user)
{
	$str = "<iq id='$id' type='get'><query xmlns='jabber:iq:auth'><username>$user</username></query></iq>";
	fputs($fd, $str);
}

function jab_set_auth($fd, $id, $user, $passwd)
{
	$str = "<iq id='$id' type='set'><query xmlns='jabber:iq:auth'><username>$user</username><resource>webjb</resource><password>$passwd</password></query></iq>";
	fputs($fd, $str);
}

function jab_send_presence($fd, $to, $presence)
{
    $str = "<presence to='$to' type='$presence'/>";
    fputs($fd, $str);
    
}

?>