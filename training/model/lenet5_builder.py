# file: training/model/lenet5_builder.py
# purpose: build LeNet-5 variant for grayscale 96x96 4-class classification
# dependencies: ModelConfig

import tensorflow as tf

from training.model.model_config import ModelConfig
from training.model.sparse_ce_loss import make_sparse_categorical_loss


def build_lenet5(cfg: ModelConfig) -> tf.keras.Model:
    """Build and compile a micro CNN optimized for ESP32 SRAM. Input: (96, 96, 1). Output: 4 classes."""
    inputs = tf.keras.Input(shape=(cfg.img_size, cfg.img_size, 1))
    
    # Heavily downsample early to reduce MAC operations
    x = tf.keras.layers.Conv2D(8, 3, strides=2, padding="same", activation="relu", kernel_regularizer=tf.keras.regularizers.l2(cfg.l2_reg))(inputs)
    x = tf.keras.layers.MaxPooling2D()(x)
    
    x = tf.keras.layers.Conv2D(16, 3, padding="same", activation="relu", kernel_regularizer=tf.keras.regularizers.l2(cfg.l2_reg))(x)
    x = tf.keras.layers.MaxPooling2D()(x)
    
    x = tf.keras.layers.Conv2D(32, 3, padding="same", activation="relu", kernel_regularizer=tf.keras.regularizers.l2(cfg.l2_reg))(x)
    x = tf.keras.layers.MaxPooling2D()(x)
    
    x = tf.keras.layers.Flatten()(x)
    
    # Keep dense layer tiny to fit in ESP32 Internal RAM (~100KB total size)
    x = tf.keras.layers.Dense(32, activation="relu", kernel_regularizer=tf.keras.regularizers.l2(cfg.l2_reg))(x)
    x = tf.keras.layers.Dropout(cfg.dropout)(x)
    
    outputs = tf.keras.layers.Dense(cfg.num_classes, activation="softmax", kernel_regularizer=tf.keras.regularizers.l2(cfg.l2_reg))(x)
    model = tf.keras.Model(inputs, outputs)
    model.compile(
        optimizer=tf.keras.optimizers.Adam(cfg.learning_rate),
        loss=make_sparse_categorical_loss(cfg.label_smoothing, cfg.num_classes),
        metrics=["accuracy"],
    )
    return model
