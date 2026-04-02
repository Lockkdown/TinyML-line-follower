# file: training/preprocess.py
# purpose: standalone preprocessing pipeline entry point
# dependencies: dataset_loader.py, train_config.py

import os
import sys
from pathlib import Path

# Suppress TensorFlow warnings
os.environ['TF_CPP_MIN_LOG_LEVEL'] = '2'

# Add project root to Python path
ROOT_DIR = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT_DIR))

from training.config.train_config import load_config
from training.data.dataset_loader import load_dataset
CONFIG_PATH = ROOT_DIR / "training" / "config" / "train_config.yaml"
DATASET_RAW = ROOT_DIR / "dataset" / "raw"


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
        break
    
    # Show val batch (no augmentation)
    for images, labels in val_ds.take(1):
        print(f"\nVal batch (no augmentation):")
        print(f"  Shape: images={images.shape}, labels={labels.shape}")
        print(f"  Range: [{images.numpy().min():.3f}, {images.numpy().max():.3f}] (normalized [-1,1])")
        break
    
    print("\n✅ Preprocessing completed successfully!")
    print("Note: Train set has random augmentation, so range varies per batch")


if __name__ == "__main__":
    main()
