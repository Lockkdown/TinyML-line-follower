# file: tools/inspect_dataset.py
# purpose: inspect dataset distribution and display sample images
# dependencies: matplotlib, opencv-python

import os
from pathlib import Path
from typing import Dict, List

import cv2
import matplotlib.pyplot as plt


def count_per_class(data_dir: str, class_names: List[str]) -> Dict[str, int]:
    """Count number of images per class in dataset directory."""
    counts = {}
    for class_name in class_names:
        class_dir = Path(data_dir) / class_name
        if class_dir.exists():
            counts[class_name] = len([f for f in class_dir.iterdir() if f.is_file()])
        else:
            counts[class_name] = 0
    return counts


def has_enough_samples(counts: Dict[str, int], min_per_class: int) -> bool:
    """Check if all classes have at least min_per_class samples."""
    return all(count >= min_per_class for count in counts.values())


def show_samples(data_dir: str, class_name: str, n: int) -> None:
    """Display n sample images from a class in a grid."""
    class_dir = Path(data_dir) / class_name
    if not class_dir.exists():
        print(f"Class directory not found: {class_dir}")
        return
    
    image_files = [f for f in class_dir.iterdir() if f.is_file()][:n]
    if not image_files:
        print(f"No images found in {class_dir}")
        return
    
    cols = min(5, n)
    rows = (n + cols - 1) // cols
    
    fig, axes = plt.subplots(rows, cols, figsize=(15, 3 * rows))
    if rows == 1:
        axes = [axes] if cols == 1 else axes
    else:
        axes = axes.flatten()
    
    for i, img_file in enumerate(image_files):
        img = cv2.imread(str(img_file), cv2.IMREAD_GRAYSCALE)
        if img is not None:
            axes[i].imshow(img, cmap='gray')
            axes[i].set_title(f"{img_file.name}")
            axes[i].axis('off')
        else:
            axes[i].text(0.5, 0.5, f"Failed to load\n{img_file.name}", 
                         ha='center', va='center', transform=axes[i].transAxes)
            axes[i].axis('off')
    
    # Hide unused subplots
    for i in range(len(image_files), len(axes)):
        axes[i].axis('off')
    
    plt.suptitle(f"Sample images from class: {class_name}")
    plt.tight_layout()
    plt.show()
