CREATE TABLE rtpproxy_attrs (
    id VARCHAR(256) NOT NULL,
    name VARCHAR(32) NOT NULL,
    value VARCHAR(255),
    type INT NOT NULL DEFAULT '0',
    flags INT NOT NULL DEFAULT '0',
    
    PRIMARY KEY (id,name)
);

