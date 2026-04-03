# file: tools/capture_helpers.py
# purpose: ESP32 snapshot fetch, save, quality/hash checks for dense capture
# dependencies: numpy, opencv-python, inspect_dataset.py

from __future__ import annotations

import hashlib
import time
from collections import deque
from dataclasses import dataclass
from pathlib import Path
from typing import Deque, Optional, Tuple
from urllib.error import HTTPError, URLError
from urllib.request import Request, urlopen

import cv2
import numpy as np

from inspect_dataset import count_per_class

CLASS_NAMES: list[str] = ["forward", "left", "right", "nothing"]
QUALITY_MIN_MEAN: float = 20.0
QUALITY_MAX_MEAN: float = 235.0
FROZEN_MAD_THRESHOLD: float = 2.0
FROZEN_RETRY_DELAY: float = 0.1
HASH_BUFFER_SIZE: int = 3


@dataclass
class CaptureConfig:
    """Runtime options for dense capture CLI."""
    esp32_ip: str
    class_label: str
    interval: float
    output_dir: str


@dataclass
class SessionStats:
    """Per-session capture and skip counters."""
    captured: int = 0
    skipped_frozen: int = 0
    skipped_duplicate: int = 0
    skipped_quality: int = 0


def capture_frame(esp32_ip: str) -> np.ndarray:
    """Fetch one JPEG snapshot from ESP32 /snapshot and return grayscale image."""
    url = f"http://{esp32_ip}/snapshot?t={int(time.time() * 1000)}"
    req = Request(url)
    try:
        with urlopen(req, timeout=10) as resp:
            data = resp.read()
    except (HTTPError, URLError, TimeoutError, OSError) as exc:
        raise ConnectionError(f"Failed to fetch snapshot: {exc}") from exc
    buf = np.frombuffer(data, dtype=np.uint8)
    image = cv2.imdecode(buf, cv2.IMREAD_GRAYSCALE)
    if image is None:
        raise ValueError("Failed to decode JPEG from ESP32")
    return image


def save_image(image: np.ndarray, class_label: str, output_dir: str) -> None:
    """Save image as {class}_{timestamp_ms}.jpg under output_dir/class_label/."""
    class_dir = Path(output_dir) / class_label
    class_dir.mkdir(parents=True, exist_ok=True)
    timestamp_ms = int(time.time() * 1000)
    path = class_dir / f"{class_label}_{timestamp_ms}.jpg"
    cv2.imwrite(str(path), image)


def compute_frame_hash(frame: np.ndarray) -> str:
    """Return MD5 hex digest of raw frame pixels."""
    return hashlib.md5(frame.tobytes()).hexdigest()


def is_frozen_frame(
    frame: np.ndarray, prev_frame: Optional[np.ndarray], threshold: float
) -> bool:
    """True if mean absolute difference from prev_frame is below threshold."""
    if prev_frame is None or frame.shape != prev_frame.shape:
        return False
    diff = np.abs(frame.astype(np.float32) - prev_frame.astype(np.float32))
    return float(np.mean(diff)) < threshold


def is_quality_frame(frame: np.ndarray) -> Tuple[bool, str]:
    """Return (ok, reason) where reason is underexposed, overexposed, or empty."""
    mean_px = float(np.mean(frame))
    if mean_px < QUALITY_MIN_MEAN:
        return False, "underexposed"
    if mean_px > QUALITY_MAX_MEAN:
        return False, "overexposed"
    return True, ""


def try_capture_once(esp32_ip: str) -> Tuple[Optional[np.ndarray], bool]:
    """Return (frame, True) on success, (None, False) on network/decode failure."""
    try:
        return capture_frame(esp32_ip), True
    except (ConnectionError, ValueError):
        return None, False


def _retry_after_frozen_first(
    esp32_ip: str,
    prev_saved: Optional[np.ndarray],
    rolling_hashes: Deque[str],
) -> Tuple[Optional[np.ndarray], bool]:
    time.sleep(FROZEN_RETRY_DELAY)
    frame2, ok2 = try_capture_once(esp32_ip)
    if not ok2:
        return None, False
    h2 = compute_frame_hash(frame2)
    if h2 in rolling_hashes or is_frozen_frame(frame2, prev_saved, FROZEN_MAD_THRESHOLD):
        rolling_hashes.append(h2)
        return None, False
    rolling_hashes.append(h2)
    return frame2, True


def fetch_frame_with_retry(
    esp32_ip: str,
    prev_saved: Optional[np.ndarray],
    rolling_hashes: Deque[str],
) -> Tuple[Optional[np.ndarray], bool]:
    """Fetch with MAD freeze check, one retry, rolling hash de-dupe across retries."""
    frame1, ok1 = try_capture_once(esp32_ip)
    if not ok1:
        time.sleep(FROZEN_RETRY_DELAY)
        frame1, ok1 = try_capture_once(esp32_ip)
    if not ok1:
        return None, False
    h1 = compute_frame_hash(frame1)
    if h1 in rolling_hashes:
        return None, False
    if not is_frozen_frame(frame1, prev_saved, FROZEN_MAD_THRESHOLD):
        rolling_hashes.append(h1)
        return frame1, True
    rolling_hashes.append(h1)
    return _retry_after_frozen_first(esp32_ip, prev_saved, rolling_hashes)


def print_session_summary(stats: SessionStats, output_dir: str, class_label: str) -> None:
    """Print session totals, skip breakdown, and on-disk counts per class."""
    counts = count_per_class(output_dir, CLASS_NAMES)
    disk = "\n".join(f"  {n}: {counts.get(n, 0)}" for n in CLASS_NAMES)
    sk = f"{stats.skipped_frozen}, duplicate: {stats.skipped_duplicate}, quality: {stats.skipped_quality}"
    print(
        f"\n=== Session summary ===\nCaptured this session ({class_label}): {stats.captured}\n"
        f"Skipped — frozen: {sk}\nImages per class on disk (under output root):\n{disk}"
    )
