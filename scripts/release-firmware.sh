#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
if [[ $# -ne 2 ]]; then
  echo 'Usage: bash scripts/release-firmware.sh v1.0.0 /path/to/keychron_q8_iso_encoder_kafka.bin' >&2
  exit 1
fi
release_tag=$1
firmware_path=$(realpath "$2")
[[ "$release_tag" =~ ^v[0-9]+\.[0-9]+\.[0-9]+([.-][A-Za-z0-9.-]+)?$ ]] || { echo 'Use a version tag such as v1.0.0.' >&2; exit 1; }
[[ -s "$firmware_path" ]] || { echo 'Firmware binary is missing or empty.' >&2; exit 1; }
command -v gh >/dev/null || { echo 'Install GitHub CLI and run gh auth login first.' >&2; exit 1; }
gh auth status
bash scripts/check.sh
# Tie the release to the exact pushed source rather than an arbitrary branch head.
[[ -z "$(git status --porcelain)" ]] || { echo 'Commit source changes before releasing.' >&2; exit 1; }
source_commit=$(git rev-parse HEAD)
scratch_dir=$(mktemp -d)
trap 'rm -rf "$scratch_dir"' EXIT
cp "$firmware_path" "$scratch_dir/keychron_q8_iso_encoder_kafka.bin"
(cd "$scratch_dir" && sha256sum keychron_q8_iso_encoder_kafka.bin > SHA256SUMS)
cat > "$scratch_dir/notes.md" <<NOTES
Keychron Q8 ISO encoder Kafka firmware.

Source commit: $source_commit

FN3/FN4 mean numeric VIA/Launcher layers 3 and 4. Each has 15 independent text or normal macros. Knob press cancels text or immediately replays a normal macro. Unicode text requires Windows WinCompose.

Export a backup before flashing. This binary is specific to the Q8 ISO encoder. Download SHA256SUMS to verify the binary. Source code is included in the release's source archive under firmware/kafka/.
NOTES
gh release create "$release_tag" \
  "$scratch_dir/keychron_q8_iso_encoder_kafka.bin" "$scratch_dir/SHA256SUMS" \
  --target "$source_commit" --title "Kafka $release_tag" --notes-file "$scratch_dir/notes.md"
