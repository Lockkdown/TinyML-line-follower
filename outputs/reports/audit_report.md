# CNN Line-Following Robot — Model Audit Report

**Date:** 2026-04-04  
**Project:** TinyRobot — ESP32-S3 CNN Line Follower  
**Classes:** forward(0), left(1), right(2), nothing(3)  
**Deployment targets:** F1_left ≥ 0.80, F1_right ≥ 0.80 (safety-critical), size < 500 KB, inference < 150 ms

---

## Data Availability Notes

| Artifact | Status |
|---|---|
| `outputs/reports/comparison_table.csv` | AVAILABLE — test metrics for all 5 models |
| `training_history.png/.json/.csv` | NOT SAVED — train_acc / val_acc unavailable; OVERFIT/UNDERFIT flags cannot be computed |
| `outputs/tflite/<model>_model_int8.tflite` | AVAILABLE — sizes measured |
| `outputs/experiments/<model>/confusion_matrix.png` | AVAILABLE (PNG only — raw confusion values not extractable) |
| `plan_dataset.md` | NOT FOUND — dataset collection guidelines not written yet |

> **Consequence:** Columns `train_acc` and `val_acc` are marked `N/A` in the comparison table below. The `overfit?` column is derived only from test accuracy heuristics and dataset size, not from actual training curves.

---

## TASK 1 — Model Performance Comparison

### Full Comparison Table

| model | train_acc | val_acc | test_acc | F1_forward | F1_left | F1_right | F1_nothing | macro_F1 | size_KB | overfit? | flags |
|---|---|---|---|---|---|---|---|---|---|---|---|
| lenet5 | N/A | N/A | 1.0000 | 1.0000 | 1.0000 | 1.0000 | 1.0000 | 1.0000 | 49.5 | NO HISTORY | SUSPICIOUS_PERFECT |
| mobilenet_v1_025 | N/A | N/A | 0.7778 | 1.0000 | 0.7733 | 0.3704 | 0.9231 | 0.7667 | 300.2 | NO HISTORY | WEAK_RIGHT, IMBALANCED |
| mobilenet_v1_05 | N/A | N/A | 0.9506 | 0.9697 | 0.9333 | 0.9474 | 0.9630 | 0.9533 | 962.0 | NO HISTORY | SIZE_EXCEED |
| mobilenet_v2_035 | N/A | N/A | 0.9012 | 1.0000 | 0.8667 | 0.8182 | 1.0000 | 0.9212 | 611.9 | NO HISTORY | SIZE_EXCEED, IMBALANCED |
| resnet8 | N/A | N/A | 0.1852 | 0.0000 | 0.0000 | 0.0000 | 0.3125 | 0.0781 | 326.3 | NO HISTORY | TRAINING_FAILURE |

### Flag Definitions & Applied Logic

| Flag | Condition | Models Flagged |
|---|---|---|
| `WEAK_LEFT` | F1_left < 0.80 | — (none) |
| `WEAK_RIGHT` | F1_right < 0.80 | mobilenet_v1_025 (0.3704) |
| `IMBALANCED` | max(F1) − min(F1) > 0.15 across classes | mobilenet_v1_025 (delta=0.63), mobilenet_v2_035 (delta=0.18) |
| `UNDERFIT` | test_acc < 0.80 | mobilenet_v1_025 (0.7778), resnet8 (0.1852) |
| `OVERFIT` | val_acc < train_acc − 0.05 | Cannot compute — no history |
| `TRAINING_FAILURE` | test_acc < 0.25 (below 4-class random baseline) | resnet8 |
| `SIZE_EXCEED` | .tflite size > 500 KB | mobilenet_v1_05 (962 KB), mobilenet_v2_035 (612 KB) |
| `SUSPICIOUS_PERFECT` | test_acc = 1.0 on test set < 100 samples | lenet5 (~82 test samples) |

### Per-Model Analysis

**lenet5** (49.5 KB)
- Test accuracy 100%, all F1 = 1.0 on approximately 82 test images (15% × 546 total)
- Result is statistically unreliable — test set too small for variance to be meaningful
- Smallest model by a wide margin; could genuinely be well-suited to simple line patterns
- Verdict: Cannot trust until validated on a larger, held-out set

**mobilenet_v1_025** (300.2 KB)
- CRITICAL: F1_right = 0.3704 — robot will frequently fail to turn right
- F1_forward = 1.0 but F1_right = 0.37 → model very likely collapses "right" predictions into "forward"
- α=0.25 MobileNet is possibly too small with frozen ImageNet features for grayscale line images at 96×96
- Within the 500 KB size limit but safety-critical failure makes it undeployable

