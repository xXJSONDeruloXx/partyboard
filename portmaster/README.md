# Party Board PortMaster bring-up

This directory contains the handheld target under active development. The
first milestone is a small SDL2/OpenGL ES probe that validates the RG28XX
window, GPU, controller, frame pacing, and Start+Select exit path before the
Mario Party runtime is connected.

The probe deliberately contains no game data and is not a playable port.

## Host build

```sh
cmake -S portmaster -B build/portmaster-host -G Ninja
cmake --build build/portmaster-host
PM_PROBE_SECONDS=5 ./build/portmaster-host/partyboard-portmaster-probe
```

## AArch64 build

The target device is AArch64. Point `PM_TARGET_LIB_DIR` at a directory holding
the device's SDL2, EGL, and GLESv2 libraries; PortMaster will use the CFW's
runtime copies rather than bundling them.

```sh
cmake -S portmaster -B build/portmaster-aarch64 -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE="$PWD/portmaster/cmake/Toolchain-aarch64-linux-gnu.cmake" \
  -DPM_TARGET_LIB_DIR=/path/to/rg28xx-libs \
  -DPM_SDL2_INCLUDE_DIR=/usr/include \
  -DPM_GLES2_INCLUDE_DIR=/usr/include
cmake --build build/portmaster-aarch64
```

The eventual package will keep the launcher, binary, logs, and user save/data
directories under `ports/partyboard/`. The Mario Party disc image will remain
user-provided and will never be committed here.
