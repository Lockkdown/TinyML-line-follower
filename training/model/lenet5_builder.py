# file: training/model/lenet5_builder.py
# purpose: build LeNet-5 variant for grayscale 96x96 4-class classification
# dependencies: ModelConfig

import tensorflow as tf

from training.model.model_config import ModelConfig


def build_lenet5(cfg: ModelConfig) -> tf.keras.Model:
    """Build and compile LeNet-5 variant. Input: (96, 96, 1). Output: 4 classes."""
    inputs = tf.keras.Input(shape=(cfg.img_size, cfg.img_size, 1))
    x = tf.keras.layers.Conv2D(32, 5, activation="relu", kernel_regularizer=tf.keras.regularizers.l2(cfg.l2_reg))(inputs)
    x = tf.keras.layers.MaxPooling2D()(x)
    x = tf.keras.layers.Conv2D(64, 5, activation="relu", kernel_regularizer=tf.keras.regularizers.l2(cfg.l2_reg))(x)
    x = tf.keras.layers.MaxPooling2D()(x)
    x = tf.keras.layers.Flatten()(x)
    x = tf.keras.layers.Dense(512, activation="relu", kernel_regularizer=tf.keras.regularizers.l2(cfg.l2_reg))(x)
    x = tf.keras.layers.Dropout(cfg.dropout)(x)
    outputs = tf.keras.layers.Dense(cfg.num_classes, activation="softmax", kernel_regularizer=tf.keras.regularizers.l2(cfg.l2_reg))(x)
    model = tf.keras.Model(inputs, outputs)
    model.compile(
        optimizer=tf.keras.optimizers.Adam(cfg.learning_rate),
        loss="sparse_categorical_crossentropy",
        metrics=["accuracy"],
    )
    return model
