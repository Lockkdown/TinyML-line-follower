# file: firmware/scripts/pio_pre_tflm.py
# purpose: pre-build patches for esp-tflite-micro + esp-nn under .pio/libdeps
# dependencies: patches/tensorflow/lite/micro/compatibility.h, patches/esp-nn/library.json

Import("env")

import json
from pathlib import Path
import shutil

# Generic kernels that are REPLACED by kernels/esp_nn/ equivalents.
# Both define the same TFLMRegistration symbol (e.g. Register_CONV_2D).
# When both are archived into libesp-tflite-micro.a, the linker picks the
# FIRST occurrence — the generic one — so the ESP-NN SIMD path is never called.
# Fix: overwrite generic files with empty stubs before compilation.
_ESP_NN_REPLACED_KERNELS = [
    "tensorflow/lite/micro/kernels/add.cc",
    "tensorflow/lite/micro/kernels/conv.cc",
    "tensorflow/lite/micro/kernels/depthwise_conv.cc",
    "tensorflow/lite/micro/kernels/fully_connected.cc",
    "tensorflow/lite/micro/kernels/mul.cc",
    "tensorflow/lite/micro/kernels/pooling.cc",
    "tensorflow/lite/micro/kernels/softmax.cc",
]


def _patch_library_json_include_dir(lib_json_path: Path) -> None:
    """Remove array includeDir from a library.json so PlatformIO 6.x LDF doesn't crash."""
    if not lib_json_path.is_file():
        return
    try:
        data = json.loads(lib_json_path.read_text(encoding="utf-8"))
    except (json.JSONDecodeError, OSError):
        return
    build = data.get("build", {})
    if not isinstance(build.get("includeDir"), list):
        return
    del build["includeDir"]
    data["build"] = build
    lib_json_path.write_text(
        json.dumps(data, indent=2, ensure_ascii=False), encoding="utf-8"
    )


def patch_esp_tflite_micro_library_json() -> None:
    project_dir = Path(env["PROJECT_DIR"])
    pioenv = env["PIOENV"]
    lib_json = (
        project_dir / ".pio" / "libdeps" / pioenv / "esp-tflite-micro" / "library.json"
    )
    _patch_library_json_include_dir(lib_json)
    if not lib_json.is_file():
        return
    try:
        data = json.loads(lib_json.read_text(encoding="utf-8"))
    except (json.JSONDecodeError, OSError):
        return
    build = data.get("build", {})
    # Remove any leftover srcFilter injected by a previous script run.
    # The stub approach handles kernel exclusion at the file level instead.
    changed = False
    if "srcFilter" in build:
        del build["srcFilter"]
        data["build"] = build
        changed = True
    if changed:
        lib_json.write_text(
            json.dumps(data, indent=2, ensure_ascii=False), encoding="utf-8"
        )


def stub_generic_kernels_replaced_by_esp_nn() -> None:
    """
    Overwrite generic kernel .cc files with empty stubs so the linker picks
    Register_CONV_2D() etc. from kernels/esp_nn/, not from the generic file.
    The _common.cc helpers shared by both versions are left intact.
    """
    project_dir = Path(env["PROJECT_DIR"])
    pioenv = env["PIOENV"]
    lib_dir = project_dir / ".pio" / "libdeps" / pioenv / "esp-tflite-micro"
    if not lib_dir.is_dir():
        return
    for kernel_rel in _ESP_NN_REPLACED_KERNELS:
        generic_path = lib_dir / kernel_rel
        esp_nn_rel = kernel_rel.replace("/kernels/", "/kernels/esp_nn/")
        esp_nn_path = lib_dir / esp_nn_rel
        if not generic_path.is_file() or not esp_nn_path.is_file():
            continue
        generic_path.write_text(
            f"// stub — replaced by {esp_nn_rel}\n", encoding="utf-8"
        )


def patch_esp_nn_library_json() -> None:
    project_dir = Path(env["PROJECT_DIR"])
    pioenv = env["PIOENV"]
    src = project_dir / "patches" / "esp-nn" / "library.json"
    dst = project_dir / ".pio" / "libdeps" / pioenv / "esp-nn" / "library.json"
    if not src.is_file():
        return
    if not dst.parent.is_dir():
        return
    shutil.copy2(src, dst)


def apply_tflite_compatibility_patches() -> None:
    project_dir = Path(env["PROJECT_DIR"])
    pioenv = env["PIOENV"]
    src = project_dir / "patches" / "tensorflow" / "lite" / "micro" / "compatibility.h"
    if not src.is_file():
        return
    rel_suffix = Path("tensorflow") / "lite" / "micro" / "compatibility.h"
    roots = [
        project_dir / ".pio" / "libdeps" / pioenv / "esp-tflite-micro",
        project_dir / ".pio" / "libdeps" / pioenv / "TensorFlowLite_ESP32" / "src",
    ]
    for root in roots:
        dst = root / rel_suffix
        if not dst.parent.is_dir():
            continue
        shutil.copy2(src, dst)


def force_build_esp_nn() -> None:
    """
    Bypass LDF and directly add esp-nn sources to the build via SCons.
    Needed because PlatformIO's LDF does not compile transitive library
    dependencies (esp-tflite-micro calls esp-nn but doesn't declare it).
    """
    project_dir = Path(env["PROJECT_DIR"])
    pioenv = env["PIOENV"]
    esp_nn_src = project_dir / ".pio" / "libdeps" / pioenv / "esp-nn" / "src"
    if not esp_nn_src.is_dir():
        return
    env.BuildSources(
        str(project_dir / ".pio" / "build" / pioenv / "esp-nn"),
        str(esp_nn_src),
        src_filter=[
            "+<**/*.c>",
            "+<**/*.S>",
            "-<convolution/esp_nn_conv_esp32p4.c>",
        ],
    )


patch_esp_tflite_micro_library_json()
patch_esp_nn_library_json()
apply_tflite_compatibility_patches()
stub_generic_kernels_replaced_by_esp_nn()
force_build_esp_nn()
