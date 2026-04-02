# file: training/data/dataset_loader.py
# purpose: build train/val/test tf.data pipelines from raw image dirs
# dependencies: image_utils.py, augmentation.py, train_config.py

import tensorflow as tf
from pathlib import Path
import numpy as np
from training.config.train_config import TrainConfig
from training.data.image_utils import normalize_to_minus1_1
from training.data.augmentation import apply_augmentation

CLASS_NAMES = ["forward", "left", "right", "nothing"]


def split_dataset(dataset: tf.data.Dataset, val_split: float, test_split: float) -> tuple[tf.data.Dataset, tf.data.Dataset, tf.data.Dataset]:
    """Split dataset into train/val/test using cardinality."""
    total = dataset.cardinality().numpy()
    val_size = int(total * val_split)
    test_size = int(total * test_split)
    train_size = total - val_size - test_size
    
    train_ds = dataset.take(train_size)
    val_ds = dataset.skip(train_size).take(val_size)
    test_ds = dataset.skip(train_size + val_size).take(test_size)
    
    return train_ds, val_ds, test_ds


def load_dataset(data_dir: str, cfg: TrainConfig) -> tuple[tf.data.Dataset, tf.data.Dataset, tf.data.Dataset]:
    """Load and split dataset from raw directory, apply preprocessing."""
    # Load all images from raw directory
    full_ds = tf.keras.utils.image_dataset_from_directory(
        data_dir,
        labels='inferred',
        label_mode='int',
        class_names=CLASS_NAMES,
        color_mode='grayscale',
        batch_size=None,
        image_size=(cfg.img_size, cfg.img_size),
        shuffle=True,
        seed=42,
        validation_split=None
    )
    
    # Normalize all images to [-1, 1]
    full_ds = full_ds.map(lambda x, y: (normalize_to_minus1_1(x), y), num_parallel_calls=tf.data.AUTOTUNE)
    
    # Split into train/val/test
    train_ds, val_ds, test_ds = split_dataset(full_ds, cfg.validation_split, cfg.test_split)
    
    # Shuffle train set per-epoch (seed=42 makes it deterministic but different each epoch)
    # reshuffle_each_iteration=True ensures different order every epoch
    total = full_ds.cardinality().numpy()
    train_size = total - int(total * cfg.validation_split) - int(total * cfg.test_split)
    train_ds = train_ds.shuffle(buffer_size=train_size, seed=42, reshuffle_each_iteration=True)
    
    # Apply augmentation to train set only
    if cfg.use_augmentation:
        train_ds = apply_augmentation(train_ds)
    
    # Batch and prefetch
    train_ds = train_ds.batch(cfg.batch_size).prefetch(tf.data.AUTOTUNE)
    val_ds = val_ds.batch(cfg.batch_size).prefetch(tf.data.AUTOTUNE)
    test_ds = test_ds.batch(cfg.batch_size).prefetch(tf.data.AUTOTUNE)
    
    return train_ds, val_ds, test_ds


def make_representative_dataset(data_dir: str, num_samples: int = 100) -> callable:
    """Generate representative dataset for INT8 quantization from train directory."""
    def generator():
        train_dir = Path(data_dir).parent / "processed" / "train"
        source_dir = train_dir if train_dir.exists() else Path(data_dir)

        all_paths = []
        for class_dir in source_dir.iterdir():
            if class_dir.is_dir():
                all_paths.extend(list(class_dir.glob("*.jpg")))
                all_paths.extend(list(class_dir.glob("*.jpeg")))
                all_paths.extend(list(class_dir.glob("*.png")))

        if not all_paths and train_dir.exists():
            source_dir = Path(data_dir)
            for class_dir in source_dir.iterdir():
                if class_dir.is_dir():
                    all_paths.extend(list(class_dir.glob("*.jpg")))
                    all_paths.extend(list(class_dir.glob("*.jpeg")))
                    all_paths.extend(list(class_dir.glob("*.png")))

        if not all_paths:
            raise FileNotFoundError(f"No representative images found in: {source_dir}")

        np.random.shuffle(all_paths)
        selected_paths = all_paths[:num_samples]

        for img_path in selected_paths:
            raw = tf.io.read_file(str(img_path))
            img = tf.image.decode_image(raw, channels=1, expand_animations=False)
            img = tf.image.resize(img, [96, 96])
            img = tf.cast(img, tf.float32)
            img = (img / 127.5) - 1.0
            img = tf.expand_dims(img, axis=0)
            yield [img]
    
    return generator
