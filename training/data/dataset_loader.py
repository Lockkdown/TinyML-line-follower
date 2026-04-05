# file: training/data/dataset_loader.py
# purpose: build train/val/test tf.data from path-based split (no leakage)
# dependencies: image_utils.py, augmentation.py, train_config.py, dataset_path_utils.py

import tensorflow as tf
from pathlib import Path

import numpy as np

from training.config.train_config import TrainConfig
from training.data.augmentation import apply_augmentation
from training.data.dataset_path_utils import CLASS_NAMES, collect_paths_and_labels, split_paths
from training.data.image_utils import normalize_to_minus1_1


def _count_by_class(paths: list[str]) -> dict[str, int]:
    """Count images per class from path parent folder name."""
    counts = {c: 0 for c in CLASS_NAMES}
    for p in paths:
        name = Path(p).parent.name
        if name in counts:
            counts[name] += 1
    return counts


def log_dataset_stats(
    train_paths: list[str],
    val_paths: list[str],
    test_paths: list[str],
    cfg: TrainConfig,
) -> None:
    """Print per-split sizes, per-class in each split, and augmentation note."""
    print("\n" + "=" * 52)
    print("  DATASET STATISTICS (path-based split, zero overlap)")
    print("=" * 52)
    for split_name, plist in ("TRAIN", train_paths), ("VAL", val_paths), ("TEST", test_paths):
        cc = _count_by_class(plist)
        print(f"  --- {split_name} ({len(plist)} files) ---")
        for cls in CLASS_NAMES:
            print(f"    {cls:<10} : {cc[cls]:>5}")
    print("-" * 52)
    print(f"  Split ratios : val={cfg.validation_split}, test={cfg.test_split}")
    if cfg.use_augmentation:
        eff = len(train_paths) * 2
        print(f"  Augmentation : ON  (2x train: {len(train_paths)} + {len(train_paths)} = {eff})")
        print("                 brightness, contrast, blur, zoom, noise (label-safe)")
    else:
        print("  Augmentation : OFF")
    print(f"  Batch size   : {cfg.batch_size}")
    print(f"  Train batches: {max(len(train_paths), 1) * (2 if cfg.use_augmentation else 1) // cfg.batch_size}")
    print("=" * 52 + "\n")


def paths_to_dataset(paths: list[str], labels: list[int], img_size: int) -> tf.data.Dataset:
    """Load grayscale images from paths; normalize to [-1, 1]."""
    def load_pair(path: tf.Tensor, label: tf.Tensor) -> tuple[tf.Tensor, tf.Tensor]:
        raw = tf.io.read_file(path)
        img = tf.image.decode_image(raw, channels=1, expand_animations=False)
        img = tf.image.resize(img, [img_size, img_size])
        return normalize_to_minus1_1(img), label

    ds = tf.data.Dataset.from_tensor_slices((paths, labels))
    return ds.map(load_pair, num_parallel_calls=tf.data.AUTOTUNE)


def load_dataset(data_dir: str, cfg: TrainConfig) -> tuple[tf.data.Dataset, tf.data.Dataset, tf.data.Dataset]:
    """Load raw images with path split, optional 2x train augmentation, then batch."""
    all_paths, all_labels = collect_paths_and_labels(data_dir)
    train_p, train_l, val_p, val_l, test_p, test_l = split_paths(
        all_paths, all_labels, cfg.validation_split, cfg.test_split
    )
    log_dataset_stats(train_p, val_p, test_p, cfg)

    train_ds = paths_to_dataset(train_p, train_l, cfg.img_size)
    val_ds = paths_to_dataset(val_p, val_l, cfg.img_size)
    test_ds = paths_to_dataset(test_p, test_l, cfg.img_size)

    if cfg.use_augmentation:
        train_ds = apply_augmentation(train_ds)
        buf = max(len(train_p) * 2, 1)
    else:
        buf = max(len(train_p), 1)

    train_ds = train_ds.shuffle(buffer_size=buf, seed=42, reshuffle_each_iteration=True)
    train_ds = train_ds.batch(cfg.batch_size).prefetch(tf.data.AUTOTUNE)
    val_ds = val_ds.batch(cfg.batch_size).prefetch(tf.data.AUTOTUNE)
    test_ds = test_ds.batch(cfg.batch_size).prefetch(tf.data.AUTOTUNE)
    return train_ds, val_ds, test_ds


def make_representative_dataset(data_dir: str, num_samples: int = 100) -> callable:
    """Representative images for INT8 quantization (same normalize as training)."""
    def generator():
        train_dir = Path(data_dir).parent / "processed" / "train"
        source_dir = train_dir if train_dir.exists() else Path(data_dir)
        all_paths: list[Path] = []
        for class_dir in source_dir.iterdir():
            if class_dir.is_dir():
                for ext in ("*.jpg", "*.jpeg", "*.png"):
                    all_paths.extend(class_dir.glob(ext))
        if not all_paths and train_dir.exists():
            source_dir = Path(data_dir)
            for class_dir in source_dir.iterdir():
                if class_dir.is_dir():
                    for ext in ("*.jpg", "*.jpeg", "*.png"):
                        all_paths.extend(class_dir.glob(ext))
        if not all_paths:
            raise FileNotFoundError(f"No representative images found in: {source_dir}")
        rng = np.random.default_rng(42)
        order = rng.permutation(len(all_paths))
        for idx in order[:num_samples]:
            img_path = all_paths[int(idx)]
            raw = tf.io.read_file(str(img_path))
            img = tf.image.decode_image(raw, channels=1, expand_animations=False)
            img = tf.image.resize(img, [96, 96])
            img = tf.cast(img, tf.float32)
            img = (img / 127.5) - 1.0
            yield [tf.expand_dims(img, axis=0)]

    return generator
