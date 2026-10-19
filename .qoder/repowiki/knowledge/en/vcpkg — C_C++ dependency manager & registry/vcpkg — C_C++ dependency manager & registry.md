---
kind: external_dependency
name: vcpkg — C/C++ dependency manager & registry
slug: vcpkg
category: external_dependency
category_hints:
    - framework_behavior
scope:
    - '**'
source_files:
    - vcpkg.json
    - CMakePresets.json
    - overlay-ports/
    - triplets/
---

### Identity
Microsoft vcpkg is the project's declared dependency manager; `CMakePresets.json` sets `CMAKE_TOOLCHAIN_FILE` to `$env{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake`, and all third-party packages are listed in `vcpkg.json`.

### Role in this repo
- Declares runtime deps (Boost, clipper2, opencv, eigen3, libigl, freetype, miniz, openvdb, protobuf, rapidjson, tinyxml2, yaml-cpp, lua, openssl, sqlpp11, magic-enum) and the optional `copyleft` feature pulling CGAL / libigl[cgal] / opencascade.
- Android/iOS builds use `VCPKG_TARGET_TRIPLET` + `VCPKG_CHAINLOAD_TOOLCHAIN_FILE` pointing at the NDK toolchain.
- Local dev uses a 52 GB vcpkg binary cache; CI fetches the registry over HTTPS.

### Stable integration shape
- Add a new library by editing `vcpkg.json`; do not hand-edit `CMakeLists.txt` for find_package calls.
- Platform-scoped deps go inside `{ name, platform }` blocks (Windows/Linux/macOS vs Android/iOS).
- The `copyleft` feature toggles GPL/LGPL kernels and switches the project license from MIT to GPL-3.0-or-later at configure time.
- Custom ports live under `overlay-ports/`; custom triplets under `triplets/`.

### Gotchas observed in conversation
- vcpkg registry fetch can hang when the git worktree is not checked out even if the object store has the baseline commit; the "fail fast then continue offline" behavior means you must force a checkout before running the full pipeline.
- On WSL behind a local MITM proxy (SteamTools), the GitHub CA bundle needs the local root cert merged into a custom CA file so `git`/`curl` used by vcpkg can validate HTTPS.