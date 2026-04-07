# file: training/model/basic_cnn_builder.py
# purpose: classic stacked Conv2D CNN baseline (no depthwise) for 96x96 grayscale
# dependencies: ModelConfig, sparse_ce_loss

import tensorflow as tf

from training.model.model_config import ModelConfig
from training.model.sparse_ce_loss import make_sparse_categorical_loss


def _l2(l2_reg: float) -> tf.keras.regularizers.L2:
    """L2 regularizer for conv/dense kernels."""
    return tf.keras.regularizers.l2(l2_reg)


def _conv_pool_block(
    x: tf.Tensor, filters: int, l2_reg: float, stride_first: int
) -> tf.Tensor:
    """Conv2D + BN + ReLU + MaxPool; first conv may stride to downsample."""
    reg = _l2(l2_reg)
    y = tf.keras.layers.Conv2D(
        filters, 3, strides=stride_first, padding="same", kernel_regularizer=reg
    )(x)
    y = tf.keras.layers.BatchNormalization()(y)
    y = tf.keras.layers.ReLU()(y)
    return tf.keras.layers.MaxPooling2D(2)(y)


def _classifier_head(x: tf.Tensor, cfg: ModelConfig) -> tf.Tensor:
    """GAP, dropout, softmax over num_classes."""
    reg = _l2(cfg.l2_reg)
    y = tf.keras.layers.GlobalAveragePooling2D()(x)
    y = tf.keras.layers.Dropout(cfg.dropout)(y)
    return tf.keras.layers.Dense(
        cfg.num_classes, activation="softmax", kernel_regularizer=reg
    )(y)


def build_basic_cnn(cfg: ModelConfig) -> tf.keras.Model:
    """Build and compile a textbook-style Conv2D stack (Ioffe & Szegedy-style BN blocks)."""
    inputs = tf.keras.Input(shape=(cfg.img_size, cfg.img_size, 1))
    x = _conv_pool_block(inputs, 32, cfg.l2_reg, stride_first=2)
    x = _conv_pool_block(x, 64, cfg.l2_reg, stride_first=1)
    x = _conv_pool_block(x, 128, cfg.l2_reg, stride_first=1)
    outputs = _classifier_head(x, cfg)
    model = tf.keras.Model(inputs, outputs)
    model.compile(
        optimizer=tf.keras.optimizers.Adam(cfg.learning_rate),
        loss=make_sparse_categorical_loss(cfg.label_smoothing, cfg.num_classes),
        metrics=["accuracy"],
    )
    return model
