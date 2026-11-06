# macOS (Apple Silicon only)

The macOS target builds an arm64 plugin for native X-Plane 12. Intel Macs and
X-Plane running under Rosetta are unsupported. The build targets macOS 11 or
newer; the simulator and aircraft may require a newer macOS version.

## Build

Install Xcode Command Line Tools (or Xcode), CMake 3.20+, and an
[X-Plane SDK 4.0 or newer](https://developer.x-plane.com/sdk/plugin-sdk-downloads/)
containing the arm64 XPLM framework. No Homebrew runtime libraries are required.

```sh
./scripts/build-macos.sh /absolute/path/to/SDK
# Detailed diagnostic build:
./scripts/build-macos.sh /absolute/path/to/SDK Debug
```

Alternatively set `XPLANE_SDK_ROOT` and omit the first argument. The script builds,
runs the existing CTest suite (including an offscreen CGL rendering regression),
packages resources, and applies an ad hoc signature. It creates:

```text
output/00_ToLissOANS-macos-arm64.zip
output/macos-release/00_ToLissOANS/
    64/mac.xpl
    assets/
    cache/
    config/
    config.ini
```

Debug output uses the `00_ToLissOANS-debug` name. Install only one flavor at a time.
The package has an ad hoc signature, not Developer ID signing or notarization.

For a direct CMake build:

```sh
cmake -S . -B build/macos-release -DSDK_ROOT=/absolute/path/to/SDK \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=arm64
cmake --build build/macos-release --parallel
ctest --test-dir build/macos-release --output-on-failure
cmake --install build/macos-release --prefix "$PWD/output/macos-release"
```

The plugin binary is also available in `dist/ToLissOANS/64/mac.xpl`; copy the
complete installed package when using it in X-Plane, since the binary needs its
assets and configuration.

## Install and use

Quit X-Plane and place `00_ToLissOANS` inside the ToLiss aircraft's `plugins`
directory, for example `X-Plane 12/Aircraft/ToLissA320/plugins/00_ToLissOANS`.
Start X-Plane natively and set the ND range to 10 NM. The plugin writes its
diagnostics and generated caches inside its own directory, which must be writable.
Keyboard shortcuts in the test window use Control+A / Control+V, matching
X-Plane's SDK key flags on macOS.

## Platform implementation

- CGL context handling and the OpenGL framework replace WGL. Legacy framebuffer
  and vertex-array calls use the macOS EXT/APPLE entry points.
- System libcurl handles HTTPS building downloads with certificate verification,
  timeouts, and the existing 32 MiB response limit.
- AppKit renders the bilingual About panel and reads clipboard text.
- Native paths are enabled before obtaining paths from XPLM; cache replacement
  uses POSIX rename, and Lua receives its native directory separator.
- The SDK XPLM framework is used only for linking. X-Plane supplies the real
  framework at runtime, following the [SDK packaging conventions](https://developer.x-plane.com/article/building-and-installing-plugins/).

Compilation, automated checks, and offscreen rendering do not establish cockpit
behavior. Check the ND overlay, mouse alignment, popup, FMS sampling and aircraft
switching in X-Plane with a supported ToLiss aircraft before a release.
