# VCPKG Dependencies

This document describes the platform compatibility configuration for dependencies in `vcpkg.json`.

## Licensing

The project license depends on whether the build links copyleft kernels:

- **CGAL** (`GPL-3.0-or-later`) and **OpenCascade** (`LGPL-2.1-only`) are copyleft. They are only available on the platforms shown in the dependency matrix below (desktop Windows/Linux/macOS, plus CGAL on Android).
- A build that includes any copyleft kernel is licensed under **GPL-3.0-or-later**.
- A build that includes none (iOS and game-console builds, or builds without the `copyleft` feature) is licensed under **MIT**.

In `vcpkg.json` this is expressed with a top-level `license` of `MIT` plus a `copyleft` feature whose `license` is `GPL-3.0-or-later`. The `copyleft` feature is enabled by default on `windows | linux | osx | android`, and **CGAL, libigl[cgal] and OpenCascade are installed only as dependencies of that feature**; when the feature is disabled they are neither installed nor linked. The effective license of a build is generated at configure time into `version.cpp` and exposed via `GetVersionInfo()` / `HsBaGetVersionJson()`.

On the CMake side, the `HSBA_COPL` option controls whether CGAL and OpenCascade are found and linked (and defines the `USE_CGAL`/`USE_OCCT` macros). The option is tri-state (default `AUTO`): with `AUTO` it resolves from the `VCPKG_MANIFEST_FEATURES` environment variable, then the `VCPKG_MANIFEST_NO_DEFAULT_FEATURES` environment variable, and finally probes whether CGAL/OpenCascade are installed (note that vcpkg default-features are NOT listed in `VCPKG_MANIFEST_FEATURES`, hence the install probe); `-DHSBA_COPL=ON/OFF` forces it explicitly. To produce an MIT-licensed build, disable the default features via the environment variable before configuring so vcpkg does not install the copyleft kernels:

```powershell
# Disable default-features (incl. copyleft); CGAL/OpenCascade are not installed or linked, HSBA_COPL becomes OFF automatically
$env:VCPKG_MANIFEST_NO_DEFAULT_FEATURES=1   # use `export VCPKG_MANIFEST_NO_DEFAULT_FEATURES=1` in bash
cmake -B build --preset x64-release
```

## Platform Exclusions

### Packages excluded for Android

The following packages are excluded on Android because they either have compatibility issues or are not suitable for mobile platforms:

- **boost-locale, boost-log, boost-dll, boost-nowide**: These Boost components have known compatibility issues on Android.
- **fontconfig**: Android uses its own font management system and does not require fontconfig.
- **bit7z**: Mobile applications generally do not need 7z compression functionality.
- **opencascade**: OpenCASCADE is too complex and is not suitable for mobile platforms.
- **sqlpp11** (MySQL/PostgreSQL features): VCPKG's MySQL and PostgreSQL support is not suitable for cross-compilation.
- **vcpkg-pkgconfig-get-modules**: Android build systems do not require pkg-config.

### Packages excluded for iOS

- **bit7z**: iOS applications generally do not need 7z compression functionality.
- **opencv**: OpenCV has build complexity issues on iOS.
- **opencascade**: OpenCASCADE is too complex and is not suitable for iOS.
- **sqlpp11** (MySQL/PostgreSQL features): VCPKG's MySQL and PostgreSQL support is not suitable for cross-compilation.

## CMakeLists.txt Dependency Fixes

### Issues addressed

1. **Conditional OpenCV lookup**: Conditioned the OpenCV search on iOS to avoid build failures.
2. **Conditional Boost.DLL linking**: Conditioned linking to Boost::dll in the `cadmodel` module.
3. **Added boost-date-time**: Added the missing `boost-date-time` dependency.
4. **Fixed boost-log linking**: Fixed Boost.Log linkage in the `fileoperator` module.

### Dependency matrix

| Component | Windows | Linux | macOS | Android | iOS |
| --------- | ------- | ----- | ----- | ------- | --- |
| boost-log | ✓ | ✓ | ✓ | ✗ | ✗ |
| boost-locale | ✓ | ✓ | ✓ | ✗ | ✗ |
| boost-dll | ✓ | ✓ | ✓ | ✗ | ✗ |
| boost-nowide | ✓ | ✓ | ✓ | ✗ | ✗ |
| boost-date-time | ✓ | ✓ | ✓ | ✓ | ✓ |
| opencv | ✓ | ✓ | ✓ | ✓ | ✗ |
| opencascade | ✓ | ✓ | ✓ | ✗ | ✗ |
| sqlpp11 (MySQL/PG) | ✓ | ✓ | ✓ | ✗ | ✗ |
| openvdb | ✓ | ✓ | ✓ | ✗ | ✗ |

## Build Recommendations

1. **Desktop platforms** (Windows/Linux/macOS): All packages should build successfully.
2. **Android platform**: Use a reduced dependency set, with SQLite as the primary dependency.
3. **iOS platform**: Exclude some complex libraries to ensure successful builds.

## Troubleshooting

If you encounter build failures:

1. Verify that all system dependencies are installed, especially `autoconf-archive` on macOS.
2. Confirm that `vcpkg` has been bootstrapped and updated correctly.
3. Review CI logs for the specific error details.
4. For mobile platforms, reconsider whether some of the excluded libraries are actually needed.
