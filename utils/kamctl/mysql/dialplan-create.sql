INSERT INTO version (table_name, table_version) values ('dialplan','2');
CREATE TABLE `dialplan` (
    `id` INT(10) UNSIGNED AUTO_INCREMENT PRIMARY KEY NOT NULL,
    `dpid` INT(11) NOT NULL,
    `pr` INT(11) NOT NULL,
    `match_op` INT(11) NOT NULL,
    `match_exp` VARCHAR(64) NOT NULL,
    `match_len` INT(11) NOT NULL,
    `subst_exp` VARCHAR(64) NOT NULL,
    `repl_exp` VARCHAR(64) NOT NULL,
    `attrs` VARCHAR(64) NOT NULL
);

