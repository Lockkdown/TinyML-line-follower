# file: tests/test_augmentation.py
# purpose: unit tests for augmentation.py transform functions
# dependencies: augmentation.py

import tensorflow as tf

from training.data.augmentation import (
    add_gaussian_noise,
    apply_augmentation,
    augment_image,
    random_brightness_contrast,
)


def test_random_brightness_contrast_output_shape():
    """random_brightness_contrast preserves input shape."""
    image = tf.random.uniform((96, 96, 1), minval=-1.0, maxval=1.0)
    result = random_brightness_contrast(image)
    assert result.shape == (96, 96, 1)


def test_random_brightness_contrast_stays_in_range():
    """random_brightness_contrast output is clipped to [-1.0, 1.0]."""
    image = tf.random.uniform((96, 96, 1), minval=-1.0, maxval=1.0)
    result = random_brightness_contrast(image)
    assert float(tf.reduce_min(result).numpy()) >= -1.0
    assert float(tf.reduce_max(result).numpy()) <= 1.0


def test_add_gaussian_noise_output_shape():
    """add_gaussian_noise preserves input shape."""
    image = tf.random.uniform((96, 96, 1), minval=-1.0, maxval=1.0)
    result = add_gaussian_noise(image)
    assert result.shape == (96, 96, 1)


def test_add_gaussian_noise_stays_in_range():
    """add_gaussian_noise output is clipped to [-1.0, 1.0]."""
    image = tf.random.uniform((96, 96, 1), minval=-1.0, maxval=1.0)
    result = add_gaussian_noise(image)
    assert float(tf.reduce_min(result).numpy()) >= -1.0
    assert float(tf.reduce_max(result).numpy()) <= 1.0


def test_augment_image_output_shape():
    """augment_image preserves input shape."""
    image = tf.random.uniform((96, 96, 1), minval=-1.0, maxval=1.0)
    result = augment_image(image)
    assert result.shape == (96, 96, 1)


def test_augment_image_stays_in_range():
    """augment_image full chain output is clipped to [-1.0, 1.0]."""
    image = tf.random.uniform((96, 96, 1), minval=-1.0, maxval=1.0)
    result = augment_image(image)
    assert float(tf.reduce_min(result).numpy()) >= -1.0
    assert float(tf.reduce_max(result).numpy()) <= 1.0


def test_augment_image_modifies_values():
    """augment_image stochastically changes the image values."""
    tf.random.set_seed(0)
    image = tf.random.uniform((96, 96, 1), minval=-0.5, maxval=0.5, seed=0)
    result = augment_image(image)
    assert not tf.reduce_all(tf.equal(image, result)).numpy()


def test_apply_augmentation_doubles_cardinality():
    """apply_augmentation concatenates original + augmented (2x samples)."""
    images = tf.random.uniform((4, 32, 32, 1), minval=-1.0, maxval=1.0)
    labels = tf.constant([0, 1, 2, 3])
    ds = tf.data.Dataset.from_tensor_slices((images, labels))
    out = apply_augmentation(ds)
    assert out.cardinality().numpy() == 8
