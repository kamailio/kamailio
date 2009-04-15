
-- SQL script for prefix route module

USE ser;

-- create the table
CREATE TABLE prefix_route (
    prefix  VARCHAR(64) NOT NULL DEFAULT "",
    route   VARCHAR(64) NOT NULL DEFAULT "",
    comment VARCHAR(64) NOT NULL DEFAULT ""
);
