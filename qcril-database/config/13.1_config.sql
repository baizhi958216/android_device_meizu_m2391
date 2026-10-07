-- SPDX-License-Identifier: Apache-2.0
-- Disable only that startup buffering; preserve carrier tables and user values.
CREATE TABLE IF NOT EXISTS qcril_properties_table (property TEXT PRIMARY KEY NOT NULL, def_val TEXT, value TEXT);
INSERT OR REPLACE INTO qcril_properties_table(property, def_val) VALUES('qcrildb_version',13.1);
UPDATE qcril_properties_table SET def_val="0" WHERE property="persist.vendor.radio.poweron_opt";
