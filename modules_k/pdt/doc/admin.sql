-- SQL script for PDT module


USE pdt;

-- create table
CREATE TABLE admin(
	name VARCHAR(20) NOT NULL PRIMARY KEY,
	passwd VARCHAR(20) NOT NULL DEFAULT ""
);

CREATE TABLE authentication (
	username VARCHAR(50) NOT NULL,
	domain VARCHAR(50) NOT NULL,
	passwd_h VARCHAR(32) NOT NULL
);

INSERT INTO admin VALUES ("admin", "admin");
