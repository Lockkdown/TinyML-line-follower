# file: training/train.py
# purpose: entry point — train all models specified in MODEL_REGISTRY
# dependencies: all builders, dataset_loader, metrics, confusion_matrix, TrainConfig

import argparse
import os
import shutil
import sys
import numpy as np
import tensorflow as tf
from pathlib import Path

# Add project root to Python path
ROOT_DIR = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT_DIR))

from training.config.train_config import load_config, TrainConfig
from training.data.dataset_loader import load_dataset
from training.model.model_config import ModelConfig
from training.model.lenet5_builder import build_lenet5
from training.model.dsep_cnn_builder import build_dsep_cnn
from training.model.resnet8_builder import build_resnet8
from training.evaluate.metrics import (
    aggregated_metrics_path,
    comparison_report_path,
    compute_metrics,
    log_metrics,
)
from training.evaluate.confusion_matrix import plot_confusion_matrix
from training.evaluate.training_history_io import write_training_history_artifacts
from training.evaluate.val_macro_f1_callback import ValMacroF1Callback

# Suppress TensorFlow warnings
os.environ['TF_CPP_MIN_LOG_LEVEL'] = '3'  # Suppress all TF messages
os.environ['TF_ENABLE_ONEDNN_OPTS'] = '0'  # Disable oneDNN custom ops warnings

# Set random seeds
tf.random.set_seed(42)
np.random.seed(42)

# Model registry
MODEL_REGISTRY: dict[str, callable] = {
    "lenet5": build_lenet5,
    "dsep_cnn": build_dsep_cnn,
    "resnet8": build_resnet8,
}

# Paths
CONFIG_PATH = ROOT_DIR / "training" / "config" / "train_config.yaml"
DATASET_RAW = ROOT_DIR / "dataset" / "raw"
OUTPUT_EXP = ROOT_DIR / "outputs" / "experiments"


def get_model_builder(model_name: str) -> callable:
    """Return builder function from MODEL_REGISTRY. Raise ValueError if unknown."""
    if model_name not in MODEL_REGISTRY:
        raise ValueError(f"Unknown model: {model_name}. Valid: {list(MODEL_REGISTRY.keys())}")
    return MODEL_REGISTRY[model_name]


def get_callbacks(exp_dir: Path, cfg: TrainConfig) -> list:
    """Checkpoint and EarlyStopping share min_delta; best weights come from file after fit."""
    best_weights_path = exp_dir / "best.weights.h5"
    min_delta = cfg.early_stopping_min_delta
    checkpoint = tf.keras.callbacks.ModelCheckpoint(
        filepath=str(best_weights_path),
        monitor="val_accuracy",
        save_best_only=True,
        save_weights_only=True,
        mode="max",
        verbose=1,
    )
    checkpoint.min_delta = min_delta
    callbacks = [
        checkpoint,
        tf.keras.callbacks.EarlyStopping(
            monitor="val_accuracy",
            patience=cfg.early_stopping_patience,
            min_delta=min_delta,
            restore_best_weights=False,
            verbose=1,
        ),
        tf.keras.callbacks.ReduceLROnPlateau(
            monitor="val_loss",
            factor=0.5,
            patience=3,
            verbose=1,
        ),
    ]
    return callbacks


