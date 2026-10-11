#!/usr/bin/env bash
# Replace Google's old apt libedgetpu with a TF 2.16.1–matched runtime (feranick).
# That version skew is the usual cause of Coral C++ segfaults after AllocateTensors/Invoke.
#
# RUN ON THE PI. Does NOT rebuild TensorFlow Lite.
set -euo pipefail

if [[ "$(uname -s)" != "Linux" ]]; then
  echo "Run this on the Raspberry Pi." >&2
  exit 1
fi

ARCH="$(dpkg --print-architecture)"
case "$ARCH" in
  arm64) ;;
  armhf) ;;
  amd64) ;;
  *)
    echo "Unsupported arch: $ARCH" >&2
    exit 1
    ;;
esac

# Matches scripts/setup_coral_cpp.sh TF 2.16.1 commit.
TAG="16.0TF2.16.1-1"
VER="16.0tf2.16.1-1"
# Use std (throttled) for USB thermal safety; swap to max if you have cooling.
PKG="libedgetpu1-std_${VER}.bookworm_${ARCH}.deb"
URL="https://github.com/feranick/libedgetpu/releases/download/${TAG}/${PKG}"

TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

echo "==> Downloading $URL"
curl -fL -o "$TMP/$PKG" "$URL"

echo "==> Removing Google apt runtime if present (keeps your TFLite build)"
sudo apt-get remove -y libedgetpu1-std libedgetpu1-max 2>/dev/null || true

echo "==> Installing matched runtime"
sudo dpkg -i "$TMP/$PKG" || sudo apt-get install -f -y

SO_DIR="/usr/lib/aarch64-linux-gnu"
if [[ "$ARCH" == "armhf" ]]; then
  SO_DIR="/usr/lib/arm-linux-gnueabihf"
elif [[ "$ARCH" == "amd64" ]]; then
  SO_DIR="/usr/lib/x86_64-linux-gnu"
fi

if [[ -e "$SO_DIR/libedgetpu.so.1" && ! -e "$SO_DIR/libedgetpu.so" ]]; then
  sudo ln -sf libedgetpu.so.1 "$SO_DIR/libedgetpu.so"
fi
sudo ldconfig

echo "==> Installed:"
find /usr/lib -name 'libedgetpu.so*' 2>/dev/null || true
python3 -c 'from ctypes.util import find_library; print("ctypes:", find_library("edgetpu"))' 2>/dev/null || true

echo
echo "Unplug/replug the Coral USB stick, then:"
echo "  source third_party/coral/env.sh"
echo "  ./compile_and_run"
echo
echo "Or isolate first (no TUI):"
echo "  ./scripts/run_coral_smoke.sh"
