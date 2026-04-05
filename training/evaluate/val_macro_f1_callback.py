# file: training/evaluate/val_macro_f1_callback.py
# purpose: keras callback — validation macro F1 each epoch (sklearn, matches metrics.py)
# dependencies: tensorflow, numpy, sklearn

import numpy as np
import tensorflow as tf
from sklearn.metrics import f1_score


class ValMacroF1Callback(tf.keras.callbacks.Callback):
    """Append validation macro F1 after each epoch (same val_ds, batched)."""

    def __init__(self, val_ds: tf.data.Dataset, num_classes: int) -> None:
        super().__init__()
        self._val_ds = val_ds
        self._num_classes = num_classes
        self.epoch_f1: list[float] = []

    def on_epoch_end(self, epoch: int, logs: dict | None = None) -> None:
        y_true: list[int] = []
        y_pred: list[int] = []
        for images, labels in self._val_ds:
            preds = self.model(images, training=False).numpy()
            y_true.extend(labels.numpy().tolist())
            y_pred.extend(np.argmax(preds, axis=1).tolist())
        macro = f1_score(
            y_true,
            y_pred,
            average="macro",
            labels=list(range(self._num_classes)),
        )
        self.epoch_f1.append(float(macro))
