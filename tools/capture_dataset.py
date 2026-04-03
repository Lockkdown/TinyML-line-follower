# file: tools/capture_dataset.py
# purpose: dense capture CLI — interval snapshots from ESP32 with filters
# dependencies: capture_helpers.py, stdlib threading/queue

from __future__ import annotations

import argparse
import queue
import select
import sys
import threading
import time
from collections import deque
from dataclasses import dataclass
from typing import Optional, Set, Tuple

import numpy as np

from capture_helpers import (
    HASH_BUFFER_SIZE,
    CaptureConfig,
    SessionStats,
    compute_frame_hash,
    fetch_frame_with_retry,
    is_quality_frame,
    print_session_summary,
    save_image,
)

CLASS_CHOICES: Tuple[str, ...] = ("forward", "left", "right", "nothing")
DEFAULT_IP: str = "192.168.43.200"
COUNTDOWN_SECONDS: int = 3


@dataclass
class _FrameSaveContext:
    """Groups mutable state for quality/duplicate/save (max 4 params on save helper)."""

    cfg: CaptureConfig
    stats: SessionStats
    seen_hashes: Set[str]


def parse_args() -> argparse.Namespace:
    """Parse CLI arguments for dense capture."""
    p = argparse.ArgumentParser(description="Dense capture from ESP32 /snapshot")
    p.add_argument("--class", dest="class_label", choices=CLASS_CHOICES, required=True)
    p.add_argument("--interval", type=float, default=0.3, help="Seconds between captures")
    p.add_argument("--output", type=str, default="dataset/raw", help="Dataset root (raw/)")
    p.add_argument("--ip", type=str, default=DEFAULT_IP, help="ESP32 IP address")
    return p.parse_args()


def _keyboard_reader(key_queue: queue.Queue[str]) -> None:
    while True:
        if sys.platform == "win32":
            import msvcrt
            if msvcrt.kbhit():
                key_queue.put(msvcrt.getch().decode("utf-8", errors="ignore").lower())
        elif select.select([sys.stdin], [], [], 0.05)[0]:
            key_queue.put(sys.stdin.read(1).lower())
        else:
            time.sleep(0.02)


def _poll_quit(key_queue: queue.Queue[str]) -> bool:
    try:
        while True:
            if key_queue.get_nowait() == "q":
                return True
    except queue.Empty:
        return False


def run_countdown(seconds: int, key_queue: queue.Queue[str]) -> bool:
    """Print 3…2…1 then GO; Q cancels."""
    for sec in range(seconds, 0, -1):
        print(f"Starting in {sec}...", flush=True)
        end = time.monotonic() + 1.0
        while time.monotonic() < end:
            if _poll_quit(key_queue):
                print("Countdown cancelled.")
                return False
            time.sleep(0.05)
    print("GO!", flush=True)
    return True


def _drain_keys(key_queue: queue.Queue[str]) -> Optional[str]:
    pending: Optional[str] = None
    try:
        while True:
            ch = key_queue.get_nowait()
            if ch == "q":
                return "quit"
            if ch == " ":
                pending = "toggle"
    except queue.Empty:
        pass
    return pending


def _sleep_poll_keys(duration: float, key_queue: queue.Queue[str]) -> Optional[str]:
    end = time.monotonic() + duration
    while time.monotonic() < end:
        act = _drain_keys(key_queue)
        if act:
            return act
        time.sleep(0.02)
    return None


def _try_save_frame(
    frame: np.ndarray, prev_saved: Optional[np.ndarray], ctx: _FrameSaveContext
) -> Optional[np.ndarray]:
    """Quality, duplicate check, save; return new reference frame for MAD."""
    good, reason = is_quality_frame(frame)
    if not good:
        ctx.stats.skipped_quality += 1
        print(f"Skipped: {reason}")
        return prev_saved
    digest = compute_frame_hash(frame)
    if digest in ctx.seen_hashes:
        ctx.stats.skipped_duplicate += 1
        print("Skipped: duplicate")
        return prev_saved
    save_image(frame, ctx.cfg.class_label, ctx.cfg.output_dir)
    ctx.seen_hashes.add(digest)
    ctx.stats.captured += 1
    c, cl = ctx.stats.captured, ctx.cfg.class_label
    print(f"Captured: {c} | Class: {cl} | Press SPACE to pause")
    return frame


def run_capture_loop(cfg: CaptureConfig, key_queue: queue.Queue[str]) -> SessionStats:
    """SPACE start/pause, Q quit; fixed-interval capture with filters."""
    stats = SessionStats()
    seen_hashes: Set[str] = set()
    save_ctx = _FrameSaveContext(cfg, stats, seen_hashes)
    rolling: deque[str] = deque(maxlen=HASH_BUFFER_SIZE)
    prev_saved: Optional[np.ndarray] = None
    capturing = False
    print("Dense capture — SPACE: start/pause, Q: quit (ESP32 must be in dataset mode)")
    while True:
        act = _drain_keys(key_queue)
        if act == "quit":
            return stats
        if act == "toggle":
            if capturing:
                capturing = False
                print("Paused.")
            elif run_countdown(COUNTDOWN_SECONDS, key_queue):
                capturing = True
        if not capturing:
            time.sleep(0.02)
            continue
        frame, ok = fetch_frame_with_retry(cfg.esp32_ip, prev_saved, rolling)
        if not ok:
            stats.skipped_frozen += 1
            print("Skipped: frozen frame")
        else:
            prev_saved = _try_save_frame(frame, prev_saved, save_ctx)
        act2 = _sleep_poll_keys(cfg.interval, key_queue)
        if act2 == "quit":
            return stats
        if act2 == "toggle":
            capturing = False
            print("Paused.")


def main() -> None:
    """CLI entry."""
    args = parse_args()
    cfg = CaptureConfig(args.ip, args.class_label, args.interval, args.output)
    key_queue: queue.Queue[str] = queue.Queue()
    threading.Thread(target=_keyboard_reader, args=(key_queue,), daemon=True).start()
    print_session_summary(run_capture_loop(cfg, key_queue), cfg.output_dir, cfg.class_label)


if __name__ == "__main__":
    main()
