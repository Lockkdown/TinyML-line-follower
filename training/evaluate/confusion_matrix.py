# file: training/evaluate/confusion_matrix.py
# purpose: plot and save confusion matrix for one model evaluation
# dependencies: matplotlib, sklearn

import matplotlib.pyplot as plt
import numpy as np
from sklearn.metrics import confusion_matrix

CONFUSION_MATRIX_VALUE_DECIMALS: int = 3


def _row_normalize_cm(cm: np.ndarray) -> np.ndarray:
    """Normalize confusion matrix counts by row (true-class recall per row)."""
    row_sums = cm.sum(axis=1)[:, np.newaxis].astype(float)
    return np.divide(
        cm.astype(float),
        row_sums,
        out=np.zeros_like(cm.astype(float), dtype=float),
        where=row_sums != 0,
    )


def _annotate_normalized_cm(ax, cm_norm: np.ndarray, decimals: int) -> None:
    """Draw value text on each cell of a row-normalized confusion matrix."""
    thresh = float(cm_norm.max()) / 2.0
    fmt = f"{{:.{decimals}f}}"
    for i in range(cm_norm.shape[0]):
        for j in range(cm_norm.shape[1]):
            val = float(cm_norm[i, j])
            ax.text(
                j,
                i,
                fmt.format(val),
                ha="center",
                va="center",
                color="white" if val > thresh else "black",
            )


def _configure_cm_axes(ax, class_names: list[str]) -> None:
    """Set ticks, labels, and rotated x tick labels."""
    n = len(class_names)
    ax.set(
        xticks=np.arange(n),
        yticks=np.arange(n),
        xticklabels=class_names,
        yticklabels=class_names,
        ylabel="True label",
        xlabel="Predicted label",
    )
    plt.setp(ax.get_xticklabels(), rotation=45, ha="right", rotation_mode="anchor")


def plot_confusion_matrix(
    y_true: list[int],
    y_pred: list[int],
    class_names: list[str],
    output_path: str,
) -> None:
    """Plot normalized confusion matrix and save PNG to output_path."""
    cm = confusion_matrix(y_true, y_pred)
    cm_norm = _row_normalize_cm(cm)
    fig, ax = plt.subplots(figsize=(8, 6))
    im = ax.imshow(cm_norm, interpolation="nearest", cmap="Blues")
    ax.figure.colorbar(im, ax=ax)
    _configure_cm_axes(ax, class_names)
    _annotate_normalized_cm(ax, cm_norm, CONFUSION_MATRIX_VALUE_DECIMALS)
    plt.tight_layout()
    plt.savefig(output_path, dpi=150, bbox_inches="tight")
    plt.close()
