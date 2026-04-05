# file: training/data/augmentation.py
# purpose: label-safe random augmentations for training images
# dependencies: none (tf only)

import tensorflow as tf

BRIGHTNESS_DELTA: float = 0.35
CONTRAST_LOWER: float = 0.5
CONTRAST_UPPER: float = 1.8
NOISE_STDDEV: float = 0.05


def random_brightness_contrast(image: tf.Tensor) -> tf.Tensor:
    """Apply random brightness then contrast. Input in [-1, 1]."""
    image = tf.image.random_brightness(image, max_delta=BRIGHTNESS_DELTA)
    image = tf.clip_by_value(image, -1.0, 1.0)
    image = tf.image.random_contrast(image, lower=CONTRAST_LOWER, upper=CONTRAST_UPPER)
    return tf.clip_by_value(image, -1.0, 1.0)


def add_gaussian_noise(image: tf.Tensor) -> tf.Tensor:
    """Add zero-mean gaussian noise in [-1, 1] space."""
    noise = tf.random.normal(shape=tf.shape(image), mean=0.0, stddev=NOISE_STDDEV)
    return tf.clip_by_value(image + noise, -1.0, 1.0)


def random_gaussian_blur(image: tf.Tensor) -> tf.Tensor:
    """Box blur via downscale + upscale; 50% probability."""
    h = tf.shape(image)[0]
    w = tf.shape(image)[1]
    blurred = tf.image.resize(
        tf.image.resize(image, [h // 2, w // 2], method="area"),
        [h, w],
        method="bilinear",
    )
    return tf.where(tf.random.uniform(()) < 0.5, blurred, image)


def random_zoom(image: tf.Tensor) -> tf.Tensor:
    """Random central-ish crop at 85-100% scale, resize back to original size."""
    h = tf.shape(image)[0]
    w = tf.shape(image)[1]
    scale = tf.random.uniform((), 0.85, 1.0)
    new_h = tf.maximum(tf.cast(tf.cast(h, tf.float32) * scale, tf.int32), 1)
    new_w = tf.maximum(tf.cast(tf.cast(w, tf.float32) * scale, tf.int32), 1)
    offset_h = tf.random.uniform((), 0, h - new_h + 1, dtype=tf.int32)
    offset_w = tf.random.uniform((), 0, w - new_w + 1, dtype=tf.int32)
    cropped = tf.image.crop_to_bounding_box(image, offset_h, offset_w, new_h, new_w)
    return tf.image.resize(cropped, [h, w])


def _augment_pixels(image: tf.Tensor) -> tf.Tensor:
    """Full safe chain: lighting, blur, zoom, noise (no flips or rotation)."""
    image = random_brightness_contrast(image)
    image = random_gaussian_blur(image)
    image = random_zoom(image)
    return add_gaussian_noise(image)


def augment_image(image: tf.Tensor) -> tf.Tensor:
    """Apply augmentation to image only (preview tools)."""
    return _augment_pixels(image)


def augment_image_pair(image: tf.Tensor, label: tf.Tensor) -> tuple[tf.Tensor, tf.Tensor]:
    """Apply augmentation; label unchanged."""
    return _augment_pixels(image), label


def apply_augmentation(dataset: tf.data.Dataset) -> tf.data.Dataset:
    """Concatenate original samples with one augmented copy each (2x cardinality)."""
    augmented = dataset.map(
        augment_image_pair, num_parallel_calls=tf.data.AUTOTUNE
    )
    return dataset.concatenate(augmented)
