# file: tools/preview_augmented_samples.py
# purpose: save preprocessed (+ optional augment) PNG previews to dataset/processed/<class>/
# dependencies: train_config.yaml, image_utils.py, augmentation.py, TrainConfig

import sys
from pathlib import Path

import tensorflow as tf
import yaml

ROOT_DIR = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT_DIR))

from training.config.train_config import TrainConfig, load_config
from training.data.augmentation import augment_image
from training.data.image_utils import normalize_to_minus1_1

CLASS_NAMES: list[str] = ["forward", "left", "right", "nothing"]
_IMAGE_EXTS: tuple[str, ...] = (".jpg", ".jpeg", ".png")
_CONFIG_PATH: Path = ROOT_DIR / "training" / "config" / "train_config.yaml"


def load_preview_samples_per_class(config_path: Path) -> int:
    """Read preview_samples_per_class from YAML (not part of TrainConfig)."""
    with open(config_path, encoding="utf-8") as handle:
        data = yaml.safe_load(handle)
    if "preview_samples_per_class" not in data:
        raise KeyError("train_config.yaml must define preview_samples_per_class")
    return int(data["preview_samples_per_class"])


def list_sorted_image_paths(class_dir: Path) -> list[Path]:
    """Return sorted image paths under one class folder."""
    paths: list[Path] = []
    for ext in _IMAGE_EXTS:
        paths.extend(class_dir.glob(f"*{ext}"))
    return sorted(paths)


def clear_class_preview_dir(out_dir: Path) -> None:
    """Ensure directory exists and remove previous *.png previews."""
    out_dir.mkdir(parents=True, exist_ok=True)
    for png in out_dir.glob("*.png"):
        png.unlink()


def decode_resize_normalize(path: Path, img_size: int) -> tf.Tensor:
    """Match dataset_loader: grayscale, resize, then [-1, 1]."""
    raw = tf.io.read_file(str(path))
    image = tf.image.decode_image(raw, channels=3, expand_animations=False)
    image = tf.image.rgb_to_grayscale(image)
    image = tf.image.resize(image, [img_size, img_size])
    return normalize_to_minus1_1(image)


def minus1_to_uint8(image: tf.Tensor) -> tf.Tensor:
    """Invert normalize_to_minus1_1 for PNG export."""
    scaled = (image + 1.0) * 127.5
    return tf.cast(tf.clip_by_value(tf.round(scaled), 0.0, 255.0), tf.uint8)


def apply_optional_augment(
    image: tf.Tensor, class_idx: int, sample_idx: int, use_augmentation: bool
) -> tf.Tensor:
    """Reproducible augment per (class, index) when augmentation is on."""
    if not use_augmentation:
        return image
    tf.random.set_seed(42 + class_idx * 1000 + sample_idx)
    return augment_image(image)


def write_png(image_u8: tf.Tensor, dest: Path) -> None:
    """Write single-channel uint8 tensor as PNG."""
    encoded = tf.io.encode_png(image_u8)
    tf.io.write_file(str(dest), encoded)


def build_preview_tensor(
    path: Path, cfg: TrainConfig, class_idx: int, sample_idx: int
) -> tf.Tensor:
    """Resize, normalize, and optionally augment one image (train pipeline parity)."""
    tensor = decode_resize_normalize(path, cfg.img_size)
    return apply_optional_augment(
        tensor, class_idx, sample_idx, cfg.use_augmentation
    )


def save_preview_to_dir(
    tensor: tf.Tensor, path: Path, sample_idx: int, out_dir: Path
) -> None:
    """Encode tensor as PNG under out_dir with deterministic filename."""
    u8 = minus1_to_uint8(tensor)
    stem = path.stem[:40]
    dest = out_dir / f"sample_{sample_idx:02d}_{stem}.png"
    write_png(u8, dest)


def write_class_previews(
    class_idx: int, class_name: str, cfg: TrainConfig, per_class: int
) -> None:
    """Write preview PNGs for one label under dataset/processed/<class_name>/."""
    raw_root = ROOT_DIR / "dataset" / "raw"
    proc_root = ROOT_DIR / "dataset" / "processed"
    class_raw = raw_root / class_name
    if not class_raw.is_dir():
        raise FileNotFoundError(f"Missing raw class directory: {class_raw}")
    paths = list_sorted_image_paths(class_raw)[:per_class]
    if not paths:
        raise FileNotFoundError(f"No images in {class_raw}")
    if len(paths) < per_class:
        print(f"Warning: {class_name} has only {len(paths)} images (< {per_class})")
    out_dir = proc_root / class_name
    clear_class_preview_dir(out_dir)
    for sample_idx, path in enumerate(paths):
        tensor = build_preview_tensor(path, cfg, class_idx, sample_idx)
        save_preview_to_dir(tensor, path, sample_idx, out_dir)


def run_preview_samples(cfg: TrainConfig, per_class: int) -> None:
    """Write up to per_class previews per label under dataset/processed/<class>/."""
    for class_idx, class_name in enumerate(CLASS_NAMES):
        write_class_previews(class_idx, class_name, cfg, per_class)


def main() -> None:
    """CLI entry: load config and write preview PNGs."""
    cfg = load_config(str(_CONFIG_PATH))
    n = load_preview_samples_per_class(_CONFIG_PATH)
    print(f"Previews: up to {n}/class, img_size={cfg.img_size}, augment={cfg.use_augmentation}")
    run_preview_samples(cfg, n)
    print("Wrote dataset/processed/<class>/sample_*.png")


if __name__ == "__main__":
    main()
