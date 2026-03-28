# file: training/data/image_utils.py
# purpose: stateless image transform utilities shared across pipeline
# dependencies: train_config.yaml (img_size only)

import numpy as np
import tensorflow as tf

IMG_SIZE: int = 96  # canonical default; pass actual value from config at call site


def resize_for_inference(image: np.ndarray, img_size: int) -> np.ndarray:
    """Resize HxWxC numpy image to (img_size, img_size, 1) grayscale uint8."""
    if image.ndim == 2:
        image = image[..., np.newaxis]
    tensor = tf.image.resize(image, [img_size, img_size])
    if tensor.shape[-1] != 1:
        tensor = tf.image.rgb_to_grayscale(tensor)
    return tf.cast(tensor, tf.uint8).numpy()


def to_grayscale(image: tf.Tensor) -> tf.Tensor:
    """Convert RGB tensor (H, W, 3) to grayscale (H, W, 1)."""
    # tf.image.rgb_to_grayscale expects 3 channels
    return tf.image.rgb_to_grayscale(image)


def normalize_to_minus1_1(image: tf.Tensor) -> tf.Tensor:
    """Cast to float32 and scale [0, 255] → [-1.0, 1.0]."""
    # formula: (pixel / 127.5) - 1.0
    image = tf.cast(image, tf.float32)
    return (image / 127.5) - 1.0


def is_valid_image(image_path: str) -> bool:
    """Return True if file exists, is readable, and decodes without error."""
    try:
        raw = tf.io.read_file(image_path)
        tf.image.decode_jpeg(raw)
        return True
    except (tf.errors.NotFoundError, tf.errors.InvalidArgumentError):
        return False
