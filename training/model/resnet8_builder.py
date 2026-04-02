# file: training/model/resnet8_builder.py
# purpose: build ResNet-8 with residual blocks for grayscale 96x96 4-class classification
# dependencies: ModelConfig

import tensorflow as tf

from training.model.model_config import ModelConfig


def _residual_block(x: tf.Tensor, filters: int, stride: int = 1, l2_reg: float = 1e-4) -> tf.Tensor:
    """One residual block: Conv→BN→ReLU→Conv→BN → Add(shortcut) → ReLU."""
    shortcut = x
    x = tf.keras.layers.Conv2D(filters, 3, strides=stride, padding="same", kernel_regularizer=tf.keras.regularizers.l2(l2_reg))(x)
    x = tf.keras.layers.BatchNormalization()(x)
    x = tf.keras.layers.ReLU()(x)
    x = tf.keras.layers.Conv2D(filters, 3, padding="same", kernel_regularizer=tf.keras.regularizers.l2(l2_reg))(x)
    x = tf.keras.layers.BatchNormalization()(x)
    
    # Add shortcut with 1x1 conv if shape mismatch
    if stride != 1 or shortcut.shape[-1] != filters:
        shortcut = tf.keras.layers.Conv2D(filters, 1, strides=stride, padding="same", kernel_regularizer=tf.keras.regularizers.l2(l2_reg))(shortcut)
        shortcut = tf.keras.layers.BatchNormalization()(shortcut)
    
    x = tf.keras.layers.Add()([x, shortcut])
    x = tf.keras.layers.ReLU()(x)
    return x


def build_resnet8(cfg: ModelConfig) -> tf.keras.Model:
    """
    Build ResNet-8: Input(96,96,1) → Conv → 3× residual_block → GAP → Dense(4).
    Compile: Adam(cfg.learning_rate), sparse_categorical_crossentropy, [accuracy].
    """
    inputs = tf.keras.Input(shape=(cfg.img_size, cfg.img_size, 1))
    x = tf.keras.layers.Conv2D(32, 3, strides=1, padding="same", kernel_regularizer=tf.keras.regularizers.l2(cfg.l2_reg))(inputs)
    x = tf.keras.layers.BatchNormalization()(x)
    x = tf.keras.layers.ReLU()(x)
    
    # 3 residual blocks with filters [32, 64, 128], stride=2 on first conv of each block
    x = _residual_block(x, 32, stride=1, l2_reg=cfg.l2_reg)
    x = _residual_block(x, 64, stride=2, l2_reg=cfg.l2_reg)
    x = _residual_block(x, 128, stride=2, l2_reg=cfg.l2_reg)
    
    x = tf.keras.layers.GlobalAveragePooling2D()(x)
    x = tf.keras.layers.Dropout(cfg.dropout)(x)
    outputs = tf.keras.layers.Dense(cfg.num_classes, activation="softmax", kernel_regularizer=tf.keras.regularizers.l2(cfg.l2_reg))(x)
    model = tf.keras.Model(inputs, outputs)
    model.compile(
        optimizer=tf.keras.optimizers.Adam(cfg.learning_rate),
        loss="sparse_categorical_crossentropy",
        metrics=["accuracy"],
    )
    return model