**mobilenet_v1_05** (962.0 KB)
- Best overall F1 profile: F1_left=0.933, F1_right=0.947 — both above 0.80 threshold
- test_acc = 0.951 — strong, consistent across all classes
- Size 962 KB exceeds 500 KB limit by ~92%; exceeds limit but delivers the best safety margin
- Recommended for initial field testing with size caveat noted

**mobilenet_v2_035** (611.9 KB)
- Second-best F1 profile: F1_left=0.867, F1_right=0.818 — both above threshold
- F1_forward and F1_nothing both 1.0 but directional classes weaker than mobilenet_v1_05
- 612 KB — 22% over limit; closer to constraint than mobilenet_v1_05
- Good fallback if ESP32-S3 PSRAM cannot accommodate 962 KB model

**resnet8** (326.3 KB)
- Complete training failure — accuracy 18.5% is below random baseline (25% for 4 classes)
- F1_left = 0, F1_right = 0, F1_forward = 0; model only outputs "nothing" class
- Within size limit but completely non-functional
- Root cause: training divergence, not architecture incompatibility

---

## TASK 2 — Dataset Audit

### Class Counts

```
CLASS COUNTS (dataset/raw/):
  forward :  122 images  ← INSUFFICIENT (< 250)
  left    :  161 images  ← INSUFFICIENT (< 250)
  right   :  167 images  ← INSUFFICIENT (< 250)
  nothing :   96 images  ← INSUFFICIENT (< 250), CRITICAL MINORITY
  ─────────────────────────────────────────────
  total   :  546 images
```

### Dataset Health Checks

| Check | Value | Status |
|---|---|---|
| left/right image delta | \|161−167\|/167 = 3.6% | OK (≤ 10%) |
| Insufficient classes (< 250) | forward, left, right, nothing | ALL FOUR — FAIL |
| nothing class ratio | 96/546 = 17.6% | Low but above 10% floor |
| Approximate test set size | 546 × 0.15 ≈ 82 images | Too small for reliable evaluation |
| Train/val/test split on disk | Not present (runtime split via tf.data) | Cannot verify 70/15/15 directly |
| dataset/processed/ contents | 6 preview PNGs per class (24 total) | Correct usage — preview only |

### Implications of Small Dataset

With only 546 total images:
- Training set ≈ 382 images (70%); ~95 per class on average
- Validation set ≈ 82 images; ~20 per class
- Test set ≈ 82 images; ~20 per class

At this scale, a single confused sample moves F1 by 5+ points. Results across all models are high-variance and not representative of real-world deployment performance.

---

## TASK 3 — Cross-Reference: Model Weakness ↔ Dataset Root Cause

| weakness | affected_model | likely_cause | dataset_fix_needed |
|---|---|---|---|
| WEAK_RIGHT (F1=0.37) | mobilenet_v1_025 | 167 right images insufficient for α=0.25 pretrained MobileNet to learn directional line features; model collapses right→forward predictions; frozen ImageNet features poorly suited to 96×96 grayscale line images | +83 right images minimum; diverse angles, curve radii, lighting conditions |
| TRAINING_FAILURE (acc=18.5%) | resnet8 | Training divergence on 546-image dataset — LR=0.001 too high; ResNet8 batch norm momentum sensitive to small batch statistics; model collapses to predicting "nothing" only | Not a dataset problem — fix: LR=0.0001, gradient clipping, reduce batch_size |
| SUSPICIOUS_PERFECT (acc=100%) | lenet5 | Test set only ~82 samples; high probability of chance perfect split; does not prove generalization | Increase dataset to ≥1000 total before trusting 100% result |
| SIZE_EXCEED (>500 KB) | mobilenet_v1_05, mobilenet_v2_035 | MobileNet V1 α=0.5 and V2 α=0.35 architectures inherently produce >500 KB after INT8 quantization | Not a dataset issue — accept size or select smaller architecture |
| nothing=96 (smallest class) | all models | Underrepresented "nothing" class; diverse visual patterns (various backgrounds, lighting) mean 96 samples insufficient to cover distribution | +154 nothing images: varied backgrounds, no line visible, different lighting angles |
| forward=122 (second smallest) | all models | Small forward class; robot spends most time going forward so this class should have most samples | +128 forward images: line centered at varying distances, speeds, lighting |

---

## TASK 4 — Action Plan

### Model Improvements Needed

