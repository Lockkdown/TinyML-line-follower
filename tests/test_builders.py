# file: tests/test_builders.py
# purpose: forward-pass smoke tests for lenet5, dsep_cnn, resnet8 builders
# dependencies: lenet5_builder.py, dsep_cnn_builder.py, resnet8_builder.py, ModelConfig

import numpy as np

from training.model.model_config import ModelConfig
from training.model.lenet5_builder import build_lenet5
from training.model.dsep_cnn_builder import build_dsep_cnn
from training.model.resnet8_builder import build_resnet8


def _make_cfg(model_name: str) -> ModelConfig:
    """Create a ModelConfig with test defaults for the given model."""
    return ModelConfig(
        model_name=model_name,
        img_size=96,
        num_classes=4,
        alpha=0.25,
        dropout=0.5,
        l2_reg=1e-4,
        use_pretrained=False,
        learning_rate=0.001,
        label_smoothing=0.0,
    )


def _fake_batch() -> np.ndarray:
    """Return fake input batch of shape (2, 96, 96, 1) as float32 in [-1, 1]."""
    return np.random.uniform(-1.0, 1.0, (2, 96, 96, 1)).astype(np.float32)


def test_lenet5_output_shape():
    """build_lenet5 forward pass with (2, 96, 96, 1) returns (2, 4)."""
    model = build_lenet5(_make_cfg("lenet5"))
    output = model(_fake_batch(), training=False)
    assert output.shape == (2, 4)


def test_lenet5_output_is_probability():
    """build_lenet5 softmax output sums to 1.0 per sample."""
    model = build_lenet5(_make_cfg("lenet5"))
    output = model(_fake_batch(), training=False).numpy()
    np.testing.assert_allclose(output.sum(axis=1), np.ones(2), atol=1e-5)


def test_dsep_cnn_output_shape():
    """build_dsep_cnn forward pass with (2, 96, 96, 1) returns (2, 4)."""
    model = build_dsep_cnn(_make_cfg("dsep_cnn"))
    output = model(_fake_batch(), training=False)
    assert output.shape == (2, 4)


def test_resnet8_output_shape():
    """build_resnet8 forward pass with (2, 96, 96, 1) returns (2, 4)."""
    model = build_resnet8(_make_cfg("resnet8"))
    output = model(_fake_batch(), training=False)
    assert output.shape == (2, 4)


def test_resnet8_output_is_probability():
    """build_resnet8 softmax output sums to 1.0 per sample."""
    model = build_resnet8(_make_cfg("resnet8"))
    output = model(_fake_batch(), training=False).numpy()
    np.testing.assert_allclose(output.sum(axis=1), np.ones(2), atol=1e-5)
