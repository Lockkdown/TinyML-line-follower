# file: training/model/mobilenet_builder.py
# purpose: build MobileNet V1/V2 with fine-tuning strategy for 4-class classification
# dependencies: ModelConfig

import tensorflow as tf

from training.model.model_config import ModelConfig


def _replicate_grayscale_channel(input_tensor: tf.Tensor) -> tf.Tensor:
    """Replicate (H,W,1) → (H,W,3) using Concat to avoid TILE op which is unsupported in some TFLM versions."""
    return tf.concat([input_tensor, input_tensor, input_tensor], axis=-1)


def _get_base_model(cfg: ModelConfig) -> tf.keras.Model:
    """Load MobileNet base (V1 or V2) with ImageNet weights, exclude top."""
    kwargs = dict(
        alpha=cfg.alpha,
        input_shape=(cfg.img_size, cfg.img_size, 3),
        include_top=False,
        weights="imagenet",
    )
    if cfg.model_name.startswith("mobilenet_v2"):
        return tf.keras.applications.MobileNetV2(**kwargs)
    return tf.keras.applications.MobileNet(**kwargs)


def _build_classification_head(base_output: tf.Tensor, cfg: ModelConfig) -> tf.Tensor:
    """Add GlobalAveragePooling + Dropout + Dense(4, softmax) on top of base."""
    x = tf.keras.layers.GlobalAveragePooling2D()(base_output)
    x = tf.keras.layers.Dropout(cfg.dropout)(x)
    return tf.keras.layers.Dense(cfg.num_classes, activation="softmax", kernel_regularizer=tf.keras.regularizers.l2(cfg.l2_reg))(x)


def build_mobilenet(cfg: ModelConfig) -> tf.keras.Model:
    """
    Build MobileNet V1/V2 with frozen base for phase-1 training.
    Input: (96, 96, 1) grayscale. Internally replicated to (96, 96, 3).
    Call unfreeze_for_fine_tuning() after phase-1 converges.
    """
    inputs = tf.keras.Input(shape=(cfg.img_size, cfg.img_size, 1))
    x = tf.keras.layers.Lambda(_replicate_grayscale_channel)(inputs)
    base_model = _get_base_model(cfg)
    base_model.trainable = False
    x = base_model(x, training=False)
    outputs = _build_classification_head(x, cfg)
    model = tf.keras.Model(inputs, outputs)
    model.compile(
        optimizer=tf.keras.optimizers.Adam(cfg.learning_rate),
        loss="sparse_categorical_crossentropy",
        metrics=["accuracy"],
    )
    return model


def unfreeze_for_fine_tuning(
    model: tf.keras.Model,
    learning_rate: float,
    unfreeze_from_layer: int = -20,
) -> tf.keras.Model:
    """
    Unfreeze last N layers of base model for phase-2 fine-tuning.
    Must recompile with lower learning_rate (recommend: original_lr / 10).
    """
    base_model = next(l for l in model.layers if isinstance(l, tf.keras.Model))
    base_model.trainable = True
    for layer in base_model.layers[:unfreeze_from_layer]:
        layer.trainable = False
    model.compile(
        optimizer=tf.keras.optimizers.Adam(learning_rate),
        loss="sparse_categorical_crossentropy",
        metrics=["accuracy"],
    )
    return model
