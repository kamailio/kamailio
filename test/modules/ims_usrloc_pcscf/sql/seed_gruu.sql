-- Trimmed GRUU-focused integration seed data (3 contacts only)
-- reg_state: 2 = PCONTACT_REGISTERED

-- T2: IMPU + barred public identities
INSERT INTO pcscf_location (
  domain, aor, host, port, protocol, received, received_port, received_proto,
  reg_state, expires, public_ids, public_ids_barred
) VALUES (
  'ims.local', 'sip:user1@ims.local', '192.168.1.10', 5060, 1,
  '192.168.1.10', 5060, 1,
  2, datetime('2030-06-01 12:00:00'),
  '<sip:user1@ims.local><tel:+15551110001>', '<tel:+15551110001>'
);

-- T3: Contact with GRUU values + temp-GRUU history
INSERT INTO pcscf_location (
  domain, aor, host, port, protocol, received, received_port, received_proto,
  reg_state, expires, public_ids, instance_id, pub_gruu, temp_gruu
) VALUES (
  'ims.local', 'sip:gruu-user@ims.local', '192.168.1.11', 5060, 1,
  '192.168.1.11', 5060, 1,
  2, datetime('2030-06-01 12:00:00'),
  '<sip:gruu-user@ims.local>',
  'urn:uuid:3333-4444', 'sip:gruu-user@ims.local;gr=urn:uuid:3333-4444',
  'sip:new-tgruu@ims.local;gr=urn:uuid:3333-4444'
);

INSERT INTO pcscf_gruu_history (location_id, temp_gruu, created, expires)
VALUES (
  2,
  'sip:old-tgruu@ims.local;gr=urn:uuid:3333-4444',
  datetime('2029-06-01 12:00:00'),
  datetime('2030-06-01 12:00:00')
);

-- T2/T7 path persistence coverage contact
INSERT INTO pcscf_location (
  domain, aor, host, port, protocol, received, received_port, received_proto,
  reg_state, expires, public_ids, path
) VALUES (
  'ims.local', 'sip:path-user@ims.local', '192.168.1.12', 5060, 1,
  '192.168.1.12', 5060, 1,
  2, datetime('2030-06-01 12:00:00'),
  '<sip:path-user@ims.local>',
  '<sip:pcscf.ims.local:4060;lr>'
);
