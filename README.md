# TinyRobot — CNN Line-Following Robot on ESP32-S3

A lightweight embedded AI project that runs real-time image classification entirely on-device using TensorFlow Lite Micro on an ESP32-S3 microcontroller. No PC required during operation.

---

## Overview

The robot captures grayscale frames from an OV3660 camera, classifies the position of a black line on the track into one of 4 classes, and drives two DC motors accordingly — all within ~50–100ms per frame.

| Class | Meaning | Action |
|-------|---------|--------|
| `forward` | Line centered | Go straight |
| `left` | Line on left side | Turn left |
| `right` | Line on right side | Turn right |
| `nothing` | No line visible | Stop / search |

---

## Hardware

| Component | Details |
|-----------|---------|
| MCU | ESP32-S3 N16R8 (16MB Flash, 8MB PSRAM) |
| Camera | OV3660 — 96×96 grayscale |
| Motor Driver | L298N dual H-bridge |
| Motors | 2× DC gearbox motors |
| Power | Li-ion power bank (5V) + LM2596 step-down for motors |

---

## Model Architecture

Five CNN architectures are trained and benchmarked. The best model (by accuracy + size + latency) is selected for deployment.

| Model | Parameters | Strategy |
|-------|-----------|---------|
| LeNet-5 variant | ~200K | Train from scratch |
| MobileNet V1 α=0.25 | ~460K | ImageNet pretrained + fine-tune |
| MobileNet V1 α=0.5 | ~1.3M | ImageNet pretrained + fine-tune |
| MobileNet V2 α=0.35 | ~400K | ImageNet pretrained + fine-tune |
| ResNet-8 | ~270K | Train from scratch |

- **Input**: `(96, 96, 1)` grayscale, normalized to `[-1.0, 1.0]`
- **Output**: 4-class softmax
- **Deployment format**: INT8 TFLite (post-training quantization)
- **Target size**: < 500KB `.tflite`

---

## Project Structure

```
line-follower-cnn/
├── training/        # Python ML code (PC only)
│   ├── config/      # Hyperparameters (train_config.yaml)
│   ├── data/        # Image preprocessing & augmentation
│   ├── model/       # Model builders + INT8 quantization
│   └── evaluate/    # Metrics, confusion matrix
├── firmware/        # ESP32-S3 Arduino C++ code
│   ├── src/         # Camera, inference, motor, controller
│   └── model/       # model_data.cc (TFLite C array)
├── tools/           # Dataset capture & inspection utilities
├── tests/           # pytest unit tests
├── dataset/         # Raw & processed images
└── outputs/         # Training artifacts (gitignored)
```

---

## Pipeline

```
OV3660 Camera (WiFi capture)
    └─→ dataset/raw/
        └─→ training/data/augmentation.py  →  dataset/processed/
            └─→ training/train.py  →  outputs/experiments/<model>/
                └─→ training/export.py  →  model_int8.tflite
                    └─→ tools/convert_to_cc.sh  →  firmware/model/model_data.cc
                        └─→ Flash to ESP32-S3  →  On-device inference
```

---

## Status

> This project is currently in active development
> Dataset collection, training results, and benchmark numbers will be added once available.

---
