# file: training/config/train_config.py
# purpose: parse train_config.yaml into TrainConfig dataclass
# dependencies: train_config.yaml

from dataclasses import dataclass
from pathlib import Path

import yaml


@dataclass
class TrainConfig:
    model_name: str
    img_size: int
    num_classes: int
    batch_size: int
    epochs: int
    fine_tune_epochs: int
    learning_rate: float
    validation_split: float
    test_split: float
    dropout: float
    l2_reg: float


def load_config(config_path: str) -> TrainConfig:
    """Load and validate training configuration from YAML file."""
    path = Path(config_path)
    if not path.exists():
        raise FileNotFoundError(f"Config file not found: {config_path}")

    try:
        with open(path, "r") as f:
            data = yaml.safe_load(f)
    except yaml.YAMLError as e:
        raise ValueError(f"Failed to parse config YAML: {config_path}") from e

    return TrainConfig(
        model_name=data["model_name"],
        img_size=data["img_size"],
        num_classes=data["num_classes"],
        batch_size=data["batch_size"],
        epochs=data["epochs"],
        fine_tune_epochs=data["fine_tune_epochs"],
        learning_rate=data["learning_rate"],
        validation_split=data["validation_split"],
        test_split=data["test_split"],
        dropout=data["dropout"],
        l2_reg=data["l2_reg"],
    )
