#!/usr/bin/env bash
# install-seqc.sh — run from the repo root inside Linux or WSL
# chmod +x install-seqc.sh
set -euo pipefail

SRC="build/dev/seqc_legacy"
DEST_DIR="/usr/local/bin"

[ -f "$SRC" ] || { echo "error: $SRC not found — build first" >&2; exit 1; }

# install = copy + set mode 755 in one step (the build output is 644 on the Windows drive)
sudo install -m 755 "$SRC" "$DEST_DIR/seqc_legacy"

hash -r
echo "installed: $(command -v seqc_legacy)"