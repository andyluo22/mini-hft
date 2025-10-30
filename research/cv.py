from __future__ import annotations
import numpy as np, pandas as pd
from typing import Iterator, Tuple

def purged_kfold_indices(ts: pd.Series, horizon_ms: int, n_splits: int = 5, embargo_ms: int = 0
) -> Iterator[Tuple[np.ndarray, np.ndarray]]:
    # normalize to int ns
    if np.issubdtype(ts.dtype, np.datetime64):
        ts_ns = ts.view("int64")
    else:
        ts_ns = pd.to_numeric(ts, errors="coerce").astype("int64")
    end_ns = ts_ns + int(horizon_ms)*1_000_000

    order = np.argsort(ts_ns.values, kind="mergesort")  # stable
    n = len(ts)
    fold_sizes = np.full(n_splits, n//n_splits, dtype=int); fold_sizes[:n%n_splits]+=1
    bounds = np.cumsum(fold_sizes)

    prev=0
    for k in range(n_splits):
        te = order[prev:bounds[k]]; prev = bounds[k]
        te_start = ts_ns.iloc[te].min()
        te_end_wo = ts_ns.iloc[te].max()
        te_end = te_end_wo + int(embargo_ms)*1_000_000

        overlaps = (ts_ns.values <= te_end_wo) & (end_ns.values >= te_start)
        embargo  = (ts_ns.values >  te_end_wo) & (ts_ns.values <= te_end)
        tr_mask = ~(overlaps | embargo)
        yield np.arange(n)[tr_mask], te