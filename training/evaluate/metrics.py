# file: training/evaluate/metrics.py
# purpose: compute and log accuracy + F1 metrics per model
# dependencies: sklearn, json

import json
from pathlib import Path

import numpy as np
from sklearn.metrics import accuracy_score, f1_score, precision_recall_fscore_support

_PROJECT_ROOT = Path(__file__).resolve().parents[2]
_COMPARISON_CSV_COLUMNS: list[str] = [
    "model_name",
    "accuracy",
    "macro_f1",
    "f1_forward",
    "f1_left",
    "f1_right",
    "f1_nothing",
]
_CSV_FLOAT_FMT = "{:.6f}"


def comparison_report_path() -> Path:
    """Absolute path to comparison_table.csv (same for every model)."""
    return _PROJECT_ROOT / "outputs" / "reports" / "comparison_table.csv"


def aggregated_metrics_path() -> Path:
    """Absolute path to metrics.json (all models, merged on each train)."""
    return _PROJECT_ROOT / "outputs" / "reports" / "metrics.json"


def _record_for_json(metrics: dict) -> dict[str, float]:
    """Map compute_metrics output to flat floats for JSON."""
    f1 = metrics["f1_per_class"]
    return {
        "accuracy": float(metrics["accuracy"]),
        "macro_f1": float(metrics["macro_f1"]),
        "f1_forward": float(f1.get("forward", 0.0)),
        "f1_left": float(f1.get("left", 0.0)),
        "f1_right": float(f1.get("right", 0.0)),
        "f1_nothing": float(f1.get("nothing", 0.0)),
    }


def merge_aggregated_metrics(metrics: dict, model_name: str) -> None:
    """Load reports/metrics.json if present; set model_name entry; write back."""
    path = aggregated_metrics_path()
    path.parent.mkdir(parents=True, exist_ok=True)
    data: dict[str, dict[str, float]] = {}
    if path.exists():
        try:
            raw = json.loads(path.read_text(encoding="utf-8"))
            if isinstance(raw, dict):
                data = {k: v for k, v in raw.items() if isinstance(v, dict)}
        except json.JSONDecodeError:
            data = {}
    data[model_name] = _record_for_json(metrics)
    path.write_text(
        json.dumps(dict(sorted(data.items())), indent=2) + "\n",
        encoding="utf-8",
    )


def _per_class_prf1(
    y_true: list[int], y_pred: list[int], class_names: list[str]
) -> tuple[dict[str, float], dict[str, float], dict[str, float], dict[str, int]]:
    """Precision, recall, F1, support per label (test counts per true class)."""
    labels = list(range(len(class_names)))
    precision, recall, f1_arr, support = precision_recall_fscore_support(
        y_true, y_pred, labels=labels, zero_division=0
    )
    return (
        dict(zip(class_names, np.asarray(precision, dtype=np.float64))),
        dict(zip(class_names, np.asarray(recall, dtype=np.float64))),
        dict(zip(class_names, np.asarray(f1_arr, dtype=np.float64))),
        dict(zip(class_names, np.asarray(support, dtype=np.int64))),
    )


def compute_metrics(
    y_true: list[int],
    y_pred: list[int],
    class_names: list[str],
) -> dict:
    """Return accuracy, macro F1, and per-class P/R/F1/support (explains F1=1 edge cases)."""
    accuracy = accuracy_score(y_true, y_pred)
    prec, rec, f1_per_class, sup = _per_class_prf1(y_true, y_pred, class_names)
    macro_f1 = f1_score(y_true, y_pred, average="macro", zero_division=0)
    return {
        "accuracy": accuracy,
        "f1_per_class": f1_per_class,
        "macro_f1": macro_f1,
        "precision_per_class": prec,
        "recall_per_class": rec,
        "support_per_class": sup,
    }


def log_metrics(metrics: dict, model_name: str) -> None:
    """Print metrics and append one row; CSV uses 6 decimals to avoid fake 1.0 from rounding."""
    print(f"\n=== {model_name} Metrics ===")
    print(f"Accuracy: {metrics['accuracy']:.6f}")
    print(f"Macro F1:  {metrics['macro_f1']:.6f}")
    print("Per class — P / R / F1 (support = số mẫu đúng nhãn trong test):")
    for cls in ("forward", "left", "right", "nothing"):
        p = metrics["precision_per_class"].get(cls, 0.0)
        r = metrics["recall_per_class"].get(cls, 0.0)
        f1v = metrics["f1_per_class"].get(cls, 0.0)
        n = metrics["support_per_class"].get(cls, 0)
        print(f"  {cls}: P={p:.6f} R={r:.6f} F1={f1v:.6f} (n={n})")

    report_path = comparison_report_path()
    report_path.parent.mkdir(parents=True, exist_ok=True)
    row_map = {
        "model_name": model_name,
        "accuracy": _CSV_FLOAT_FMT.format(metrics["accuracy"]),
        "macro_f1": _CSV_FLOAT_FMT.format(metrics["macro_f1"]),
        "f1_forward": _CSV_FLOAT_FMT.format(metrics["f1_per_class"].get("forward", 0)),
        "f1_left": _CSV_FLOAT_FMT.format(metrics["f1_per_class"].get("left", 0)),
        "f1_right": _CSV_FLOAT_FMT.format(metrics["f1_per_class"].get("right", 0)),
        "f1_nothing": _CSV_FLOAT_FMT.format(metrics["f1_per_class"].get("nothing", 0)),
    }
    line = ",".join(row_map[col] for col in _COMPARISON_CSV_COLUMNS) + "\n"
    if not report_path.exists():
        report_path.write_text(",".join(_COMPARISON_CSV_COLUMNS) + "\n")
    with report_path.open("a", encoding="utf-8") as handle:
        handle.write(line)
    merge_aggregated_metrics(metrics, model_name)
