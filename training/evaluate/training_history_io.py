# file: training/evaluate/training_history_io.py
# purpose: merge Keras History objects, attach val_macro_f1, save JSON
# dependencies: json, pathlib, tensorflow

import json
from pathlib import Path

import tensorflow as tf


def merge_fit_histories(
    histories: list[tf.keras.callbacks.History],
) -> dict[str, list]:
    """Concatenate history dicts from phase-1 and phase-2 fits (same keys)."""
    if not histories:
        return {}
    keys: set[str] = set()
    for h in histories:
        keys.update(h.history.keys())
    merged: dict[str, list] = {k: [] for k in sorted(keys)}
    for h in histories:
        for key in merged:
            if key in h.history:
                merged[key].extend(h.history[key])
    return merged


def build_training_record(
    merged: dict[str, list],
    val_macro_f1: list[float],
) -> dict:
    """Add epoch index and val_macro_f1; lengths must match loss series."""
    loss_series = merged.get("loss", [])
    n = len(loss_series)
    if len(val_macro_f1) != n:
        raise ValueError(
            f"val_macro_f1 length {len(val_macro_f1)} != merged loss length {n}"
        )
    out: dict = {"epoch": list(range(1, n + 1))}
    out.update(merged)
    out["val_macro_f1"] = val_macro_f1
    return out


def save_training_history_json(record: dict, path: Path) -> None:
    """Write training record as JSON (floats lists)."""
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(record, indent=2), encoding="utf-8")


def write_training_history_artifacts(
    histories: list[tf.keras.callbacks.History],
    val_macro_f1: list[float],
    output_dir: Path,
) -> None:
    """Merge histories, save training_history.json, plot training_history.png."""
    from training.evaluate.plot_training_curves import plot_training_curves

    merged = merge_fit_histories(histories)
    record = build_training_record(merged, val_macro_f1)
    json_path = output_dir / "training_history.json"
    png_path = output_dir / "training_history.png"
    save_training_history_json(record, json_path)
    plot_training_curves(record, png_path)
