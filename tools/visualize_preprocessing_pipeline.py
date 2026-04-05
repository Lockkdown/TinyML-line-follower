# file: tools/visualize_preprocessing_pipeline.py
# purpose: save one figure — raw RGB → grayscale → resize → normalize (báo cáo)
# dependencies: train_config.py, image_utils.py, matplotlib, tensorflow

import argparse
import sys
from pathlib import Path

import matplotlib.pyplot as plt
import numpy as np
import tensorflow as tf

ROOT_DIR = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT_DIR))

from training.config.train_config import load_config
from training.data.image_utils import normalize_to_minus1_1


def _minus1_to_uint8(image: tf.Tensor) -> tf.Tensor:
    """Map [-1,1] float to uint8 for display."""
    scaled = (image + 1.0) * 127.5
    return tf.cast(tf.clip_by_value(tf.round(scaled), 0.0, 255.0), tf.uint8)


def _load_stages(path: Path, img_size: int) -> tuple[tf.Tensor, tf.Tensor, tf.Tensor, tf.Tensor]:
    """Return RGB, grayscale full-res, resized gray, normalized tensor."""
    raw = tf.io.read_file(str(path))
    rgb = tf.image.decode_image(raw, channels=3, expand_animations=False)
    gray = tf.image.rgb_to_grayscale(rgb)
    small = tf.image.resize(gray, [img_size, img_size])
    normed = normalize_to_minus1_1(small)
    return rgb, gray, small, normed


def _first_raw_image() -> Path:
    """Pick first sorted jpg/jpeg/png under dataset/raw/forward/."""
    raw = ROOT_DIR / "dataset" / "raw" / "forward"
    for pattern in ("*.jpg", "*.jpeg", "*.png"):
        found = sorted(raw.glob(pattern))
        if found:
            return found[0]
    raise FileNotFoundError(f"No image files under {raw}")


def _save_figure(
    rgb: tf.Tensor,
    gray: tf.Tensor,
    small: tf.Tensor,
    normed: tf.Tensor,
    img_size: int,
    out_path: Path,
) -> None:
    """Save 1×4 PNG; cột 1–2 tỉ lệ 4:3, cột 3–4 vuông — cùng chiều cao hiển thị."""
    # 320×240 → rộng hơn vuông; width_ratios khớp AR để ảnh 1–2 không bị lùn hơn 3–4
    fig, axes = plt.subplots(
        1,
        4,
        figsize=(15.0, 4.0),
        layout="constrained",
        gridspec_kw={"width_ratios": [4.0 / 3.0, 4.0 / 3.0, 1.0, 1.0]},
    )
    panels = [
        (rgb, "1. Raw JPEG (RGB)", True),
        (gray, "2. Grayscale (kích thước gốc)", False),
        (small, f"3. Resize {img_size}×{img_size}", False),
        (_minus1_to_uint8(normed), "4. Chuẩn hóa [-1,1] (hiển thị uint8)", False),
    ]
    for ax, (tensor, title, is_rgb) in zip(axes, panels):
        arr = tensor.numpy()
        if is_rgb:
            ax.imshow(arr.astype(np.uint8), aspect="equal")
        else:
            g = arr.squeeze()
            im = ax.imshow(g, cmap="gray", vmin=0, vmax=255, aspect="equal")
            if title.startswith("4."):
                cbar = fig.colorbar(im, ax=ax, fraction=0.046, pad=0.02)
                cbar.set_label("Pixel (0–255)", rotation=90)
        ax.set_title(title)
        ax.axis("off")
    out_path.parent.mkdir(parents=True, exist_ok=True)
    fig.savefig(out_path, dpi=150, bbox_inches="tight")
    plt.close(fig)


def main() -> None:
    """CLI entry."""
    parser = argparse.ArgumentParser(description="Vẽ pipeline tiền xử lý (4 bước).")
    parser.add_argument("--input", type=str, default="", help="Đường dẫn ảnh .jpg/.png")
    parser.add_argument(
        "--output",
        type=str,
        default=str(ROOT_DIR / "docs" / "report" / "figures" / "preprocessing_pipeline.png"),
    )
    args = parser.parse_args()
    cfg = load_config(str(ROOT_DIR / "training" / "config" / "train_config.yaml"))
    in_path = Path(args.input) if args.input else _first_raw_image()
    out_path = Path(args.output)
    rgb, gray, small, normed = _load_stages(in_path, cfg.img_size)
    _save_figure(rgb, gray, small, normed, cfg.img_size, out_path)
    print(f"Saved: {out_path}")


if __name__ == "__main__":
    main()
