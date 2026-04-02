# file: training/evaluate/metrics.py
# purpose: compute and log accuracy + F1 metrics per model
# dependencies: sklearn

from pathlib import Path
from sklearn.metrics import accuracy_score, f1_score


def compute_metrics(
    y_true: list[int],
    y_pred: list[int],
    class_names: list[str]
) -> dict:
    """Return {accuracy, f1_per_class, macro_f1}."""
    accuracy = accuracy_score(y_true, y_pred)
    
    # F1 score per class
    f1_per_class = f1_score(y_true, y_pred, average=None, labels=range(len(class_names)))
    f1_per_class = dict(zip(class_names, f1_per_class))
    
    # Macro F1
    macro_f1 = f1_score(y_true, y_pred, average='macro')
    
    return {
        'accuracy': accuracy,
        'f1_per_class': f1_per_class,
        'macro_f1': macro_f1
    }


def log_metrics(metrics: dict, model_name: str) -> None:
    """Print metrics table and append row to outputs/reports/comparison_table.csv."""
    # Print to console
    print(f"\n=== {model_name} Metrics ===")
    print(f"Accuracy: {metrics['accuracy']:.4f}")
    print(f"Macro F1:  {metrics['macro_f1']:.4f}")
    print("F1 per class:")
    for cls, f1 in metrics['f1_per_class'].items():
        print(f"  {cls}: {f1:.4f}")
    
    # Append to CSV
    report_path = Path("outputs/reports/comparison_table.csv")
    report_path.parent.mkdir(parents=True, exist_ok=True)
    
    # Prepare row
    row = {
        'model_name': model_name,
        'accuracy': f"{metrics['accuracy']:.4f}",
        'macro_f1': f"{metrics['macro_f1']:.4f}"
    }
    for cls in ['forward', 'left', 'right', 'nothing']:
        row[f'f1_{cls}'] = f"{metrics['f1_per_class'].get(cls, 0):.4f}"
    
    # Write header if file doesn't exist
    if not report_path.exists():
        header = ','.join(row.keys()) + '\n'
        report_path.write_text(header)
    
    # Append row
    with report_path.open('a') as f:
        f.write(','.join(row.values()) + '\n')
