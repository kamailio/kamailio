-- SQL script for PDT module


USE pdt;

-- create table
CREATE TABLE admin(
	name VARCHAR(20) NOT NULL PRIMARY KEY,
	passwd VARCHAR(20) NOT NULL DEFAULT ""
);

INSERT INTO admin VALUES ("admin", "admin");
