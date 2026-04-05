# file: training/data/dataset_path_utils.py
# purpose: collect image paths and split train/val/test with zero overlap
# dependencies: numpy, pathlib

from pathlib import Path

import numpy as np

CLASS_NAMES: list[str] = ["forward", "left", "right", "nothing"]


def collect_paths_and_labels(data_dir: str) -> tuple[list[str], list[int]]:
    """Collect sorted paths per class with matching integer labels."""
    all_paths: list[str] = []
    all_labels: list[int] = []
    for label_idx, cls in enumerate(CLASS_NAMES):
        cls_dir = Path(data_dir) / cls
        if not cls_dir.is_dir():
            continue
        class_paths: list[str] = []
        for pattern in ("*.jpg", "*.jpeg", "*.png"):
            class_paths.extend(sorted(str(p) for p in cls_dir.glob(pattern)))
        all_paths.extend(class_paths)
        all_labels.extend([label_idx] * len(class_paths))
    return all_paths, all_labels


def split_paths(
    paths: list[str],
    labels: list[int],
    val_split: float,
    test_split: float,
    seed: int = 42,
) -> tuple[list[str], list[int], list[str], list[int], list[str], list[int]]:
    """Shuffle indices once; assign test, then val, then train. Partition is exact, no leaks."""
    total = len(paths)
    if total == 0:
        return [], [], [], [], [], []
    unique = set(paths)
    if len(unique) != total:
        raise ValueError(
            "Duplicate image paths in dataset; each file must appear once before splitting."
        )
    if len(labels) != total:
        raise ValueError("paths and labels length mismatch.")
    rng = np.random.default_rng(seed)
    order = rng.permutation(total)
    test_n = int(total * test_split)
    val_n = int(total * val_split)
    test_idx = order[:test_n]
    val_idx = order[test_n : test_n + val_n]
    train_idx = order[test_n + val_n :]

    def pick(idxs: np.ndarray) -> tuple[list[str], list[int]]:
        idx_list = idxs.tolist()
        return [paths[i] for i in idx_list], [labels[i] for i in idx_list]

    train_p, train_l = pick(train_idx)
    val_p, val_l = pick(val_idx)
    test_p, test_l = pick(test_idx)
    assert len(set(train_p) & set(val_p)) == 0
    assert len(set(train_p) & set(test_p)) == 0
    assert len(set(val_p) & set(test_p)) == 0
    union = set(train_p) | set(val_p) | set(test_p)
    assert union == unique
    assert len(train_p) + len(val_p) + len(test_p) == total
    return train_p, train_l, val_p, val_l, test_p, test_l
