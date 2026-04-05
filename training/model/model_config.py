# file: training/model/model_config.py
# purpose: define ModelConfig dataclass for model construction parameters
# dependencies: none

from dataclasses import dataclass


VALID_MODEL_NAMES: tuple[str, ...] = (
    "lenet5",
    "dsep_cnn",
    "resnet8",
)


@dataclass
class ModelConfig:
    model_name: str
    img_size: int
    num_classes: int
    alpha: float
    dropout: float
    l2_reg: float
    use_pretrained: bool
    learning_rate: float
    label_smoothing: float

    def __post_init__(self) -> None:
        """Validate model configuration fields on instantiation."""
        if self.model_name not in VALID_MODEL_NAMES:
            raise ValueError(
                f"Invalid model_name '{self.model_name}'. "
                f"Must be one of: {VALID_MODEL_NAMES}"
            )
        if self.img_size <= 0:
            raise ValueError(f"img_size must be positive, got {self.img_size}")
        if self.num_classes <= 0:
            raise ValueError(f"num_classes must be positive, got {self.num_classes}")
        if self.label_smoothing < 0 or self.label_smoothing >= 1:
            raise ValueError(
                f"label_smoothing must be in [0, 1), got {self.label_smoothing}"
            )
