-- Binary label: did the mid move up by >= 1 tick within horizon?
-- For Day 2, we ignore ticks and just compare delta > 0 (you can quantize later).
-- Uses "first event at or after ts + horizon" per symbol.

-- Parameterize horizons using MATERIALIZED views later. For now, 20 ms example.
CREATE OR REPLACE VIEW ml_labels_20ms AS
WITH fut AS (
  SELECT m1.ts, m1.symbol,
         (
           SELECT m2.mid
           FROM ml_mid m2
           WHERE m2.symbol = m1.symbol
             AND m2.ts >= m1.ts + INTERVAL '20 milliseconds'
           ORDER BY m2.ts
           LIMIT 1
         ) AS mid_fut
  FROM ml_mid m1
)
SELECT
  m.ts,
  m.symbol,
  CASE WHEN fut.mid_fut IS NULL THEN NULL
       WHEN fut.mid_fut > m.mid THEN 1
       ELSE 0
  END AS y_up_20ms
FROM ml_mid m
JOIN fut ON (fut.ts = m.ts AND fut.symbol = m.symbol);