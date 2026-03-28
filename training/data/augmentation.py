# file: training/data/augmentation.py
# purpose: apply random augmentation transforms to training images
# dependencies: image_utils.py

import tensorflow as tf

BRIGHTNESS_DELTA: float = 0.25   # ±25% brightness shift
CONTRAST_LOWER: float   = 0.75   # contrast range lower bound
CONTRAST_UPPER: float   = 1.25   # contrast range upper bound
NOISE_STDDEV: float     = 0.05   # gaussian noise std in [-1,1] space (~6/255)


def random_brightness_contrast(image: tf.Tensor) -> tf.Tensor:
    """Apply random brightness then random contrast. Input must be [-1, 1]."""
    # brightness: tf.image.random_brightness — delta in normalized space
    # contrast:   tf.image.random_contrast
    # clip to [-1, 1] after each op
    image = tf.image.random_brightness(image, max_delta=BRIGHTNESS_DELTA)
    image = tf.clip_by_value(image, -1.0, 1.0)
    image = tf.image.random_contrast(image, lower=CONTRAST_LOWER, upper=CONTRAST_UPPER)
    image = tf.clip_by_value(image, -1.0, 1.0)
    return image


def add_gaussian_noise(image: tf.Tensor) -> tf.Tensor:
    """Add zero-mean gaussian noise. Input must be [-1, 1]. Clip after."""
    noise = tf.random.normal(shape=tf.shape(image), mean=0.0, stddev=NOISE_STDDEV)
    return tf.clip_by_value(image + noise, -1.0, 1.0)


def apply_augmentation(image: tf.Tensor) -> tf.Tensor:
    """Apply full augmentation chain to a single image tensor."""
    # order: brightness_contrast → gaussian_noise
    image = random_brightness_contrast(image)
    image = add_gaussian_noise(image)
    return image
