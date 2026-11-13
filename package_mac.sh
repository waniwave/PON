#!/bin/bash
# Builds the macOS VST3 + AU (universal: Apple Silicon + Intel, macOS 11.0+), ad-hoc signs them and creates
#   dist/PON-<version>-mac.zip  =  PON.vst3/ + PON.component/ + README.txt
# Runs on macOS with Xcode command line tools + CMake (used by GitHub Actions, works on any Mac).
#   ./package_mac.sh            plugin only
#   ./package_mac.sh --tests    also build and run PONRenderTest
#
# Developer ID で署名・公証する場合は README.md の「macOS: 署名と公証」を参照。
set -euo pipefail
cd "$(dirname "$0")"

TESTS=OFF
[[ "${1:-}" == "--tests" ]] && TESTS=ON

VERSION=$(sed -n 's/.*project(PON VERSION \([0-9.]*\).*/\1/p' CMakeLists.txt)
BUILD=build-mac

cmake -S . -B "$BUILD" -G "Unix Makefiles" \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" \
      -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0 \
      -DPON_BUILD_TESTS=$TESTS
cmake --build "$BUILD" --config Release -j "$(sysctl -n hw.ncpu)"

if [[ "$TESTS" == "ON" ]]; then
    "$BUILD/PONRenderTest_artefacts/Release/PONRenderTest" "$BUILD/renders"
fi

VST3="$BUILD/PON_artefacts/Release/VST3/PON.vst3"
AU="$BUILD/PON_artefacts/Release/AU/PON.component"
[[ -d "$VST3" ]] || { echo "ERROR: $VST3 not found"; exit 1; }
[[ -d "$AU" ]]   || { echo "ERROR: $AU not found"; exit 1; }

# Developer ID がない場合: アドホック署名（Apple Silicon で読み込むには最低限これが必要）
for B in "$VST3" "$AU"; do
    codesign --force --deep --sign - "$B"
    codesign --verify --deep --strict "$B"
done
lipo -info "$VST3/Contents/MacOS/PON"
lipo -info "$AU/Contents/MacOS/PON"

STAGE=dist/stage-mac
ZIP="dist/PON-$VERSION-mac.zip"
rm -rf "$STAGE" "$ZIP"
mkdir -p "$STAGE"
cp -R "$VST3" "$STAGE/"
cp -R "$AU" "$STAGE/"
cp dist/README_mac.txt "$STAGE/README.txt"
# ditto は署名・シンボリックリンク・属性を壊さずに ZIP にする
ditto -c -k --sequesterRsrc "$STAGE" "$ZIP"
rm -rf "$STAGE"

echo ""
echo "PACKAGE CREATED: $ZIP"
