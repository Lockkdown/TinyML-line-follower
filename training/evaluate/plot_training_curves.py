# file: training/evaluate/plot_training_curves.py
# purpose: plot train/val loss and val macro F1 from training record or JSON path
# dependencies: json, matplotlib, pathlib

import json
from pathlib import Path

import matplotlib.pyplot as plt


def load_training_record(json_path: Path) -> dict:
    """Load training history JSON written by save_training_history_json."""
    return json.loads(json_path.read_text(encoding="utf-8"))


def plot_training_curves(record: dict, output_path: Path) -> None:
    """Save two-panel figure: loss vs epoch, val macro F1 vs epoch."""
    epochs = record["epoch"]
    fig, axes = plt.subplots(2, 1, figsize=(8.0, 7.0), constrained_layout=True)
    ax_loss, ax_f1 = axes
    ax_loss.plot(epochs, record["loss"], label="train loss")
    ax_loss.plot(epochs, record["val_loss"], label="val loss")
    ax_loss.set_xlabel("epoch")
    ax_loss.set_ylabel("loss")
    ax_loss.legend(loc="upper right")
    ax_loss.set_title("Loss")
    ax_f1.plot(epochs, record["val_macro_f1"], label="val macro F1", color="green")
    ax_f1.set_xlabel("epoch")
    ax_f1.set_ylabel("macro F1")
    ax_f1.legend(loc="lower right")
    ax_f1.set_title("Validation macro F1")
    output_path.parent.mkdir(parents=True, exist_ok=True)
    fig.savefig(output_path, dpi=150)
    plt.close(fig)


def replot_from_json(json_path: Path, png_path: Path | None = None) -> None:
    """Re-render PNG from existing JSON (default PNG next to JSON)."""
    record = load_training_record(json_path)
    out = png_path if png_path else json_path.with_suffix(".png")
    plot_training_curves(record, out)
