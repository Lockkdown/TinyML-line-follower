# file: training/model/sparse_ce_loss.py
# purpose: factory for sparse categorical loss with optional label smoothing
# dependencies: tensorflow

import tensorflow as tf


def _smoothed_sparse_fn(num_classes: int, label_smoothing: float):
    """Build loss(y_true, y_pred) compatible with older Keras (no SparseCE label_smoothing)."""

    def loss_fn(y_true: tf.Tensor, y_pred: tf.Tensor) -> tf.Tensor:
        y_true = tf.cast(tf.reshape(y_true, [-1]), tf.int32)
        one_hot = tf.one_hot(y_true, depth=num_classes, dtype=tf.float32)
        smooth = tf.cast(label_smoothing, tf.float32)
        k = tf.cast(num_classes, tf.float32)
        smoothed = one_hot * (1.0 - smooth) + smooth / k
        per_sample = tf.keras.losses.categorical_crossentropy(
            smoothed, y_pred, from_logits=False
        )
        return tf.reduce_mean(per_sample)

    return loss_fn


def make_sparse_categorical_loss(label_smoothing: float, num_classes: int):
    """Return Keras loss for integer labels; optional smoothing softens one-hot targets."""
    if label_smoothing < 0 or label_smoothing >= 1:
        raise ValueError(f"label_smoothing must be in [0, 1), got {label_smoothing}")
    if num_classes <= 0:
        raise ValueError(f"num_classes must be positive, got {num_classes}")
    if label_smoothing == 0:
        return "sparse_categorical_crossentropy"
    return _smoothed_sparse_fn(num_classes, label_smoothing)
