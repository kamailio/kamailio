#!/usr/bin/php4 -q
<?php
// $Id$
// fifo_server.php - fifo/internet relay


/*
 * Copyright (C) 2004 Juha Heinanen
 *
 * This file is part of ser, a free SIP server.
 *
 * ser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * For a license to use the ser software under conditions
 * other than those described here, or to purchase support for this
 * software, please contact iptel.org by e-mail at the following addresses:
 *    info@iptel.org
 *
 * ser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * History:
 * -------
 * 2004-06-11: fifo_server.php introduced as interim solution
 *             until ser fifo natively supports tcp/ip access
 */

error_reporting (E_ALL);

require("/etc/ser/fifo_server.cfg");

/* Check if fifo server is needed */
if ($fifo_server_address == NULL) {
  return;
}

/* Allow the script to hang around waiting for connections. */
set_time_limit (0);

/* Turn on implicit output flushing so we see what we're getting
 * as it comes in. */
ob_implicit_flush ();

$fifo_clients = "/etc/ser/fifo_server.clients";

global $mtime, $clients;
$mtime = 0;

function fifo_allow($fifo_clients, $addr) {
  global $mtime, $clients;
  $long_addr = ip2long($addr);
  if (!file_exists($fifo_clients)) {
    echo "fifo_server.clients file does not exist!\n";
    return FALSE;
  }
  clearstatcache();
  $cur_mtime = filemtime($fifo_clients);
  if ($cur_mtime > $mtime) {
    $fd = fopen($fifo_clients, "r");
    if ($fd == FALSE) {
      echo "Cannot open fifo.clients file!\n";
      return FALSE;
    }
    $clients = array();
    while (!feof ($fd)) {
      $client = ip2long(fgets($fd, 4096));
      if ($client != -1) {
	$clients[] = $client;
      }
    }
    fclose ($fd);
    $mtime = $cur_mtime;
  }
  return in_array($long_addr, $clients, TRUE);
}

if (($sock = socket_create (AF_INET, SOCK_STREAM, 0)) < 0) {
    echo "socket_create() failed: " . socket_strerror ($sock) . "\n";
    return;
}

if (($ret = socket_bind($sock, $fifo_server_address, $fifo_server_port)) < 0) {
  echo "socket_bind() failed: " . socket_strerror ($ret) . "\n";
  socket_close($sock);
  return;
}

if (($ret = socket_listen ($sock, 5)) < 0) {
  echo "socket_listen() failed: " . socket_strerror ($ret) . "\n";
  socket_close($sock);
  return;
}

do {
  if (($msgsock = socket_accept($sock)) < 0) {
    echo "socket_accept() failed: ".socket_strerror($msgsock)."\n";
    socket_close($msgsock);
    continue;
  }

  socket_getpeername($msgsock, $addr);

  if (!fifo_allow($fifo_clients, $addr)) {
    $msg = "403 Forbidden\n";
    socket_write($msgsock, $msg, strlen($msg));
    socket_shutdown($msgsock);
    socket_close($msgsock);
    continue;
  }
      
  if (FALSE === ($fifo_cmd = socket_read ($msgsock, 8192, PHP_BINARY_READ))) {
    echo "socket_read() failed: ".socket_strerror(socket_last_error($msgsock))."\n";
    socket_shutdown($msgsock);
    socket_close($msgsock);
    continue;
  }

  $fifo_reply_file_name = "ser_fifo_reply_".rand();
  $fifo_reply_file = "/tmp/".$fifo_reply_file_name;

  $fifo_cmd = str_replace("REPLY_FILE_NAME", $fifo_reply_file_name, $fifo_cmd);
  
  /* add command separator */
  $fifo_cmd=$fifo_cmd."\n";
  
  $fifo_handle=fopen( "/tmp/ser_fifo", "w" );
  if (!$fifo_handle) {
    $msg = "sorry -- cannot open write fifo";
    socket_write($msgsock, $msg, strlen($msg));
    socket_shutdown($msgsock);
    socket_close($msgsock);
    continue;
  }
  
  /* create fifo for replies */
  @system("mkfifo -m 666 ".$fifo_reply_file);

  /* write fifo command */
  if (fwrite( $fifo_handle, $fifo_cmd) == -1) {
    @unlink($fifo_reply_file);
    @fclose($fifo_handle);
    $msg = "sorry -- fifo writing error";
    socket_write($msgsock, $msg, strlen($msg));
    socket_shutdown($msgsock);
    socket_close($msgsock);
    continue;
  }

  @fclose($fifo_handle);
  
  /* read output now */
  @$fp = fopen($fifo_reply_file, "r");
  if (!$fp) {
    @unlink($fifo_reply_file);
    $msg = "sorry -- reply fifo opening error";
    socket_write($msgsock, $msg, strlen($msg));
    socket_shutdown($msgsock);
    socket_close($msgsock);
    continue;
  }
  $status = fgetS($fp, 256);
  
  if (!$status) {
    @unlink($fifo_reply_file);
    $msg = "sorry -- reply fifo reading error";
    socket_write($msgsock, $msg, strlen($msg));
    socket_shutdown($msgsock);
    socket_close($msgsock);
    continue;
  }
  
  socket_write($msgsock, $status, strlen($status));
  
  $rest = fread($fp, 8192);
  @unlink($fifo_reply_file);

  socket_write($msgsock, $rest, strlen($rest));

  socket_close ($msgsock);

} while (true);

socket_close ($sock);

?>
