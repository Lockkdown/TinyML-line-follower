# file: tools/regenerate_confusion_matrix.py
# purpose: re-evaluate test set from best.weights.h5 and redraw confusion_matrix.png
# dependencies: train_config, dataset_loader, dsep_cnn_builder (via get_model_builder)

import argparse
import os
import sys
from dataclasses import replace
from pathlib import Path

import numpy as np
import tensorflow as tf

ROOT_DIR = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT_DIR))

os.environ["TF_CPP_MIN_LOG_LEVEL"] = "3"
os.environ["TF_ENABLE_ONEDNN_OPTS"] = "0"

from training.config.train_config import load_config, TrainConfig
from training.data.dataset_loader import load_dataset
from training.evaluate.confusion_matrix import plot_confusion_matrix
from training.model.model_config import ModelConfig
from training.train import CONFIG_PATH, DATASET_RAW, OUTPUT_EXP, get_model_builder


def _collect_predictions(model: tf.keras.Model, test_ds: tf.data.Dataset) -> tuple[list[int], list[int]]:
    """Run inference on test_ds; return parallel label lists."""
    y_true: list[int] = []
    y_pred: list[int] = []
    for images, labels in test_ds:
        preds = model(images, training=False).numpy()
        y_true.extend(labels.numpy().tolist())
        y_pred.extend(np.argmax(preds, axis=1).tolist())
    return y_true, y_pred


def _build_model(cfg: TrainConfig) -> tf.keras.Model:
    """Construct compiled Keras model for cfg.model_name."""
    model_cfg = ModelConfig(
        model_name=cfg.model_name,
        img_size=cfg.img_size,
        num_classes=cfg.num_classes,
        alpha=0.25,
        dropout=cfg.dropout,
        l2_reg=cfg.l2_reg,
        use_pretrained=False,
        learning_rate=cfg.learning_rate,
        label_smoothing=cfg.label_smoothing,
    )
    builder = get_model_builder(cfg.model_name)
    return builder(model_cfg)


def _regenerate_for_config(cfg: TrainConfig) -> Path:
    """Load weights, run test predictions, save confusion_matrix.png; return output path."""
    _, _, test_ds = load_dataset(str(DATASET_RAW), cfg)
    model = _build_model(cfg)
    weights_path = OUTPUT_EXP / cfg.model_name / "best.weights.h5"
    if not weights_path.is_file():
        raise FileNotFoundError(f"Missing weights: {weights_path}")
    model.load_weights(str(weights_path))
    y_true, y_pred = _collect_predictions(model, test_ds)
    names = ["forward", "left", "right", "nothing"]
    out = OUTPUT_EXP / cfg.model_name / "confusion_matrix.png"
    plot_confusion_matrix(y_true, y_pred, names, str(out))
    return out


def main() -> None:
    """Load best weights, plot row-normalized confusion matrix to experiments/<model>/"""
    parser = argparse.ArgumentParser(description="Regenerate confusion_matrix.png from best weights.")
    parser.add_argument(
        "--model-name",
        type=str,
        default=None,
        help="Override train_config.yaml model_name (default: from config).",
    )
    args = parser.parse_args()
    cfg = load_config(str(CONFIG_PATH))
    if args.model_name:
        cfg = replace(cfg, model_name=args.model_name)
    out = _regenerate_for_config(cfg)
    print(f"Wrote {out}")


if __name__ == "__main__":
    main()
