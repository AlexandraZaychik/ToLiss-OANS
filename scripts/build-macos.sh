#!/bin/bash
set -euo pipefail

project_root="$(cd "$(dirname "$0")/.." && pwd)"
sdk_root="${1:-${XPLANE_SDK_ROOT:-}}"
flavor="${2:-Release}"
if [[ "$(uname -s)" != Darwin || "$(uname -m)" != arm64 ]]; then
  echo "Run this script natively on an Apple Silicon Mac." >&2
  exit 1
fi
if [[ -z "$sdk_root" || ! -f "$sdk_root/CHeaders/XPLM/XPLMPlugin.h" ]]; then
  echo "Usage: $0 /path/to/SDK [Release|Debug] (or set XPLANE_SDK_ROOT)" >&2
  exit 1
fi
case "$flavor" in
  Release) build_name=macos-release; debug=OFF; package_name=ToLissOANS ;;
  Debug) build_name=macos-debug; debug=ON; package_name=ToLissOANS-debug ;;
  *) echo "Build flavor must be Release or Debug." >&2; exit 1 ;;
esac
build_dir="$project_root/build/$build_name"
package_root="$project_root/output/$build_name"
cmake -S "$project_root" -B "$build_dir" \
  -DSDK_ROOT="$sdk_root" -DCMAKE_OSX_ARCHITECTURES=arm64 \
  -DCMAKE_BUILD_TYPE="$flavor" -DOANS_DEBUG="$debug"
cmake --build "$build_dir" --parallel
ctest --test-dir "$build_dir" --output-on-failure
cmake --install "$build_dir" --prefix "$package_root"
binary="$package_root/00_$package_name/64/mac.xpl"
[[ "$(lipo -archs "$binary")" == arm64 ]]
codesign --force --sign - "$binary"
codesign --verify --strict "$binary"
archive="$project_root/output/00_$package_name-macos-arm64.zip"
ditto -c -k --norsrc --noextattr --keepParent "$package_root/00_$package_name" "$archive"
echo "Built and packaged: $archive"
