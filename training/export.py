# file: training/export.py
# purpose: entry point — quantize all models and save tflite + stats
# dependencies: quantize.py, dataset_loader.make_representative_dataset()

import os
import sys
from pathlib import Path

# Add project root to Python path
ROOT_DIR = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT_DIR))

from training.model.quantize import quantize_model, save_tflite, verify_tflite, log_model_stats
from training.data.dataset_loader import make_representative_dataset

# Suppress TensorFlow warnings
os.environ['TF_CPP_MIN_LOG_LEVEL'] = '3'  # Suppress all TF messages
os.environ['TF_ENABLE_ONEDNN_OPTS'] = '0'  # Disable oneDNN custom ops warnings

# Paths
DATASET_RAW = ROOT_DIR / "dataset" / "raw"
OUTPUT_EXP = ROOT_DIR / "outputs" / "experiments"
OUTPUT_TFLITE = ROOT_DIR / "outputs" / "tflite"

# Models to export
MODEL_NAMES = ["lenet5", "mobilenet_v1_025", "mobilenet_v1_05", "mobilenet_v2_035", "resnet8"]


def export_tflite(model_name: str, saved_model_path: str, output_dir: str) -> None:
    """Full export pipeline: quantize → save → verify → log stats."""
    print(f"\n=== Exporting {model_name} ===")
    
    # Create representative dataset
    print("Creating representative dataset...")
    rep_dataset = make_representative_dataset(str(DATASET_RAW), num_samples=100)
    
    # Quantize model
    print("Quantizing to INT8...")
    tflite_bytes = quantize_model(saved_model_path, rep_dataset)
    
    # Save TFLite
    tflite_path = Path(output_dir) / "model_int8.tflite"
    save_tflite(tflite_bytes, str(tflite_path))
    print(f"TFLite saved: {tflite_path}")
    
    # Verify
    print("Verifying TFLite model...")
    stats = verify_tflite(str(tflite_path))
    log_model_stats(stats, model_name)
    
    # Copy to canonical outputs/tflite/ if this is the best model
    # (For now, copy all models - user can select later)
    OUTPUT_TFLITE.mkdir(parents=True, exist_ok=True)
    canonical_path = OUTPUT_TFLITE / f"{model_name}_model_int8.tflite"
    Path(tflite_path).rename(canonical_path)
    print(f"Moved to: {canonical_path}")


def main():
    """Export all trained models to TFLite."""
    print("=== CNN Line-Follower Export ===")
    print("Exporting all models to INT8 TFLite...")
    
    for model_name in MODEL_NAMES:
        saved_model_path = OUTPUT_EXP / model_name / "saved_model"
        output_dir = OUTPUT_EXP / model_name
        
        if saved_model_path.exists():
            export_tflite(model_name, str(saved_model_path), str(output_dir))
        else:
            print(f"\n⚠️  SavedModel not found for {model_name}: {saved_model_path}")
            print(f"   Run training first: python training/train.py")
    
    print(f"\n{'='*50}")
    print("Export complete!")
    print(f"All models saved to: {OUTPUT_TFLITE}")
    print(f"{'='*50}")


if __name__ == "__main__":
    main()
