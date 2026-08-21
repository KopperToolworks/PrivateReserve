#!/usr/bin/env bash
# Copy production firmware from the shop tree into this folder.
# Run from the secret-door repo. Does nothing useful in the published
# GitHub tree (there is no ../../firmware shop copy).
set -euo pipefail

here="$(cd "$(dirname "$0")" && pwd)"
shop="$(cd "$here/../../firmware" && pwd)"

if [[ ! -f "$shop/src/prod/main.cpp" ]]; then
  echo "Shop firmware not found at $shop" >&2
  echo "This script is for the secret-door repo, not the published tree." >&2
  exit 1
fi

mkdir -p "$here/src" "$here/images"

rsync -a --delete --exclude='.git' "$shop/src/prod/" "$here/src/prod/"

for f in bottle_key.cpp bottle_key.h adc_util.cpp adc_util.h \
         as5600.cpp as5600.h config.h ec11_calibration.h \
         wifi_secrets.h.example; do
  cp "$shop/src/$f" "$here/src/$f"
done

# Public SoftAP defaults only. Never copy the shop wifi_secrets.h (STA).
cp "$here/src/wifi_secrets.h.example" "$here/src/wifi_secrets.h"

bin="$shop/.pio/build/tdisplay_s3_prod/firmware.bin"
if [[ -f "$bin" ]]; then
  cp "$bin" "$here/images/tdisplay_s3_prod.bin"
  echo "copied images/tdisplay_s3_prod.bin"
else
  echo "no shop firmware.bin yet (build tdisplay_s3_prod to include it)"
fi

echo "copied production sources from $shop"
