# file: training/model/shallow_cnn_builder.py
# purpose: single-conv + flatten + dense weak baseline (high param count vs GAP models)
# dependencies: ModelConfig, sparse_ce_loss

import tensorflow as tf

from training.model.model_config import ModelConfig
from training.model.sparse_ce_loss import make_sparse_categorical_loss


def _l2(l2_reg: float) -> tf.keras.regularizers.L2:
    """L2 regularizer for conv/dense kernels."""
    return tf.keras.regularizers.l2(l2_reg)


def _conv_pool(x: tf.Tensor, l2_reg: float) -> tf.Tensor:
    """One Conv2D + ReLU + MaxPool."""
    reg = _l2(l2_reg)
    y = tf.keras.layers.Conv2D(
        16, 3, padding="same", activation="relu", kernel_regularizer=reg
    )(x)
    return tf.keras.layers.MaxPooling2D(2)(y)


def _classifier_head(x: tf.Tensor, cfg: ModelConfig) -> tf.Tensor:
    """Flatten, dropout, softmax."""
    reg = _l2(cfg.l2_reg)
    y = tf.keras.layers.Flatten()(x)
    y = tf.keras.layers.Dropout(cfg.dropout)(y)
    return tf.keras.layers.Dense(
        cfg.num_classes, activation="softmax", kernel_regularizer=reg
    )(y)


def build_shallow_cnn(cfg: ModelConfig) -> tf.keras.Model:
    """Build and compile a deliberately shallow CNN for comparison reports."""
    inputs = tf.keras.Input(shape=(cfg.img_size, cfg.img_size, 1))
    x = _conv_pool(inputs, cfg.l2_reg)
    outputs = _classifier_head(x, cfg)
    model = tf.keras.Model(inputs, outputs)
    model.compile(
        optimizer=tf.keras.optimizers.Adam(cfg.learning_rate),
        loss=make_sparse_categorical_loss(cfg.label_smoothing, cfg.num_classes),
        metrics=["accuracy"],
    )
    return model
