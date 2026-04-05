# file: tools/convert_to_cc.py
# purpose: convert TFLite binary to C++ array MODEL_DATA for firmware (Windows-friendly)
# dependencies: stdlib only

from __future__ import annotations

import argparse
import sys
from pathlib import Path


def _format_array_body(data: bytes, bytes_per_line: int) -> str:
    """Format bytes as C array initializer lines."""
    lines: list[str] = []
    for offset in range(0, len(data), bytes_per_line):
        chunk = data[offset : offset + bytes_per_line]
        parts = ", ".join(f"0x{b:02x}" for b in chunk)
        lines.append(f"  {parts},")
    return "\n".join(lines)


def tflite_to_model_cc(input_path: Path, output_path: Path) -> None:
    """Read .tflite and write firmware/model-style model_data.cc."""
    raw = input_path.read_bytes()
    body = _format_array_body(raw, 12)
    header = """// file: firmware/model/model_data.cc
// purpose: TFLite model converted to C array with 4-byte alignment
// dependencies: generated from .tflite model

// Ensure 4-byte alignment for TFLite flatbuffer
alignas(4) const unsigned char MODEL_DATA[] = {
"""
    footer = f"""}};

const unsigned int MODEL_DATA_LEN = sizeof(MODEL_DATA);
"""
    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_text(header + body + "\n" + footer, encoding="utf-8")


def main() -> None:
    """CLI: python tools/convert_to_cc.py <input.tflite> <output.cc>"""
    parser = argparse.ArgumentParser(
        description="Convert TFLite flatbuffer to MODEL_DATA C++ array."
    )
    parser.add_argument("input", nargs="?", default=None, help="Path to .tflite")
    parser.add_argument("output", nargs="?", default=None, help="Path to output .cc")
    args = parser.parse_args()
    root = Path(__file__).resolve().parent.parent
    input_path = Path(args.input) if args.input else root / "outputs" / "tflite" / "model_int8.tflite"
    output_path = Path(args.output) if args.output else root / "firmware" / "model" / "model_data.cc"
    if not input_path.is_file():
        print(f"Error: input not found: {input_path}", file=sys.stderr)
        sys.exit(1)
    tflite_to_model_cc(input_path, output_path)
    print(f"Wrote {output_path} ({input_path.stat().st_size} bytes)")


if __name__ == "__main__":
    main()
