-- SQL script for PDT module

DROP DATABASE IF EXISTS pdt;

CREATE DATABASE pdt;

USE pdt;

-- create table
CREATE TABLE domains(
	code INTEGER NOT NULL PRIMARY KEY,
	domain VARCHAR(255) NOT NULL DEFAULT ""
);

