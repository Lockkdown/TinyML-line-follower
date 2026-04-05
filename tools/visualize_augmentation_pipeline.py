# file: tools/visualize_augmentation_pipeline.py
# purpose: một hàng |Δ| giữa các bước — đúng chuỗi trong augmentation.py
# dependencies: augmentation.py, image_utils.py, train_config.py, matplotlib

import argparse
import sys
from pathlib import Path

import matplotlib.pyplot as plt
import matplotlib.colors as mcolors
import numpy as np
import tensorflow as tf

ROOT_DIR = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT_DIR))

from training.config.train_config import load_config
from training.data.augmentation import (
    add_gaussian_noise,
    random_brightness_contrast,
    random_gaussian_blur,
    random_zoom,
)
from training.data.image_utils import normalize_to_minus1_1

_DIFF_LABELS: list[str] = [
    "Δ: random_brightness_contrast",
    "Δ: random_gaussian_blur",
    "Δ: random_zoom",
    "Δ: add_gaussian_noise",
]


def _decode_normalize(path: Path, img_size: int) -> tf.Tensor:
    """RGB → gray → resize → [-1,1] (giống preview / train)."""
    raw = tf.io.read_file(str(path))
    image = tf.image.decode_image(raw, channels=3, expand_animations=False)
    image = tf.image.rgb_to_grayscale(image)
    image = tf.image.resize(image, [img_size, img_size])
    return normalize_to_minus1_1(image)


def _run_augment_chain(img: tf.Tensor, seed: int) -> list[tf.Tensor]:
    """Cùng thứ tự _augment_pixels trong augmentation.py."""
    tf.random.set_seed(seed)
    s1 = random_brightness_contrast(img)
    s2 = random_gaussian_blur(s1)
    s3 = random_zoom(s2)
    s4 = add_gaussian_noise(s3)
    return [img, s1, s2, s3, s4]


def _pairwise_absdiff(stages: list[tf.Tensor]) -> list[np.ndarray]:
    """|tensor_i − tensor_{i−1}| trong [-1, 1]."""
    out: list[np.ndarray] = []
    for i in range(1, len(stages)):
        a = stages[i - 1].numpy().squeeze().astype(np.float32)
        b = stages[i].numpy().squeeze().astype(np.float32)
        out.append(np.abs(b - a))
    return out


def _max_blur_delta(img: tf.Tensor, seed: int) -> float:
    """max |s2−s1|; =0 khi RNG chọn không blur (50%)."""
    stages = _run_augment_chain(img, seed)
    d = _pairwise_absdiff(stages)
    return float(np.max(d[1]))


def _pick_seed_blur_nonzero(img: tf.Tensor, limit: int = 512) -> int:
    """Chọn seed để nhánh blur có hiệu lực — hình báo cáo không bị Δ=0 toàn khung."""
    for seed in range(limit):
        if _max_blur_delta(img, seed) > 1e-5:
            return seed
    return 0


def _imshow_diff(ax, fig, d: np.ndarray) -> None:
    """Hiển thị |Δ|; LogNorm khi max rất nhỏ (blur 50% có thể gần 0)."""
    dmax = float(np.max(d))
    if dmax <= 0.0:
        im = ax.imshow(np.zeros_like(d), cmap="magma", vmin=0.0, vmax=1.0)
    elif dmax < 1e-4:
        lo = max(dmax * 1e-3, 1e-12)
        hi = max(dmax, lo * 10.0)
        d_safe = np.maximum(d, lo)
        im = ax.imshow(d_safe, cmap="magma", norm=mcolors.LogNorm(vmin=lo, vmax=hi))
    else:
        im = ax.imshow(d, cmap="magma", vmin=0.0, vmax=dmax)
    fig.colorbar(im, ax=ax, fraction=0.046, pad=0.02)


def _save_diff_row(diffs: list[np.ndarray], out_path: Path, seed: int) -> None:
    """Một hàng 4 ô: Δ sau mỗi hàm; suptitle ghi seed (reproduce)."""
    fig, axes = plt.subplots(1, 4, figsize=(13.0, 3.8), layout="constrained")
    for ax, d, title in zip(axes, diffs, _DIFF_LABELS):
        _imshow_diff(ax, fig, d)
        ax.set_title(title, fontsize=9)
        ax.axis("off")
    fig.suptitle(f"tf.random seed = {seed}", fontsize=8, y=1.02)
    out_path.parent.mkdir(parents=True, exist_ok=True)
    fig.savefig(out_path, dpi=150, bbox_inches="tight")
    plt.close(fig)


def _first_raw_image() -> Path:
    """Ảnh đầu tiên trong dataset/raw/forward/."""
    raw = ROOT_DIR / "dataset" / "raw" / "forward"
    for pattern in ("*.jpg", "*.jpeg", "*.png"):
        found = sorted(raw.glob(pattern))
        if found:
            return found[0]
    raise FileNotFoundError(f"No image files under {raw}")


def main() -> None:
    """CLI entry."""
    parser = argparse.ArgumentParser(
        description="Một hàng |Δ| theo augmentation.py (4 bước)."
    )
    parser.add_argument("--input", type=str, default="")
    parser.add_argument(
        "--output",
        type=str,
        default=str(ROOT_DIR / "docs" / "report" / "figures" / "augmentation_pipeline.png"),
    )
    parser.add_argument(
        "--seed",
        type=int,
        default=-1,
        help="RNG seed; -1 = tự chọn seed sao cho blur có Δ khác 0",
    )
    args = parser.parse_args()
    cfg = load_config(str(ROOT_DIR / "training" / "config" / "train_config.yaml"))
    in_path = Path(args.input) if args.input else _first_raw_image()
    img = _decode_normalize(in_path, cfg.img_size)
    seed = args.seed if args.seed >= 0 else _pick_seed_blur_nonzero(img)
    stages = _run_augment_chain(img, seed)
    diffs = _pairwise_absdiff(stages)
    _save_diff_row(diffs, Path(args.output), seed)
    print(f"Saved: {args.output} (seed={seed})")


if __name__ == "__main__":
    main()
