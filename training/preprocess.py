# file: training/preprocess.py
# purpose: standalone preprocessing pipeline entry point
# dependencies: dataset_loader.py, train_config.py

import os
import sys
from pathlib import Path
import matplotlib.pyplot as plt
import numpy as np

# Suppress TensorFlow warnings
os.environ['TF_CPP_MIN_LOG_LEVEL'] = '2'

# Add project root to Python path
ROOT_DIR = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT_DIR))

from training.config.train_config import load_config
from training.data.dataset_loader import load_dataset
CONFIG_PATH = ROOT_DIR / "training" / "config" / "train_config.yaml"
DATASET_RAW = ROOT_DIR / "dataset" / "raw"
PREVIEW_DIR = ROOT_DIR / "outputs" / "reports"
CLASS_NAMES = ["forward", "left", "right", "nothing"]


def denormalize_image(image: np.ndarray) -> np.ndarray:
    """Convert image from [-1, 1] back to [0, 1] for visualization."""
    return np.clip((image + 1.0) / 2.0, 0.0, 1.0)


def save_batch_preview(images: np.ndarray, labels: np.ndarray, title: str, output_path: Path) -> None:
    """Save a preview grid of one batch to disk."""
    preview_count = min(8, len(images))
    rows = 2
    cols = 4
    figure, axes = plt.subplots(rows, cols, figsize=(12, 6))
    axes = axes.flatten()
    for index in range(preview_count):
        axes[index].imshow(denormalize_image(images[index].squeeze()), cmap="gray")
        axes[index].set_title(CLASS_NAMES[int(labels[index])])
        axes[index].axis("off")
    for index in range(preview_count, len(axes)):
        axes[index].axis("off")
    figure.suptitle(title)
    figure.tight_layout()
    output_path.parent.mkdir(parents=True, exist_ok=True)
    figure.savefig(output_path, dpi=150, bbox_inches="tight")
    plt.close(figure)


def main():
    """Run preprocessing pipeline and print dataset info."""
    print("=== CNN Line-Follower Preprocessing ===")
    
    # Load config
    cfg = load_config(str(CONFIG_PATH))
    print(f"Config: img_size={cfg.img_size}, batch_size={cfg.batch_size}")
    print(f"Splits: train={1-cfg.validation_split-cfg.test_split:.0%}, val={cfg.validation_split:.0%}, test={cfg.test_split:.0%}")
    
    # Load and preprocess dataset
    print(f"\nLoading dataset from: {DATASET_RAW}")
    try:
        train_ds, val_ds, test_ds = load_dataset(str(DATASET_RAW), cfg)
    except Exception as e:
        print(f"Error loading dataset: {e}")
        sys.exit(1)
    
    # Calculate actual image counts
    train_count = sum(1 for _ in train_ds.unbatch())
    val_count = sum(1 for _ in val_ds.unbatch())
    test_count = sum(1 for _ in test_ds.unbatch())
    total_count = train_count + val_count + test_count
    
    print("\n=== Dataset Summary ===")
    print(f"Total images: {total_count}")
    print(f"Train: {train_count} images ({train_count/total_count:.1%}) - {len(train_ds)} batches")
    print(f"Val:   {val_count} images ({val_count/total_count:.1%}) - {len(val_ds)} batches")
    print(f"Test:  {test_count} images ({test_count/total_count:.1%}) - {len(test_ds)} batches")
    
    # Show train batch (with augmentation)
    for images, labels in train_ds.take(1):
        print(f"\nTrain batch (with augmentation):")
        print(f"  Shape: images={images.shape}, labels={labels.shape}")
        print(f"  Range: [{images.numpy().min():.3f}, {images.numpy().max():.3f}] (normalized [-1,1])")
        train_preview_path = PREVIEW_DIR / "train_augmented_preview.png"
        save_batch_preview(images.numpy(), labels.numpy(), "Train batch with augmentation", train_preview_path)
        print(f"  Preview saved: {train_preview_path}")
        break
    
    # Show val batch (no augmentation)
    for images, labels in val_ds.take(1):
        print(f"\nVal batch (no augmentation):")
        print(f"  Shape: images={images.shape}, labels={labels.shape}")
        print(f"  Range: [{images.numpy().min():.3f}, {images.numpy().max():.3f}] (normalized [-1,1])")
        val_preview_path = PREVIEW_DIR / "val_preprocessed_preview.png"
        save_batch_preview(images.numpy(), labels.numpy(), "Validation batch after preprocessing", val_preview_path)
        print(f"  Preview saved: {val_preview_path}")
        break
    
    print("\n✅ Preprocessing completed successfully!")
    print("Note: Train set has random augmentation, so range varies per batch")


if __name__ == "__main__":
    main()
