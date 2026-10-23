#!/usr/bin/env bash
# ──────────────────────────────────────────────────────
# build.sh — Build Dynamicam .levipack for LeviLaunchroid
# Usage: ./build.sh [--clean] [Release|Debug]
# ──────────────────────────────────────────────────────
set -euo pipefail

ABIS=("arm64-v8a")
BUILD_TYPE="${2:-Release}"
CLEAN=false

for arg in "$@"; do
    case "$arg" in
        --clean) CLEAN=true ;;
    esac
done

SRC_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SRC_DIR/.." && pwd)"
PRELOADER_ROOT="$REPO_ROOT/app/src/main/cpp/preloader"
DIST_BASE="$SRC_DIR/dist"

if [[ ! -d "$PRELOADER_ROOT/include/pl/Config.hpp" ]]; then
    echo "❌ preloader-android SDK not found at $PRELOADER_ROOT"
    exit 1
fi

resolve_ndk() {
    local ndk=""
    if [[ -n "${ANDROID_NDK_HOME:-}" ]]; then
        ndk="$ANDROID_NDK_HOME"
    elif [[ -n "${ANDROID_NDK_ROOT:-}" ]]; then
        ndk="$ANDROID_NDK_ROOT"
    else
        for root in "${ANDROID_HOME:-}" "${ANDROID_SDK_ROOT:-}"; do
            if [[ -n "$root" && -d "$root/ndk" ]]; then
                ndk="$(ls -1d "$root/ndk/"*/ | sort -r | head -1)"
                break
            fi
            if [[ -n "$root" && -d "$root/ndk-bundle" ]]; then
                ndk="$root/ndk-bundle"
                break
            fi
        done
    fi
    if [[ -z "$ndk" || ! -f "$ndk/build/cmake/android.toolchain.cmake" ]]; then
        echo "❌ Android NDK not found."
        echo "   Set ANDROID_NDK_HOME or ANDROID_NDK_ROOT, or run with --ndk /path/to/ndk"
        exit 1
    fi
    echo "$ndk"
}

NDK_PATH=$(resolve_ndk)
TOOLCHAIN="$NDK_PATH/build/cmake/android.toolchain.cmake"
echo "📦 Using NDK: $NDK_PATH"
echo "📦 Using Preloader: $PRELOADER_ROOT"

build_host_config_gen() {
    local host_build_dir="$SRC_DIR/build/host-$BUILD_TYPE"
    rm -rf "$host_build_dir"
    mkdir -p "$host_build_dir"
    cd "$host_build_dir"
    cmake -S "$SRC_DIR" \
          -B . \
          -G Ninja \
          "-DCMAKE_BUILD_TYPE=$BUILD_TYPE" \
          "-DPRELOADER_ANDROID_ROOT=$PRELOADER_ROOT"
    cmake --build . --target dynamicam_config_gen
    echo "✅ Host config generator built"
    cd "$SRC_DIR"
}

get_generator() {
    local gen="$SRC_DIR/build/host-$BUILD_TYPE/dynamicam_config_gen"
    if [[ ! -x "$gen" ]]; then
        echo "❌ Config generator not found in $gen"
        exit 1
    fi
    echo "$gen"
}

build_android_abi() {
    local abi="$1"
    local android_build_dir="$SRC_DIR/build/android-${abi}-${BUILD_TYPE}"
    local dist_dir="$DIST_BASE/${abi}/dynamicam"

    if $CLEAN; then rm -rf "$android_build_dir" "$dist_dir"; fi
    mkdir -p "$android_build_dir"
    mkdir -p "$dist_dir/config"
    cd "$android_build_dir"

    echo "🔨 Building $abi ($BUILD_TYPE)..."
    cmake -S "$SRC_DIR" \
          -B . \
          -G Ninja \
          "-DCMAKE_TOOLCHAIN_FILE=$TOOLCHAIN" \
          "-DANDROID_ABI=$abi" \
          "-DANDROID_PLATFORM=android-26" \
          "-DCMAKE_BUILD_TYPE=$BUILD_TYPE" \
          "-DPRELOADER_ANDROID_ROOT=$PRELOADER_ROOT"
    cmake --build . --target dynamicam

    # Find the built .so
    local lib_file="$android_build_dir/out/$abi/libdynamicam.so"
    if [[ ! -f "$lib_file" ]]; then
        # Try alternate path
        lib_file="$android_build_dir/lib$abi/libdynamicam.so"
    fi
    if [[ ! -f "$lib_file" ]]; then
        echo "❌ Built library not found!"
        find "$android_build_dir" -name "*.so" 2>/dev/null || true
        exit 1
    fi

    echo "📋 Copying files to $dist_dir..."
    cp "$SRC_DIR/manifest.json" "$dist_dir/"
    cp "$lib_file" "$dist_dir/libdynamicam.so"
    cp "$SRC_DIR/config/config.json" "$dist_dir/" 2>/dev/null || true
    cp "$SRC_DIR/config/config.schema.json" "$dist_dir/" 2>/dev/null || true

    # Run config generator if available
    local gen
    gen=$(get_generator)
    if [[ -x "$gen" ]]; then
        echo "🔄 Running config generator..."
        "$gen" "$dist_dir"
    fi

    echo "✅ $abi complete → $dist_dir"
    cd "$SRC_DIR"
}

package_levipack() {
    local abi="$1"
    local dist_dir="$DIST_BASE/${abi}/dynamicam"
    local pack="$DIST_BASE/${abi}/dynamicam.levipack"
    local tmp_zip="$DIST_BASE/${abi}/dynamicam.zip"

    echo "📦 Creating $pack ..."
    (cd "$dist_dir" && zip -r -q "$tmp_zip" .)
    mv "$tmp_zip" "$pack"
    echo "✅ $pack created"
}

# ── Main ───────────────────────────────────────────────────

build_host_config_gen

generator=$(get_generator)

for abi in "${ABIS[@]}"; do
    build_android_abi "$abi"
    package_levipack "$abi"
done

echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "🎉 Build complete! Artifacts:"
echo ""
for abi in "${ABIS[@]}"; do
    echo "  📁 ${DIST_BASE}/${abi}/dynamicam/"
    echo "  📄 ${DIST_BASE}/${abi}/dynamicam.levipack"
done
echo ""
echo "To install: copy 'dynamicam/' directory into your LeviLaunchroid mods folder,"
echo "or import the .levipack through the launcher UI."
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
