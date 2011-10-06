#!/usr/bin/php -q
<?php

// This script upgrades lcr tables from sr 3.1 to sr 3.2

$db_host = "localhost";
$db_name = "ser";
$db_user = "rwuser";
$db_pass = "password";

function db_connect($db_name) {
  global $db_host, $db_user, $db_pass, $db_conn;
  $db_conn = @mysql_connect($db_host, $db_user, $db_pass)
	  or die("Failed to connect to database '$db_name': " .
		 mysql_error() . "!\n" );
  if (!mysql_select_db($db_name)) {
    mysql_close($db_conn);
    die("Unknown database: '$db_name'!\n");
  }
}

function db_disconnect() {
  global $db_conn;
  mysql_close($db_conn) or
    die ("Failed to disconnect from database!\n");
}
  
function upgrade_lcr_gw_table() {

  // upgrade 'ser/lcr_gw' table

  global $db_name;

  db_connect($db_name);

  $result = mysql_query("SELECT table_version FROM version WHERE table_name='lcr_rule'")
    or die("Failed to select from 'ser/version' table!\n");
  if (mysql_num_rows($result) == 2) {
    db_disconnect();
    echo "Nothing to do for 'ser/lcr_gw' table\n";
    return;
  }

  mysql_query("ALTER TABLE lcr_gw CHANGE COLUMN tag prefix VARCHAR(16) DEFAULT NULL")
    or die("Failed to alter 'ser/lcr_gw' table!\n");
  mysql_query("ALTER TABLE lcr_gw ADD COLUMN tag VARCHAR(64) DEFAULT NULL AFTER prefix")
    or die("Failed to alter 'ser/lcr_gw' table!\n");
  mysql_query("ALTER TABLE lcr_gw DROP INDEX lcr_id_ip_addr_port_hostname_idx")
    or die ("Failed to drop 'ser/lcr_gw' index\n");
  mysql_query("ALTER TABLE lcr_gw ADD INDEX lcr_id_idx (lcr_id)")
    or die ("Failed to add 'ser/lcr_gw' index\n");
  mysql_query("UPDATE version SET table_version=2 WHERE table_name='lcr_gw'")
    or die ("Failed to update into 'ser/version' table\n");
  db_disconnect();

  echo "Table 'ser/lcr_gw' upgraded\n";
}

echo "Starting upgrade ...\n";

upgrade_lcr_gw_table();

echo "Upgrade done.\n";

?>
