# API 参考文档

<cite>
**本文档中引用的文件**  
- [IModel.hpp](file://base/IModel.hpp)
- [PolygonFill.hpp](file://2D/PolygonFill.hpp)
- [PolygonFill.cpp](file://2D/PolygonFill.cpp)
- [IZipper.hpp](file://fileoperator/IZipper.hpp)
- [IUnzipper.hpp](file://fileoperator/IUnzipper.hpp)
- [zipper.hpp](file://fileoperator/zipper.hpp)
- [unzipper.hpp](file://fileoperator/unzipper.hpp)
- [bit7z_zipper.hpp](file://fileoperator/bit7z_zipper.hpp)
- [bit7z_unzipper.hpp](file://fileoperator/bit7z_unzipper.hpp)
- [logger.hpp](file://logger/logger.hpp)
- [logger.cpp](file://logger/logger.cpp)
- [app_config.hpp](file://utils/app_config.hpp)
- [app_config.cpp](file://utils/app_config.cpp)
- [polygon_fill_test.cpp](file://tests/PolygonFill/polygon_fill_test.cpp)
- [zipper_test.cpp](file://tests/FilesOperator/zipper_test.cpp)
</cite>

## 目录
1. [IModel 接口](#imodel-接口)
2. [PolygonFill 类](#polygonfill-类)
3. [Zipper 和 Unzipper 类](#zipper-和-unzipper-类)
4. [LoggerSingletone 类](#loggersingletone-类)
5. [AppConfigSingletone 类](#appconfigsingletone-类)

## IModel 接口

`IModel` 是 HsBaSlicer 中所有3D模型的抽象基类，定义了模型加载、保存、变换和查询的基本接口。所有具体模型类（如 `OcctModel`、`IglModel`）都必须实现此接口。

### 虚函数签名与功能描述

| 函数签名 | 参数类型 | 返回值 | 功能描述 |
| :--- | :--- | :--- | :--- |
| `virtual bool Load(std::string_view fileName) = 0;` | `fileName`: 要加载的文件路径（UTF-8编码） | `bool`: 加载成功返回 `true`，失败返回 `false` | 从指定文件加载模型数据。支持的格式由具体实现决定。 |
| `virtual bool Save(std::string_view fileName, const ModelFormat format) const = 0;` | `fileName`: 要保存的文件路径（UTF-8编码）<br>`format`: 指定的保存格式（如 `ModelFormat::STL`、`ModelFormat::OBJ`） | `bool`: 保存成功返回 `true`，失败返回 `false` | 将模型以指定格式保存到文件。 |
| `virtual void Translate(const Eigen::Vector3f& translation) = 0;` | `translation`: 三维平移向量 | `void` | 对模型进行平移变换。 |
| `virtual void Rotate(const Eigen::Quaternionf& rotation) = 0;` | `rotation`: 四元数表示的旋转变换 | `void` | 对模型进行旋转变换。 |
| `virtual void Scale(const float scale) = 0;` | `scale`: 统一缩放因子 | `void` | 对模型进行等比缩放。 |
| `virtual void Scale(const Eigen::Vector3f& scale) = 0;` | `scale`: 三维缩放向量（X, Y, Z轴分别缩放） | `void` | 对模型进行非等比缩放。 |
| `virtual void Transform(const Eigen::Isometry3f& transform) = 0;` | `transform`: Eigen的等距变换矩阵 | `void` | 对模型应用一个包含旋转和平移的变换。 |
| `virtual void Transform(const Eigen::Matrix4f& transform) = 0;` | `transform`: 4x4齐次变换矩阵 | `void` | 对模型应用一个完整的4x4齐次变换。 |
| `virtual void Transform(const Eigen::Transform<float, 3, Eigen::Affine>& transform) = 0;` | `transform`: Eigen的仿射变换对象 | `void` | 对模型应用一个仿射变换。 |
| `virtual void BoundingBox(Eigen::Vector3f& min, Eigen::Vector3f& max) const = 0;` | `min`: 输出参数，返回包围盒的最小角点坐标<br>`max`: 输出参数，返回包围盒的最大角点坐标 | `void` | 计算并返回模型的轴对齐包围盒（AABB）。 |
| `virtual float Volume() const = 0;` | 无 | `float`: 模型的体积 | 计算并返回模型的体积。对于非封闭模型，行为未定义。 |
| `virtual std::pair<Eigen::MatrixXf,Eigen::MatrixXi> TriangleMesh() const = 0;` | 无 | `std::pair<Eigen::MatrixXf,Eigen::MatrixXi>`: 第一个矩阵是顶点坐标（Nx3），第二个矩阵是面片索引（Mx3） | 将模型转换为三角网格表示，返回顶点和面片数据。 |

**典型调用方式**
```cpp
// 假设 model 是一个 IModel 的派生类实例
Eigen::Vector3f translation(10.0f, 0.0f, 0.0f);
model.Translate(translation);

Eigen::Vector3f min, max;
model.BoundingBox(min, max);
float volume = model.Volume();
```

**Section sources**
- [IModel.hpp](file://base/IModel.hpp#L19-L34)
- [OcctModel.cpp](file://cadmodel/OcctModel.cpp#L141-L194)
- [IglModel.cpp](file://meshmodel/IglModel.cpp#L54-L78)

## PolygonFill 类

`PolygonFill` 提供了一系列用于生成2D多边形填充路径的静态函数。这些函数广泛用于生成切片路径。

### 填充方法与参数配置

| 函数签名 | 参数配置 | 功能描述 |
| :--- | :--- | :--- |
| `Polygons LineFill(const Polygons& poly, double spacing, double angle_deg, double lineThickness = 0.5);` | `poly`: 要填充的多边形<br>`spacing`: 填充线之间的间距<br>`angle_deg`: 填充线的角度（度）<br>`lineThickness`: 线条厚度（可选，默认0.5） | 生成一组平行直线填充路径。每条路径都是一个独立的两点线段。 |
| `Polygons SimpleZigzagFill(const Polygons& poly, double spacing, double angle_deg, double lineThickness = 0.5);` | `poly`: 要填充的多边形<br>`spacing`: 填充线之间的间距<br>`angle_deg`: 填充线的角度（度）<br>`lineThickness`: 线条厚度（可选，默认0.5） | 生成一个简单的锯齿形填充路径。通过连接相邻扫描线的线段中心点来形成连续的锯齿路径。 |
| `Polygons ZigzagFill(const Polygons& poly, double spacing, double angle_deg, double lineThickness = 0.5);` | `poly`: 要填充的多边形<br>`spacing`: 填充线之间的间距<br>`angle_deg`: 填充线的角度（度）<br>`lineThickness`: 线条厚度（可选，默认0.5） | 生成一个更复杂的锯齿形填充路径。与 `SimpleZigzagFill` 相比，它会尝试连接相邻行的线段以形成更长的连续路径，并在无法连接时使用桥接。 |
| `Polygons OffsetFill(const Polygons& poly, double spacing, Clipper2Lib::JoinType join_type = Clipper2Lib::JoinType::Square);` | `poly`: 要填充的多边形<br>`spacing`: 偏移间距<br>`join_type`: 偏移连接类型（Square, Bevel, Round, Miter） | 通过连续向内偏移多边形边界来生成填充路径。 |
| `Polygons CompositeOffsetFill(const Polygons& poly, double spacing, double offsetStep, int outwardCount, int inwardCount, FillMode mode, double angle_deg, double lineThickness = 0.5, Clipper2Lib::JoinType join_type = Clipper2Lib::JoinType::Square);` | `poly`: 要填充的多边形<br>`spacing`: 填充间距<br>`offsetStep`: 偏移步长<br>`outwardCount`: 向外偏移的次数<br>`inwardCount`: 向内偏移的次数<br>`mode`: 填充模式（Line, SimpleZigzag, Zigzag）<br>`angle_deg`: 填充角度<br>`lineThickness`: 线条厚度<br>`join_type`: 连接类型 | 先进行多次偏移，然后对最内层的偏移多边形应用指定模式的填充。 |
| `Polygons HybridFill(const Polygons& poly, double spacing, double offsetStep, int outwardCount, int inwardCount, FillMode mode, double angle_deg, double lineThickness = 0.5, Clipper2Lib::JoinType join_type = Clipper2Lib::JoinType::Square);` | 参数同 `CompositeOffsetFill` | 先进行偏移，然后对最后一次偏移产生的所有多边形应用指定模式的填充。 |
| `Polygons LuaCustomFill(const Polygons& poly, const std::string& scriptPath, const std::string& functionName = "generate_fill", double lineThickness = 0.5);` | `poly`: 要填充的多边形<br>`scriptPath`: Lua脚本文件路径<br>`functionName`: 脚本中填充函数的名称（可选，默认"generate_fill"）<br>`lineThickness`: 线条厚度（可选，默认0.5） | 使用外部Lua脚本定义的函数来生成填充路径。 |
| `Polygons LuaCustomFillString(const Polygons& poly, const std::string& script, const std::string& functionName = "generate_fill", double lineThickness = 0.5);` | `poly`: 要填充的多边形<br>`script`: Lua脚本代码字符串<br>`functionName`: 脚本中填充函数的名称（可选，默认"generate_fill"）<br>`lineThickness`: 线条厚度（可选，默认0.5） | 使用内联Lua脚本代码来生成填充路径。 |

**使用示例**
```cpp
// 创建一个正方形多边形
PolygonD polyd;
polyd.emplace_back(Point2{0,0});
polyd.emplace_back(Point2{10000,0});
polyd.emplace_back(Point2{10000,10000});
polyd.emplace_back(Point2{0,10000});
Polygons poly = { Integerization(polyd) };

// 生成45度角的直线填充
Polygons lines = LineFill(poly, 1000.0, 45.0);

// 生成锯齿形填充
Polygons zigzag = ZigzagFill(poly, 1000.0, 45.0);

// 使用Lua脚本进行自定义填充
std::filesystem::path script_path = "custom_fill.lua";
Polygons lua_fill = LuaCustomFill(poly, script_path.string(), "customFill", 100.0);
```

**Section sources**
- [PolygonFill.hpp](file://2D/PolygonFill.hpp#L12-L42)
- [PolygonFill.cpp](file://2D/PolygonFill.cpp#L15-L800)
- [polygon_fill_test.cpp](file://tests/PolygonFill/polygon_fill_test.cpp#L11-L117)

## Zipper 和 Unzipper 类

`Zipper` 和 `Unzipper` 类提供了对ZIP等压缩格式的读写支持。`Zipper` 用于创建和写入压缩包，`Unzipper` 用于读取和解压压缩包。

### 压缩/解压接口

#### Zipper 类
`Zipper` 类继承自 `IZipper` 接口，使用 `miniz` 库实现。

| 函数签名 | 参数类型 | 返回值 | 功能描述 |
| :--- | :--- | :--- | :--- |
| `Zipper(MinizCompression compression);` | `compression`: 压缩级别（No, Fast, Tight） | 构造函数 | 创建一个 `Zipper` 实例，并指定压缩级别。 |
| `void AddByteFile(std::string_view name, const std::string& data) override;` | `name`: 压缩包内的文件名<br>`data`: 要添加的文件数据（字节流） | `void` | 将内存中的字节数据作为文件添加到压缩包。如果文件名已存在，抛出异常。 |
| `void AddFile(std::string_view name, std::string_view path) override;` | `name`: 压缩包内的文件名<br>`path`: 要添加的本地文件路径 | `void` | 将本地磁盘上的文件添加到压缩包。如果文件名已存在，抛出异常。 |
| `void AddByteFileIgnoreDuplicate(std::string_view name, const std::string& data) override;` | `name`: 压缩包内的文件名<br>`data`: 要添加的文件数据（字节流） | `void` | 添加字节数据，如果文件名已存在，则在原文件名后添加 `_duplicate` 后缀。 |
| `void AddFileIgnoreDuplicate(std::string_view name, std::string_view path) override;` | `name`: 压缩包内的文件名<br>`path`: 要添加的本地文件路径 | `void` | 添加本地文件，如果文件名已存在，则在原文件名后添加 `_duplicate` 后缀。 |
| `void Save(std::string_view filePath) override;` | `filePath`: 要保存的压缩包文件路径 | `void` | 将所有已添加的文件写入到指定的压缩包文件中。 |

#### Unzipper 类
`Unzipper` 类是一个具体的类，用于解压ZIP文件。

| 函数签名 | 参数类型 | 返回值 | 功能描述 |
| :--- | :--- | :--- | :--- |
| `static std::shared_ptr<Unzipper> Create();` | 无 | `std::shared_ptr<Unzipper>`: 新创建的 `Unzipper` 实例 | 静态工厂方法，创建并返回一个 `Unzipper` 实例。 |
| `void ReadFromFile(std::string_view path, bool reopen = false);` | `path`: ZIP文件的路径<br>`reopen`: 是否重新打开文件（可选，默认`false`） | `void` | 从指定路径加载ZIP文件。 |
| `std::shared_ptr<UnzipperStream> GetStream(std::string_view part_file);` | `part_file`: 压缩包内文件的名称 | `std::shared_ptr<UnzipperStream>`: 指向文件流的智能指针 | 获取压缩包内指定文件的输入流，可用于读取文件内容。 |
| `static void SetMaxMemSize(size_t size = 1024 * 1024 * 1024);` | `size`: 最大内存使用量（字节） | `void` | 设置解压时允许使用的最大内存量（默认1GB）。超过此限制的文件将被解压到临时目录。 |

**支持的格式和错误处理机制**
- **支持的格式**: `Zipper` 和 `Unzipper` 主要支持ZIP格式。项目中还提供了 `Bit7zZipper` 和 `Bit7zUnzipper` 类，它们基于 `bit7z` 库，支持更广泛的格式，包括：7z, XZ, BZIP2, GZIP, TAR, RAR, ISO等。
- **错误处理机制**:
  - `Zipper` 在添加重复文件名时会抛出 `InvalidArgumentError` 异常。
  - `Save` 方法在压缩失败时会抛出 `IOError` 异常。
  - `Unzipper` 在文件不存在或路径错误时会抛出 `IOError` 异常。
  - 所有操作都应使用 `try-catch` 块进行异常处理。

**典型调用方式**
```cpp
// 创建并使用 Zipper
HsBa::Slicer::Zipper zipper(HsBa::Slicer::MinizCompression::Tight);
zipper.AddByteFile("test.txt", "Hello, World!");
zipper.Save("output.zip");

// 创建并使用 Unzipper
auto unzipper = HsBa::Slicer::Unzipper::Create();
unzipper->ReadFromFile("output.zip");
auto stream = unzipper->GetStream("test.txt");
std::string content((std::istreambuf_iterator<char>(*stream)), std::istreambuf_iterator<char>());
```

**Section sources**
- [IZipper.hpp](file://fileoperator/IZipper.hpp#L14-L18)
- [IUnzipper.hpp](file://fileoperator/IUnzipper.hpp#L111-L122)
- [zipper.hpp](file://fileoperator/zipper.hpp#L28-L43)
- [unzipper.hpp](file://fileoperator/unzipper.hpp#L15-L34)
- [bit7z_zipper.hpp](file://fileoperator/bit7z_zipper.hpp#L46-L59)
- [bit7z_unzipper.hpp](file://fileoperator/bit7z_unzipper.hpp#L20-L49)
- [zipper_test.cpp](file://tests/FilesOperator/zipper_test.cpp#L24-L143)

## LoggerSingletone 类

`LoggerSingletone` 是一个单例模式的日志记录器，提供不同级别的日志输出功能。

### 静态日志方法与日志级别控制

| 静态方法 | 参数类型 | 功能描述 |
| :--- | :--- | :--- |
| `static void Log(std::string_view message, const int log_lv, const std::source_location& location = std::source_location::current());` | `message`: 日志消息<br>`log_lv`: 日志级别（0-5）<br>`location`: 源代码位置（自动填充） | 通用日志记录函数，根据 `log_lv` 参数输出不同级别的日志。 |
| `static void LogDebug(std::string_view message, const std::source_location& location = std::source_location::current());` | `message`: 日志消息<br>`location`: 源代码位置（自动填充） | 输出调试级别（Debug）的日志。 |
| `static void LogInfo(std::string_view message, const std::source_location& location = std::source_location::current());` | `message`: 日志消息<br>`location`: 源代码位置（自动填充） | 输出信息级别（Info）的日志。 |
| `static void LogWarning(std::string_view message, const std::source_location& location = std::source_location::current());` | `message`: 日志消息<br>`location`: 源代码位置（自动填充） | 输出警告级别（Warning）的日志。 |
| `static void LogError(std::string_view message, const std::source_location& location = std::source_location::current());` | `message`: 日志消息<br>`location`: 源代码位置（自动填充） | 输出错误级别（Error）的日志。 |

**日志级别控制**
- 日志级别从低到高依次为：Trace(0), Debug(1), Info(2), Warning(3), Error(4), Fatal(5)。
- 实际输出的日志级别由配置文件 `logcfg.ini` 决定。在 `logger.cpp` 中，程序会读取该配置文件中的 `log_level` 或 `log_level_debug` 值来设置最低输出级别。
- 如果配置文件不存在，会使用默认级别（Debug模式下为1，Release模式下为3）。
- 日志可以同时输出到控制台和文件（如果 `use_log_file` 配置为 `true`）。

**典型调用方式**
```cpp
// 使用静态方法
HsBa::Slicer::Log::LoggerSingletone::LogInfo("Application started.");
HsBa::Slicer::Log::LoggerSingletone::LogError("Failed to load model.");

// 使用用户定义字面量 (UDL)
"Processing data..."_log_info();
"An error occurred!"_log_error();
```

**Section sources**
- [logger.hpp](file://logger/logger.hpp#L21-L25)
- [logger.cpp](file://logger/logger.cpp#L154-L262)

## AppConfigSingletone 类

`AppConfigSingletone` 是一个单例模式的配置管理器，用于提供运行时配置信息。

### 运行时配置方法

| 方法 | 参数类型 | 返回值 | 功能描述 |
| :--- | :--- | :--- | :--- |
| `std::string GetSevenZPath() const;` | 无 | `std::string`: 7-Zip可执行文件的路径 | 获取7-Zip命令行工具（7z.exe）的路径。该路径在程序启动时初始化，用于调用外部7-Zip工具进行压缩/解压操作。 |

**说明**
- 该类通过单例模式确保全局只有一个配置实例。
- `GetSevenZPath()` 方法是线程安全的，使用 `std::shared_mutex` 保护对 `sevenZ_path_` 成员的访问。
- 具体的路径值是在程序其他地方初始化的，`AppConfigSingletone` 负责提供一个统一的访问接口。

**典型调用方式**
```cpp
// 获取7-Zip工具的路径
std::string sevenZPath = HsBa::Slicer::AppConfigSingletone::GetInstance().GetSevenZPath();
// 使用该路径调用7-Zip命令
// system((sevenZPath + " a archive.zip file.txt").c_str());
```

**Section sources**
- [app_config.hpp](file://utils/app_config.hpp#L14)
- [app_config.cpp](file://utils/app_config.cpp#L32-L37)