def run_training(cfg: TrainConfig) -> None:
    """Train model(s) from MODEL_REGISTRY; respects cfg.model_name when set."""
    train_ds, val_ds, test_ds = load_dataset(str(DATASET_RAW), cfg)

    models_to_train = (
        [cfg.model_name] if cfg.model_name in MODEL_REGISTRY
        else list(MODEL_REGISTRY.keys())
    )
    print(f"\n=== Training {models_to_train} ===")
    print(f"Dataset loaded. Starting training for {len(models_to_train)} model(s)...")
    print(
        "Path-based split (seed=42, zero overlap); "
        "augmentation on train only; "
        "test metrics from identical test_ds."
    )

    for model_name in models_to_train:
        print(f"\n{'='*50}")
        print(f"Training: {model_name}")
        print(f"{'='*50}")

        model_cfg = ModelConfig(
            model_name=model_name,
            img_size=cfg.img_size,
            num_classes=cfg.num_classes,
            alpha=0.25,
            dropout=cfg.dropout,
            l2_reg=cfg.l2_reg,
            use_pretrained=False,
            learning_rate=cfg.learning_rate,
            label_smoothing=cfg.label_smoothing,
        )

        builder = get_model_builder(model_name)
        model = builder(model_cfg)

        output_dir = OUTPUT_EXP / model_name
        output_dir.mkdir(parents=True, exist_ok=True)
        best_weights_path = output_dir / "best.weights.h5"
        confusion_matrix_path = output_dir / "confusion_matrix.png"
        saved_model_path = output_dir / "saved_model"
        history_json = output_dir / "training_history.json"
        history_png = output_dir / "training_history.png"

        if best_weights_path.exists():
            best_weights_path.unlink()
        if confusion_matrix_path.exists():
            confusion_matrix_path.unlink()
        if saved_model_path.exists():
            shutil.rmtree(saved_model_path)
        if history_json.exists():
            history_json.unlink()
        if history_png.exists():
            history_png.unlink()

        print(f"\nTraining {model_name}")
        f1_cb = ValMacroF1Callback(val_ds, cfg.num_classes)
        callbacks = get_callbacks(output_dir, cfg) + [f1_cb]

        history_phase1 = model.fit(
            train_ds,
            validation_data=val_ds,
            epochs=cfg.epochs,
            callbacks=callbacks,
            verbose=2,
        )

        fit_histories: list[tf.keras.callbacks.History] = [history_phase1]

        write_training_history_artifacts(fit_histories, f1_cb.epoch_f1, output_dir)
        print(f"Training curves saved: {history_json}, {history_png}")

        if not best_weights_path.exists():
            raise FileNotFoundError(f"Best weights not found: {best_weights_path}")
        model.load_weights(str(best_weights_path))
        print(
            f"Loaded best val_accuracy checkpoint (min_delta={cfg.early_stopping_min_delta}): "
            f"{best_weights_path}"
        )

        print(f"\nEvaluating {model_name} on test set...")
        y_true = []
        y_pred = []

        for images, labels in test_ds:
            preds = model(images, training=False).numpy()
            y_true.extend(labels.numpy())
            y_pred.extend(np.argmax(preds, axis=1))

        metrics = compute_metrics(y_true, y_pred, ["forward", "left", "right", "nothing"])
        log_metrics(metrics, model_name)

        plot_confusion_matrix(y_true, y_pred, ["forward", "left", "right", "nothing"], str(confusion_matrix_path))
        print(f"Confusion matrix saved: {confusion_matrix_path}")

        model.export(str(saved_model_path))
        print(f"Model saved: {saved_model_path}")


def _parse_args() -> argparse.Namespace:
    """CLI: optional reset of report files (default append metrics across runs)."""
    parser = argparse.ArgumentParser(description="Train CNN line-follower models.")
    parser.add_argument(
        "--reset-comparison-csv",
        action="store_true",
        help="Delete comparison_table.csv and metrics.json before training; default keeps/appends.",
    )
    return parser.parse_args()


def main() -> None:
    """Main entry point."""
    args = _parse_args()
    print("=== CNN Line-Follower Training ===")

    cfg = load_config(str(CONFIG_PATH))
    print(
        f"Config loaded: {cfg.model_name}, epochs={cfg.epochs}, "
        f"fine_tune_epochs={cfg.fine_tune_epochs}, label_smoothing={cfg.label_smoothing}"
    )
    report_path = comparison_report_path()
    metrics_json = aggregated_metrics_path()
    if args.reset_comparison_csv:
        if report_path.exists():
            report_path.unlink()
            print(f"Reset report: {report_path}")
        if metrics_json.exists():
            metrics_json.unlink()
            print(f"Reset report: {metrics_json}")

    run_training(cfg)

    print(f"\n{'='*50}")
    print("Training complete for all models!")
    print(f"{'='*50}")


if __name__ == "__main__":
    main()
