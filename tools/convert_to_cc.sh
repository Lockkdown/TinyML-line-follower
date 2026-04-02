#!/bin/bash

# Convert TFLite model to C array with proper 4-byte alignment
# Usage: ./convert_to_cc.sh <input.tflite> <output.cc>

set -e

INPUT_FILE="$1"
OUTPUT_FILE="$2"

if [ -z "$INPUT_FILE" ] || [ -z "$OUTPUT_FILE" ]; then
    echo "Usage: $0 <input.tflite> <output.cc>"
    exit 1
fi

# Get the directory of this script
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

# Default paths if not provided
if [ "$INPUT_FILE" = "default" ]; then
    INPUT_FILE="$PROJECT_ROOT/outputs/tflite/model_int8.tflite"
fi

if [ "$OUTPUT_FILE" = "default" ]; then
    OUTPUT_FILE="$PROJECT_ROOT/firmware/model/model_data.cc"
fi

echo "Converting $INPUT_FILE to $OUTPUT_FILE..."

# Create the header with proper alignment
cat > "$OUTPUT_FILE" << 'EOF'
// file: firmware/model/model_data.cc
// purpose: TFLite model converted to C array with 4-byte alignment
// dependencies: generated from .tflite model

// Ensure 4-byte alignment for TFLite flatbuffer
alignas(4) const unsigned char MODEL_DATA[] = {
EOF

# Convert the binary to hex array, ensuring the first byte is properly aligned
# xxd automatically creates properly aligned arrays
xxd -i "$INPUT_FILE" | sed 's/unsigned char/const unsigned char/' >> "$OUTPUT_FILE"

# Add the length constant
echo "" >> "$OUTPUT_FILE"
echo "const unsigned int MODEL_DATA_LEN = sizeof(MODEL_DATA);" >> "$OUTPUT_FILE"
echo ";" >> "$OUTPUT_FILE"

echo "Conversion complete!"
echo "Model size: $(stat -f%z "$INPUT_FILE" 2>/dev/null || stat -c%s "$INPUT_FILE") bytes"
echo "Array length: $(grep -c '0x' "$OUTPUT_FILE") bytes"
