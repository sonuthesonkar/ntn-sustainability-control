/*------------------------------------------------------------------------*
 * Copyright (c) 2026 Sonu Sonkar.                                        *
 * Licensed under the MIT License.                                        *
 * See the LICENSE file in the project root for full license information. *
 *------------------------------------------------------------------------*/

-- For scaling up in future.
CREATE EXTENSION IF NOT EXISTS timescaledb;

-- Create tables for each KPI.
DO $$ 
DECLARE 
    tbl TEXT;
    kpis TEXT[] := ARRAY['congestion', 'prb_util', 'traffic_load', 'ran_energy', 'carbon_intensity', 'isac_quality', 'mobility_rate'];
BEGIN 
    FOREACH tbl IN ARRAY kpis LOOP 
        EXECUTE format('CREATE TABLE IF NOT EXISTS %I (id SERIAL PRIMARY KEY, value FLOAT, updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP)', tbl);
    END LOOP; 
END $$;

-- Keep latest state.
CREATE TABLE IF NOT EXISTS ntn_state (
  id INT PRIMARY KEY DEFAULT 1,
  ntn_state INT NOT NULL,
  critical_count INT NOT NULL,
  recovery_count INT NOT NULL
);

-- Default initial state, everything's normal.
INSERT INTO ntn_state VALUES (1, 0, 0, 0)
ON CONFLICT DO NOTHING;

-- Maintain history.
CREATE TABLE IF NOT EXISTS history (
  ts TIMESTAMPTZ NOT NULL,
  congestion REAL, 
  prb_util REAL, 
  traffic_load REAL, 
  ran_energy REAL, 
  carbon_intensity REAL, 
  isac_quality REAL, 
  mobility_rate REAL,
  crisis_score REAL NOT NULL,
  ntn_state INT NOT NULL
);

-- Future proofing for scale up.
SELECT create_hypertable('history', 'ts');

-- Only one user/browser can update KPIs, at any time.
CREATE TABLE IF NOT EXISTS controller (
  id INTEGER PRIMARY KEY CHECK (id = 1),
  owner_id TEXT,
  acquired_at TIMESTAMP DEFAULT now()
);

-- Initialize controller.
INSERT INTO controller (id, owner_id)
VALUES (1, NULL)
ON CONFLICT (id) DO NOTHING;
