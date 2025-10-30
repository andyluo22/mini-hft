import pandas as pd
from research.features import compute_features

def test_shapes_and_nans():
    df = pd.DataFrame({
        "ts":[0,1,2,3],
        "symbol":["X","X","X","X"],
        "bid_px":[99,99,100,100],
        "ask_px":[101,101,101,102],
        "bid_sz":[10,11,12,13],
        "ask_sz":[9,8,8,7],
    })
    feats = compute_features(df.rename(columns={"ts":"ts_ns"}))
    assert {"q_imbalance","microprice","spread","ofi_l1","mid"}.issubset(feats.columns)
    assert feats.isna().sum().sum() == 0