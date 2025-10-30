-- Requires:
--   ml_mid(ts timestamptz, symbol text, mid numeric)
--   ref_symbols(symbol text primary key, tick_size numeric)
-- label = 1 iff mid(t+20ms_first) - mid(t) >= tick_size; NULL if no future mid
CREATE OR REPLACE VIEW ml_labels_1tick_20ms AS
WITH fut AS (
  SELECT
    m1.ts, m1.symbol, m1.mid AS mid_now, rs.tick_size,
    (
      SELECT m2.mid
      FROM ml_mid m2
      WHERE m2.symbol = m1.symbol
        AND m2.ts >= m1.ts + INTERVAL '20 milliseconds'
      ORDER BY m2.ts
      LIMIT 1
    ) AS mid_fut
  FROM ml_mid m1
  JOIN ref_symbols rs USING (symbol)
)
SELECT
  ts, symbol,
  CASE WHEN mid_fut IS NULL THEN NULL
       WHEN (mid_fut - mid_now) >= tick_size THEN 1
       ELSE 0 END AS y_up_1tick_20ms
FROM fut;