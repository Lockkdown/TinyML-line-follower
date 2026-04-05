# file: tests/test_dataset_split.py
# purpose: verify path split is a partition with no leakage
# dependencies: dataset_path_utils

import numpy as np

from training.data.dataset_path_utils import split_paths


def test_split_paths_partition_and_disjoint():
    """Train, val, test are disjoint and cover all paths exactly once."""
    paths = [f"/data/img_{i:03d}.jpg" for i in range(100)]
    labels = list(range(100))
    rng = np.random.default_rng(0)
    order = rng.permutation(100)
    paths = [paths[i] for i in order]
    labels = [labels[i] for i in order]

    tr_p, tr_l, v_p, v_l, te_p, te_l = split_paths(
        paths, labels, val_split=0.15, test_split=0.20, seed=42
    )

    assert len(tr_p) + len(v_p) + len(te_p) == 100
    assert set(tr_p) | set(v_p) | set(te_p) == set(paths)
    assert len(set(tr_p) & set(v_p)) == 0
    assert len(set(tr_p) & set(te_p)) == 0
    assert len(set(v_p) & set(te_p)) == 0


def test_split_paths_rejects_duplicates():
    """Duplicate paths in input must raise before split."""
    paths = ["/a/x.jpg", "/a/x.jpg", "/b/y.jpg"]
    labels = [0, 0, 1]
    try:
        split_paths(paths, labels, 0.2, 0.2, seed=1)
    except ValueError as exc:
        assert "Duplicate" in str(exc)
    else:
        raise AssertionError("expected ValueError")
