# file: tests/test_builders.py
# purpose: forward-pass smoke tests for CNN builders
# dependencies: builders, ModelConfig

import numpy as np

from training.model.model_config import ModelConfig
from training.model.lenet5_builder import build_lenet5
from training.model.dsep_cnn_builder import build_dsep_cnn
from training.model.basic_cnn_builder import build_basic_cnn
from training.model.shallow_cnn_builder import build_shallow_cnn


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


def _assert_softmax_sums(model_name: str, builder) -> None:
    """Forward pass; softmax rows sum to 1."""
    model = builder(_make_cfg(model_name))
    output = model(_fake_batch(), training=False).numpy()
    np.testing.assert_allclose(output.sum(axis=1), np.ones(2), atol=1e-5)


def test_lenet5_output_shape():
    """build_lenet5 forward pass with (2, 96, 96, 1) returns (2, 4)."""
    model = build_lenet5(_make_cfg("lenet5"))
    output = model(_fake_batch(), training=False)
    assert output.shape == (2, 4)


def test_lenet5_output_is_probability():
    """build_lenet5 softmax output sums to 1.0 per sample."""
    _assert_softmax_sums("lenet5", build_lenet5)


def test_dsep_cnn_output_shape():
    """build_dsep_cnn forward pass with (2, 96, 96, 1) returns (2, 4)."""
    model = build_dsep_cnn(_make_cfg("dsep_cnn"))
    output = model(_fake_batch(), training=False)
    assert output.shape == (2, 4)


def test_dsep_cnn_output_is_probability():
    """build_dsep_cnn softmax output sums to 1.0 per sample."""
    _assert_softmax_sums("dsep_cnn", build_dsep_cnn)


def test_basic_cnn_output_shape():
    """build_basic_cnn forward pass returns (2, 4)."""
    model = build_basic_cnn(_make_cfg("basic_cnn"))
    output = model(_fake_batch(), training=False)
    assert output.shape == (2, 4)


def test_basic_cnn_output_is_probability():
    """build_basic_cnn softmax output sums to 1.0 per sample."""
    _assert_softmax_sums("basic_cnn", build_basic_cnn)


def test_shallow_cnn_output_shape():
    """build_shallow_cnn forward pass returns (2, 4)."""
    model = build_shallow_cnn(_make_cfg("shallow_cnn"))
    output = model(_fake_batch(), training=False)
    assert output.shape == (2, 4)


def test_shallow_cnn_output_is_probability():
    """build_shallow_cnn softmax output sums to 1.0 per sample."""
    _assert_softmax_sums("shallow_cnn", build_shallow_cnn)