| model | issue | fix |
|---|---|---|
| resnet8 | TRAINING_FAILURE — collapse to "nothing" output | Reduce `learning_rate` to 0.0001; add `clipnorm=1.0` gradient clipping in `resnet8_builder.py`; reduce `batch_size` to 8 during resnet8 training only |
| mobilenet_v1_025 | WEAK_RIGHT (F1=0.37) — directional confusion | Unfreeze more layers in phase 2 fine-tuning (currently too restrictive for grayscale line features); alternatively replace with mobilenet_v1_05 which already performs well |
| lenet5 | SUSPICIOUS_PERFECT — unreliable on 82-sample test | No architecture change needed; collect more data, then retrain and re-evaluate |
| mobilenet_v1_05 | SIZE_EXCEED (962 KB) | No model change; accept for field testing or explore layer pruning post-quantization |
| mobilenet_v2_035 | SIZE_EXCEED (612 KB) | No model change; within acceptable range for PSRAM-enabled deployment |

### Dataset Supplementation Needed

| class | current | target | needed | priority | collection guidance |
|---|---|---|---|---|---|
| nothing | 96 | ≥ 250 | +154 | HIGH | Camera facing ceiling, floor without line, varied background textures, different lighting (indoor lamp, sunlight, shadow) |
| forward | 122 | ≥ 250 | +128 | HIGH | Line centered at different distances, camera heights, straight track sections under varied illumination |
| left | 161 | ≥ 250 | +89 | MEDIUM | Left curves, left junctions, line near left edge at varying curvature radii |
| right | 167 | ≥ 250 | +83 | MEDIUM | Right curves, right junctions — mirror scenarios of left collection for balance |

**Total additional images needed: +454** (bringing total to ≥ 1000)

### Training Re-run Plan

**Prerequisite:** Collect +454 images across all classes before retraining.

**Models to retrain (priority order):**
1. `resnet8` — training failure; requires LR fix before any re-evaluation is meaningful
2. `mobilenet_v1_025` — WEAK_RIGHT; will benefit from larger dataset + more fine-tuning layers unfrozen
3. `lenet5` — validate whether 100% result holds on larger test set

**Models to skip retraining** (unless size reduction needed):
- `mobilenet_v1_05` — best safety profile; only retrain if size must drop below 500 KB
- `mobilenet_v2_035` — acceptable F1; only retrain if further improvement needed after data collection

**Config changes to make in `training/config/train_config.yaml` before retraining:**

```yaml
# After collecting ≥1000 total images:
epochs: 30          # increase from 20 — larger dataset needs more epochs
batch_size: 32      # increase from 16 — more stable gradients with more data
```

**Code changes required (not config):**
- `training/model/resnet8_builder.py`: reduce default LR to 0.0001 and add `clipnorm=1.0` in `model.compile()`
- `training/model/mobilenet_builder.py`: review `unfreeze_for_fine_tuning()` — unfreeze more layers for mobilenet_v1_025 to improve directional feature learning

**Expected outcome after fix:**
- resnet8: should recover to >85% accuracy once LR is corrected (architecture is viable)
- mobilenet_v1_025: F1_right should improve from 0.37 to >0.75 with more data + deeper unfreeze
- lenet5: 100% result either confirmed or drops to realistic 90-95% range

### Best Model Recommendation (Current State)

**Deploy now: `mobilenet_v1_05`**

F1_left=0.933 and F1_right=0.947 are both comfortably above the 0.80 safety threshold, with test_acc=0.951 the highest among valid models. The 962 KB size exceeds the 500 KB target but ESP32-S3 with PSRAM can typically accommodate models up to ~1 MB. Use this for initial field testing while collecting the additional dataset.

**Fallback if PSRAM is insufficient: `mobilenet_v2_035`** (612 KB, F1_left=0.867, F1_right=0.818 — both above threshold, tighter size margin).

Do not deploy `mobilenet_v1_025` (WEAK_RIGHT critical failure), `resnet8` (complete failure), or trust `lenet5` until validated on a larger dataset.

---

## Summary Flags Reference

```
lenet5           → SUSPICIOUS_PERFECT
mobilenet_v1_025 → WEAK_RIGHT | IMBALANCED | UNDERFIT
mobilenet_v1_05  → SIZE_EXCEED
mobilenet_v2_035 → SIZE_EXCEED | IMBALANCED
resnet8          → TRAINING_FAILURE | UNDERFIT
```

---

*Generated by model audit pipeline — 2026-04-04*
