CREATE TABLE nds_trusted_domains (
    id SERIAL PRIMARY KEY NOT NULL,
    trusted_domain VARCHAR(83) DEFAULT '' NOT NULL
);

INSERT INTO version (table_name, table_version) values ('nds_trusted_domains','1');

CREATE TABLE s_cscf (
    id SERIAL PRIMARY KEY NOT NULL,
    name VARCHAR(83) DEFAULT '' NOT NULL,
    s_cscf_uri VARCHAR(83) DEFAULT '' NOT NULL
);

INSERT INTO version (table_name, table_version) values ('s_cscf','1');

CREATE TABLE s_cscf_capabilities (
    id SERIAL PRIMARY KEY NOT NULL,
    id_s_cscf INTEGER DEFAULT 0 NOT NULL,
    capability INTEGER DEFAULT 0 NOT NULL
);

CREATE INDEX s_cscf_capabilities_idx_capability ON s_cscf_capabilities (capability);
CREATE INDEX s_cscf_capabilities_idx_id_s_cscf ON s_cscf_capabilities (id_s_cscf);

INSERT INTO version (table_name, table_version) values ('s_cscf_capabilities','1');
