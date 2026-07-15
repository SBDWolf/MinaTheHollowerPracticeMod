#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
WIN_BIN_DIR="$SCRIPT_DIR/../x64/Release"
LINUX_BIN_DIR="$SCRIPT_DIR/../build"
FILES_DIR="$SCRIPT_DIR/../otherFiles"

OUT_DIR="$SCRIPT_DIR/out"
mkdir -p "$OUT_DIR"

package_platform() {
    local ext="$1"
    local bin_dir="$2"
    local zip_name="$3"
    
    local temp_dir
    temp_dir=$(mktemp -d "$OUT_DIR/.tmp_XXXXXX")
    mkdir -p "$temp_dir/PracticeMod/data"

    if [[ ! -f "$bin_dir/mod.${ext}" ]]; then
        echo "Warning: mod.${ext} not found in $bin_dir"
        return 1
    fi
    if [[ ! -f "$FILES_DIR/mod.yc" ]]; then
        echo "Warning: mod.yc not found in $FILES_DIR"
        return 1
    fi
    if [[ ! -f "$FILES_DIR/debugdraw.font.yc" ]]; then
        echo "Warning: debugdraw.font.yc not found in $FILES_DIR"
        return 1
    fi

    cp "$bin_dir/mod.${ext}" "$temp_dir/PracticeMod/"
    cp "$FILES_DIR/mod.yc" "$temp_dir/PracticeMod/"
    cp "$FILES_DIR/debugdraw.font.yc" "$temp_dir/PracticeMod/data/"

    echo "Attempting to create zip..."
    
    (cd "$temp_dir" && zip -r "$OUT_DIR/$zip_name" PracticeMod/)
    rm -rf "$temp_dir"
}

echo "Packaging mod.dll..."
package_platform dll "$WIN_BIN_DIR" "PracticeMod_Windows.zip" || true
echo
echo "Packaging mod.so..."
package_platform so "$LINUX_BIN_DIR" "PracticeMod_Linux.zip" || true

echo "Exiting."
