# VCPKG Dependencies

This document describes the platform compatibility configuration for dependencies in `vcpkg.json`.

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
