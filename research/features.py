# research/features.py
import argparse
import pandas as pd
from pathlib import Path

def load_book(csv_path: Path) -> pd.DataFrame:
    df = pd.read_csv(csv_path)
    # Expect columns: ts, symbol, bid_px, bid_sz, ask_px, ask_sz
    # ts can be ISO8601 or integer ns/us/ms. Normalize to ns int.
    if pd.api.types.is_datetime64_any_dtype(df["ts"]):
        ts_ns = df["ts"].view("int64") # if ts already Pandas datetime64
    else:
        # try parse; if numeric assume already epoch-*; guess units by magnitude
        try: # if string use pd to parse it to df ts
            parsed = pd.to_datetime(df["ts"], utc=True, errors="coerce")
            if parsed.notna().any():
                ts_ns = parsed.view("int64")
            else: # numeric need to guess units
                ts = pd.to_numeric(df["ts"], errors="coerce")
                # Heuristic: 1e18 ~ ns, 1e15 ~ us, 1e12 ~ ms, 1e9 ~ s
                s = ts.dropna().astype("int64")
                scale = 1
                if s.max() < 1e12: scale = int(1e9)     # seconds → ns
                elif s.max() < 1e13: scale = int(1e6)   # ms → ns
                elif s.max() < 1e16: scale = int(1e3)   # us → ns
                else: scale = 1                          # already ns
                ts_ns = (ts * scale).astype("int64")
        except Exception:
            raise ValueError("Unrecognized ts format; provide ISO8601 or epoch*")
    df["ts_ns"] = ts_ns
    return (df
            .drop(columns=["ts"], errors="ignore")
            .sort_values(["symbol", "ts_ns"])
            .reset_index(drop=True))

def compute_features(book: pd.DataFrame) -> pd.DataFrame:
    b = book.copy()
    # Basic guards
    for c in ["bid_px","ask_px","bid_sz","ask_sz"]:
        if c not in b.columns: raise ValueError(f"missing column: {c}")

    # Mid, spread
    b["mid"] = (b["bid_px"] + b["ask_px"]) / 2.0
    b["spread"] = (b["ask_px"] - b["bid_px"])

    # Queue imbalance
    total_sz = (b["bid_sz"] + b["ask_sz"]).replace(0, pd.NA)
    b["q_imbalance"] = (b["bid_sz"] - b["ask_sz"]) / total_sz

    # Microprice
    b["microprice"] = (b["bid_px"] * b["ask_sz"] + b["ask_px"] * b["bid_sz"]) / total_sz

    # OFI (L1 proxy) via lagged rows per symbol
    b[["pb","qb","pa","qa"]] = (
        b.groupby("symbol", sort=False)[["bid_px","bid_sz","ask_px","ask_sz"]]
         .shift(1).rename(columns={"bid_px":"pb","bid_sz":"qb","ask_px":"pa","ask_sz":"qa"})
    )
    def ofi_row(r):
        inc_bid = r["bid_px"] > r["pb"] if pd.notna(r["pb"]) else False
        eq_bid  = r["bid_px"] == r["pb"] if pd.notna(r["pb"]) else False
        inc_ask = r["ask_px"] < r["pa"] if pd.notna(r["pa"]) else False
        eq_ask  = r["ask_px"] == r["pa"] if pd.notna(r["pa"]) else False
        term_bid = (r["bid_sz"] if inc_bid else (r["bid_sz"] - r["qb"] if eq_bid and pd.notna(r["qb"]) else 0.0))
        term_ask = (r["ask_sz"] if inc_ask else (r["ask_sz"] - r["qa"] if eq_ask and pd.notna(r["qa"]) else 0.0))
        return term_bid - term_ask
    b["ofi_l1"] = b.apply(ofi_row, axis=1)

    out = b[["ts_ns","symbol","q_imbalance","microprice","spread","ofi_l1","mid"]].copy()
    # NaN guards: drop rows with any NaN in feature columns
    out = out.dropna().reset_index(drop=True)
    return out

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--book", type=Path, required=True, help="CSV with L1 book snapshots")
    ap.add_argument("--out",  type=Path, required=True, help="Output .parquet or .csv")
    args = ap.parse_args()

    book = load_book(args.book)
    feats = compute_features(book)

    args.out.parent.mkdir(parents=True, exist_ok=True)
    if args.out.suffix.lower() == ".parquet":
      feats.to_parquet(args.out, index=False)
    else:
      feats.to_csv(args.out, index=False)

    print(f"[features] rows={len(feats)} cols={list(feats.columns)} → {args.out}")

if __name__ == "__main__":
    main()
