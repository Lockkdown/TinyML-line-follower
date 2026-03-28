# file: tests/test_image_utils.py
# purpose: unit tests for image_utils.py transform functions
# dependencies: image_utils.py

import numpy as np
import pytest
import tensorflow as tf

from training.data.image_utils import (
    is_valid_image,
    normalize_to_minus1_1,
    resize_for_inference,
    to_grayscale,
)


def test_resize_for_inference_output_shape():
    """resize_for_inference returns (img_size, img_size, 1)."""
    image = np.random.randint(0, 255, (240, 240, 3), dtype=np.uint8)
    result = resize_for_inference(image, img_size=96)
    assert result.shape == (96, 96, 1)


def test_resize_for_inference_dtype_is_uint8():
    """resize_for_inference returns uint8."""
    image = np.random.randint(0, 255, (240, 240, 3), dtype=np.uint8)
    result = resize_for_inference(image, img_size=96)
    assert result.dtype == np.uint8


def test_resize_for_inference_grayscale_input():
    """resize_for_inference handles single-channel input without conversion."""
    image = np.random.randint(0, 255, (240, 240, 1), dtype=np.uint8)
    result = resize_for_inference(image, img_size=96)
    assert result.shape == (96, 96, 1)


def test_to_grayscale_output_shape():
    """to_grayscale converts (H, W, 3) → (H, W, 1)."""
    image = tf.random.uniform((96, 96, 3), minval=0.0, maxval=255.0)
    result = to_grayscale(image)
    assert result.shape == (96, 96, 1)


def test_normalize_to_minus1_1_range():
    """normalize_to_minus1_1 produces values strictly in [-1.0, 1.0]."""
    image = tf.constant(np.random.randint(0, 256, (96, 96, 1), dtype=np.uint8))
    result = normalize_to_minus1_1(image)
    assert float(tf.reduce_min(result).numpy()) >= -1.0
    assert float(tf.reduce_max(result).numpy()) <= 1.0 + 1e-6


def test_normalize_to_minus1_1_zero_maps_to_minus_one():
    """Pixel value 0 maps to -1.0."""
    image = tf.zeros((1, 1, 1), dtype=tf.uint8)
    result = normalize_to_minus1_1(image)
    assert float(result.numpy()[0, 0, 0]) == pytest.approx(-1.0)


def test_normalize_to_minus1_1_max_maps_to_one():
    """Pixel value 255 maps to ~1.0."""
    image = tf.constant([[[255]]], dtype=tf.uint8)
    result = normalize_to_minus1_1(image)
    assert float(result.numpy()[0, 0, 0]) == pytest.approx(1.0, abs=1e-3)


def test_is_valid_image_nonexistent_returns_false():
    """is_valid_image returns False for a nonexistent path."""
    assert is_valid_image("/nonexistent/path/image.jpg") is False


def test_is_valid_image_valid_jpeg_returns_true(tmp_path):
    """is_valid_image returns True for a valid JPEG file."""
    dummy = tf.zeros((10, 10, 1), dtype=tf.uint8)
    jpeg_bytes = tf.image.encode_jpeg(dummy, format="grayscale")
    image_path = str(tmp_path / "test.jpg")
    tf.io.write_file(image_path, jpeg_bytes)
    assert is_valid_image(image_path) is True
