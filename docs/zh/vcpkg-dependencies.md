# VCPKG依赖包平台兼容性说明

本文档说明了`vcpkg.json`中各依赖包的平台兼容性配置。

## 许可证

项目许可取决于构建是否链接 Copyleft 内核：

- **CGAL**（`GPL-3.0-or-later`）与 **OpenCascade**（`LGPL-2.1-only`）为 Copyleft 许可，仅在下方依赖矩阵所列平台可用（桌面 Windows/Linux/macOS，另 Android 可用 CGAL）。
- 包含任一 Copyleft 内核的构建，采用 **GPL-3.0-or-later** 许可。
- 不包含任何 Copyleft 内核的构建（iOS 与游戏主机），采用 **MIT** 许可。

在 `vcpkg.json` 中通过顶层 `license` 为 `MIT`、外加 `license` 为 `GPL-3.0-or-later` 的 `copyleft` feature 来表达；该 feature 在 `windows | linux | osx | android` 平台默认启用。构建的实际许可以在配置期生成到 `version.cpp`，并通过 `GetVersionInfo()` / `HsBaGetVersionJson()` 查询。

## 平台排除说明

### Android平台排除的包

以下包在Android平台上被排除，因为它们要么有兼容性问题，要么在移动平台上不适用：

- **boost-locale, boost-log, boost-dll, boost-nowide**: 这些Boost组件在Android上有已知的兼容性问题
- **fontconfig**: Android有自己的字体管理系统，不需要fontconfig
- **bit7z**: 移动应用通常不需要7z压缩功能
- **opencascade**: OpenCASCADE过于复杂，不适合移动平台
- **sqlpp11** (MySQL/PostgreSQL特性): VCPKG的MySQL和PostgreSQL不适用交叉编译
- **vcpkg-pkgconfig-get-modules**: Android构建系统不需要pkg-config

### iOS平台排除的包

- **bit7z**: iOS应用通常不需要7z压缩功能
- **opencv**: OpenCV在iOS上有构建复杂性问题
- **opencascade**: OpenCASCADE过于复杂，不适合iOS平台
- **sqlpp11** (MySQL/PostgreSQL特性): VCPKG的MySQL和PostgreSQL不适用交叉编译

## CMakeLists.txt依赖修复

### 修复的问题

1. **OpenCV条件查找**: 在iOS平台上条件化OpenCV查找，避免构建失败
2. **Boost dll条件链接**: cadmodel模块中条件化Boost::dll链接
3. **boost-date-time添加**: 添加缺失的boost-date-time依赖
4. **boost-log链接修复**: 修复fileoperator模块中boost-log的链接

### 依赖矩阵

| 组件 | Windows | Linux | macOS | Android | iOS |
| ------ | --------- | ------- | ------- | --------- | ---- |
| boost-log | ✓ | ✓ | ✓ | ✗ | ✗ |
| boost-locale | ✓ | ✓ | ✓ | ✗ | ✗ |
| boost-dll | ✓ | ✓ | ✓ | ✗ | ✗ |
| boost-nowide | ✓ | ✓ | ✓ | ✗ | ✗ |
| boost-date-time | ✓ | ✓ | ✓ | ✓ | ✓ |
| opencv | ✓ | ✓ | ✓ | ✓ | ✗ |
| opencascade | ✓ | ✓ | ✓ | ✗ | ✗ |
| sqlpp11 (MySQL/PG) | ✓ | ✓ | ✓ | ✗ | ✗ |
| opencascade | ✓ | ✓ | ✓ | ✗ | ✗ |

## 构建建议

1. **桌面平台** (Windows/Linux/macOS): 所有包都应该能正常构建
2. **Android平台**: 使用简化版的依赖，主要依赖SQLite
3. **iOS平台**: 排除了一些复杂的库以确保构建成功

## 故障排除

如果遇到构建失败：

1. 检查是否所有系统依赖都已安装 (特别是macOS上的autoconf-archive)
2. 确认vcpkg已正确引导和更新
3. 检查CI日志中的具体错误信息
4. 对于移动平台，考虑是否真的需要某些被排除的库
