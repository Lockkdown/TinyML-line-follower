# file: training/train.py
# purpose: entry point — train all models specified in MODEL_REGISTRY
# dependencies: all builders, dataset_loader, metrics, confusion_matrix, TrainConfig

import os
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
from training.model.mobilenet_builder import build_mobilenet, unfreeze_for_fine_tuning
from training.model.resnet8_builder import build_resnet8
from training.evaluate.metrics import compute_metrics, log_metrics
from training.evaluate.confusion_matrix import plot_confusion_matrix

# Suppress TensorFlow warnings
os.environ['TF_CPP_MIN_LOG_LEVEL'] = '3'  # Suppress all TF messages
os.environ['TF_ENABLE_ONEDNN_OPTS'] = '0'  # Disable oneDNN custom ops warnings

# Set random seeds
tf.random.set_seed(42)
np.random.seed(42)

# Model registry
MODEL_REGISTRY: dict[str, callable] = {
    "lenet5": build_lenet5,
    "mobilenet_v1_025": build_mobilenet,
    "mobilenet_v1_05": build_mobilenet,
    "mobilenet_v2_035": build_mobilenet,
    "resnet8": build_resnet8,
}

# Alpha values for MobileNet variants
MOBILENET_ALPHA: dict[str, float] = {
    "mobilenet_v1_025": 0.25,
    "mobilenet_v1_05": 0.5,
    "mobilenet_v2_035": 0.35,
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


def get_callbacks(model_name: str, output_dir: str) -> list:
    """
    Return standard callback list:
    - ModelCheckpoint: save best val_accuracy to outputs/experiments/<model_name>/
    - EarlyStopping: patience=10, monitor=val_loss, restore_best_weights=True
    - ReduceLROnPlateau: factor=0.5, patience=5, monitor=val_loss
    """
    callbacks = [
        tf.keras.callbacks.EarlyStopping(
            monitor='val_loss',
            patience=5,
            min_delta=0.001,
            restore_best_weights=True,
            verbose=1
        ),
        tf.keras.callbacks.ReduceLROnPlateau(
            monitor='val_loss',
            factor=0.5,
            patience=3,
            verbose=1
        ),
        tf.keras.callbacks.ModelCheckpoint(
            filepath=output_dir / "best.keras",
            monitor='val_accuracy',
            save_best_only=True,
            verbose=1
        )
    ]
    return callbacks


def run_training(cfg: TrainConfig) -> None:
    """Full training loop for all models in MODEL_REGISTRY."""
    # Load dataset once
    train_ds, val_ds, test_ds = load_dataset(str(DATASET_RAW), cfg)
    
    print(f"\n=== Training All Models ===")
    print(f"Dataset: {len(train_ds)} train, {len(val_ds)} val, {len(test_ds)} test batches")
    
    for model_name in MODEL_REGISTRY.keys():
        print(f"\n{'='*50}")
        print(f"Training: {model_name}")
        print(f"{'='*50}")
        
        # Create model config
        model_cfg = ModelConfig(
            model_name=model_name,
            img_size=cfg.img_size,
            num_classes=cfg.num_classes,
            alpha=MOBILENET_ALPHA.get(model_name, 0.25),
            dropout=cfg.dropout,
            l2_reg=cfg.l2_reg,
            use_pretrained=model_name.startswith("mobilenet"),
            learning_rate=cfg.learning_rate
        )
        
        # Build model
        builder = get_model_builder(model_name)
        model = builder(model_cfg)
        
        # Setup output directory
        output_dir = OUTPUT_EXP / model_name
        output_dir.mkdir(parents=True, exist_ok=True)
        
        # Phase 1 training
        print(f"\nPhase 1: Training {model_name}")
        callbacks = get_callbacks(model_name, output_dir)
        
        history = model.fit(
            train_ds,
            validation_data=val_ds,
            epochs=cfg.epochs,
            callbacks=callbacks,
            verbose=2
        )
        
        # Phase 2: Fine-tuning for MobileNet
        if model_name.startswith("mobilenet"):
            print(f"\nPhase 2: Fine-tuning {model_name}")
            model = unfreeze_for_fine_tuning(model, cfg.learning_rate / 10)
            
            history_fine = model.fit(
                train_ds,
                validation_data=val_ds,
                epochs=cfg.fine_tune_epochs,
                callbacks=callbacks,
                verbose=2
            )
        
        # Evaluate on test set
        print(f"\nEvaluating {model_name} on test set...")
        y_true = []
        y_pred = []
        
        for images, labels in test_ds:
            preds = model(images, training=False).numpy()
            y_true.extend(labels.numpy())
            y_pred.extend(np.argmax(preds, axis=1))
        
        # Compute and log metrics
        metrics = compute_metrics(y_true, y_pred, ["forward", "left", "right", "nothing"])
        log_metrics(metrics, model_name)
        
        # Plot confusion matrix
        cm_path = output_dir / "confusion_matrix.png"
        plot_confusion_matrix(y_true, y_pred, ["forward", "left", "right", "nothing"], str(cm_path))
        print(f"Confusion matrix saved: {cm_path}")
        
        # Save model
        saved_model_path = output_dir / "saved_model"
        model.export(str(saved_model_path))
        print(f"Model saved: {saved_model_path}")


def main():
    """Main entry point."""
    print("=== CNN Line-Follower Training ===")
    
    # Load config
    cfg = load_config(str(CONFIG_PATH))
    print(f"Config loaded: {cfg.model_name}, epochs={cfg.epochs}, fine_tune_epochs={cfg.fine_tune_epochs}")
    
    # Run training
    run_training(cfg)
    
    print(f"\n{'='*50}")
    print("Training complete for all models!")
    print(f"{'='*50}")


if __name__ == "__main__":
    main()
