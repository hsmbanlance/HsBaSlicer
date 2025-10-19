# Android skeleton

This folder contains a minimal Android app skeleton for packaging the native libraries produced by the HsBaSlicer CMake build.

How to use:

1. Build native libs for Android (example using CMake):

   mkdir -p build-android && cd build-android
   cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake -DVCPKG_CHAINLOAD_TOOLCHAIN_FILE=$ANDROID_NDK/build/cmake/android.toolchain.cmake -DVCPKG_TARGET_TRIPLET=arm64-android -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-21 ..
   cmake --build . --config Release

2. Copy produced .so into the app jniLibs directory (or let CI do it):

   cp ${CMAKE_BINARY_DIR}/android_libs/arm64-v8a/*.so android/app/src/main/jniLibs/arm64-v8a/

3. Open the `android` folder in Android Studio and run or build the APK.

Note: This is a minimal skeleton. You may want to add a Gradle wrapper, signing configs, and a small Java/Kotlin Activity that loads the native library via System.loadLibrary("HsBaSlicer").

CI trigger: 2025-10-18T00:00:00Z
