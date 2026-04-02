# file: training/model/quantize.py
# purpose: INT8 post-training quantization of SavedModel → TFLite flatbuffer
# dependencies: dataset_loader.make_representative_dataset()

import tensorflow as tf
import numpy as np
from pathlib import Path


def quantize_model(
    saved_model_path: str,
    representative_dataset: callable
) -> bytes:
    """
    Convert SavedModel to INT8 TFLite flatbuffer.
    representative_dataset: generator yielding (1, 96, 96, 1) float32 in [-1, 1].
    Returns raw TFLite bytes.
    """
    converter = tf.lite.TFLiteConverter.from_saved_model(saved_model_path)
    converter.optimizations = [tf.lite.Optimize.DEFAULT]
    converter.representative_dataset = representative_dataset
    converter.target_spec.supported_ops = [tf.lite.OpsSet.TFLITE_BUILTINS_INT8]
    converter.inference_input_type = tf.int8
    converter.inference_output_type = tf.int8
    return converter.convert()


def save_tflite(tflite_bytes: bytes, output_path: str) -> None:
    """Write TFLite bytes to output_path. Creates parent dirs if needed."""
    Path(output_path).parent.mkdir(parents=True, exist_ok=True)
    Path(output_path).write_bytes(tflite_bytes)


def verify_tflite(tflite_path: str) -> dict:
    """
    Load TFLite model, run one dummy inference, return stats.
    Returns {input_shape, input_dtype, output_shape, output_dtype, file_size_kb}.
    Raises AssertionError if output_shape != (1, 4) or dtype != int8.
    """
    # Load interpreter
    interpreter = tf.lite.Interpreter(model_path=tflite_path)
    interpreter.allocate_tensors()
    
    # Get details
    input_details = interpreter.get_input_details()[0]
    output_details = interpreter.get_output_details()[0]
    input_shape = input_details['shape'].tolist()
    output_shape = output_details['shape'].tolist()
    
    # Verify specs
    assert input_shape == [1, 96, 96, 1], f"Wrong input shape: {input_shape}"
    assert input_details['dtype'] == np.int8, f"Wrong input dtype: {input_details['dtype']}"
    assert output_shape == [1, 4], f"Wrong output shape: {output_shape}"
    assert output_details['dtype'] == np.int8, f"Wrong output dtype: {output_details['dtype']}"
    
    # Run dummy inference
    dummy_input = np.zeros(input_details['shape'], dtype=np.int8)
    interpreter.set_tensor(input_details['index'], dummy_input)
    interpreter.invoke()
    output = interpreter.get_tensor(output_details['index'])
    
    # Return stats
    file_size_kb = Path(tflite_path).stat().st_size / 1024
    
    return {
        'input_shape': input_shape,
        'input_dtype': str(input_details['dtype']),
        'output_shape': output_shape,
        'output_dtype': str(output_details['dtype']),
        'file_size_kb': file_size_kb,
        'verified': True
    }


def log_model_stats(stats: dict, model_name: str) -> None:
    """Print model stats table. Append size_kb and verified=True to comparison_table.csv."""
    print(f"\n=== {model_name} TFLite Stats ===")
    print(f"File size: {stats['file_size_kb']:.1f} KB")
    print(f"Input: {stats['input_dtype']} {stats['input_shape']}")
    print(f"Output: {stats['output_dtype']} {stats['output_shape']}")
    print(f"Verified: {stats['verified']}")
