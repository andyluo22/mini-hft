-- L1 features: queue imbalance, microprice, spread, OFI
-- Create as views so you can materialize later.

-- Helper: midprice
CREATE OR REPLACE VIEW ml_mid AS
SELECT
  ts, symbol,
  (bid_px + ask_px) / 2.0 AS mid
FROM book_events;

-- Queue imbalance: (bid_sz - ask_sz) / (bid_sz + ask_sz)
CREATE OR REPLACE VIEW ml_imbalance AS
SELECT
  ts, symbol,
  CASE
    WHEN (bid_sz + ask_sz) > 0 THEN (bid_sz - ask_sz) / NULLIF(bid_sz + ask_sz, 0)
    ELSE NULL
  END AS q_imbalance
FROM book_events;

-- Microprice: (bid_px * ask_sz + ask_px * bid_sz) / (bid_sz + ask_sz)
CREATE OR REPLACE VIEW ml_microprice AS
SELECT
  ts, symbol,
  CASE
    WHEN (bid_sz + ask_sz) > 0
      THEN (bid_px * ask_sz + ask_px * bid_sz) / NULLIF(bid_sz + ask_sz, 0)
    ELSE NULL
  END AS microprice
FROM book_events;

-- OFI (Order Flow Imbalance) approximation from L1 changes
-- Using adjacent rows per symbol ordered by ts
CREATE OR REPLACE VIEW ml_ofi AS
WITH x AS (
  SELECT
    ts, symbol,
    bid_px, bid_sz, ask_px, ask_sz,
    LAG(bid_px) OVER (PARTITION BY symbol ORDER BY ts) AS pb,
    LAG(bid_sz) OVER (PARTITION BY symbol ORDER BY ts) AS qb,
    LAG(ask_px) OVER (PARTITION BY symbol ORDER BY ts) AS pa,
    LAG(ask_sz) OVER (PARTITION BY symbol ORDER BY ts) AS qa
  FROM book_events
)
SELECT
  ts, symbol,
  -- One common L1 OFI proxy:
  -- +Δbid_sz when bid price unchanged; +new bid size if bid price increased
  -- -Δask_sz when ask price unchanged; -new ask size if ask price decreased
  (CASE
     WHEN pb IS NULL THEN 0
     WHEN bid_px > pb THEN bid_sz
     WHEN bid_px = pb THEN COALESCE(bid_sz - qb, 0)
     ELSE 0
   END)
  -
  (CASE
     WHEN pa IS NULL THEN 0
     WHEN ask_px < pa THEN ask_sz
     WHEN ask_px = pa THEN COALESCE(ask_sz - qa, 0)
     ELSE 0
   END) AS ofi_l1
FROM x;

-- Spread (ticks or raw)
CREATE OR REPLACE VIEW ml_spread AS
SELECT ts, symbol, (ask_px - bid_px) AS spread
FROM book_events;

-- Final feature view (join on ts,symbol)
CREATE OR REPLACE VIEW ml_features AS
SELECT
  f.ts, f.symbol,
  i.q_imbalance,
  m.microprice,
  s.spread,
  o.ofi_l1,
  mid.mid AS midprice
FROM book_events f
JOIN ml_imbalance  i  USING (ts, symbol)
JOIN ml_microprice m  USING (ts, symbol)
JOIN ml_spread     s  USING (ts, symbol)
JOIN ml_mid        mid USING (ts, symbol);