# file: training/model/dsep_cnn_builder.py
# purpose: depthwise-separable micro-CNN for grayscale 96x96, 4-class output
# dependencies: ModelConfig

import tensorflow as tf

from training.model.model_config import ModelConfig
from training.model.sparse_ce_loss import make_sparse_categorical_loss


def _sep_block(x: tf.Tensor, filters: int, l2_reg: float, strides: int = 1) -> tf.Tensor:
    """One SeparableConv2D block with BN and ReLU (same padding; stride 2 on first block shrinks maps early)."""
    reg = tf.keras.regularizers.l2(l2_reg)
    y = tf.keras.layers.SeparableConv2D(
        filters,
        3,
        strides=strides,
        padding="same",
        depthwise_regularizer=reg,
        pointwise_regularizer=reg,
    )(x)
    y = tf.keras.layers.BatchNormalization()(y)
    return tf.keras.layers.ReLU()(y)


def _conv_stem(inputs: tf.Tensor, l2_reg: float) -> tf.Tensor:
    """Initial strided conv for downsampling."""
    reg = tf.keras.regularizers.l2(l2_reg)
    y = tf.keras.layers.Conv2D(
        16, 3, strides=2, padding="same", kernel_regularizer=reg
    )(inputs)
    y = tf.keras.layers.BatchNormalization()(y)
    return tf.keras.layers.ReLU()(y)


def _feature_trunk(x: tf.Tensor, l2_reg: float) -> tf.Tensor:
    """Stack of separable blocks and pools; first block stride 2 lowers TFLite arena on device."""
    y = _sep_block(x, 32, l2_reg, strides=2)
    y = tf.keras.layers.MaxPooling2D(2)(y)
    y = _sep_block(y, 48, l2_reg)
    y = tf.keras.layers.MaxPooling2D(2)(y)
    return _sep_block(y, 64, l2_reg)


def build_dsep_cnn(cfg: ModelConfig) -> tf.keras.Model:
    """Build and compile DSep-CNN for ESP32-scale deployment."""
    inputs = tf.keras.Input(shape=(cfg.img_size, cfg.img_size, 1))
    reg = tf.keras.regularizers.l2(cfg.l2_reg)
    x = _conv_stem(inputs, cfg.l2_reg)
    x = _feature_trunk(x, cfg.l2_reg)
    x = tf.keras.layers.GlobalAveragePooling2D()(x)
    x = tf.keras.layers.Dropout(cfg.dropout)(x)
    outputs = tf.keras.layers.Dense(
        cfg.num_classes, activation="softmax", kernel_regularizer=reg
    )(x)
    model = tf.keras.Model(inputs, outputs)
    model.compile(
        optimizer=tf.keras.optimizers.Adam(cfg.learning_rate),
        loss=make_sparse_categorical_loss(cfg.label_smoothing, cfg.num_classes),
        metrics=["accuracy"],
    )
    return model
