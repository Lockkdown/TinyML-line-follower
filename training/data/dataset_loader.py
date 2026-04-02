# file: training/data/dataset_loader.py
# purpose: build train/val/test tf.data pipelines from raw image dirs
# dependencies: image_utils.py, augmentation.py, train_config.py

import tensorflow as tf
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
    
    # Apply augmentation to train set only
    train_ds = apply_augmentation(train_ds)
    
    # Batch and prefetch
    train_ds = train_ds.batch(cfg.batch_size).prefetch(tf.data.AUTOTUNE)
    val_ds = val_ds.batch(cfg.batch_size).prefetch(tf.data.AUTOTUNE)
    test_ds = test_ds.batch(cfg.batch_size).prefetch(tf.data.AUTOTUNE)
    
    return train_ds, val_ds, test_ds
